#pragma once

#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/configuration.h>
#include <fcitx/misc.h>

namespace fcitx {

FCITX_CONFIGURATION(
    VietKeyConfig,
    Option<InputMode, VietKeyInputMode, DefaultNamedConfigValue{"InputMode", "Telex"}> inputMode;
    Option<bool, VietKeyBool, DefaultNamedConfigValue{"AutoSwitch", "True"}> autoSwitch;
    Option<bool, VietKeyBool, DefaultNamedConfigValue{"SpellCheck", "True"}> spellCheck;
    Option<bool, VietKeyBool, DefaultNamedConfigValue{"PredictionEnabled", "True"}> predictionEnabled;
    Option<int, IntConfigValue, DefaultNamedConfigValue{"PredictionMaxCandidates", "5"}> predictionMaxCandidates;
    Option<bool, VietKeyBool, DefaultNamedConfigValue{"EmojiEnabled", "True"}> emojiEnabled;
    Option<std::string, Key, DefaultNamedConfigValue{"EmojiTrigger", ":"}> emojiTrigger;
    Option<bool, VietKeyBool, DefaultNamedConfigValue{"AutoCorrect", "True"}> autoCorrect;
    Option<double, DoubleConfigValue, DefaultNamedConfigValue{"AutoCorrectThreshold", "0.85"}> autoCorrectThreshold;
    Option<bool, VietKeyBool, DefaultNamedConfigValue{"ShowFrequencyHint", "False"}> showFrequencyHint;
    Option<CandidateLayout, VietKeyCandidateLayout, DefaultNamedConfigValue{"CandidateLayout", "Vertical"}> candidateLayout;
    Option<int, IntConfigValue, DefaultNamedConfigValue{"PageSize", "9"}> pageSize;
    Option<Key, Key, DefaultNamedConfigValue{"QuickSwitch", "Control+Shift"}> quickSwitch;
    Option<Key, Key, DefaultNamedConfigValue{"CommitKey", "space"}> commitKey;
    Option<std::string, KeyList, DefaultNamedConfigValue{"AppOverrideList", "code;alacritty;kitty;foot;wezterm;terminal;konsole"}> appOverrideList;
);

enum class InputMode { Telex, VNI };
enum class CandidateLayout { Horizontal, Vertical };

FCITX_CONFIGURATION_ENUM(InputMode, N_("Telex"), N_("VNI"));
FCITX_CONFIGURATION_ENUM(CandidateLayout, N_("Horizontal"), N_("Vertical"));

} // namespace fcitx
