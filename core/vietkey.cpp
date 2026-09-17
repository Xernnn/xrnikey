#include "vietkey.h"
#include "vietkey_internal.h"
#include <cstring>
#include <cstdlib>

namespace vietkey {

/* Finalize the pending syllable in `s->buffer`, writing the committed text
 * (plus `trailing`, e.g. ' ' or a punctuation mark) into `r`. Shared by
 * Space and by any other non-letter key that ends a word. */
static void finalize_word(VKStateImpl* s, VKResult* r, char trailing) {
    std::string out = apply_tone_to_syllable(s->buffer, s->tone);
    out = nfc_compose(out);

    /* A tone key or a vowel modifier (ư/ơ/ô/â/ê/ă/đ) is unambiguous user
     * intent to type Vietnamese — e.g. typing "longf" for "lòng" must never
     * be second-guessed just because "long" also happens to be an English
     * whitelist word. Only fall back to the English/invalid-syllable guess
     * when nothing Vietnamese-specific was actually typed. */
    bool has_vn_marker = s->tone != VK_TONE_NONE;
    if (!has_vn_marker) {
        for (unsigned char c : s->buffer) {
            if (c >= 0x80) { has_vn_marker = true; break; }
        }
    }

    /* Must check the raw (untoned) buffer, not `out`: is_valid_vn_syllable's
     * nucleus table only recognizes untoned ASCII shapes ("oi", not "õi"),
     * so checking the toned string would misclassify every toned Vietnamese
     * word as "not a valid syllable" -> English passthrough, silently
     * dropping the tone. */
    std::string final_text =
        (!has_vn_marker && (s->passthrough_word || vk_is_english_word(s->buffer.c_str())))
            ? s->buffer : out;

    if (s->instant_commit) {
        /* Reconcile with whatever was already committed incrementally
         * (e.g. an English-word override only detected at word end), then
         * append the trailing char to the same commit event. */
        diff_commit(s->committed, final_text, r);
    } else {
        std::strncpy(r->commit, final_text.c_str(), sizeof(r->commit) - 1);
        r->commit[sizeof(r->commit) - 1] = '\0';
    }
    size_t len = strlen(r->commit);
    if (len < sizeof(r->commit) - 1) r->commit[len] = trailing;

    s->buffer.clear();
    s->committed.clear();
    s->tone = VK_TONE_NONE;
    s->passthrough_word = false;
    s->char_count_no_vowel = 0;
    s->undo_stack.clear();
    r->consumed = 1;
}

} /* namespace vietkey */

extern "C" {

VKState* vk_state_new(int mode) {
    auto* s = new vietkey::VKStateImpl(mode);
    return reinterpret_cast<VKState*>(s);
}

void vk_state_free(VKState* state) {
    delete reinterpret_cast<vietkey::VKStateImpl*>(state);
}

void vk_state_reset(VKState* state) {
    if (!state) return;
    auto* s = reinterpret_cast<vietkey::VKStateImpl*>(state);
    s->buffer.clear();
    s->committed.clear();
    s->tone = VK_TONE_NONE;
    s->passthrough_word = false;
    s->char_count_no_vowel = 0;
    s->undo_stack.clear();
}

void vk_state_set_instant_commit(VKState* state, int enabled) {
    if (!state) return;
    auto* s = reinterpret_cast<vietkey::VKStateImpl*>(state);
    s->instant_commit = enabled != 0;
}

char* vk_state_get_preedit(VKState* state) {
    if (!state) return nullptr;
    auto* s = reinterpret_cast<vietkey::VKStateImpl*>(state);
    std::string display = vietkey::apply_tone_to_syllable(s->buffer, s->tone);
    display = vietkey::nfc_compose(display);
    char* out = static_cast<char*>(std::malloc(display.size() + 1));
    if (out) std::strcpy(out, display.c_str());
    return out;
}

VKResult vk_process_key(VKState* state, uint32_t keysym, uint32_t modifiers) {
    VKResult r{};
    r.consumed = 0;
    r.reset = 0;

    if (!state) return r;
    auto* s = reinterpret_cast<vietkey::VKStateImpl*>(state);

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
        if (s->mode == VK_MODE_TELEX) vietkey::telex_backspace(s, &r);
        else vietkey::vni_backspace(s, &r);
        return r;
    }

    /* Space — commit current syllable/word */
    if (keysym == 0x20) {
        if (!s->buffer.empty()) {
            vietkey::finalize_word(s, &r, ' ');
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
        if (!s->buffer.empty()) {
            vietkey::finalize_word(s, &r, 0);
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
        bool is_vni_digit = s->mode == VK_MODE_VNI && c >= '0' && c <= '9';
        if (!is_letter && !is_vni_digit && !s->buffer.empty()) {
            vietkey::finalize_word(s, &r, c);
            return r;
        }

        if (s->mode == VK_MODE_TELEX) vietkey::telex_process_char(s, c, &r);
        else vietkey::vni_process_char(s, c, &r);
        return r;
    }

    return r;
}

} /* extern "C" */
