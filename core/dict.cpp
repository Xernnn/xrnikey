#include "xrnikey.h"
#include <fstream>
#include <vector>
#include <string>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct TrieNode {
    int32_t base;
    int32_t check;
};

struct XKDictImpl {
    void*  mmap_data;
    size_t mmap_size;
    int    fd;
    int32_t* base_array;
    int32_t* check_array;
    char*    tail;
    int      num_nodes;
    std::vector<std::pair<std::string, int>> user_words;
};

static const int32_t TRIE_ROOT = 1;
/* Every word ends with a 0x00 edge to a leaf whose (negative) base carries the
 * frequency. A word that is also a prefix of another word therefore keeps a
 * real children-offset base on its own node -- see data/dict/build_dict.sh. */
static const uint8_t TRIE_TERMINATOR = 0;

static int32_t trie_walk(XKDictImpl* d, uint8_t c, int32_t s) {
    if (d->base_array[s] < 0) return -1;  /* value leaf: no outgoing edges */
    int32_t next = d->base_array[s] + c;
    if (next >= 0 && next < d->num_nodes && d->check_array[next] == s)
        return next;
    return -1;
}

static bool trie_has_value(XKDictImpl* d, int32_t s) {
    return d->base_array[s] < 0;
}

static int trie_get_freq(XKDictImpl* d, int32_t s) {
    return -d->base_array[s];
}

extern "C" {

XKDict* xk_dict_load(const char* path) {
    if (!path) return nullptr;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return nullptr;

    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return nullptr; }

    void* data = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) { close(fd); return nullptr; }

    auto* d = new XKDictImpl{};
    d->fd = fd;
    d->mmap_data = data;
    d->mmap_size = st.st_size;

    auto* header = static_cast<int32_t*>(data);
    d->num_nodes = header[0];
    d->base_array = header + 1;
    d->check_array = d->base_array + d->num_nodes;
    d->tail = reinterpret_cast<char*>(d->check_array + d->num_nodes);

    return reinterpret_cast<XKDict*>(d);
}

void xk_dict_free(XKDict* dict) {
    if (!dict) return;
    auto* d = reinterpret_cast<XKDictImpl*>(dict);
    if (d->mmap_data) munmap(d->mmap_data, d->mmap_size);
    if (d->fd >= 0) close(d->fd);
    delete d;
}

/* The user's own words (xk-dict add) count alongside the built-in trie; a user word's
 * frequency wins when it is higher, so adding a known word can also lift it. */
static int user_freq(const XKDictImpl* d, const char* word) {
    int best = 0;
    for (const auto& uw : d->user_words)
        if (uw.first == word && uw.second > best) best = uw.second;
    return best;
}

static int trie_lookup(XKDictImpl* d, const char* word) {
    int32_t s = TRIE_ROOT;
    for (const char* p = word; *p; ++p) {
        s = trie_walk(d, static_cast<uint8_t>(*p), s);
        if (s < 0) return 0;
    }
    int32_t t = trie_walk(d, TRIE_TERMINATOR, s);
    if (t >= 0 && trie_has_value(d, t)) return trie_get_freq(d, t);
    return 0;
}

int xk_dict_lookup(XKDict* dict, const char* word) {
    if (!dict || !word) return 0;
    auto* d = reinterpret_cast<XKDictImpl*>(dict);
    return std::max(trie_lookup(d, word), user_freq(d, word));
}

int xk_dict_complete(XKDict* dict, const char* prefix, char** results, int max_results) {
    if (!dict || !prefix || !results || max_results <= 0) return 0;
    auto* d = reinterpret_cast<XKDictImpl*>(dict);

    struct Candidate { std::string word; int freq; };
    std::vector<Candidate> cands;

    /* User words first, so the trie's walk below can't crowd them out of the cap. */
    const std::string pre(prefix);
    for (const auto& uw : d->user_words) {
        if (uw.first.compare(0, pre.size(), pre) != 0) continue;
        bool seen = false;
        for (auto& c : cands)
            if (c.word == uw.first) { c.freq = std::max(c.freq, uw.second); seen = true; }
        if (!seen) cands.push_back({uw.first, uw.second});
    }

    int32_t s = TRIE_ROOT;
    for (const char* p = prefix; *p && s >= 0; ++p)
        s = trie_walk(d, static_cast<uint8_t>(*p), s);

    /* DFS from current node (none when the prefix isn't in the trie: user words only) */
    std::vector<std::pair<int32_t, std::string>> stack;
    if (s >= 0) stack.push_back({s, pre});
    const size_t user_count = cands.size();

    while (!stack.empty() && static_cast<int>(cands.size() - user_count) < max_results * 4) {
        auto [node, word] = stack.back();
        stack.pop_back();

        int32_t term = trie_walk(d, TRIE_TERMINATOR, node);
        if (term >= 0 && trie_has_value(d, term)) {
            int freq = trie_get_freq(d, term);
            bool seen = false;
            for (size_t i = 0; i < user_count; ++i)
                if (cands[i].word == word) { cands[i].freq = std::max(cands[i].freq, freq); seen = true; }
            if (!seen) cands.push_back({word, freq});
        }

        for (int c = 1; c < 256; ++c) {
            int32_t next = trie_walk(d, static_cast<uint8_t>(c), node);
            if (next >= 0) {
                stack.push_back({next, word + static_cast<char>(c)});
            }
        }
    }

    std::sort(cands.begin(), cands.end(),
              [](const Candidate& a, const Candidate& b) { return a.freq > b.freq; });

    int count = std::min(max_results, static_cast<int>(cands.size()));
    for (int i = 0; i < count; ++i) {
        results[i] = strdup(cands[i].word.c_str());
    }
    return count;
}

void xk_dict_add_word(XKDict* dict, const char* word, int freq) {
    if (!dict || !word) return;
    auto* d = reinterpret_cast<XKDictImpl*>(dict);
    d->user_words.push_back({word, freq});
}

/* Lines are "word<TAB>frequency" (what xk-dict add writes). Bad lines are skipped: this runs
 * inside the input method, and an exception escaping this C function would take Fcitx5 down. */
void xk_dict_reload_user(XKDict* dict, const char* user_path) {
    if (!dict || !user_path) return;
    auto* d = reinterpret_cast<XKDictImpl*>(dict);
    d->user_words.clear();
    std::ifstream f(user_path);
    std::string line;
    while (std::getline(f, line) && d->user_words.size() < 100000) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t tab = line.find('\t');
        if (tab == 0 || tab == std::string::npos) continue;
        const char* num = line.c_str() + tab + 1;
        char* end = nullptr;
        errno = 0;
        long freq = std::strtol(num, &end, 10);
        if (end == num || errno == ERANGE || freq < 0 || freq > INT32_MAX) continue;
        d->user_words.push_back({line.substr(0, tab), static_cast<int>(freq)});
    }
}

} /* extern "C" */
