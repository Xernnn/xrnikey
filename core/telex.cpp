#include "xrnikey_internal.h"
#include <algorithm>
#include <cctype>
#include <vector>

namespace xrnikey {

static void push_undo(XKStateImpl* s) {
    UndoEntry e;
    e.buffer = s->buffer;
    e.tone = s->tone;
    s->undo_stack.push_back(e);
    if (s->undo_stack.size() > 50) s->undo_stack.erase(s->undo_stack.begin());
}

static void finalize_syllable(XKStateImpl* s, XKResult* r) {
    std::string out = apply_tone_to_syllable(s->buffer, s->tone);
    out = nfc_compose(out);
    std::strncpy(r->commit, out.c_str(), sizeof(r->commit) - 1);
    s->buffer.clear();
    s->tone = XK_TONE_NONE;
    s->passthrough_word = false;
    s->char_count_no_vowel = 0;
}

/* Telex's `w` is a SYLLABLE-level modifier, not a previous-character one.
 * Unikey accepts it anywhere after the nucleus, so "nguoiw" and "duongw" have
 * to reach ngươi / dương exactly as "nguwowi" does.
 *
 * The old version fired only when the buffer ENDED in o/a/u. "nguoiw" ends in
 * 'i', so the w was appended literally and a following tone key then landed on
 * the wrong vowel: "nguoiwf" produced "nguoìw". It also horned a single vowel
 * and returned, so the "uo" cluster that Vietnamese writes "ươ" came out "uơ".
 *
 * Both rules below matter: find the nucleus even behind a coda, and treat
 * u+o as one unit that takes a horn on BOTH halves. */

struct WGlyph { size_t off; int len; uint32_t cp; };

/* Lowercase base letter of a nucleus vowel (circumflex/breve/horn forms all
 * fold to their base here), or 0 if the glyph is not a nucleus vowel. */
static char nucleus_base(uint32_t cp) {
    switch (cp) {
        case 'a': case 'A': case 0x0103: case 0x0102:
        case 0x00E2: case 0x00C2: return 'a';
        case 'e': case 'E': case 0x00EA: case 0x00CA: return 'e';
        case 'i': case 'I': return 'i';
        case 'o': case 'O': case 0x00F4: case 0x00D4:
        case 0x01A1: case 0x01A0: return 'o';
        case 'u': case 'U': case 0x01B0: case 0x01AF: return 'u';
        case 'y': case 'Y': return 'y';
        default: return 0;
    }
}

/* The glyph `w` turns this vowel into: u->ư, o->ơ, a->ă, preserving case.
 * Already-marked vowels map to themselves so a second `w` is idempotent.
 * 0 for vowels that take no `w` mark (e, i, y, and the circumflex forms). */
static uint32_t w_mark_of(uint32_t cp) {
    switch (cp) {
        case 'u': case 0x01B0: return 0x01B0;
        case 'U': case 0x01AF: return 0x01AF;
        case 'o': case 0x01A1: return 0x01A1;
        case 'O': case 0x01A0: return 0x01A0;
        case 'a': case 0x0103: return 0x0103;
        case 'A': case 0x0102: return 0x0102;
        default: return 0;
    }
}

/* Callers must rewrite glyphs RIGHT-TO-LEFT: replacing one changes the byte
 * offsets of everything after it, but leaves earlier offsets valid. */
static void replace_glyph(std::string& buf, const std::vector<WGlyph>& gs,
                          int k, uint32_t cp) {
    char enc[8];
    int n = utf8_encode(cp, enc);
    if (n > 0) buf.replace(gs[k].off, static_cast<size_t>(gs[k].len), enc,
                           static_cast<size_t>(n));
}

/* Returns false when there is no nucleus vowel to mark, so the caller can
 * emit a literal 'w' (leading "w", English words like "new"). */
static bool apply_w_mod(XKStateImpl* s) {
    std::string& buf = s->buffer;

    std::vector<WGlyph> gs;
    for (size_t i = 0; i < buf.size(); ) {
        uint32_t cp = 0;
        int len = 1;
        if (utf8_decode(buf.c_str() + i, &cp, &len) != 0 || len <= 0) {
            cp = static_cast<unsigned char>(buf[i]);
            len = 1;
        }
        gs.push_back({i, len, cp});
        i += static_cast<size_t>(len);
    }

    /* Rightmost run of nucleus vowels, skipping any coda after it. */
    int first = -1, last = -1;
    for (int k = static_cast<int>(gs.size()) - 1; k >= 0; --k) {
        if (nucleus_base(gs[k].cp)) {
            if (last < 0) last = k;
            first = k;
        } else if (last >= 0) {
            break;
        }
    }
    if (last < 0) return false;

    /* "uo" -- including a half-applied "ưo" / "uơ" -- is horned on both. */
    for (int k = first; k < last; ++k) {
        if (nucleus_base(gs[k].cp) == 'u' &&
            nucleus_base(gs[k + 1].cp) == 'o') {
            uint32_t mu = w_mark_of(gs[k].cp);
            uint32_t mo = w_mark_of(gs[k + 1].cp);
            if (mu && mo) {
                replace_glyph(buf, gs, k + 1, mo);
                replace_glyph(buf, gs, k, mu);
                return true;
            }
        }
    }

    /* Otherwise the last markable vowel of the nucleus takes it. */
    for (int k = last; k >= first; --k) {
        uint32_t m = w_mark_of(gs[k].cp);
        if (m) {
            replace_glyph(buf, gs, k, m);
            return true;
        }
    }
    return false;
}

void telex_backspace(XKStateImpl* s, XKResult* r) {
    if (!s->undo_stack.empty()) {
        UndoEntry e = s->undo_stack.back();
        s->undo_stack.pop_back();
        s->buffer = e.buffer;
        s->tone = e.tone;
    } else if (!s->buffer.empty()) {
        s->buffer.pop_back();
        if (s->buffer.empty()) s->tone = XK_TONE_NONE;
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

void telex_process_char(XKStateImpl* s, char c, XKResult* r) {
    char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

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

    /* Double-press escape: same key twice quickly outputs raw */
    if (lc == s->last_key && s->double_press_escape) {
        s->buffer += c;
        s->double_press_escape = false;
        s->passthrough_word = true;
        std::strncpy(r->preedit, s->buffer.c_str(), sizeof(r->preedit) - 1);
        if (s->instant_commit) {
            diff_commit(s->committed, s->buffer, r);
            s->committed = s->buffer;
        }
        r->consumed = 1;
        return;
    }

    /* Tone marks — only once a vowel already precedes (plain or already
     * transformed, e.g. the ê in "thê"); otherwise the key is a literal
     * leading consonant (the 'x' in "xin", the 's' in "sinh"), not a tone
     * signal. */
    if (find_tone_vowel_index(s->buffer) >= 0) {
        if (lc == 's') { push_undo(s); s->tone = XK_TONE_SAC; goto update; }
        if (lc == 'f') { push_undo(s); s->tone = XK_TONE_HUYEN; goto update; }
        if (lc == 'r') { push_undo(s); s->tone = XK_TONE_HOI; goto update; }
        if (lc == 'x') { push_undo(s); s->tone = XK_TONE_NGA; goto update; }
        if (lc == 'j') { push_undo(s); s->tone = XK_TONE_NANG; goto update; }
    }

    /* Vowel modifications */
    if (lc == 'w') {
        push_undo(s);
        if (!apply_w_mod(s)) {
            s->buffer += 'w';
        }
        goto update;
    }

    if (lc == 'd' && !s->buffer.empty() &&
        std::tolower(static_cast<unsigned char>(s->buffer.back())) == 'd') {
        push_undo(s);
        /* Keep the case the user typed: "DD" is Đ, not đ. */
        bool upper = s->buffer.back() >= 'A' && s->buffer.back() <= 'Z';
        s->buffer.pop_back();
        s->buffer += upper ? "\xC4\x90"   /* Đ */
                           : "\xC4\x91";  /* đ */
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
    }

    /* Regular character */
    push_undo(s);
    s->buffer += c;
    s->last_key = lc;

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

} /* namespace xrnikey */
