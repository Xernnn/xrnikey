#include "candidate_provider.h"
#include "config.h"
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/standardpaths.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/addonfactory.h>
#include <notifications_public.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/surroundingtext.h>
#include <fcitx/text.h>
#include <fcitx/userinterface.h>
#include <xrnikey.h>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

namespace fcitx {

/* Per-InputContext state, attached via InputContextProperty — this is how
 * addons carry custom state per IC in Fcitx5; a real InputContext is always
 * owned and created by the connected frontend (X11/Wayland/DBus), never by
 * the input method engine itself. */
class XrnikeyState : public InputContextProperty {
public:
    explicit XrnikeyState(int mode) : state_(xk_state_new(mode)) {}
    ~XrnikeyState() override {
        if (state_) xk_state_free(state_);
    }

    XKState *state() { return state_; }
    void reset() {
        if (state_) xk_state_reset(state_);
        composing_ = false;
    }

    /* Whether this input context has EVER handed us valid surrounding text.
     * Sticky on purpose: an app that supports it may still not have sent any
     * by the first keystroke, and flipping mechanism mid-word would corrupt
     * the edit. Apps that never send it (terminals) stay false forever. */
    bool sawSurrounding() const { return sawSurrounding_; }
    void noteSurrounding(bool valid) { if (valid) sawSurrounding_ = true; }

    /* The in-place vs preedit choice is latched for the duration of a word. */
    bool composing() const { return composing_; }
    void setComposing(bool v) { composing_ = v; }
    bool instantForWord() const { return instantForWord_; }
    void setInstantForWord(bool v) { instantForWord_ = v; }

    /* The last couple of finished words, as n-gram context for ranking the
     * next one. Kept per input context so switching windows does not carry
     * one document's phrasing into another. */
    const std::vector<std::string> &history() const { return history_; }
    void noteWord(const std::string &w) {
        if (w.empty()) return;
        history_.push_back(w);
        if (history_.size() > 2) history_.erase(history_.begin());
    }
    const std::string &lastPreedit() const { return lastPreedit_; }
    void setLastPreedit(std::string v) { lastPreedit_ = std::move(v); }

private:
    XKState *state_;
    bool sawSurrounding_ = false;
    bool composing_ = false;
    bool instantForWord_ = false;
    std::vector<std::string> history_;
    std::string lastPreedit_;
};

/* A candidate the user can actually pick. The stock DisplayOnlyCandidateWord
 * renders but has an empty select(), so the candidate window stayed inert
 * even once it had data behind it. */
class XrnikeyCandidateWord : public CandidateWord {
public:
    XrnikeyCandidateWord(
        std::string text,
        std::function<void(InputContext *, const std::string &)> commit)
        : CandidateWord(Text(text)), text_(std::move(text)),
          commit_(std::move(commit)) {}

    void select(InputContext *ic) const override { commit_(ic, text_); }

private:
    std::string text_;
    std::function<void(InputContext *, const std::string &)> commit_;
};

class XrnikeyEngine : public InputMethodEngineV3 {
public:
    XrnikeyEngine(Instance *instance)
        : instance_(instance), config_(std::make_shared<XrnikeyConfig>()),
          factory_([this](InputContext &) {
              return new XrnikeyState(config_->inputMode.value() ==
                                               InputMode::VNI
                                           ? XK_MODE_VNI
                                           : XK_MODE_TELEX);
          }) {
        instance_->inputContextManager().registerProperty("xrnikeyState",
                                                            &factory_);
        for (int i = 0; i < 9; ++i) {
            selectionKeys_.emplace_back(
                static_cast<KeySym>(FcitxKey_1 + i));
        }
        reloadConfig();
    }

    ~XrnikeyEngine() override = default;

    FCITX_ADDON_DEPENDENCY_LOADER(notifications, instance_->addonManager());

    void activate(const InputMethodEntry &, InputContextEvent &event) override {
        event.inputContext()->updatePreedit();
    }

    void deactivate(const InputMethodEntry &, InputContextEvent &event) override {
        event.inputContext()->updatePreedit();
    }

    void keyEvent(const InputMethodEntry &, KeyEvent &keyEvent) override {
        auto *ic = keyEvent.inputContext();
        if (!ic) return;

        /* Fcitx5 delivers keyEvent for BOTH press and release. Two things go
         * wrong if releases are not dropped here:
         *   1. every keystroke reaches xk_process_key twice, so a single 'a'
         *      composes as "aa" -> "â";
         *   2. filterAndAccept()ing a release swallows it, so the client
         *      still believes the key is held and starts auto-repeating it —
         *      which floods the compositor and looks like a frozen screen.
         * Only presses drive composition. */
        if (keyEvent.isRelease()) return;

        /* Toggle word suggestions. Checked BEFORE the shortcut guard below,
         * because the binding is itself a modifier chord and would otherwise
         * be handed straight to the application. */
        /* normalize() both sides so the binding matches however it is
         * written in the config -- "Control+Alt+p" and "Control+Alt+P" are
         * different keys to Fcitx5 (the latter implies Shift). */
        if (keyEvent.key().normalize().check(
                config_->togglePrediction.value().normalize())) {
            bool on = !config_->predictionEnabled.value();
            config_->predictionEnabled.setValue(on);
            safeSaveAsIni(*config_, StandardPathsType::PkgConfig,
                          "conf/xrnikey5.conf");
            if (!on) ic->inputPanel().setCandidateList(nullptr);
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            if (auto *n = notifications()) {
                /* sendNotification, not showTip: a tip is shown once per id
                 * and then suppressed forever, which is useless for state
                 * that flips. replaceId reuses the same popup instead of
                 * stacking one per toggle. */
                notifyId_ = n->call<INotifications::sendNotification>(
                    _("Xrnikey"), notifyId_, "xrnikey", _("Xrnikey"),
                    on ? _("Word suggestions on")
                       : _("Word suggestions off"),
                    std::vector<std::string>(), 2000,
                    NotificationActionCallback(),
                    NotificationClosedCallback());
            }
            keyEvent.filterAndAccept();
            return;
        }

        /* Modifier-only keys carry no character, and Ctrl/Alt/Super chords are
         * application shortcuts — never consume either. (Ctrl+Space and other
         * IME switch keys are handled by Fcitx5 before reaching an engine.) */
        if (keyEvent.key().isModifier()) return;
        if (keyEvent.key().states().testAny(
                KeyStates{KeyState::Ctrl, KeyState::Alt, KeyState::Super}))
            return;

        auto *icState = ic->propertyFor(&factory_);
        XKState *state = icState->state();
        if (!state) return;

        /* Number keys pick a candidate. Telex only: VNI spells its tones and
         * vowel marks WITH digits ("a1" -> á), so 1-9 there belong to the
         * engine and must never be stolen by the candidate window. */
        if (config_->inputMode.value() == InputMode::Telex) {
            if (auto candidateList = ic->inputPanel().candidateList()) {
                int idx = keyEvent.key().keyListIndex(selectionKeys_);
                if (idx >= 0 && idx < candidateList->size()) {
                    candidateList->candidate(idx).select(ic);
                    keyEvent.filterAndAccept();
                    return;
                }
            }
        }

        uint32_t keysym = KeySym(keyEvent.key().sym());
        uint32_t mods = 0;
        if (keyEvent.key().states() & KeyState::Ctrl) mods |= 0x04;
        if (keyEvent.key().states() & KeyState::Shift) mods |= 0x01;
        if (keyEvent.key().states() & KeyState::Alt) mods |= 0x08;

        /* Instant-commit (Unikey-style): correct previously committed text
         * in place via surrounding-text deletion instead of buffering a
         * visible preedit until the word is finished. Requires the focused
         * app to support SurroundingText; otherwise fall back to the
         * classic preedit/composition-buffer behavior. Re-evaluated every
         * key since capability/config can change while the IC is alive. */
        /* Unikey-style instant commit corrects already-committed text in
         * place, which means deleting it again. That delete is only safe
         * where the app supplies surrounding text: the text-input protocol
         * then applies it atomically with the commit. Without it the only
         * tool is synthesised BackSpace keys, and those reach the app by a
         * different channel than the commit with no ordering guarantee --
         * konsole reproducibly scrambled "chaof" into "choa" that way.
         *
         * So the mechanism is chosen per app, from whether surrounding text
         * has actually been seen (not merely advertised -- konsole claims the
         * capability and never sends any), and latched for the whole word so
         * it cannot change mid-syllable. Apps that support it (Kate, browsers,
         * most GUI toolkits) get the seamless in-place edit; terminals fall
         * back to the classic preedit, which is the only thing correct there. */
        icState->noteSurrounding(ic->surroundingText().isValid());
        if (!icState->composing()) {
            const auto &overrides = config_->appOverrideList.value();
            bool forcedPreedit =
                std::find(overrides.begin(), overrides.end(),
                          ic->program()) != overrides.end();
            icState->setInstantForWord(
                !forcedPreedit && config_->instantCommit.value() &&
                ic->capabilityFlags().test(CapabilityFlag::SurroundingText) &&
                icState->sawSurrounding());
        }
        bool instantMode = icState->instantForWord();
        xk_state_set_instant_commit(state, instantMode ? 1 : 0);

        XKResult result = xk_process_key(state, keysym, mods);

        if (result.consumed) {
            keyEvent.filterAndAccept();
        }

        if (result.reset) {
            icState->reset();
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            return;
        }

        if (instantMode) {
            /* Remove the characters this engine previously committed for the
             * syllable in progress. deleteSurroundingText is the clean path --
             * one atomic edit, no synthetic input -- but it needs the app to
             * actually report what surrounds the cursor. Where it does not,
             * forward real BackSpace keys instead; that works in every app and
             * is what Unikey and ibus-bamboo's backspace mode do. */
            deleteCommitted(ic, result.delete_before);
            if (result.commit[0]) {
                ic->commitString(result.commit);
            }
            ic->inputPanel().reset();
        } else {
            if (result.commit[0]) {
                ic->commitString(result.commit);
                icState->reset();
            }

            if (result.preedit[0] || result.commit[0]) {
                Text preedit;
                if (result.preedit[0]) {
                    preedit.append(result.preedit);
                    preedit.setCursor(static_cast<int>(utf8::length(result.preedit)));
                }
                ic->inputPanel().setPreedit(preedit);
            } else {
                ic->inputPanel().reset();
            }
        }

        /* Update candidates if prediction enabled */
        /* The clear below must run even when prediction is OFF. Previously
         * the whole block was skipped in that case, so a list already on
         * screen stayed there forever -- turning suggestions off left the
         * last ones visible. A one-character prefix is also skipped: it
         * matches most of the dictionary and is pure noise. */
        std::vector<XrnikeyCandidate> candidates;
        if (config_->predictionEnabled.value() && result.preedit[0] &&
            utf8::length(std::string(result.preedit)) >= 2) {
            candidates = candidateProvider_.getCandidates(
                result.preedit, config_->predictionMaxCandidates.value(),
                icState->history());
        }
        {
            if (!candidates.empty()) {
                auto list = std::make_unique<CommonCandidateList>();
                list->setSelectionKey(selectionKeys_);
                list->setPageSize(config_->pageSize.value());
                list->setLayoutHint(
                    config_->candidateLayout.value() == CandidateLayout::Vertical
                        ? CandidateLayoutHint::Vertical
                        : CandidateLayoutHint::Horizontal);
                for (size_t i = 0; i < candidates.size() && i < 9; ++i) {
                    list->append<XrnikeyCandidateWord>(
                        candidates[i].text,
                        [this](InputContext *c, const std::string &w) {
                            commitCandidate(c, w);
                        });
                }
                ic->inputPanel().setCandidateList(std::move(list));
            } else {
                ic->inputPanel().setCandidateList(nullptr);
            }
        }

        /* A word just ended: remember it as context for the next one. */
        if (result.preedit[0] == '\0' && !icState->lastPreedit().empty()) {
            icState->noteWord(icState->lastPreedit());
        }
        icState->setLastPreedit(result.preedit);
        icState->setComposing(result.preedit[0] != '\0');

        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    /* Remove `count` codepoints of already-committed text before the cursor.
     * deleteSurroundingText is atomic with the commit where the app supplies
     * surrounding text; the BackSpace fallback is for apps that do not, and
     * is why instant mode is only enabled where surrounding text exists. */
    void deleteCommitted(InputContext *ic, int count) {
        if (count <= 0) return;
        if (ic->capabilityFlags().test(CapabilityFlag::SurroundingText) &&
            ic->surroundingText().isValid()) {
            ic->deleteSurroundingText(-count, count);
        } else {
            for (int i = 0; i < count; ++i) {
                ic->forwardKey(Key(FcitxKey_BackSpace), false);
                ic->forwardKey(Key(FcitxKey_BackSpace), true);
            }
        }
    }

    /* Replace the syllable in progress with a chosen candidate. In instant
     * mode that syllable is already in the document and has to be deleted
     * first; in preedit mode it only ever lived in the panel. */
    void commitCandidate(InputContext *ic, const std::string &word) {
        auto *icState = ic->propertyFor(&factory_);
        if (icState->instantForWord()) {
            char *cur = xk_state_get_preedit(icState->state());
            if (cur) {
                std::string shown(cur);
                std::free(cur);
                deleteCommitted(ic, static_cast<int>(utf8::length(shown)));
            }
        }
        ic->commitString(word);
        icState->reset();
        ic->inputPanel().reset();
        icState->setComposing(false);
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    void reset(const InputMethodEntry &, InputContextEvent &event) override {
        auto *ic = event.inputContext();
        auto *icState = ic->propertyFor(&factory_);
        icState->reset();
        ic->inputPanel().reset();
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    void filterKey(const InputMethodEntry &, KeyEvent &) override {}

    void reloadConfig() override {
        readAsIni(*config_, StandardPathsType::PkgConfig,
                  "conf/xrnikey5.conf");
        candidateProvider_.setConfig(config_);
        /* Loads the data once, then re-reads the user's words on every reload. */
        candidateProvider_.load(XRNIKEY_DATA_DIR);
    }

    const Configuration *getConfig() const override { return config_.get(); }
    void setConfig(const RawConfig &config) override {
        config_->load(config, true);
        safeSaveAsIni(*config_, StandardPathsType::PkgConfig,
                      "conf/xrnikey5.conf");
        candidateProvider_.setConfig(config_);
    }

private:
    Instance *instance_;
    std::shared_ptr<XrnikeyConfig> config_;
    CandidateProvider candidateProvider_;
    FactoryFor<XrnikeyState> factory_;
    KeyList selectionKeys_;
    uint32_t notifyId_ = 0;
};

class XrnikeyEngineFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        return new XrnikeyEngine(manager->instance());
    }
};

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::XrnikeyEngineFactory);
