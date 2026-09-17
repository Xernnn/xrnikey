#include "vietkey_internal.h"
#include <cctype>

namespace vietkey {

static void push_undo_vni(VKStateImpl* s) {
    UndoEntry e;
    e.buffer = s->buffer;
    e.tone = s->tone;
    s->undo_stack.push_back(e);
    if (s->undo_stack.size() > 50) s->undo_stack.erase(s->undo_stack.begin());
}

static void apply_vni_vowel_mod(VKStateImpl* s, char vowel, int mod) {
    std::string& buf = s->buffer;
    for (int i = static_cast<int>(buf.size()) - 1; i >= 0; --i) {
        char c = std::tolower(static_cast<unsigned char>(buf[i]));
        if (c == vowel) {
            std::string repl;
            if (mod == 6) {
                if (vowel == 'a') repl = "\xC3\xA2";
                else if (vowel == 'e') repl = "\xC3\xAA";
                else if (vowel == 'o') repl = "\xC3\xB4";
            } else if (mod == 7) {
                /* o7 -> ơ, u7 -> ư */
                if (vowel == 'o') repl = "\xC6\xA1";
                else if (vowel == 'u') repl = "\xC6\xB0";
            } else if (mod == 8 && vowel == 'a') {
                repl = "\xC4\x83";
            } else if (mod == 9 && vowel == 'd') {
                repl = "\xC4\x91";
            }
            if (!repl.empty()) {
                buf = buf.substr(0, i) + repl + buf.substr(i + 1);
            }
            return;
        }
    }
}

void vni_backspace(VKStateImpl* s, VKResult* r) {
    if (!s->undo_stack.empty()) {
        UndoEntry e = s->undo_stack.back();
        s->undo_stack.pop_back();
        s->buffer = e.buffer;
        s->tone = e.tone;
    } else if (!s->buffer.empty()) {
        s->buffer.pop_back();
        if (s->buffer.empty()) s->tone = VK_TONE_NONE;
    } else {
        /* Nothing buffered — let the real backspace through to the app. */
        return;
    }

    std::string display = nfc_compose(apply_tone_to_syllable(s->buffer, s->tone));
    std::strncpy(r->preedit, display.c_str(), sizeof(r->preedit) - 1);
    if (s->instant_commit) {
        diff_commit(s->committed, display, r);
        s->committed = display;
    }
    r->consumed = 1;
}

void vni_process_char(VKStateImpl* s, char c, VKResult* r) {
    /* Decide English vs Vietnamese from the syllable shape typed SO FAR.
     * Vietnamese cannot spell "wo-", "sh-", "hel-", so once the buffer has
     * left the space of possible Vietnamese syllables, every later key is
     * literal -- otherwise a tone key simply because a vowel precedes turns
     * "word" into "wỏd" and "hello" into "helló".
     *
     * Deliberately tested against the buffer BEFORE this key is applied: the
     * modifier keys build transient forms ("nguw" on the way to "ngư") that
     * would be judged invalid mid-transform. */
    if (!s->passthrough_word && !can_begin_vn_syllable(s->buffer)) {
        s->passthrough_word = true;
    }

    if (s->passthrough_word) {
        s->buffer += c;
        std::strncpy(r->preedit, s->buffer.c_str(), sizeof(r->preedit) - 1);
        if (s->instant_commit) {
            diff_commit(s->committed, s->buffer, r);
            s->committed = s->buffer;
        }
        r->consumed = 1;
        return;
    }

    if (c >= '1' && c <= '9') {
        int digit = c - '0';

        /* A tone digit can come right after the vowel or, just as
         * naturally, after the whole syllable including its coda
         * consonant ("duoc5" for "được") — so this must check for a vowel
         * anywhere in the buffer, not only at its very end. */
        if (digit >= 1 && digit <= 5 && find_tone_vowel_index(s->buffer) >= 0) {
            push_undo_vni(s);
            s->tone = digit;
            goto update;
        }

        if (digit == 6 || digit == 7 || digit == 8) {
            push_undo_vni(s);
            char vowels[] = {'a', 'e', 'o', 'u', 'i', 'y', 'd'};
            for (char v : vowels) {
                for (int i = static_cast<int>(s->buffer.size()) - 1; i >= 0; --i) {
                    if (std::tolower(static_cast<unsigned char>(s->buffer[i])) == v) {
                        apply_vni_vowel_mod(s, v, digit);
                        goto update;
                    }
                }
            }
            /* No vowel precedes — literal digit */
            push_undo_vni(s);
            s->buffer += c;
            goto update;
        }

        if (digit == 9) {
            push_undo_vni(s);
            apply_vni_vowel_mod(s, 'd', 9);
            goto update;
        }

        if (digit == 0) {
            push_undo_vni(s);
            s->tone = VK_TONE_NONE;
            goto update;
        }

        /* Literal digit when no vowel context */
        if (find_tone_vowel_index(s->buffer) < 0) {
            push_undo_vni(s);
            s->buffer += c;
            goto update;
        }
    }

    push_undo_vni(s);
    s->buffer += c;

    /* The "three characters and still no vowel means English" heuristic that
     * used to live here fired on "ngh" -- a perfectly good Vietnamese onset --
     * so "nghieeng" never composed to "nghiêng". can_begin_vn_syllable()
     * above decides this properly from the real onset/coda tables. */

update:
    {
        std::string display = apply_tone_to_syllable(s->buffer, s->tone);
        display = nfc_compose(display);
        std::strncpy(r->preedit, display.c_str(), sizeof(r->preedit) - 1);
        if (s->instant_commit) {
            diff_commit(s->committed, display, r);
            s->committed = display;
        }
    }
    r->consumed = 1;
}

} /* namespace vietkey */
