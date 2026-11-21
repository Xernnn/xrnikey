/* Where the letter keys are, and how likely one is hit for another: the
 * channel model behind typo correction (restore.cpp).
 *
 * The phone's QWERTY rows, in key widths: q..p from x = 0, a..l from 0.5,
 * z..m from 1.5, one row apart. Aiming at key i, the finger lands on key t
 * with probability
 *
 *   P(t | i) = 1 - e                         when t == i
 *            = e * w(i, t) / sum_t' w(i, t')  otherwise, w = exp(-d^2 / 2s^2)
 *
 * for the keys t within 1.5 key widths of i, and 0 further away. A touch
 * keyboard that knows where the finger landed can do better (the spec's
 * per-tap neighbour weights); this is the layout's default. */
#include "keys.h"

#include <array>
#include <cmath>

namespace xrnkey {

namespace {

struct Pos {
    double x, y;
};

std::array<Pos, 26> positions() {
    std::array<Pos, 26> p{};
    const char* rows[3] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
    const double start[3] = {0.0, 0.5, 1.5};
    for (int r = 0; r < 3; ++r)
        for (int i = 0; rows[r][i]; ++i)
            p[static_cast<size_t>(rows[r][i] - 'a')] = {start[r] + i, static_cast<double>(r)};
    return p;
}

struct Table {
    /* log10 P(typed t | meant i) - log10 P(typed i | meant i), or 0 for
     * keys too far apart to be confused. */
    double cost[26][26]{};
    bool near[26][26]{};
};

Table build() {
    Table t;
    const auto p = positions();
    constexpr double kSigma = 0.6, kReach = 1.5;
    for (int i = 0; i < 26; ++i) {
        double w[26]{}, sum = 0;
        for (int j = 0; j < 26; ++j) {
            if (i == j) continue;
            const double dx = p[static_cast<size_t>(i)].x - p[static_cast<size_t>(j)].x;
            const double dy = p[static_cast<size_t>(i)].y - p[static_cast<size_t>(j)].y;
            const double d = std::sqrt(dx * dx + dy * dy);
            if (d > kReach) continue;
            w[j] = std::exp(-d * d / (2 * kSigma * kSigma));
            sum += w[j];
        }
        for (int j = 0; j < 26; ++j) {
            if (w[j] <= 0) continue;
            t.near[i][j] = true;
            t.cost[i][j] = std::log10(kTypoRate * w[j] / sum) - std::log10(1.0 - kTypoRate);
        }
    }
    return t;
}

const Table& table() {
    static const Table t = build();
    return t;
}

}  // namespace

const std::vector<char>& keys_near(char typed) {
    static const std::array<std::vector<char>, 26> near = [] {
        std::array<std::vector<char>, 26> n;
        const Table& t = table();
        for (int typed_key = 0; typed_key < 26; ++typed_key)
            for (int meant = 0; meant < 26; ++meant)
                if (t.near[meant][typed_key]) n[static_cast<size_t>(typed_key)].push_back(static_cast<char>('a' + meant));
        return n;
    }();
    static const std::vector<char> none;
    if (typed < 'a' || typed > 'z') return none;
    return near[static_cast<size_t>(typed - 'a')];
}

double typo_cost(char meant, char typed) {
    if (meant < 'a' || meant > 'z' || typed < 'a' || typed > 'z' || meant == typed) return 0.0;
    const Table& t = table();
    return t.near[meant - 'a'][typed - 'a'] ? t.cost[meant - 'a'][typed - 'a'] : -99.0;
}

}  // namespace xrnkey
