#include "vietkey5.h"
#include "candidate_provider.h"
#include <fcitx/inputcontext.h>
#include <fcitx/userinterface.h>
#include <fcitx/text.h>
#include <fcitx-utils/utf8.h>
#include <vietkey.h>
#include <unordered_map>
#include <string>

namespace fcitx {

class VietKeyInputContext : public InputContext {
public:
    VietKeyInputContext(VietKeyEngine *engine, InputContextManager &icManager)
        : InputContext(engine, icManager), engine_(engine) {
        state_ = vk_state_new(VK_MODE_TELEX);
    }

    ~VietKeyInputContext() override {
        if (state_) vk_state_free(state_);
    }

    VKState* state() { return state_; }
    void resetState() {
        if (state_) vk_state_reset(state_);
    }

private:
    VietKeyEngine *engine_;
    VKState *state_;
};

class VietKeyEngine : public InputMethodEngineV3 {
public:
    VietKeyEngine(Instance *instance)
        : instance_(instance),
          config_(std::make_shared<VietKeyConfig>()) {
        reloadConfig();
    }

    ~VietKeyEngine() override = default;

    void activate(const InputMethodEntry &entry,
                  InputContextEvent &event) override {
        auto *ic = event.inputContext();
        ic->updatePreedit();
    }

    void deactivate(const InputMethodEntry &entry,
                    InputContextEvent &event) override {
        auto *ic = event.inputContext();
        ic->updatePreedit();
    }

    void keyEvent(const InputMethodEntry &entry, KeyEvent &keyEvent) override {
        auto *ic = static_cast<VietKeyInputContext *>(keyEvent.inputContext());
        if (!ic) return;

        VKState *state = ic->state();
        if (!state) return;

        uint32_t keysym = KeySym(keyEvent.key().sym());
        uint32_t mods = 0;
        if (keyEvent.key().states() & KeyState::Ctrl) mods |= 0x04;
        if (keyEvent.key().states() & KeyState::Shift) mods |= 0x01;
        if (keyEvent.key().states() & KeyState::Alt) mods |= 0x08;

        VKResult result = vk_process_key(state, keysym, mods);

        if (result.consumed) {
            keyEvent.filterAndAccept();
        }

        if (result.reset) {
            ic->resetState();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
            return;
        }

        if (result.commit[0]) {
            ic->commitString(result.commit);
            ic->resetState();
        }

        if (result.preedit[0] || result.commit[0]) {
            Text preedit;
            if (result.preedit[0]) {
                preedit.append(result.preedit);
                preedit.setCursor(utf8Length(result.preedit));
            }
            ic->inputPanel().setPreedit(preedit);
        } else {
            ic->inputPanel().reset();
        }

        /* Update candidates if prediction enabled */
        if (config_->predictionEnabled.value() && result.preedit[0]) {
            auto candidates = candidateProvider_.getCandidates(
                result.preedit, config_->predictionMaxCandidates.value());
            if (!candidates.empty()) {
                auto list = std::make_unique<CommonCandidateList>();
                for (size_t i = 0; i < candidates.size() && i < 9; ++i) {
                    list->append(CandidateWord(Text(candidates[i].text)));
                }
                ic->inputPanel().setCandidateList(std::move(list));
            }
        }

        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
    }

    void reset(const InputMethodEntry &entry,
               InputContextEvent &event) override {
        auto *ic = static_cast<VietKeyInputContext *>(event.inputContext());
        if (ic) {
            ic->resetState();
            ic->inputPanel().reset();
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
        }
    }

    void filterKey(const InputMethodEntry &entry, KeyEvent &keyEvent) override {}

    void reloadConfig() override {
        readAsJson(config_, "conf/vietkey5.conf");
        candidateProvider_.setConfig(config_);
    }

    const Configuration *getConfig() const override { return config_.get(); }
    void setConfig(const RawConfig &config) override {
        config_->load(config, true);
        safeSaveAsJson(config_, "conf/vietkey5.conf");
        candidateProvider_.setConfig(config_);
    }

    InputContext *createIC(InputContextManager &icManager) override {
        return new VietKeyInputContext(this, icManager);
    }

private:
    Instance *instance_;
    std::shared_ptr<VietKeyConfig> config_;
    CandidateProvider candidateProvider_;
};

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::VietKeyEngineFactory);
