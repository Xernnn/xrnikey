#include "vietkey.h"
#include "vietkey_internal.h"
#include <cstring>
#include <cstdlib>

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
    s->tone = VK_TONE_NONE;
    s->passthrough_word = false;
    s->char_count_no_vowel = 0;
    s->undo_stack.clear();
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
            std::string out = vietkey::apply_tone_to_syllable(s->buffer, s->tone);
            out = vietkey::nfc_compose(out);

            if (s->passthrough_word || vk_is_english_word(out.c_str())) {
                std::strncpy(r.commit, s->buffer.c_str(), sizeof(r.commit) - 1);
            } else {
                std::strncpy(r.commit, out.c_str(), sizeof(r.commit) - 1);
            }
            r.commit[sizeof(r.commit) - 1] = '\0';

            /* Append space */
            size_t len = strlen(r.commit);
            if (len < sizeof(r.commit) - 1) r.commit[len] = ' ';

            s->buffer.clear();
            s->tone = VK_TONE_NONE;
            s->passthrough_word = false;
            s->char_count_no_vowel = 0;
            s->undo_stack.clear();
            r.consumed = 1;
        }
        return r;
    }

    /* Printable ASCII */
    if (keysym >= 0x20 && keysym <= 0x7E) {
        char c = static_cast<char>(keysym);
        if (s->mode == VK_MODE_TELEX) vietkey::telex_process_char(s, c, &r);
        else vietkey::vni_process_char(s, c, &r);
        return r;
    }

    return r;
}

} /* extern "C" */
