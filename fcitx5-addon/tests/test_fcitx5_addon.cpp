/* End-to-end tests of the Fcitx5 addon: a real fcitx::Instance loads the
 * built libxrnikey5.so, and a fake client -- an InputContext with its own
 * text buffer -- types into it the way GTK, Qt and Wayland clients do.
 *
 * The client applies everything the engine sends (commits, preedit,
 * surrounding-text deletions, forwarded keys) to its document, and applies
 * every key the engine does NOT accept the way a text field would (Return
 * inserts a newline, BackSpace deletes, ...). So a test can say "typing
 * these keys leaves this text in the field", which is what a user sees.
 */
#include "config.h"

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fcitx-config/rawconfig.h>
#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/testing.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/surroundingtext.h>

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace fcitx;

namespace {

Instance *gInstance = nullptr;

AddonInstance *engine() {
    return gInstance->addonManager().addon("xrnikey5", true);
}

/* Set the addon's options: everything at its default except `overrides`. */
void configure(const std::map<std::string, std::string> &overrides = {}) {
    XrnikeyConfig defaults;
    RawConfig raw;
    defaults.save(raw);
    for (const auto &[key, value] : overrides) {
        raw.setValueByPath(key, value);
    }
    engine()->setConfig(raw);
}

/* Byte offset of codepoint `n` in `s`. */
size_t byteOffset(const std::string &s, size_t n) {
    return utf8::ncharByteLength(s.begin(), n);
}

class FakeClient : public InputContext {
public:
    struct Options {
        std::string program = "testapp";
        /* "dbus" (GTK/Qt through the fcitx5 IM modules), "wayland_v2"
         * (zwp_input_method_v2: GTK/Qt/Chromium/Firefox on Wayland) or
         * "xim" (X11 apps under XWayland). */
        std::string frontend = "dbus";
        bool preedit = true;     /* shows preedit inline */
        bool surrounding = true; /* reports the text around the cursor */
        /* Commits its own inline preedit on focus-out and reset, as
         * fcitx5-gtk, fcitx5-qt and the Wayland frontend do. */
        bool unfocusCommit = true;
        CapabilityFlags extra;
        /* Surrounding-text reports reach the engine this many key presses
         * late (0: the client answers before the next key). */
        int lag = 0;
    };

    explicit FakeClient(Options options)
        : InputContext(gInstance->inputContextManager(), options.program),
          options_(std::move(options)) {
        created();
        CapabilityFlags flags = options_.extra;
        if (options_.preedit) {
            flags |= CapabilityFlag::Preedit;
            flags |= CapabilityFlag::FormattedPreedit;
        }
        /* The Wayland frontend always claims SurroundingText, whether or
         * not the app ever sends any (fcitx5 waylandimserverv2.cpp,
         * baseFlags). */
        if (options_.surrounding || options_.frontend == "wayland_v2") {
            flags |= CapabilityFlag::SurroundingText;
        }
        if (options_.unfocusCommit) {
            flags |= CapabilityFlag::ClientUnfocusCommit;
        }
        setCapabilityFlags(flags);
        focusIn();
        gInstance->setCurrentInputMethod(this, "xrnikey", true);
        report();
        deliver(true);
    }
    ~FakeClient() override { destroy(); }

    const char *frontend() const override { return options_.frontend.c_str(); }

    /* What is in the text field. */
    const std::string &text() const { return text_; }
    /* What the user sees: the field with the inline preedit at the cursor. */
    std::string shown() const {
        std::string s = text_;
        s.insert(byteOffset(s, cursor_), preedit_);
        return s;
    }
    const std::string &preedit() const { return preedit_; }
    /* Keys the engine let through, in order (as Key::toString()). */
    const std::vector<std::string> &passedKeys() const { return passed_; }
    bool lastKeyHandled() const { return lastHandled_; }

    /* Press and release one key, e.g. "Return", "Control+1", "a". */
    bool key(const Key &k) {
        ++keyCount_;
        deliver(false);
        KeyEvent press(this, k, false);
        lastHandled_ = keyEvent(press);
        KeyEvent release(this, k, true);
        keyEvent(release);
        if (!lastHandled_) {
            passed_.push_back(k.toString());
            applyKey(k);
        }
        return lastHandled_;
    }
    bool key(const std::string &name) { return key(Key(name)); }

    /* Type text one key per character. */
    void type(const std::string &keys) {
        for (char c : keys) {
            if (c == ' ') {
                key(Key(FcitxKey_space));
            } else if (c >= 'A' && c <= 'Z') {
                key(Key(static_cast<KeySym>(c), KeyState::Shift));
            } else {
                key(Key(static_cast<KeySym>(static_cast<unsigned char>(c))));
            }
        }
    }

    /* The user clicks at codepoint `pos`. Widgets fed through the fcitx5
     * IM modules commit their inline preedit and send Reset; a Wayland
     * text-input-v3 client only reports the new cursor. */
    void click(size_t pos) {
        if (options_.frontend != "wayland_v2") {
            if (options_.unfocusCommit) {
                commitOwnPreedit();
            }
            reset();
        }
        cursor_ = anchor_ = pos;
        report();
        deliver(true);
    }

    /* The app changes its own text (autocomplete, a chat box clearing). */
    void appSetsText(std::string text, size_t cursor) {
        text_ = std::move(text);
        cursor_ = anchor_ = cursor;
        report();
        deliver(true);
    }

    /* The app selects [from, to) without the user typing (e.g. inline
     * autocompletion). */
    void appSelects(size_t from, size_t to) {
        anchor_ = from;
        cursor_ = to;
        report();
        deliver(true);
    }

    void focusAway() {
        if (options_.unfocusCommit) {
            commitOwnPreedit();
        }
        focusOut();
    }
    void focusBack() {
        focusIn();
        report();
        deliver(true);
    }

    /* Deliver every surrounding-text report still in flight. */
    void flush() { deliver(true); }

    /* The app stops reporting surrounding text (as the Wayland frontend
     * does when the text-input is deactivated). */
    void invalidateSurrounding() {
        pending_.clear();
        surroundingText().invalidate();
        updateSurroundingText();
    }

    /* Keys the engine synthesised with forwardKey(). */
    const std::vector<std::string> &forwardedKeys() const {
        return forwarded_;
    }

protected:
    void commitStringImpl(const std::string &s) override {
        eraseSelection();
        text_.insert(byteOffset(text_, cursor_), s);
        cursor_ += utf8::length(s);
        anchor_ = cursor_;
        commits_.push_back(s);
        report();
    }

    void deleteSurroundingTextImpl(int offset, unsigned int size) override {
        if (options_.frontend == "wayland_v2") {
            deleteLikeWaylandV2(offset, size);
        } else {
            /* D-Bus clients count characters themselves. */
            long start = static_cast<long>(cursor_) + offset;
            if (start < 0 || start + size > utf8::length(text_)) {
                return;
            }
            auto b = byteOffset(text_, start);
            auto e = byteOffset(text_, start + size);
            text_.erase(b, e - b);
            if (offset < 0) {
                cursor_ = start;
            }
            anchor_ = cursor_;
        }
        report();
    }

    void forwardKeyImpl(const ForwardKeyEvent &event) override {
        if (!event.isRelease()) {
            forwarded_.push_back(event.rawKey().toString());
            applyKey(event.rawKey());
        }
    }

    void updatePreeditImpl() override {
        preedit_ = inputPanel().clientPreedit().toString();
    }

private:
    /* zwp_input_method_v2.delete_surrounding_text takes BYTE lengths, so
     * fcitx5 converts the engine's character counts using ITS copy of the
     * surrounding text (waylandimserverv2.cpp,
     * deleteSurroundingTextDelegate). The app then deletes that many bytes
     * from its real text -- which is wrong if fcitx5's copy is stale. */
    void deleteLikeWaylandV2(int offset, unsigned int size) {
        const auto &local = surroundingText();
        if (offset > 0 || offset + static_cast<long>(size) < 0) {
            return;
        }
        long cursor = local.cursor();
        if (cursor + offset < 0) {
            return;
        }
        const auto &ltext = local.text();
        size_t len = utf8::length(ltext);
        size_t start = cursor + offset;
        size_t end = start + size;
        if (static_cast<size_t>(cursor) > len || start > len || end > len) {
            return;
        }
        auto startBytes = utf8::ncharByteLength(ltext.begin(), start);
        auto cursorBytes = utf8::ncharByteLength(ltext.begin(), cursor);
        auto sizeBytes =
            utf8::ncharByteLength(ltext.begin() + startBytes, size);
        size_t before = cursorBytes - startBytes;
        size_t after = startBytes + sizeBytes - cursorBytes;

        size_t realCursor = byteOffset(text_, cursor_);
        if (before > realCursor || realCursor + after > text_.size()) {
            return;
        }
        text_.erase(realCursor - before, before + after);
        realCursor -= before;
        /* The result may no longer be valid UTF-8; tests check for that. */
        cursor_ = utf8::lengthValidated(text_.begin(),
                                        text_.begin() + realCursor);
        if (cursor_ == utf8::INVALID_LENGTH) {
            cursor_ = 0;
            corrupted_ = true;
        }
        anchor_ = cursor_;
    }

public:
    bool corrupted() const {
        return corrupted_ || !utf8::validate(text_);
    }

private:
    void eraseSelection() {
        if (anchor_ == cursor_) {
            return;
        }
        auto from = std::min(anchor_, cursor_);
        auto to = std::max(anchor_, cursor_);
        auto b = byteOffset(text_, from);
        auto e = byteOffset(text_, to);
        text_.erase(b, e - b);
        cursor_ = anchor_ = from;
    }

    void commitOwnPreedit() {
        if (preedit_.empty()) {
            return;
        }
        std::string p = std::move(preedit_);
        preedit_.clear();
        eraseSelection();
        text_.insert(byteOffset(text_, cursor_), p);
        cursor_ += utf8::length(p);
        anchor_ = cursor_;
        report();
    }

    void applyKey(const Key &k) {
        if (k.states().testAny(
                KeyStates{KeyState::Ctrl, KeyState::Alt, KeyState::Super})) {
            return; /* an app shortcut, not text */
        }
        switch (k.sym()) {
        case FcitxKey_Return:
        case FcitxKey_KP_Enter:
            insertText("\n");
            return;
        case FcitxKey_Tab:
            insertText("\t");
            return;
        case FcitxKey_BackSpace:
            if (anchor_ != cursor_) {
                eraseSelection();
            } else if (cursor_ > 0) {
                auto b = byteOffset(text_, cursor_ - 1);
                auto e = byteOffset(text_, cursor_);
                text_.erase(b, e - b);
                anchor_ = --cursor_;
            }
            report();
            return;
        case FcitxKey_Left:
            if (cursor_ > 0) {
                --cursor_;
            }
            anchor_ = cursor_;
            report();
            return;
        case FcitxKey_Right:
            if (cursor_ < utf8::length(text_)) {
                ++cursor_;
            }
            anchor_ = cursor_;
            report();
            return;
        case FcitxKey_Home:
            cursor_ = anchor_ = 0;
            report();
            return;
        case FcitxKey_End:
            cursor_ = anchor_ = utf8::length(text_);
            report();
            return;
        default:
            break;
        }
        auto sym = static_cast<uint32_t>(k.sym());
        if (sym >= 0x20 && sym <= 0x7e) {
            insertText(std::string(1, static_cast<char>(sym)));
        }
    }

    void insertText(const std::string &s) {
        eraseSelection();
        text_.insert(byteOffset(text_, cursor_), s);
        cursor_ += utf8::length(s);
        anchor_ = cursor_;
        report();
    }

    /* Queue a surrounding-text report of the current state. */
    void report() {
        if (!options_.surrounding) {
            return;
        }
        pending_.push_back({keyCount_, text_, cursor_, anchor_});
    }

    /* Hand queued reports to Fcitx5, oldest first: all of them, or only
     * those at least `lag` key presses old. */
    void deliver(bool all) {
        while (!pending_.empty() &&
               (all || keyCount_ - pending_.front().keyIndex >
                           options_.lag)) {
            auto r = std::move(pending_.front());
            pending_.pop_front();
            surroundingText().setText(r.text, r.cursor, r.anchor);
            updateSurroundingText();
        }
    }

    struct Report {
        int keyIndex;
        std::string text;
        size_t cursor, anchor;
    };

    Options options_;
    std::string text_;
    size_t cursor_ = 0, anchor_ = 0;
    std::string preedit_;
    std::vector<std::string> commits_;
    std::vector<std::string> passed_;
    std::vector<std::string> forwarded_;
    std::deque<Report> pending_;
    int keyCount_ = 0;
    bool lastHandled_ = false;
    bool corrupted_ = false;
};

FakeClient::Options preeditApp() {
    FakeClient::Options o;
    o.program = "testapp";
    return o;
}

/* A text-input-v3 app (GTK, Firefox, Chromium) on Hyprland: the Wayland
 * frontend, and the program name is the app id fcitx5 gets from the
 * compositor's toplevel list. */
FakeClient::Options waylandApp(std::string appId = "org.example.Editor",
                               int lag = 0) {
    FakeClient::Options o;
    o.program = std::move(appId);
    o.frontend = "wayland_v2";
    o.lag = lag;
    return o;
}

/* A GUI app that reports surrounding text: instant commit is allowed. */
FakeClient::Options instantApp() {
    FakeClient::Options o;
    o.program = "testapp";
    return o;
}

} // namespace

TEST_CASE("preedit mode composes a word and commits it on space",
          "[basic]") {
    configure({{"InstantCommit", "False"}});
    FakeClient c(preeditApp());
    c.type("xin chaof");
    CHECK(c.text() == "xin ");
    c.type(" ");
    CHECK(c.text() == "xin chào ");
    CHECK(c.preedit().empty());
}

TEST_CASE("instant mode puts each word in the field as it is typed",
          "[basic]") {
    configure({{"InstantCommit", "True"}});
    FakeClient c(instantApp());
    c.type("xin chaof");
    CHECK(c.text() == "xin chào");
    CHECK(c.preedit().empty());
    c.type(" ");
    CHECK(c.text() == "xin chào ");
}

TEST_CASE("Return commits the word and inserts the newline in one press",
          "[keys]") {
    for (const char *instant : {"False", "True"}) {
        INFO("InstantCommit=" << instant);
        configure({{"InstantCommit", instant}});
        FakeClient c(instantApp());
        c.type("chaof");
        CHECK_FALSE(c.key("Return"));
        CHECK(c.text() == "chào\n");
        CHECK(c.preedit().empty());
        /* The next word starts fresh: no tone lands on the last one. */
        c.type("s ");
        CHECK(c.text() == "chào\ns ");
    }
}

TEST_CASE("every key the core does not compose with ends the word and "
          "passes through",
          "[keys]") {
    /* key name, what the fake field does with it */
    const std::vector<std::pair<std::string, std::string>> keys{
        {"Tab", "chào\t"},          {"Escape", "chào"},
        {"Up", "chào"},             {"Down", "chào"},
        {"Page_Up", "chào"},        {"Page_Down", "chào"},
        {"Home", "chào"},           {"End", "chào"},
        {"Delete", "chào"},         {"F5", "chào"},
        {"KP_Enter", "chào\n"},     {"Control+a", "chào"},
        {"Control+Return", "chào"}, {"Alt+x", "chào"},
        {"Super+d", "chào"},
    };
    for (const char *instant : {"False", "True"}) {
        for (const auto &[name, expected] : keys) {
            INFO("InstantCommit=" << instant << " key=" << name);
            configure({{"InstantCommit", instant}});
            FakeClient c(instantApp());
            c.type("chaof");
            CHECK_FALSE(c.key(name));
            CHECK(c.passedKeys().back() == Key(name).toString());
            CHECK(c.text() == expected);
            CHECK(c.preedit().empty());
            c.key("End");
            c.type("s ");
            CHECK(c.text() == expected + "s ");
        }
    }
}

TEST_CASE("an arrow key mid-word never lets the next key edit the old word",
          "[keys]") {
    configure({{"InstantCommit", "True"}});
    FakeClient c(instantApp());
    c.type("vie");
    c.key("Left");
    /* Starts a new word between i and e instead of turning the e before
     * the (moved) cursor into ê. */
    c.type("e");
    CHECK(c.text() == "viee");
}

TEST_CASE("the word being composed is shown inline in the app, underlined",
          "[leave]") {
    configure({{"InstantCommit", "False"}});
    FakeClient c(preeditApp());
    c.type("xin chaof");
    CHECK(c.preedit() == "chào");
    CHECK(c.shown() == "xin chào");
    REQUIRE(c.inputPanel().clientPreedit().size() == 1);
    CHECK(c.inputPanel().clientPreedit().formatAt(0) &
          TextFormatFlag::Underline);
    CHECK(c.inputPanel().preedit().empty()); /* not also in the popup */
}

TEST_CASE("a click mid-word keeps the word and starts the next one fresh",
          "[leave]") {
    for (bool unfocusCommit : {true, false}) {
        INFO("client commits its own preedit: " << unfocusCommit);
        configure({{"InstantCommit", "False"}});
        auto o = preeditApp();
        o.unfocusCommit = unfocusCommit;
        FakeClient c(o);
        c.type("chaof");
        c.click(0);
        CHECK(c.text() == "chào"); /* once: committed by client OR engine */
        CHECK(c.preedit().empty());
        c.type("s ");
        CHECK(c.text() == "s chào");
    }
}

TEST_CASE("focus moving away mid-word keeps the word exactly once",
          "[leave]") {
    for (bool unfocusCommit : {true, false}) {
        INFO("client commits its own preedit: " << unfocusCommit);
        configure({{"InstantCommit", "False"}});
        auto o = preeditApp();
        o.unfocusCommit = unfocusCommit;
        FakeClient c(o);
        c.type("chaof");
        c.focusAway();
        CHECK(c.text() == "chào");
        c.focusBack();
        CHECK(c.preedit().empty());
        c.type("s ");
        CHECK(c.text() == "chàos ");
    }
}

TEST_CASE("an app without inline preedit still gets the word on focus-out "
          "and reset",
          "[leave]") {
    configure({{"InstantCommit", "False"}});
    auto o = preeditApp();
    o.preedit = false; /* the word sits in Fcitx5's popup */
    {
        FakeClient c(o);
        c.type("chaof");
        c.focusAway();
        CHECK(c.text() == "chào");
    }
    {
        FakeClient c(o);
        c.type("chaof");
        c.click(0);
        CHECK(c.text() == "chào");
    }
}

TEST_CASE("switching input method mid-word commits the word", "[leave]") {
    configure({{"InstantCommit", "False"}});
    FakeClient c(preeditApp());
    c.type("chaof");
    gInstance->setCurrentInputMethod(&c, "keyboard-us", true);
    CHECK(c.text() == "chào");
    CHECK(c.preedit().empty());
}

TEST_CASE("instant mode: leaving a word mid-way only forgets it", "[leave]") {
    configure({{"InstantCommit", "True"}});
    {
        FakeClient c(instantApp());
        c.type("chaof");
        c.click(0);
        CHECK(c.text() == "chào");
        c.type("s ");
        CHECK(c.text() == "s chào");
    }
    {
        FakeClient c(instantApp());
        c.type("chaof");
        c.focusAway();
        c.focusBack();
        c.type("s ");
        CHECK(c.text() == "chàos ");
    }
    {
        FakeClient c(instantApp());
        c.type("chaof");
        gInstance->setCurrentInputMethod(&c, "keyboard-us", true);
        CHECK(c.text() == "chào");
    }
}

TEST_CASE("password and sensitive fields get the keys untouched",
          "[password]") {
    for (const char *instant : {"False", "True"}) {
        INFO("InstantCommit=" << instant);
        configure({{"InstantCommit", instant}});
        auto o = instantApp();
        o.extra = CapabilityFlag::Sensitive;
        FakeClient c(o);
        c.type("maatj khaaur");
        CHECK(c.text() == "maatj khaaur");
        CHECK(c.preedit().empty());
        CHECK_FALSE(c.lastKeyHandled());
        c.key("BackSpace");
        CHECK(c.text() == "maatj khaau");
    }

    /* Fcitx5 normally switches password fields to the keyboard layout
     * itself; with AllowInputMethodForPassword the engine sees them. */
    RawConfig allow;
    allow.setValueByPath("Behavior/AllowInputMethodForPassword", "True");
    gInstance->globalConfig().load(allow, true);
    {
        configure({{"InstantCommit", "False"}});
        auto o = preeditApp();
        o.extra = CapabilityFlag::Password;
        FakeClient c(o);
        REQUIRE(gInstance->inputMethod(&c) == "xrnikey");
        c.type("ddaay");
        CHECK(c.text() == "ddaay");
    }
    allow.setValueByPath("Behavior/AllowInputMethodForPassword", "False");
    gInstance->globalConfig().load(allow, true);
}

TEST_CASE("word suggestions are off by default", "[suggest]") {
    configure();
    FakeClient c(preeditApp());
    c.type("ch");
    CHECK_FALSE(c.inputPanel().candidateList());
}

TEST_CASE("digits are typed, never taken as a suggestion pick",
          "[suggest]") {
    for (const char *instant : {"False", "True"}) {
        INFO("InstantCommit=" << instant);
        configure({{"InstantCommit", instant}, {"PredictionEnabled", "True"}});
        FakeClient c(instantApp());
        c.type("ch");
        REQUIRE(c.inputPanel().candidateList());
        REQUIRE(c.inputPanel().candidateList()->size() > 0);
        c.type("1");
        CHECK(c.text() == "ch1");
        c.type(" mp3 ");
        CHECK(c.text() == "ch1 mp3 ");
    }
}

TEST_CASE("Ctrl+1..9 picks a suggestion", "[suggest]") {
    for (const char *instant : {"False", "True"}) {
        INFO("InstantCommit=" << instant);
        configure({{"InstantCommit", instant}, {"PredictionEnabled", "True"}});
        FakeClient c(instantApp());
        c.type("xin ch");
        auto list = c.inputPanel().candidateList();
        REQUIRE(list);
        REQUIRE(list->size() >= 2);
        std::string second = list->candidate(1).text().toString();
        CHECK(c.key("Control+2"));
        CHECK(c.text() == "xin " + second);
        CHECK(c.preedit().empty());
        CHECK_FALSE(c.inputPanel().candidateList());
    }
}

TEST_CASE("Ctrl+digit with no suggestion showing stays a shortcut",
          "[suggest]") {
    configure({{"InstantCommit", "False"}});
    FakeClient c(preeditApp());
    c.type("ch");
    CHECK_FALSE(c.key("Control+1"));
    CHECK(c.text() == "ch");
}

TEST_CASE("fields that ask for no spell checking get no suggestions",
          "[suggest]") {
    configure({{"PredictionEnabled", "True"}});
    auto o = preeditApp();
    o.extra = CapabilityFlag::NoSpellCheck;
    FakeClient c(o);
    c.type("ch");
    CHECK_FALSE(c.inputPanel().candidateList());
    c.type("aof ");
    CHECK(c.text() == "chào "); /* still Vietnamese */
}

TEST_CASE("changing the input mode applies to fields that are already open",
          "[config]") {
    configure({{"InputMode", "Telex"}, {"InstantCommit", "False"}});
    FakeClient c(preeditApp());
    c.type("a1 aa ");
    CHECK(c.text() == "a1 â ");
    configure({{"InputMode", "VNI"}, {"InstantCommit", "False"}});
    c.type("a1 aa ");
    CHECK(c.text() == "a1 â á aa ");
    configure({{"InputMode", "Telex"}, {"InstantCommit", "False"}});
    c.type("chaof");
    /* Switching mid-word keeps the word, then types on in the new mode. */
    configure({{"InputMode", "VNI"}, {"InstantCommit", "False"}});
    c.type(" a1 ");
    CHECK(c.text() == "a1 â á aa chào á ");
}

TEST_CASE("only options that do something are offered", "[config]") {
    XrnikeyConfig config;
    RawConfig raw;
    config.save(raw);
    for (const char *dead : {"AutoSwitch", "SpellCheck", "EmojiEnabled",
                             "EmojiTrigger", "QuickSwitch", "CommitKey",
                             "ShowFrequencyHint"}) {
        INFO(dead);
        CHECK_FALSE(raw.get(dead));
    }
}

TEST_CASE("instant commit is opt-in", "[instant]") {
    configure();
    FakeClient c(instantApp());
    c.type("chaof");
    CHECK(c.text().empty());
    CHECK(c.preedit() == "chào");
}

TEST_CASE("instant commit survives an app that reports surrounding text "
          "late",
          "[instant]") {
    /* Fast typing outruns the app's surrounding-text reports. On Wayland
     * Fcitx5 converts each deletion to bytes from its stale copy, which
     * used to delete half of an "ê" or the wrong letters. */
    const std::string sentence = "tieengs vieetj ddepj quas nhuw thees ";
    const std::string expected = "tiếng việt đẹp quá như thế ";
    for (int lag : {0, 1, 2, 3}) {
        for (const char *frontend : {"wayland_v2", "dbus"}) {
            INFO("lag=" << lag << " frontend=" << frontend);
            configure({{"InstantCommit", "True"}});
            auto o = waylandApp("org.example.Editor", lag);
            o.frontend = frontend;
            FakeClient c(o);
            c.type(sentence);
            c.flush();
            CHECK_FALSE(c.corrupted());
            CHECK(c.text() == expected);
            CHECK(c.forwardedKeys().empty());
        }
    }
}

/* Type `steps`: text, or a key name in <angle brackets>. */
void play(FakeClient &c, const std::vector<std::string> &steps) {
    for (const auto &step : steps) {
        if (step.size() > 2 && step.front() == '<' && step.back() == '>') {
            c.key(step.substr(1, step.size() - 2));
        } else {
            c.type(step);
        }
    }
    c.flush();
}

TEST_CASE("instant commit gives the same text however late the app reports",
          "[instant]") {
    const std::vector<std::vector<std::string>> scripts{
        {"vieetj", "<Return>", "nam ", "tieengs", "<BackSpace>",
         "<BackSpace>", "s "},
        {"ddaay laf", "<BackSpace>", "f ", "<Left>", "<Right>", "nhaf "},
        {"thuwr nghieemj ", "<Return>", "<Return>", "xongs", "<Tab>",
         "rooif."},
        {"hello world, toi ddi hocj "},
    };
    for (const char *frontend : {"wayland_v2", "dbus"}) {
        for (size_t n = 0; n < scripts.size(); ++n) {
            std::string reference;
            for (int lag : {0, 1, 2, 3, 5}) {
                INFO("frontend=" << frontend << " script=" << n
                                 << " lag=" << lag);
                configure({{"InstantCommit", "True"}});
                auto o = waylandApp("org.example.Editor", lag);
                o.frontend = frontend;
                FakeClient c(o);
                play(c, scripts[n]);
                CHECK_FALSE(c.corrupted());
                CHECK(c.forwardedKeys().empty());
                if (lag == 0) {
                    reference = c.text();
                } else {
                    CHECK(c.text() == reference);
                }
            }
        }
    }
}

TEST_CASE("instant commit never deletes text the cursor moved onto",
          "[instant]") {
    configure({{"InstantCommit", "True"}});
    SECTION("a click elsewhere (Wayland sends no reset, only the cursor)") {
        FakeClient c(waylandApp());
        c.type("xin vie");
        c.click(3);
        c.type("e ");
        /* The e starts a new word where the cursor now is. Before, it was
         * taken as the second e of "vie" and deleted the "n" of "xin". */
        CHECK(c.text() == "xine  vie");
    }
    SECTION("the app rewrites the word (auto-capitalisation)") {
        FakeClient c(waylandApp());
        c.type("vie");
        c.appSetsText("Vie", 3);
        c.type("e ");
        CHECK(c.text() == "Viee ");
    }
    SECTION("the app selects text (inline autocompletion)") {
        FakeClient c(waylandApp());
        c.type("vie");
        c.appSetsText("vietnam", 3);
        c.appSelects(7, 3);
        c.type("e ");
        CHECK(c.text() == "viee ");
    }
    SECTION("the app stops reporting surrounding text") {
        FakeClient c(instantApp());
        c.type("vie");
        c.invalidateSurrounding();
        c.type("e ");
        CHECK(c.forwardedKeys().empty()); /* no synthesised BackSpace */
        CHECK(c.text() == "viee ");
    }
}

TEST_CASE("terminals and editors in the default list use preedit",
          "[instant]") {
    configure({{"InstantCommit", "True"}});
    /* App ids as Hyprland reports them, plus one Fcitx5 learns only from
     * the field's content purpose. */
    for (const char *app :
         {"Alacritty", "foot", "kitty", "org.wezfurlong.wezterm",
          "com.mitchellh.ghostty", "org.kde.konsole", "org.gnome.Terminal",
          "code", "libreoffice-writer"}) {
        INFO(app);
        FakeClient c(waylandApp(app));
        c.type("chaof");
        CHECK(c.text().empty());
        CHECK(c.preedit() == "chào");
    }
    auto o = waylandApp("someterm");
    o.extra = CapabilityFlag::Terminal;
    FakeClient c(o);
    c.type("chaof");
    CHECK(c.text().empty());
    CHECK(c.preedit() == "chào");
}

TEST_CASE("apps that report surrounding text get instant commit",
          "[instant]") {
    configure({{"InstantCommit", "True"}});
    for (const char *app : {"zen", "chromium", "org.gnome.TextEditor"}) {
        INFO(app);
        FakeClient c(waylandApp(app));
        c.type("chaof");
        CHECK(c.text() == "chào");
        CHECK(c.preedit().empty());
    }
    /* No surrounding text (foot, Alacritty, kitty never send any; XIM
     * cannot): preedit, whatever the name. */
    auto o = waylandApp("unknown");
    o.surrounding = false;
    FakeClient c(o);
    c.type("chaof");
    CHECK(c.text().empty());
    CHECK(c.preedit() == "chào");
}

int main(int argc, char *argv[]) {
    /* Nothing here may reach the user's session: no display, no session
     * bus, and every file the addon reads under a throwaway directory. */
    namespace fs = std::filesystem;
    /* One directory per process: ctest runs the cases in parallel. */
    std::string tmpl = std::string(XRNIKEY_TEST_TMP) + "/home-XXXXXX";
    if (!mkdtemp(tmpl.data())) {
        std::perror("mkdtemp");
        return 1;
    }
    fs::path home = tmpl;
    fs::create_directories(home / ".local/share/xrnikey");
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_DATA_HOME", (home / ".local/share").c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);
    setenv("XRNIKEY_DATA_DIR", XRNIKEY_TEST_DATA_DIR, 1);
    unsetenv("DISPLAY");
    unsetenv("WAYLAND_DISPLAY");
    setenv("DBUS_SESSION_BUS_ADDRESS", "disabled:", 1);

    setupTestingEnvironment(XRNIKEY_TEST_TMP, {XRNIKEY_TEST_ADDON_DIR},
                            {XRNIKEY_TEST_FCITX_DATA});

    char arg0[] = "test_fcitx5_addon";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=testim,xrnikey5";
    char *fargv[] = {arg0, arg1, arg2};
    Instance instance(3, fargv);
    instance.addonManager().registerDefaultLoader(nullptr);
    gInstance = &instance;

    int result = 1;
    EventDispatcher dispatcher;
    dispatcher.attach(&instance.eventLoop());
    dispatcher.schedule([&]() {
        if (!engine()) {
            std::fprintf(stderr, "xrnikey5 addon did not load\n");
        } else {
            auto group = instance.inputMethodManager().currentGroup();
            group.inputMethodList().clear();
            group.inputMethodList().push_back(
                InputMethodGroupItem("keyboard-us"));
            group.inputMethodList().push_back(InputMethodGroupItem("xrnikey"));
            group.setDefaultInputMethod("");
            instance.inputMethodManager().setGroup(group);
            result = Catch::Session().run(argc, argv);
        }
        dispatcher.detach();
        instance.exit();
    });
    instance.exec();
    fs::remove_all(home);
    return result;
}
