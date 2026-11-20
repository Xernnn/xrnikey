#include "xrnikey.h"
#include "xrnikey_internal.h"
#include <cstring>
#include <cstdlib>

namespace xrnikey {

/* Longest word kept as one composition. Nothing this long is a Vietnamese
 * syllable, and it keeps every commit and preedit well inside XKResult's
 * 64-byte fields (a held-down key autorepeats into one endless "word"). */
static const size_t kMaxWordKeys = 32;

static void copy_out(char (&dst)[64], const std::string& src) {
    std::strncpy(dst, src.c_str(), sizeof(dst) - 1);
    dst[sizeof(dst) - 1] = '\0';
}

/* Show the word in progress: preedit, and in instant mode the correction to
 * the text already in the document. */
static void show_word(XKStateImpl* s, XKResult* r) {
    std::string display = word_display(s->word, s->opt);
    copy_out(r->preedit, display);
    if (s->instant_commit) {
        diff_commit(s->committed, display, r);
        s->committed = display;
    }
    r->consumed = 1;
}

/* Finalize the pending word, writing the committed text (plus `trailing`,
 * e.g. ' ' or a punctuation mark) into `r`. Shared by Space and by any other
 * non-letter key that ends a word. */
static void finalize_word(XKStateImpl* s, XKResult* r, char trailing) {
    std::string final_text = word_final(s->word, s->opt);

    if (s->instant_commit) {
        /* Reconcile with whatever was already committed incrementally
         * (e.g. an English word only restored at word end), then append the
         * trailing char to the same commit event. */
        diff_commit(s->committed, final_text, r);
    } else {
        copy_out(r->commit, final_text);
    }
    size_t len = strlen(r->commit);
    if (trailing && len < sizeof(r->commit) - 1) {
        r->commit[len] = trailing;
        r->commit[len + 1] = '\0';
    }

    s->word.clear();
    s->committed.clear();
    r->preedit[0] = '\0';
    r->consumed = 1;
}

/* A key that ends the word but is the application's to handle (Enter, Tab,
 * Escape, arrows, Home/End, Delete, shortcuts): finish the word and let the
 * key through. Preedit mode commits the word first, so Enter sends the line
 * with the word in it; instant mode already has the word in the document,
 * so `commit`/`delete_before` stay empty unless auto-restore changes the
 * word as it ends ("terms" shown as "tém" becomes "terms"). Either way the
 * state is empty afterwards: after vie <Left> et, "et" is a new word, not a
 * stale "viee". */
static void end_word(XKStateImpl* s, XKResult* r) {
    if (!s->word.empty()) finalize_word(s, r, 0);
    r->consumed = 0;
}

static bool is_modifier_keysym(uint32_t keysym) {
    return keysym == 0 ||
           (keysym >= 0xFFE1 && keysym <= 0xFFEE) ||  /* Shift, Control, Caps, Meta, Alt, Super, Hyper */
           (keysym >= 0xFE01 && keysym <= 0xFE13) ||  /* ISO_Level3_Shift, ISO_Level5_Shift, groups */
           keysym == 0xFF7E || keysym == 0xFF7F;      /* Mode_switch, Num_Lock */
}

static void word_key(XKStateImpl* s, XKResult* r, char c) {
    if (s->word.keys.size() >= kMaxWordKeys) {
        /* Close the long word and go on with this key as plain text: no
         * Vietnamese word is this long, and composing again from here would
         * turn the middle of it into a new syllable. */
        finalize_word(s, r, 0);
        std::string done = r->commit;
        int deleted = r->delete_before;
        s->word.literal = true;
        s->word.keys.push_back(c);
        word_replay(s->word, s->mode, s->opt);
        show_word(s, r);
        if (s->instant_commit) copy_out(r->commit, done + r->commit);
        else copy_out(r->commit, done);
        r->delete_before = deleted;
        return;
    }
    s->word.keys.push_back(c);
    word_replay(s->word, s->mode, s->opt);
    show_word(s, r);
}

static void backspace(XKStateImpl* s, XKResult* r) {
    if (s->word.empty()) return;  /* nothing buffered: the app deletes */
    word_backspace(s->word, s->mode, s->opt);
    show_word(s, r);
}

} /* namespace xrnikey */

extern "C" {

XKState* xk_state_new(int mode) {
    auto* s = new xrnikey::XKStateImpl(mode);
    return reinterpret_cast<XKState*>(s);
}

void xk_state_free(XKState* state) {
    delete reinterpret_cast<xrnikey::XKStateImpl*>(state);
}

void xk_state_reset(XKState* state) {
    if (!state) return;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);
    s->word.clear();
    s->committed.clear();
}

void xk_state_set_instant_commit(XKState* state, int enabled) {
    if (!state) return;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);
    s->instant_commit = enabled != 0;
}

void xk_state_set_option(XKState* state, int option, int value) {
    if (!state) return;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);
    int current = xk_state_get_option(state, option);
    if (current < 0 || current == (value != 0)) return;  /* unknown, or no change */
    switch (option) {
        case XK_OPT_AUTO_RESTORE: s->opt.auto_restore = value != 0; break;
        case XK_OPT_MODERN_STYLE: s->opt.modern_style = value != 0; break;
        case XK_OPT_FREE_MARKING: s->opt.free_marking = value != 0; break;
        case XK_OPT_W_SHORTCUT: s->opt.w_shortcut = value != 0; break;
        default: return;
    }
    /* The word in progress is recomposed under the new option. */
    xrnikey::word_replay(s->word, s->mode, s->opt);
}

int xk_state_get_option(XKState* state, int option) {
    if (!state) return -1;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);
    switch (option) {
        case XK_OPT_AUTO_RESTORE: return s->opt.auto_restore ? 1 : 0;
        case XK_OPT_MODERN_STYLE: return s->opt.modern_style ? 1 : 0;
        case XK_OPT_FREE_MARKING: return s->opt.free_marking ? 1 : 0;
        case XK_OPT_W_SHORTCUT: return s->opt.w_shortcut ? 1 : 0;
        default: return -1;
    }
}

char* xk_state_get_preedit(XKState* state) {
    if (!state) return nullptr;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);
    std::string display = xrnikey::word_display(s->word, s->opt);
    char* out = static_cast<char*>(std::malloc(display.size() + 1));
    if (out) std::strcpy(out, display.c_str());
    return out;
}

XKResult xk_process_key(XKState* state, uint32_t keysym, uint32_t modifiers) {
    XKResult r{};
    r.consumed = 0;
    r.reset = 0;

    if (!state) return r;
    auto* s = reinterpret_cast<xrnikey::XKStateImpl*>(state);

    /* Ctrl+Shift toggle */
    if ((modifiers & 0x04) && (modifiers & 0x01)) {
        s->enabled = !s->enabled;
        r.reset = 1;
        r.consumed = 1;
        return r;
    }

    if (!s->enabled) {
        r.consumed = 0;
        return r;
    }

    /* A modifier on its own (Shift for a capital, Ctrl before a shortcut)
     * changes nothing. */
    if (xrnikey::is_modifier_keysym(keysym)) return r;

    /* Ctrl/Alt/Super chords belong to the application. */
    if (modifiers & (0x04 | 0x08 | 0x40 | (1u << 26))) {
        xrnikey::end_word(s, &r);
        return r;
    }

    /* Backspace */
    if (keysym == 0xFF08) {
        xrnikey::backspace(s, &r);
        return r;
    }

    /* Space — commit current syllable/word */
    if (keysym == 0x20) {
        if (!s->word.empty()) {
            xrnikey::finalize_word(s, &r, ' ');
        }
        return r;
    }

    /* Printable ASCII */
    if (keysym >= 0x20 && keysym <= 0x7E) {
        char c = static_cast<char>(keysym);
        bool is_letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        /* VNI's tone/vowel-mod digits are meaningful in that mode; every
         * other non-letter key (punctuation, digits in Telex) ends the
         * pending word first — same as Space — then is committed as
         * itself, matching how real IMEs treat any non-letter as a word
         * boundary (e.g. "cõi," must not stay stuck mid-composition). */
        bool is_vni_digit = s->mode == XK_MODE_VNI && c >= '0' && c <= '9';
        if (!is_letter && !is_vni_digit && !s->word.empty()) {
            xrnikey::finalize_word(s, &r, c);
            return r;
        }
        /* With no word in progress, a non-letter (in VNI also a digit: it
         * has no letter to change yet) is text on its own, and the word
         * starts at the next letter. It used to become the word's first
         * key, so the word could never be a syllable: "(vieetj)" stayed
         * "(vieetj)", "2tieengs" "2tieengs". */
        if (!is_letter && s->word.empty()) {
            r.commit[0] = c;
            r.commit[1] = '\0';
            r.consumed = 1;
            return r;
        }

        xrnikey::word_key(s, &r, c);
        return r;
    }

    /* Enter, Tab, Escape, arrows, Home/End, PageUp/Down, Delete, Insert, and
     * any other key without a character here. Enter used to be consumed when
     * a word was pending, so every chat message needed Enter twice, and the
     * cursor keys left the word buffered: after vie <Left> et the engine
     * still edited "vie" and in instant mode deleted the wrong characters. */
    xrnikey::end_word(s, &r);
    return r;
}

XKResult xk_state_flush(XKState* state) {
    XKResult r{};
    if (!state) return r;
    xrnikey::end_word(reinterpret_cast<xrnikey::XKStateImpl*>(state), &r);
    return r;
}

} /* extern "C" */
