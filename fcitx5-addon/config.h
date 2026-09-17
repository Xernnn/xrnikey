#pragma once

#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/key.h>
#include <string>
#include <vector>

namespace fcitx {

enum class InputMode { Telex, VNI };
FCITX_CONFIG_ENUM_NAME_WITH_I18N(InputMode, N_("Telex"), N_("VNI"));

enum class CandidateLayout { Horizontal, Vertical };
FCITX_CONFIG_ENUM_NAME_WITH_I18N(CandidateLayout, N_("Horizontal"),
                                  N_("Vertical"));

/* fcitx5's Option<> ships marshallers for bool/int/string/Key/... but not
 * double; add one so AutoCorrectThreshold can be a real fraction instead of
 * being rescaled into an int. */
inline void marshallOption(RawConfig &config, double value) {
    config = std::to_string(value);
}
inline bool unmarshallOption(double &value, const RawConfig &config, bool) {
    try {
        value = std::stod(config.value());
    } catch (...) {
        return false;
    }
    return true;
}

/* FCITX_SPECIALIZE_TYPENAME registers a free configTypeNameHelper overload,
 * but OptionTypeName<T>::get() only sees it through a using-declaration
 * fixed at optiontypename.h's own parse point, and ADL never applies to a
 * fundamental type like double* (no associated namespace). Specializing the
 * OptionTypeName template directly is the reliable extension point. */
template <>
struct OptionTypeName<double> {
    static std::string get() { return "Double"; }
};

FCITX_CONFIGURATION(
    VietKeyConfig,
    Option<InputMode, NoConstrain<InputMode>, DefaultMarshaller<InputMode>,
           InputModeI18NAnnotation>
        inputMode{this, "InputMode", _("Input Mode"), InputMode::Telex};
    Option<bool> autoSwitch{this, "AutoSwitch", _("Auto Switch"), true};
    Option<bool> instantCommit{this, "InstantCommit", _("Instant Commit"),
                                true};
    Option<bool> spellCheck{this, "SpellCheck", _("Spell Check"), true};
    Option<bool> predictionEnabled{this, "PredictionEnabled",
                                    _("Prediction Enabled"), true};
    Option<int, IntConstrain> predictionMaxCandidates{
        this, "PredictionMaxCandidates", _("Prediction Max Candidates"), 5,
        IntConstrain(1, 20)};
    Option<bool> emojiEnabled{this, "EmojiEnabled", _("Emoji Enabled"), true};
    Option<std::string> emojiTrigger{this, "EmojiTrigger", _("Emoji Trigger"),
                                      ":"};
    Option<bool> autoCorrect{this, "AutoCorrect", _("Auto Correct"), true};
    Option<double> autoCorrectThreshold{this, "AutoCorrectThreshold",
                                         _("Auto Correct Threshold"), 0.85};
    Option<bool> showFrequencyHint{this, "ShowFrequencyHint",
                                    _("Show Frequency Hint"), false};
    Option<CandidateLayout, NoConstrain<CandidateLayout>,
           DefaultMarshaller<CandidateLayout>, CandidateLayoutI18NAnnotation>
        candidateLayout{this, "CandidateLayout", _("Candidate Layout"),
                         CandidateLayout::Vertical};
    Option<int, IntConstrain> pageSize{this, "PageSize", _("Page Size"), 9,
                                        IntConstrain(1, 10)};
    Option<Key> quickSwitch{this, "QuickSwitch", _("Quick Switch"),
                             Key("Control+Shift")};
    Option<Key> commitKey{this, "CommitKey", _("Commit Key"), Key("space")};
    /* Flips PredictionEnabled at runtime and writes it back to the config
     * file, so the candidate window can be turned off mid-sentence without
     * opening the settings UI. */
    Option<Key> togglePrediction{this, "TogglePrediction",
                                  _("Toggle Word Suggestions"),
                                  /* lowercase: an uppercase letter in a
                                   * Fcitx5 key string means Shift+letter, so
                                   * "Control+Alt+P" would only fire with
                                   * Shift held and plain Ctrl+Alt+p never
                                   * matched at all. */
                                  Key("Control+Alt+p")};
    /* Programs that must use the classic preedit instead of correcting text
     * in place: terminals and editors that report no usable surrounding
     * text, where in-place edits race and scramble the word. Matched against
     * the app's program name as Fcitx5 reports it. */
    Option<std::vector<std::string>> appOverrideList{
        this, "AppOverrideList", _("Force Preedit In These Apps"),
        {"code", "alacritty", "kitty", "foot", "wezterm", "terminal",
         "konsole"}};
);

} // namespace fcitx
