/* xk-bench: what the engine costs per call, with a pair of real models.
 *
 * usage: xk-bench VI.xklm EN.xklm
 *
 * Prints, in milliseconds: opening the models; the first restoration call
 * (anything built lazily lands there); a run of three words typed without
 * marks, with correction off and on (p50, p95, p99 over the same sentences
 * typed word by word with look-back); and one Telex key through a state.
 * Sentences are made up. */
#include <xrnkey.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

double ms_since(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}

void report(const char* what, std::vector<double> v) {
    std::sort(v.begin(), v.end());
    auto at = [&](double q) { return v[std::min(v.size() - 1, static_cast<size_t>(q * static_cast<double>(v.size())))]; };
    std::printf("%-44s p50 %7.3f  p95 %7.3f  p99 %7.3f  max %7.3f ms  (%zu calls)\n", what, at(0.50),
                at(0.95), at(0.99), v.back(), v.size());
}

const std::vector<std::vector<const char*>> kSentences = {
    {"toi", "khong", "biet", "ban", "co", "di", "duoc", "khong"},
    {"hom", "nay", "troi", "mua", "to", "qua"},
    {"chieu", "nay", "hop", "luc", "may", "gio", "vay"},
    {"minh", "dang", "o", "quan", "ca", "phe", "gan", "cong", "ty"},
    {"gui", "file", "cho", "anh", "qua", "email", "nhe"},
    {"cuoi", "tuan", "ca", "nha", "ve", "que", "an", "gio"},
    {"khonh", "bieg", "dc", "gi", "het"},
    {"we", "can", "chat", "after", "the", "meeting"},
};

std::vector<double> runs(XKModels* m, int rounds) {
    std::vector<double> t;
    for (int r = 0; r < rounds; ++r)
        for (const auto& s : kSentences) {
            std::vector<std::string> out{"<s>"};
            for (size_t i = 0; i < s.size(); ++i) {
                const size_t from = i >= 2 ? i - 2 : 0;
                std::vector<const char*> ctx;
                for (size_t k = out.size() - (i - from) > 3 ? out.size() - (i - from) - 3 : 0;
                     k < out.size() - (i - from); ++k)
                    ctx.push_back(out[k].c_str());
                std::vector<const char*> typed(s.begin() + static_cast<long>(from), s.begin() + static_cast<long>(i) + 1);
                std::vector<char*> got(typed.size(), nullptr);
                const auto t0 = Clock::now();
                const int n = xk_restore_run(m, ctx.data(), static_cast<int>(ctx.size()), typed.data(),
                                             static_cast<int>(typed.size()), nullptr, got.data());
                t.push_back(ms_since(t0));
                out.resize(out.size() - (i - from));
                for (int k = 0; k < n; ++k) {
                    out.emplace_back(got[static_cast<size_t>(k)]);
                    std::free(got[static_cast<size_t>(k)]);
                }
            }
        }
    return t;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: xk-bench VI.xklm EN.xklm\n");
        return 2;
    }
    auto t0 = Clock::now();
    XKLm* vi = xk_lm_open(argv[1]);
    XKLm* en = xk_lm_open(argv[2]);
    const double open_ms = ms_since(t0);
    if (!vi || !en) {
        std::fprintf(stderr, "xk-bench: cannot open the models\n");
        return 1;
    }
    XKModels* m = xk_models_new(vi, en);
    std::printf("%-44s %7.3f ms\n", "open both models", open_ms);

    t0 = Clock::now();
    const char* ctx[] = {"<s>"};
    const char* typed[] = {"khong"};
    char* out[1];
    xk_restore_run(m, ctx, 1, typed, 1, nullptr, out);
    std::free(out[0]);
    std::printf("%-44s %7.3f ms\n", "first restoration call", ms_since(t0));

    xk_models_set_correction(m, XK_CORRECT_OFF);
    report("run of up to 3 words, correction off", runs(m, 20));
    xk_models_set_correction(m, XK_CORRECT_FLEXIBLE);
    report("run of up to 3 words, correction flexible", runs(m, 20));

    std::vector<double> keys;
    XKState* s = xk_state_new(XK_MODE_TELEX);
    xk_state_set_models(s, m);
    const std::string text = "tooi khoong bieets banj cos ddi dduowcj khoong my list hai lits nuwowcs ";
    for (int r = 0; r < 200; ++r)
        for (char c : text) {
            const auto k0 = Clock::now();
            xk_process_key(s, static_cast<unsigned char>(c), 0);
            keys.push_back(ms_since(k0));
        }
    report("one Telex key through a state", keys);
    xk_state_free(s);
    xk_models_free(m);
    xk_lm_free(vi);
    xk_lm_free(en);
    return 0;
}
