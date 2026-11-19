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

static void word_key(XKStateImpl* s, XKResult* r, char c) {
    if (s->word.keys.size() >= kMaxWordKeys) {
        /* Close the long word and start a new one with this key. */
        finalize_word(s, r, 0);
        std::string done = r->commit;
        int deleted = r->delete_before;
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
    switch (option) {
        case XK_OPT_AUTO_RESTORE: s->opt.auto_restore = value != 0; break;
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

    /* Return/Enter registers the pending syllable exactly as Space does, but
     * contributes no character of its own. Previously Enter was not handled
     * at all: the composing block was abandoned and the word never committed.
     *
     * The key is CONSUMED when it actually finalized something, so a second
     * Enter is needed to insert the newline. That is deliberate rather than
     * passing the Enter straight through: a commit travels to the client over
     * the text-input channel while a forwarded key travels over the keyboard
     * one, with no ordering guarantee between them -- the same gap that
     * scrambles in-place edits in terminals. An Enter that overtook its own
     * commit would run a half-typed command line, so the word is registered
     * first and the newline is left to the next press. With nothing pending,
     * Enter passes through untouched as normal. */
    if (keysym == 0xFF0D || keysym == 0xFF8D) { /* Return, KP_Enter */
        if (!s->word.empty()) {
            xrnikey::finalize_word(s, &r, 0);
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

        xrnikey::word_key(s, &r, c);
        return r;
    }

    return r;
}

} /* extern "C" */
