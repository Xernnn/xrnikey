// The language model reader, against the small models built from the
// made-up text in data/lm/test.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>
#include <vector>

#include "lm_format.h"
#include "xrnikey.h"

namespace {

const std::string kVi = std::string(XRNIKEY_TEST_LM_DIR) + "/vi.xklm";

float lp(XKLm* m, std::vector<const char*> ctx, const char* w) {
    return xk_lm_logprob(m, ctx.data(), static_cast<int>(ctx.size()), w);
}

std::vector<char> slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string spit(const std::string& name, const std::vector<char>& bytes) {
    std::string path = std::string(XRNIKEY_TEST_TMP) + "/" + name;
    std::ofstream(path, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return path;
}

}  // namespace

TEST_CASE("a model knows its words by id and by text", "[lm]") {
    XKLm* m = xk_lm_open(kVi.c_str());
    REQUIRE(m != nullptr);
    CHECK(xk_lm_order(m) == 3);
    CHECK(std::string(xk_lm_word(m, 0)) == "<unk>");
    CHECK(std::string(xk_lm_word(m, 1)) == "<s>");
    int id = xk_lm_id(m, "không");
    REQUIRE(id > 2);
    CHECK(std::string(xk_lm_word(m, id)) == "không");
    CHECK(xk_lm_id(m, "khong") == 0);       // a spelling the text never had
    CHECK(xk_lm_word(m, xk_lm_vocab_size(m)) == nullptr);
    xk_lm_free(m);
}

TEST_CASE("every context's probabilities sum to one", "[lm]") {
    XKLm* m = xk_lm_open(kVi.c_str());
    REQUIRE(m != nullptr);
    // An unknown context word is <unk>, which the text never had: its
    // distribution is the unigram one. A lookup in its empty child range
    // used to match the next word's first child instead.
    std::vector<std::vector<const char*>> contexts = {
        {}, {"<s>"}, {"tôi"}, {"tôi", "không"}, {"<s>", "bạn"}, {"xyzzy"}, {"xyzzy", "không"},
        {"cái", "bàn"}, {"hai"},
    };
    for (auto& ctx : contexts) {
        double sum = 0;
        for (int id = 0; id < xk_lm_vocab_size(m); ++id) {
            const char* w = xk_lm_word(m, id);
            if (std::string(w) == "<s>") continue;
            sum += std::pow(10.0, lp(m, ctx, w));
        }
        INFO("context of " << ctx.size() << " words, first " << (ctx.empty() ? "-" : ctx[0]));
        CHECK_THAT(sum, Catch::Matchers::WithinAbs(1.0, 0.01));
    }
    xk_lm_free(m);
}

TEST_CASE("words seen after a context score above words that were not", "[lm]") {
    XKLm* m = xk_lm_open(kVi.c_str());
    REQUIRE(m != nullptr);
    CHECK(lp(m, {"không"}, "thể") > lp(m, {"không"}, "bàn"));
    CHECK(lp(m, {"dược"}, "phẩm") > lp(m, {}, "phẩm"));
    CHECK(lp(m, {"cái"}, "bàn") > lp(m, {"cái"}, "bạn"));
    CHECK(lp(m, {"chào"}, "bạn") > lp(m, {"chào"}, "bàn"));
    // Words the model never saw score alike, each with a share of the
    // unknown mass; it used to give each one all of it, so any unseen
    // spelling outranked every rare word the model knew.
    CHECK(lp(m, {}, "zzzz") == lp(m, {}, "qqqq"));
    CHECK(lp(m, {}, "zzzz") < lp(m, {}, "<unk>"));
    CHECK(lp(m, {"tôi"}, "zzzz") < lp(m, {"tôi"}, "<unk>"));
    xk_lm_free(m);
}

TEST_CASE("a model inside a larger file opens at its offset", "[lm]") {
    std::vector<char> model = slurp(kVi);
    REQUIRE(model.size() > 1000);
    // Not page-aligned, as inside a zip, with bytes after it too.
    std::vector<char> packed(5000 + model.size() + 300, 'x');
    std::memcpy(packed.data() + 5000, model.data(), model.size());
    std::string path = spit("packed.bin", packed);

    int fd = open(path.c_str(), O_RDONLY);
    REQUIRE(fd >= 0);
    XKLm* inner = xk_lm_open_fd(fd, 5000, static_cast<int64_t>(model.size()));
    close(fd);  // the mapping outlives the descriptor
    REQUIRE(inner != nullptr);
    XKLm* whole = xk_lm_open(kVi.c_str());
    REQUIRE(whole != nullptr);
    CHECK(lp(inner, {"tôi", "không"}, "thể") == lp(whole, {"tôi", "không"}, "thể"));
    CHECK(xk_lm_vocab_size(inner) == xk_lm_vocab_size(whole));
    xk_lm_free(inner);
    xk_lm_free(whole);
}

TEST_CASE("files that are not whole models are refused", "[lm]") {
    std::vector<char> model = slurp(kVi);
    REQUIRE(model.size() > 1000);
    CHECK(xk_lm_open("/nonexistent/vi.xklm") == nullptr);

    std::vector<char> cut(model.begin(), model.begin() + static_cast<long>(model.size() / 2));
    CHECK(xk_lm_open(spit("cut.xklm", cut).c_str()) == nullptr);

    std::vector<char> magic = model;
    magic[0] = 'Y';
    CHECK(xk_lm_open(spit("magic.xklm", magic).c_str()) == nullptr);

    // An offset that points past the end.
    std::vector<char> bad = model;
    xrnikey::lmfmt::Header h;
    std::memcpy(&h, bad.data(), sizeof(h));
    h.vocab_index = h.total_size + 64;
    std::memcpy(bad.data(), &h, sizeof(h));
    CHECK(xk_lm_open(spit("offset.xklm", bad).c_str()) == nullptr);

    CHECK(xk_lm_logprob(nullptr, nullptr, 0, "a") == xrnikey::lmfmt::kNever);
}
