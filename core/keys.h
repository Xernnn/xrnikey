/* The letter keys' layout and the typo channel model (keys.cpp). */
#ifndef XRNIKEY_KEYS_H
#define XRNIKEY_KEYS_H

#include <vector>

namespace xrnikey {

/* The chance a letter is mistyped as a neighbouring key. Measured on
 * held-out text with 3 % typos (xk-eval, dev): 0.02 fixes 91 % of them and
 * changes 0.24 % of correct words; 0.05 fixes 92 % but changes 0.49 %. */
constexpr double kTypoRate = 0.02;

/* The letters whose key lies near `typed` (a-z): what was perhaps meant. */
const std::vector<char>& keys_near(char typed);
/* log10 of how much likelier it is that `meant` was typed as itself than
 * as `typed` (negative; -99 for keys too far apart). */
double typo_cost(char meant, char typed);

}  // namespace xrnikey

#endif
