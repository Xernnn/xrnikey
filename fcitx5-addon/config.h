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
    XrnkeyConfig,
    Option<InputMode, NoConstrain<InputMode>, DefaultMarshaller<InputMode>,
           InputModeI18NAnnotation>
        inputMode{this, "InputMode", _("Input Mode"), InputMode::Telex};
    /* Off by default until it has been tried in real apps: instant commit
     * edits text the app already has, and only the addon tests (a model
     * client) have exercised the safety checks so far. */
    Option<bool> instantCommit{this, "InstantCommit", _("Instant Commit"),
                                false};
    /* Typing rules, passed to the core (xk_state_set_option). */
    Option<bool> autoRestore{this, "AutoRestore",
                             _("Restore keys of words that are not Vietnamese"),
                             true};
    Option<bool> modernStyle{this, "ModernStyle",
                             _("Use oà, uý (instead of òa, úy)"), false};
    Option<bool> freeMarking{this, "FreeMarking",
                             _("Allow marks and tones after the final consonant"),
                             true};
    Option<bool> wShortcut{this, "WShortcut", _("Type w alone as ư"), true};
    /* Off by default: the dictionary is a small seed list, and a
     * suggestion window over every word is noise for most typing. */
    Option<bool> predictionEnabled{this, "PredictionEnabled",
                                    _("Prediction Enabled"), false};
    /* Keys that pick suggestion 1-9 on the current page. Never plain
     * digits: those are text (mp3, a1, 2024) in Telex and tone marks in
     * VNI, and taking them for picks turned "ch1" into "chị". */
    KeyListOption candidateSelectionKeys{
        this,
        "CandidateSelectionKeys",
        _("Pick Suggestion 1-9"),
        {Key("Control+1"), Key("Control+2"), Key("Control+3"),
         Key("Control+4"), Key("Control+5"), Key("Control+6"),
         Key("Control+7"), Key("Control+8"), Key("Control+9")},
        KeyListConstrain()};
    /* Abbreviations from ~/.local/share/xrnkey/macros.txt (xk-macro),
     * expanded when the word ends, as in Bamboo. */
    Option<bool> macro{this, "Macro", _("Expand Abbreviations"), false};
    /* "Ko" expands with a capital first letter, "KO" in capitals; "ko" as
     * written. */
    Option<bool> capitalizeMacro{this, "CapitalizeMacro",
                                 _("Match the Abbreviation's Case"), true};
    Option<int, IntConstrain> predictionMaxCandidates{
        this, "PredictionMaxCandidates", _("Prediction Max Candidates"), 5,
        IntConstrain(1, 20)};
    Option<bool> autoCorrect{this, "AutoCorrect", _("Auto Correct"), true};
    Option<double> autoCorrectThreshold{this, "AutoCorrectThreshold",
                                         _("Auto Correct Threshold"), 0.85};
    Option<CandidateLayout, NoConstrain<CandidateLayout>,
           DefaultMarshaller<CandidateLayout>, CandidateLayoutI18NAnnotation>
        candidateLayout{this, "CandidateLayout", _("Candidate Layout"),
                         CandidateLayout::Vertical};
    Option<int, IntConstrain> pageSize{this, "PageSize", _("Page Size"), 9,
                                        IntConstrain(1, 10)};
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
    /* Apps that always get the classic preedit, even with InstantCommit:
     * shell-style patterns, matched without case against the app id or
     * program name and against its last dot-separated part (so "wezterm"
     * covers org.wezfurlong.wezterm). Instant commit already falls back to
     * preedit in any app that never reports surrounding text (foot,
     * Alacritty and kitty send none; XIM cannot) or marks the field as a
     * terminal; the list is for apps that do report some but where
     * editing it in place is wrong or unreliable:
     *  - terminals: the text before the cursor belongs to the shell, and
     *    deleting it races the shell's own echo (konsole claims surrounding
     *    text and scrambled "chaof" into "choa");
     *  - VS Code and forks: Monaco's hidden textarea only mirrors part of
     *    the line (fcitx5-lotus #338 needed Monaco-specific fixes);
     *  - LibreOffice: its KDE backend's surrounding text is broken
     *    (LibreOffice bug 152519, typing with Unikey).
     * Browsers and Electron apps are left to the surrounding-text checks. */
    Option<std::vector<std::string>> appOverrideList{
        this, "AppOverrideList", _("Force Preedit In These Apps"),
        {"alacritty", "kitty", "foot", "footclient", "wezterm", "ghostty",
         "konsole", "terminal", "*-terminal", "xterm", "urxvt", "st",
         "st-256color", "terminator", "tilix", "kgx", "code", "code-*",
         "codium", "vscodium", "cursor", "libreoffice*", "soffice*"}};
);

} // namespace fcitx
