#include "candidate_provider.h"
#include "config.h"
#include "macro_table.h"
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
#include <deque>
#include <fnmatch.h>
#include <functional>
#include <string>
#include <vector>

namespace fcitx {

namespace {

/* Codepoint-wise helpers on UTF-8 strings. */
std::string lastChars(const std::string &s, size_t n) {
    size_t len = utf8::length(s);
    if (len <= n) return s;
    return s.substr(utf8::ncharByteLength(s.begin(), len - n));
}
std::string firstChars(const std::string &s, size_t n) {
    return s.substr(0, utf8::ncharByteLength(s.begin(),
                                             std::min(n, utf8::length(s))));
}
std::string dropLastChars(const std::string &s, size_t n) {
    size_t len = utf8::length(s);
    if (n >= len) return {};
    return s.substr(0, utf8::ncharByteLength(s.begin(), len - n));
}
bool endsWith(const std::string &s, const std::string &suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

/* What is before the cursor in the app, as far as instant commit needs to
 * know: the app's own surrounding-text reports, plus the edits this engine
 * sent since (which a fast typist outruns -- the app reports each change
 * back some milliseconds later). Only the last kTail characters are kept.
 *
 * states_ runs from the last state the app confirmed (front) to the state
 * once every edit sent so far is applied (back). A report matching one of
 * them is the app catching up; a report matching none means something else
 * changed the text or moved the cursor -- a click, the app editing its own
 * text, a selection -- and the word in progress may no longer be ours to
 * edit. */
class ClientText {
public:
    static constexpr size_t kTail = 64;

    /* Whether the app's text is known well enough to edit in place. */
    bool known() const { return !states_.empty() && !stale_; }
    /* Whether the app has said anything since this was created or since it
     * stopped reporting. */
    bool heardFrom() const { return !states_.empty() || stale_; }
    bool diverged() const { return diverged_; }
    bool selection() const { return selection_; }
    void startWord() { diverged_ = false; }

    /* A key the engine let through may have changed anything, so the text
     * is unknown until the app reports a state that is not one of those
     * expected before that key (reports still in flight from before it
     * must not be taken for the new state). */
    void forget() {
        stale_ = true;
        diverged_ = true;
    }

    void report(const SurroundingText &surrounding) {
        if (!surrounding.isValid()) {
            states_.clear();
            stale_ = false;
            diverged_ = true;
            selection_ = false;
            after_.clear();
            return;
        }
        const auto &text = surrounding.text();
        size_t cursorBytes =
            utf8::ncharByteLength(text.begin(), surrounding.cursor());
        std::string before = text.substr(0, cursorBytes);
        State now{lastChars(before, kTail), utf8::length(before) <= kTail};
        /* This engine only ever edits before the cursor, so what follows
         * it stays as the app last reported it. */
        std::string rest = text.substr(cursorBytes);
        after_ = firstChars(rest, kTail);
        afterComplete_ = utf8::length(rest) <= kTail;
        selection_ = surrounding.anchor() != surrounding.cursor();
        if (!selection_) {
            /* The earliest match: the same text can recur ("ti", "tie",
             * then "ti" again on the way to "tiê"), and reports arrive in
             * order, so a later report can still match a later state. */
            for (size_t i = 0; i < states_.size(); ++i) {
                if (matches(now, states_[i])) {
                    states_.erase(states_.begin(), states_.begin() + i);
                    return;
                }
            }
        }
        /* Not the app catching up with this engine's edits: something else
         * changed the text or moved the cursor (or, after forget(), this is
         * the first report from after that key). */
        if (heardFrom() || selection_) diverged_ = true;
        states_.assign(1, now);
        stale_ = false;
    }

    /* This engine deleted `del` characters before the cursor and inserted
     * `ins`. */
    void edit(size_t del, const std::string &ins) {
        if (!known()) return;
        State next = states_.back();
        if (del > utf8::length(next.tail)) {
            forget();
            return;
        }
        next.tail = dropLastChars(next.tail, del) + ins;
        if (utf8::length(next.tail) > kTail) {
            next.tail = lastChars(next.tail, kTail);
            next.complete = false;
        }
        states_.push_back(std::move(next));
        if (states_.size() > 32) states_.pop_front();
    }

    /* Preedit mode: where a word started -- the text this engine expects
     * around the cursor then -- to compare later reports with. */
    struct Mark {
        bool valid = false;
        std::string before, after; /* up to kTail characters each side */
        bool beforeComplete = false, afterComplete = false;
    };
    Mark mark() const {
        if (!known()) return {};
        return {true, states_.back().tail, after_, states_.back().complete,
                afterComplete_};
    }

    /* Whether `surrounding` has `word` as text exactly where `at` was --
     * the text that was before the cursor, the word, the text that was
     * after it -- with the cursor somewhere else. A cursor right after the
     * word is not taken as a move: an app that (against the text-input
     * protocol) reports its preedit as part of its text looks just like
     * that while the word is being typed. */
    static bool holdsWordAt(const Mark &at, const SurroundingText &surrounding,
                            const std::string &word) {
        if (!at.valid || word.empty() || !surrounding.isValid()) return false;
        const auto &text = surrounding.text();
        std::string before = text.substr(
            0, utf8::ncharByteLength(text.begin(), surrounding.cursor()));
        const std::string upToWord = at.before + word;
        const std::string placed = upToWord + at.after;
        size_t pos;
        if (at.beforeComplete) {
            pos = text.compare(0, placed.size(), placed) == 0 ? 0
                                                              : std::string::npos;
        } else {
            pos = text.find(placed);
        }
        if (pos == std::string::npos) return false;
        if (at.afterComplete && pos + placed.size() != text.size()) return false;
        return before.size() != pos + upToWord.size();
    }

private:
    struct State {
        std::string tail;
        bool complete; /* tail is everything before the cursor */
    };
    static bool matches(const State &report, const State &expected) {
        return expected.complete
                   ? report.complete && report.tail == expected.tail
                   : endsWith(report.tail, expected.tail);
    }

    /* From the last state the app confirmed (front) to the state once every
     * edit sent so far is applied (back). */
    std::deque<State> states_;
    std::string after_; /* after the cursor in the last report */
    bool afterComplete_ = false;
    bool stale_ = false;
    bool diverged_ = false;
    bool selection_ = false;
};

/* Per-InputContext state, attached via InputContextProperty — this is how
 * addons carry custom state per IC in Fcitx5; a real InputContext is always
 * owned and created by the connected frontend (X11/Wayland/DBus), never by
 * the input method engine itself. */
class XrnikeyState : public InputContextProperty {
public:
    explicit XrnikeyState(int mode) : state_(xk_state_new(mode)), mode_(mode) {}
    ~XrnikeyState() override {
        if (state_) xk_state_free(state_);
    }

    XKState *state() { return state_; }
    int mode() const { return mode_; }
    /* Telex/VNI is chosen when the core state is created, so a change of
     * mode needs a new one. */
    void setMode(int mode) {
        if (mode == mode_) return;
        if (state_) xk_state_free(state_);
        state_ = xk_state_new(mode);
        mode_ = mode;
        composing_ = false;
    }
    void reset() {
        if (state_) xk_state_reset(state_);
        composing_ = false;
        shown_.clear();
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
    void startWord(bool instant) {
        instantForWord_ = instant;
        inField_.clear();
        wordMark_ = instant ? ClientText::Mark{} : client_.mark();
        shown_.clear();
        client_.startWord();
    }

    /* Preedit mode: where the word started, and every preedit shown for
     * it with the core's keys at the time. */
    struct Shown {
        std::string keys, preedit;
    };
    const ClientText::Mark &wordMark() const { return wordMark_; }
    const std::vector<Shown> &shown() const { return shown_; }
    void noteShown(std::string keys, std::string preedit) {
        shown_.push_back({std::move(keys), std::move(preedit)});
    }

    ClientText &client() { return client_; }
    /* Instant mode: the characters of the word in progress that this
     * engine has put in the field. Nothing outside it is ever deleted. */
    const std::string &inField() const { return inField_; }
    void setInField(std::string v) { inField_ = std::move(v); }

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
    int mode_;
    bool sawSurrounding_ = false;
    bool composing_ = false;
    bool instantForWord_ = false;
    ClientText client_;
    ClientText::Mark wordMark_;
    std::vector<Shown> shown_;
    std::string inField_;
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
              return new XrnikeyState(configuredMode());
          }) {
        instance_->inputContextManager().registerProperty("xrnikeyState",
                                                            &factory_);
        /* Every surrounding-text report from an app, to check that the text
         * instant commit is about to edit is still the text it typed. */
        surroundingWatcher_ = instance_->watchEvent(
            EventType::InputContextSurroundingTextUpdated,
            EventWatcherPhase::Default, [this](Event &event) {
                auto *ic = static_cast<InputContextEvent &>(event).inputContext();
                auto *icState = ic->propertyFor(&factory_);
                icState->noteSurrounding(ic->surroundingText().isValid());
                icState->client().report(ic->surroundingText());
                if (icState->composing() && !icState->instantForWord()) {
                    takeWordCommittedByApp(ic, icState);
                }
            });
        reloadConfig();
    }

    ~XrnikeyEngine() override = default;

    FCITX_ADDON_DEPENDENCY_LOADER(notifications, instance_->addonManager());

    void activate(const InputMethodEntry &, InputContextEvent &event) override {
        event.inputContext()->updatePreedit();
    }

    /* Called on focus-out and when the user switches input method. The
     * word in progress is kept, never dropped (see leaveWord). */
    void deactivate(const InputMethodEntry &, InputContextEvent &event) override {
        auto *ic = event.inputContext();
        leaveWord(ic, ic->propertyFor(&factory_),
                  event.type() == EventType::InputContextFocusOut);
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

        /* Pick a suggestion (Ctrl+1..9 by default). Checked before the
         * word-ending keys below, which include every Ctrl chord; with no
         * suggestion showing, the chord stays an app shortcut. */
        if (auto candidateList = ic->inputPanel().candidateList()) {
            int idx = keyEvent.key().keyListIndex(
                config_->candidateSelectionKeys.value());
            if (idx >= 0 && idx < candidateList->size()) {
                candidateList->candidate(idx).select(ic);
                keyEvent.filterAndAccept();
                return;
            }
        }

        /* Modifier-only keys carry no character and do not end a word. */
        if (keyEvent.key().isModifier()) return;

        auto *icState = ic->propertyFor(&factory_);
        /* A mode changed in the settings (or by "Reload config") applies to
         * fields that are already open, from their next key; it used to
         * be fixed when the field was created, so it needed the app
         * restarted. A word in progress is kept first. */
        if (icState->mode() != configuredMode()) {
            leaveWord(ic, icState, false);
            icState->setMode(configuredMode());
        }
        XKState *state = icState->state();
        if (!state) return;
        applyOptions(state);
        /* Cheap, and covers states made since (new fields, a mode change). */
        xk_state_set_models(state, viLm_.get(), enLm_.get());

        /* A key the engine lets through may change the app's text in any
         * way (Return, BackSpace, a paste shortcut, an arrow key moving the
         * cursor), so what instant commit knows of it is void until the app
         * reports again. */
        struct ForgetIfPassed {
            KeyEvent &key;
            XrnikeyState *icState;
            ~ForgetIfPassed() {
                if (!key.accepted()) icState->client().forget();
            }
        } forgetIfPassed{keyEvent, icState};

        /* Password and other sensitive fields get every key untouched: no
         * Telex/VNI, no preedit (which would show the password), no
         * suggestions. Fcitx5 itself already switches Password fields to
         * the keyboard layout unless AllowInputMethodForPassword is set,
         * but not Sensitive ones (text-input-v3 SENSITIVE_DATA), and with
         * that option on, "maatj" in a password box used to become "mật". */
        if (ic->capabilityFlags().testAny(CapabilityFlag::PasswordOrSensitive)) {
            endWord(ic, icState);
            return;
        }

        /* The core composes with printable keys and BackSpace. Every other
         * key -- Return, Tab, Esc, arrows, Home/End, PageUp/Down, Delete,
         * F-keys, and Ctrl/Alt/Super shortcuts -- ends the word: finish it
         * so the text the user sees stays in the field, then let the key
         * through to the app. Return used to be swallowed whenever a word
         * was pending, so every chat message needed Enter twice; the rest
         * left the word buffered, and the next key edited a word the
         * cursor had already moved away from.
         *
         * Letting the key through after the commit is ordered: both reach
         * the app over the same connection (the D-Bus batch reply, the
         * Wayland input-method + virtual-keyboard requests, the XIM
         * connection), commit first. (Ctrl+Space and other IME switch keys
         * are handled by Fcitx5 before reaching an engine.) */
        if (!isComposingKey(keyEvent.key())) {
            /* Return and Tab end an abbreviation too; Tab is then used up,
             * as in Bamboo (it completes the word instead of moving on). */
            if (isAbbreviationEnd(keyEvent.key()) &&
                expandAbbreviation(ic, icState, "")) {
                if (keyEvent.key().check(FcitxKey_Tab)) {
                    keyEvent.filterAndAccept();
                }
                return;
            }
            endWord(ic, icState);
            return;
        }

        uint32_t keysym = KeySym(keyEvent.key().sym());
        /* Space or punctuation after an abbreviation: the expansion, then
         * the character. */
        if (endsWord(keysym) &&
            expandAbbreviation(ic, icState,
                               std::string(1, static_cast<char>(keysym)))) {
            keyEvent.filterAndAccept();
            return;
        }
        uint32_t mods = 0;
        if (keyEvent.key().states() & KeyState::Ctrl) mods |= 0x04;
        if (keyEvent.key().states() & KeyState::Shift) mods |= 0x01;
        if (keyEvent.key().states() & KeyState::Alt) mods |= 0x08;

        /* Instant commit (Unikey-style) puts each keystroke's result in the
         * field at once and corrects it in place, instead of holding the word
         * in a preedit until Space. Correcting means deleting what was typed,
         * so it is only done where that is safe, decided at the start of each
         * word and latched for it:
         *  - the app has actually sent surrounding text (not merely claimed
         *    the capability: the Wayland frontend claims it for every app,
         *    and foot, Alacritty and kitty never send any), with no
         *    selection;
         *  - it is not a terminal (content purpose Terminal, or a name in
         *    AppOverrideList): there "the text before the cursor" is a
         *    shell's, and edits race the shell's echo;
         * and, while the word is typed, the app's reports must keep matching
         * what the engine typed (ClientText). If they stop -- a click
         * elsewhere, the app rewriting or selecting text -- the word is left
         * as it is and this key starts a new word, in preedit. Nothing is
         * ever deleted that this engine did not put there, and BackSpace is
         * never synthesised: it reaches the app by another path than the
         * commit, and konsole scrambled "chaof" into "choa" that way. */
        /* An app's first report can predate this addon being loaded. */
        if (ic->surroundingText().isValid()) {
            icState->noteSurrounding(true);
            if (!icState->client().heardFrom()) {
                icState->client().report(ic->surroundingText());
            }
        }
        if (icState->composing() && icState->instantForWord() &&
            !inPlaceEditSafe(icState)) {
            clearWord(ic, icState);
            icState->startWord(false);
        } else if (!icState->composing()) {
            icState->startWord(instantAllowed(ic, icState));
        }
        bool instantMode = icState->instantForWord();
        xk_state_set_instant_commit(state, instantMode ? 1 : 0);

        XKResult result = xk_process_key(state, keysym, mods);

        if (result.reset) {
            if (result.consumed) keyEvent.filterAndAccept();
            clearWord(ic, icState);
            return;
        }

        if (instantMode &&
            !editInPlace(ic, icState, result.delete_before, result.commit)) {
            /* Not expected after the check above; if the edit still cannot
             * be made safely, redo this key as the start of a preedit word. */
            clearWord(ic, icState);
            icState->startWord(false);
            instantMode = false;
            xk_state_set_instant_commit(state, 0);
            result = xk_process_key(state, keysym, mods);
        }

        if (result.consumed) {
            keyEvent.filterAndAccept();
        }

        if (instantMode) {
            ic->inputPanel().reset();
            ic->updatePreedit();
        } else {
            /* The core empties its word when it commits one; a commit with
             * a preedit still left is a word that went on (a very long one
             * is closed and the rest kept as typed), so the state is not
             * reset here. */
            if (result.commit[0]) {
                commitText(ic, icState, result.commit);
            }

            showPreedit(ic, result.preedit);
            if (result.preedit[0]) {
                icState->noteShown(coreKeys(state), result.preedit);
            }
        }

        /* Update candidates if prediction enabled */
        /* The clear below must run even when prediction is OFF. Previously
         * the whole block was skipped in that case, so a list already on
         * screen stayed there forever -- turning suggestions off left the
         * last ones visible. A one-character prefix is also skipped: it
         * matches most of the dictionary and is pure noise. */
        std::vector<XrnikeyCandidate> candidates;
        /* No suggestions where the app asked for no spell checking (code
         * editors, URL and search boxes, identifiers). */
        if (config_->predictionEnabled.value() &&
            !ic->capabilityFlags().test(CapabilityFlag::NoSpellCheck) &&
            result.preedit[0] &&
            utf8::length(std::string(result.preedit)) >= 2) {
            candidates = candidateProvider_.getCandidates(
                result.preedit, config_->predictionMaxCandidates.value(),
                icState->history());
        }
        {
            if (!candidates.empty()) {
                auto list = std::make_unique<CommonCandidateList>();
                list->setSelectionKey(config_->candidateSelectionKeys.value());
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

    /* The core's keys for the word in progress, as typed ("aa" while it
     * shows "â"). */
    static std::string coreKeys(XKState *state) {
        char *typed = xk_state_get_keys(state);
        std::string keys = typed ? typed : "";
        std::free(typed);
        return keys;
    }

    /* Preedit mode, and the app reported its text. Fcitx5's Wayland
     * frontend sends no Reset when the user clicks elsewhere in a
     * text-input-v3 app; there is only this report. Chromium and Firefox
     * commit their preedit themselves at the click, so the report has the
     * word as text where it was typed, and the word is the app's now:
     * committing it again would type it twice, and going on composing put
     * the next key on it ("ab chaof", a click at the start and "s " gave
     * "cháo ab chào"). So forget it. The report can come a key or two late,
     * after the app committed an earlier preedit of the word; the keys
     * typed since then start the next word. An app that moves the preedit
     * along with the cursor (GTK) or drops it (Qt) leaves no such trace,
     * and the word goes on as before. */
    void takeWordCommittedByApp(InputContext *ic, XrnikeyState *icState) {
        const auto &shown = icState->shown();
        for (size_t k = shown.size(); k-- > 0;) {
            if (!ClientText::holdsWordAt(icState->wordMark(),
                                         ic->surroundingText(),
                                         shown[k].preedit)) {
                continue;
            }
            const std::string done = shown[k].keys;
            std::string keys = coreKeys(icState->state());
            std::string later;
            if (keys.compare(0, done.size(), done) == 0) {
                later = keys.substr(done.size());
            }
            clearWord(ic, icState);
            if (later.empty()) return;
            XKState *state = icState->state();
            icState->startWord(false);
            xk_state_set_instant_commit(state, 0);
            XKResult r{};
            for (char c : later) {
                r = xk_process_key(state, static_cast<unsigned char>(c), 0);
                if (r.commit[0]) commitText(ic, icState, r.commit);
                if (r.preedit[0]) icState->noteShown(coreKeys(state), r.preedit);
            }
            showPreedit(ic, r.preedit);
            icState->setLastPreedit(r.preedit);
            icState->setComposing(r.preedit[0] != '\0');
            return;
        }
    }

    /* The typing options are read per key, so a settings change applies to
     * every input context at once (setting an unchanged option is free). */
    void applyOptions(XKState *state) {
        xk_state_set_option(state, XK_OPT_AUTO_RESTORE, *config_->autoRestore);
        xk_state_set_option(state, XK_OPT_MODERN_STYLE, *config_->modernStyle);
        xk_state_set_option(state, XK_OPT_FREE_MARKING, *config_->freeMarking);
        xk_state_set_option(state, XK_OPT_W_SHORTCUT, *config_->wShortcut);
    }

    int configuredMode() const {
        return config_->inputMode.value() == InputMode::VNI ? XK_MODE_VNI
                                                            : XK_MODE_TELEX;
    }

    /* A printable key the core takes as the end of a word: Space, and
     * any non-letter except VNI's tone and mark digits. */
    bool endsWord(uint32_t keysym) const {
        if (keysym == ' ') return true;
        if (keysym < 0x21 || keysym > 0x7e) return false;
        bool letter = (keysym >= 'a' && keysym <= 'z') ||
                      (keysym >= 'A' && keysym <= 'Z');
        bool digit = keysym >= '0' && keysym <= '9';
        return !letter && !(digit && configuredMode() == XK_MODE_VNI);
    }

    static bool isAbbreviationEnd(const Key &key) {
        return (key.check(FcitxKey_Return) || key.check(FcitxKey_KP_Enter) ||
                key.check(FcitxKey_Tab));
    }

    /* If Macro is on and the word in progress is an abbreviation, replace
     * it with its expansion followed by `trailing`, and end the word. The
     * keys as typed are tried first, then the word as shown (so both "aa"
     * and "đc", typed "ddc", can be abbreviations). */
    bool expandAbbreviation(InputContext *ic, XrnikeyState *icState,
                            const std::string &trailing) {
        if (!config_->macro.value() || !icState->composing()) return false;
        macros_.refresh();
        if (macros_.empty()) return false;
        char *pending = xk_state_get_preedit(icState->state());
        std::string shown = pending ? pending : "";
        std::free(pending);
        if (shown.empty()) return false;
        bool matchCase = config_->capitalizeMacro.value();
        /* The core's keys, not a copy kept here: BackSpace takes a
         * character's tone and mark keys with it, so "cas" BackSpace is
         * "c" and must not still match "ca". */
        auto expansion = macros_.expand(coreKeys(icState->state()), matchCase);
        if (!expansion) expansion = macros_.expand(shown, matchCase);
        if (!expansion) return false;
        std::string text = *expansion + trailing;
        if (icState->instantForWord()) {
            if (!editInPlace(ic, icState,
                             static_cast<int>(utf8::length(icState->inField())),
                             text.c_str())) {
                return false;
            }
        } else {
            commitText(ic, icState, text);
        }
        icState->noteWord(*expansion);
        clearWord(ic, icState);
        return true;
    }

    static bool isComposingKey(const Key &key) {
        if (key.states().testAny(KeyStates{
                KeyState::Ctrl, KeyState::Alt, KeyState::Super,
                KeyState::Super2, KeyState::Hyper, KeyState::Hyper2,
                KeyState::Meta})) {
            return false;
        }
        uint32_t sym = static_cast<uint32_t>(key.sym());
        return (sym >= 0x20 && sym <= 0x7e) || sym == FcitxKey_BackSpace;
    }

    /* Finish the word in progress, keeping what the user sees. In preedit
     * mode that means committing it. In instant mode it is already in the
     * field; the core still gets its end-of-word pass (xk_state_flush) so a
     * correction it makes only at word end -- restoring the keys of a word
     * that is not Vietnamese -- is applied in place. */
    void endWord(InputContext *ic, XrnikeyState *icState) {
        XKState *state = icState->state();
        char *pending = state ? xk_state_get_preedit(state) : nullptr;
        std::string shown = pending ? pending : "";
        std::free(pending);
        if (!shown.empty()) {
            XKResult r = xk_state_flush(state);
            if (icState->instantForWord()) {
                /* A last correction is skipped if it cannot be made safely. */
                editInPlace(ic, icState, r.delete_before, r.commit);
            } else {
                commitText(ic, icState,
                           r.commit[0] ? std::string(r.commit) : shown);
            }
            icState->noteWord(shown);
        }
        clearWord(ic, icState);
    }

    /* Forget the word in progress without touching the field. */
    void clearWord(InputContext *ic, XrnikeyState *icState) {
        icState->reset();
        icState->setLastPreedit("");
        ic->inputPanel().reset();
        ic->updatePreedit();
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    bool instantAllowed(InputContext *ic, XrnikeyState *icState) {
        return config_->instantCommit.value() && !appForcesPreedit(ic) &&
               !ic->capabilityFlags().test(CapabilityFlag::Terminal) &&
               ic->capabilityFlags().test(CapabilityFlag::SurroundingText) &&
               icState->sawSurrounding() && ic->surroundingText().isValid() &&
               icState->client().known() && !icState->client().selection();
    }

    static bool inPlaceEditSafe(XrnikeyState *icState) {
        return icState->client().known() && !icState->client().diverged() &&
               !icState->client().selection();
    }

    /* AppOverrideList entries are shell-style patterns, matched without case
     * against the program name and against its last dot-separated part:
     * on Hyprland the Wayland frontend reports the app id from the
     * compositor's window list ("org.wezfurlong.wezterm", "Alacritty"),
     * the D-Bus one the process name ("konsole"). An empty name (no app id
     * available) matches nothing; the surrounding-text checks still apply. */
    bool appForcesPreedit(InputContext *ic) const {
        const std::string &program = ic->program();
        if (program.empty()) return false;
        std::string lastPart = program.substr(program.rfind('.') + 1);
        for (const auto &pattern : config_->appOverrideList.value()) {
            if (fnmatch(pattern.c_str(), program.c_str(), FNM_CASEFOLD) == 0 ||
                fnmatch(pattern.c_str(), lastPart.c_str(), FNM_CASEFOLD) == 0) {
                return true;
            }
        }
        return false;
    }

    void commitText(InputContext *ic, XrnikeyState *icState,
                    const std::string &text) {
        ic->commitString(text);
        icState->client().edit(0, text);
    }

    /* Instant mode: replace the last `del` characters this engine put in
     * the field for the word in progress with `commit`. Returns false,
     * changing nothing, when that is not known to be safe. */
    bool editInPlace(InputContext *ic, XrnikeyState *icState, int del,
                     const char *commit) {
        if (del > 0) {
            const std::string &mine = icState->inField();
            if (!inPlaceEditSafe(icState) ||
                static_cast<size_t>(del) > utf8::length(mine)) {
                return false;
            }
            /* The Wayland frontend turns this character count into bytes
             * from its own copy of the surrounding text, which lags behind
             * fast typing: "ê" counted as the "e" it replaced deleted half
             * a character. The characters being deleted are exactly the
             * end of what this engine typed, so hand it that. */
            unsigned int len = utf8::length(mine);
            SurroundingText reported = ic->surroundingText();
            ic->surroundingText().setText(mine, len, len);
            ic->deleteSurroundingText(-del, del);
            ic->surroundingText() = reported;
            icState->setInField(dropLastChars(mine, del));
            icState->client().edit(del, "");
        }
        if (commit && commit[0]) {
            commitText(ic, icState, commit);
            icState->setInField(icState->inField() + commit);
        }
        return true;
    }

    /* Replace the syllable in progress with a chosen candidate. In instant
     * mode that syllable is already in the document and has to be deleted
     * first; in preedit mode it only ever lived in the panel. */
    void commitCandidate(InputContext *ic, const std::string &word) {
        auto *icState = ic->propertyFor(&factory_);
        if (icState->instantForWord()) {
            editInPlace(ic, icState,
                        static_cast<int>(utf8::length(icState->inField())),
                        word.c_str());
        } else {
            commitText(ic, icState, word);
        }
        clearWord(ic, icState);
    }

    /* A click in the field, or the app resetting the input method. */
    void reset(const InputMethodEntry &, InputContextEvent &event) override {
        auto *ic = event.inputContext();
        leaveWord(ic, ic->propertyFor(&factory_),
                  ic->capabilityFlags().test(
                      CapabilityFlag::ClientUnfocusCommit));
    }

    /* The cursor or focus leaves the word in progress: keep it in the field
     * and start the next key on a fresh word. Instant mode: it is already in
     * the field, so only forget it -- and never delete anything, since the
     * cursor may be somewhere else by now. Preedit mode: commit it, unless
     * someone else already does. `inlineCommittedElsewhere` says whether a
     * preedit shown inline is committed by the client or by Fcitx5:
     *  - on Reset, clients with ClientUnfocusCommit (the fcitx5 GTK and Qt
     *    modules) commit their inline preedit themselves first;
     *  - on focus-out, those clients do the same, and for the others Fcitx5
     *    commits the client preedit server-side before calling deactivate;
     *  - on switching input method nobody does.
     * A preedit the app could not show inline (no Preedit capability, so it
     * sat in Fcitx5's popup) is always ours to commit. Previously all three
     * paths dropped the word: click mid-word and it was gone, and on
     * focus-out the core even kept it, so the next key landed on it. */
    void leaveWord(InputContext *ic, XrnikeyState *icState,
                   bool inlineCommittedElsewhere) {
        bool shownInline = !ic->inputPanel().clientPreedit().empty();
        if (!icState->instantForWord() &&
            !(shownInline && inlineCommittedElsewhere)) {
            endWord(ic, icState);
        } else {
            clearWord(ic, icState);
        }
    }

    /* Show the word in progress inline in the app, underlined, where the
     * app supports it (as fcitx5-bamboo and -unikey do); otherwise in
     * Fcitx5's popup. Previously it always went to the popup: no inline
     * text, and nothing for the client to commit on focus-out. */
    void showPreedit(InputContext *ic, const std::string &text) {
        auto &panel = ic->inputPanel();
        Text preedit;
        bool inlinePreedit = ic->capabilityFlags().test(CapabilityFlag::Preedit);
        if (!text.empty()) {
            preedit.append(text, inlinePreedit ? TextFormatFlag::Underline
                                               : TextFormatFlag::NoFlag);
            preedit.setCursor(static_cast<int>(preedit.textLength()));
        }
        if (inlinePreedit) {
            panel.setClientPreedit(preedit);
            panel.setPreedit(Text());
        } else {
            panel.setPreedit(preedit);
            panel.setClientPreedit(Text());
        }
        ic->updatePreedit();
    }

    void filterKey(const InputMethodEntry &, KeyEvent &) override {}

    void reloadConfig() override {
        readAsIni(*config_, StandardPathsType::PkgConfig,
                  "conf/xrnikey5.conf");
        candidateProvider_.setConfig(config_);
        macros_.refresh();
        /* Loads the data once, then re-reads the user's words on every reload.
         * XRNIKEY_DATA_DIR in the environment points it at an uninstalled
         * build's data (the addon tests use this). */
        const char *dataDir = std::getenv("XRNIKEY_DATA_DIR");
        const std::string dir = dataDir && *dataDir ? dataDir : XRNIKEY_DATA_DIR;
        candidateProvider_.load(dir);
        /* The language models that choose between a word's English and
         * Vietnamese readings in context (data/lm/README.md). Loaded once:
         * they are read-only, and every field's state points at them.
         * Without them the core keeps its per-word rule. */
        if (!viLm_ && !enLm_) {
            viLm_.reset(xk_lm_open((dir + "/lm/vi.xklm").c_str()));
            enLm_.reset(xk_lm_open((dir + "/lm/en.xklm").c_str()));
        }
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
    MacroTable macros_;
    std::unique_ptr<XKLm, decltype(&xk_lm_free)> viLm_{nullptr, &xk_lm_free};
    std::unique_ptr<XKLm, decltype(&xk_lm_free)> enLm_{nullptr, &xk_lm_free};
    FactoryFor<XrnikeyState> factory_;
    std::unique_ptr<HandlerTableEntry<EventHandler>> surroundingWatcher_;
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
