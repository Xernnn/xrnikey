#pragma once

#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/instance.h>
#include <fcitx/candidatelist.h>
#include <fcitx/action.h>
#include <memory>
#include "config.h"

namespace fcitx {

class VietKeyEngine : public InputMethodEngineV3 {
public:
    VietKeyEngine(Instance *instance);
    ~VietKeyEngine() override;

    void activate(const InputMethodEntryEntry &entry,
                  InputContextEvent &event) override;
    void deactivate(const InputMethodEntryEntry &entry,
                    InputContextEvent &event) override;
    void keyEvent(const InputMethodEntry &entry, KeyEvent &keyEvent) override;
    void reset(const InputMethodEntry &entry,
               InputContextEvent &event) override;
    void filterKey(const InputMethodEntry &entry, KeyEvent &keyEvent) override;

    auto factory() { return factory_; }

private:
    Instance *instance_;
    std::unique_ptr<VietKeyFactory> factory_;
    std::shared_ptr<VietKeyConfig> config_;
};

class VietKeyEngineFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        return new VietKeyEngine(manager->instance());
    }
};

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::VietKeyEngineFactory);
