#include "vietkey.h"
#include <fstream>
#include <vector>
#include <string>
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

struct VKDictImpl {
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

static int32_t trie_walk(VKDictImpl* d, uint8_t c, int32_t s) {
    if (d->base_array[s] < 0) return -1;  /* value leaf: no outgoing edges */
    int32_t next = d->base_array[s] + c;
    if (next >= 0 && next < d->num_nodes && d->check_array[next] == s)
        return next;
    return -1;
}

static bool trie_has_value(VKDictImpl* d, int32_t s) {
    return d->base_array[s] < 0;
}

static int trie_get_freq(VKDictImpl* d, int32_t s) {
    return -d->base_array[s];
}

extern "C" {

VKDict* vk_dict_load(const char* path) {
    if (!path) return nullptr;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return nullptr;

    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return nullptr; }

    void* data = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) { close(fd); return nullptr; }

    auto* d = new VKDictImpl{};
    d->fd = fd;
    d->mmap_data = data;
    d->mmap_size = st.st_size;

    auto* header = static_cast<int32_t*>(data);
    d->num_nodes = header[0];
    d->base_array = header + 1;
    d->check_array = d->base_array + d->num_nodes;
    d->tail = reinterpret_cast<char*>(d->check_array + d->num_nodes);

    return reinterpret_cast<VKDict*>(d);
}

void vk_dict_free(VKDict* dict) {
    if (!dict) return;
    auto* d = reinterpret_cast<VKDictImpl*>(dict);
    if (d->mmap_data) munmap(d->mmap_data, d->mmap_size);
    if (d->fd >= 0) close(d->fd);
    delete d;
}

int vk_dict_lookup(VKDict* dict, const char* word) {
    if (!dict || !word) return 0;
    auto* d = reinterpret_cast<VKDictImpl*>(dict);
    int32_t s = TRIE_ROOT;
    for (const char* p = word; *p; ++p) {
        s = trie_walk(d, static_cast<uint8_t>(*p), s);
        if (s < 0) return 0;
    }
    int32_t t = trie_walk(d, TRIE_TERMINATOR, s);
    if (t >= 0 && trie_has_value(d, t)) return trie_get_freq(d, t);
    return 0;
}

int vk_dict_complete(VKDict* dict, const char* prefix, char** results, int max_results) {
    if (!dict || !prefix || !results || max_results <= 0) return 0;
    auto* d = reinterpret_cast<VKDictImpl*>(dict);

    int32_t s = TRIE_ROOT;
    for (const char* p = prefix; *p; ++p) {
        s = trie_walk(d, static_cast<uint8_t>(*p), s);
        if (s < 0) return 0;
    }

    struct Candidate { std::string word; int freq; };
    std::vector<Candidate> cands;

    /* DFS from current node */
    std::vector<std::pair<int32_t, std::string>> stack;
    stack.push_back({s, std::string(prefix)});

    while (!stack.empty() && static_cast<int>(cands.size()) < max_results * 4) {
        auto [node, word] = stack.back();
        stack.pop_back();

        int32_t term = trie_walk(d, TRIE_TERMINATOR, node);
        if (term >= 0 && trie_has_value(d, term)) {
            cands.push_back({word, trie_get_freq(d, term)});
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

void vk_dict_add_word(VKDict* dict, const char* word, int freq) {
    if (!dict || !word) return;
    auto* d = reinterpret_cast<VKDictImpl*>(dict);
    d->user_words.push_back({word, freq});
}

void vk_dict_reload_user(VKDict* dict, const char* user_path) {
    if (!dict || !user_path) return;
    auto* d = reinterpret_cast<VKDictImpl*>(dict);
    d->user_words.clear();
    std::ifstream f(user_path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t tab = line.find('\t');
        if (tab != std::string::npos) {
            std::string w = line.substr(0, tab);
            int freq = std::stoi(line.substr(tab + 1));
            d->user_words.push_back({w, freq});
        }
    }
}

} /* extern "C" */
