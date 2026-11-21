/* Personal words, behind XKUser (learn.cpp). */
#ifndef XRNKEY_LEARN_H
#define XRNKEY_LEARN_H

#include <string>
#include <unordered_map>

namespace xrnkey {

struct User {
    std::unordered_map<std::string, double> words;  /* token -> count */
    std::unordered_map<std::string, double> pairs;  /* "prev word" -> count */
    std::unordered_map<std::string, double> left;   /* prev -> sum of its pairs' counts */
    double total = 0;                                /* sum of words' counts */

    void add_word(const std::string& word, double n);
    void add_pair(const std::string& prev, const std::string& word, double n);
    /* Rebuild `total` and `left` after entries went. */
    void recount();
    /* P(word | prev) with the owner's pairs mixed into `p_models`. */
    double blend(double p_models, const std::string& prev, const std::string& word) const;
};

}  // namespace xrnkey

#endif
