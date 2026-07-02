#include "vietkey_internal.h"
#include <algorithm>
#include <cctype>

namespace vietkey {

static void push_undo(VKStateImpl* s) {
    UndoEntry e;
    e.buffer = s->buffer;
    e.tone = s->tone;
    s->undo_stack.push_back(e);
    if (s->undo_stack.size() > 50) s->undo_stack.erase(s->undo_stack.begin());
}

static void finalize_syllable(VKStateImpl* s, VKResult* r) {
    std::string out = apply_tone_to_syllable(s->buffer, s->tone);
    out = nfc_compose(out);
    std::strncpy(r->commit, out.c_str(), sizeof(r->commit) - 1);
    s->buffer.clear();
    s->tone = VK_TONE_NONE;
    s->passthrough_word = false;
    s->char_count_no_vowel = 0;
}

static void apply_vowel_mod(std::string& buf, char trigger, char replacement_base) {
    for (int i = static_cast<int>(buf.size()) - 1; i >= 0; --i) {
        if (std::tolower(static_cast<unsigned char>(buf[i])) == replacement_base) {
            buf[i] = replacement_base;
            return;
        }
    }
    buf += replacement_base;
}

static void apply_w_mod(VKStateImpl* s) {
    std::string& buf = s->buffer;
    for (int i = static_cast<int>(buf.size()) - 1; i >= 0; --i) {
        char c = std::tolower(static_cast<unsigned char>(buf[i]));
        if (c == 'o') {
            buf[i] = static_cast<char>(0xC6); /* placeholder — use ơ via compose */
            buf = buf.substr(0, i) + "o" + buf.substr(i + 1);
            /* Mark with special: replace last o with ow pattern stored */
            buf[i] = 'o';
            /* Use Unicode ơ U+01A1 */
            char ohorn[3] = {'\xC6', '\xA1', '\0'};
            buf = buf.substr(0, i) + ohorn + buf.substr(i + 1);
            return;
        }
        if (c == 'u') {
            char uhorn[3] = {'\xC6', '\xB0', '\0'};
            buf = buf.substr(0, i) + uhorn + buf.substr(i + 1);
            return;
        }
        if (c == 'a') {
            char abreve[3] = {'\xC4', '\x83', '\0'};
            buf = buf.substr(0, i) + abreve + buf.substr(i + 1);
            return;
        }
    }
}

void telex_backspace(VKStateImpl* s, VKResult* r) {
    if (!s->undo_stack.empty()) {
        UndoEntry e = s->undo_stack.back();
        s->undo_stack.pop_back();
        s->buffer = e.buffer;
        s->tone = e.tone;
        std::strncpy(r->preedit, nfc_compose(s->buffer).c_str(), sizeof(r->preedit) - 1);
        r->consumed = 1;
        return;
    }
    if (!s->buffer.empty()) {
        s->buffer.pop_back();
        if (s->buffer.empty()) s->tone = VK_TONE_NONE;
        std::strncpy(r->preedit, nfc_compose(s->buffer).c_str(), sizeof(r->preedit) - 1);
        r->consumed = 1;
    }
}

void telex_process_char(VKStateImpl* s, char c, VKResult* r) {
    char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (s->passthrough_word) {
        s->buffer += c;
        std::strncpy(r->preedit, s->buffer.c_str(), sizeof(r->preedit) - 1);
        r->consumed = 1;
        return;
    }

    /* Double-press escape: same key twice quickly outputs raw */
    if (lc == s->last_key && s->double_press_escape) {
        s->buffer += c;
        s->double_press_escape = false;
        s->passthrough_word = true;
        std::strncpy(r->preedit, s->buffer.c_str(), sizeof(r->preedit) - 1);
        r->consumed = 1;
        return;
    }

    /* Tone marks */
    if (lc == 's') { push_undo(s); s->tone = VK_TONE_SAC; goto update; }
    if (lc == 'f') { push_undo(s); s->tone = VK_TONE_HUYEN; goto update; }
    if (lc == 'r') { push_undo(s); s->tone = VK_TONE_HOI; goto update; }
    if (lc == 'x') { push_undo(s); s->tone = VK_TONE_NGA; goto update; }
    if (lc == 'j') { push_undo(s); s->tone = VK_TONE_NANG; goto update; }

    /* Vowel modifications */
    if (lc == 'w') {
        push_undo(s);
        if (!s->buffer.empty()) {
            char last = std::tolower(static_cast<unsigned char>(s->buffer.back()));
            if (last == 'o' || last == 'a' || last == 'u') {
                apply_w_mod(s);
            } else {
                s->buffer += 'w';
            }
        } else {
            s->buffer += 'w';
        }
        goto update;
    }

    if (lc == 'd' && !s->buffer.empty() &&
        std::tolower(static_cast<unsigned char>(s->buffer.back())) == 'd') {
        push_undo(s);
        s->buffer.pop_back();
        s->buffer += "\xC4\x91"; /* đ */
        goto update;
    }

    /* Double vowel patterns */
    if (!s->buffer.empty()) {
        char last = std::tolower(static_cast<unsigned char>(s->buffer.back()));
        if (lc == last && (lc == 'a' || lc == 'e' || lc == 'o')) {
            push_undo(s);
            s->buffer.pop_back();
            if (lc == 'a') s->buffer += "\xC3\xA2"; /* â */
            else if (lc == 'e') s->buffer += "\xC3\xAA"; /* ê */
            else if (lc == 'o') s->buffer += "\xC3\xB4"; /* ô */
            goto update;
        }
        if (lc == 'w' && last == 'o') {
            push_undo(s);
            s->buffer.pop_back();
            s->buffer += "\xC6\xA1"; /* ơ */
            goto update;
        }
        if (lc == 'w' && last == 'a') {
            push_undo(s);
            s->buffer.pop_back();
            s->buffer += "\xC4\x83"; /* ă */
            goto update;
        }
    }

    /* Regular character */
    push_undo(s);
    s->buffer += c;
    s->last_key = lc;

    if (!has_vietnamese_vowel_pattern(s->buffer)) {
        s->char_count_no_vowel++;
        if (s->char_count_no_vowel >= 3) {
            s->passthrough_word = true;
        }
    } else {
        s->char_count_no_vowel = 0;
    }

update:
    {
        std::string display = apply_tone_to_syllable(s->buffer, s->tone);
        display = nfc_compose(display);
        std::strncpy(r->preedit, display.c_str(), sizeof(r->preedit) - 1);
    }
    r->consumed = 1;
}

} /* namespace vietkey */
