/* Scoring words in context with the language models: one mixture that the
 * English/Vietnamese choice (bilingual_switch.cpp) and accent restoration
 * (restore.cpp) share, so a word scores the same whichever way it was
 * typed. */
#ifndef XRNIKEY_SCORE_H
#define XRNIKEY_SCORE_H

#include "lm_internal.h"
#include "xrnikey_internal.h"

#include <string>

namespace xrnikey {

struct User;

/* What scoring uses, behind XKModels. Nothing is owned. */
struct Models {
    const Lm* vi = nullptr;
    const Lm* en = nullptr;
    User* user = nullptr;  /* personal words (learn.cpp); learning writes to it */
};

/* P(next frame | language of the word before); LANG_NONE is the start of
 * a sentence. */
double frame_prob(uint8_t prev, uint8_t next);
/* The language of the latest word in the context, skipping numbers and
 * pauses; LANG_NONE at the start of a sentence. */
uint8_t previous_lang(const Context& c);
/* log10 P(token | context) under one model; an unknown token gets its
 * share of the unknown mass. */
float token_logprob(const Lm& m, const Context& c, const std::string& token);

/* log10 P(token next | context) under the mixture
 *
 *   P = P(vi frame | prev) * P_vi(token) + P(en frame | prev) * P_en(token)
 *
 * The English part counts only for a-z tokens, and only with both models
 * (with one model, it is that model alone). The owner's personal words,
 * when there are any, are mixed in on top (learn.cpp). `lang`, when given,
 * gets the frame that explains the token better. */
double mix_logprob(const Models& m, const Context& c, const std::string& token, uint8_t* lang = nullptr);

bool is_ascii_word(const std::string& t);
/* Learn `text` as typed after the context's last word (learn.cpp). */
void learn_word(User& u, const Context& c, const std::string& text);
/* Add a finished word, or a number, to the context, with its language. */
void context_push_text(const Models* m, Context& c, const std::string& text);
/* Add a punctuation key to the context: , ; : pause, . ! ? end the sentence. */
void context_push_char(const Models* m, Context& c, char ch);
/* The context for `words` (oldest first). "<s>" marks the start of a
 * sentence; without it the words follow the start only if `start`. */
Context context_from(const Models* m, const char* const* words, int n, bool start);

}  // namespace xrnikey

#endif
