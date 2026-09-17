#include "xrnikey.h"
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

struct EmojiEntry {
    std::string character;
    std::string name;
    std::string shortcode;
    std::vector<std::string> tags;
    std::vector<std::string> vi_tags;
};

struct XKEmojiDBImpl {
    std::vector<EmojiEntry> entries;
};

static bool fuzzy_match(const std::string& query, const std::string& target) {
    if (target.find(query) != std::string::npos) return true;
    size_t qi = 0;
    for (char c : target) {
        if (qi < query.size() && c == query[qi]) qi++;
    }
    return qi == query.size();
}

extern "C" {

XKEmojiDB* xk_emoji_load(const char* json_path) {
    if (!json_path) return nullptr;

    auto* db = new XKEmojiDBImpl{};
    std::ifstream f(json_path);
    if (!f.is_open()) { delete db; return nullptr; }

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    /* Minimal JSON parser for emoji entries */
    size_t pos = 0;
    while ((pos = content.find("\"char\"", pos)) != std::string::npos) {
        EmojiEntry entry;

        auto extract_string = [&](const char* key) -> std::string {
            size_t k = content.find(key, pos);
            if (k == std::string::npos) return "";
            size_t start = content.find('"', k + strlen(key));
            if (start == std::string::npos) return "";
            start++;
            size_t end = content.find('"', start);
            if (end == std::string::npos) return "";
            return content.substr(start, end - start);
        };

        entry.character = extract_string("\"char\"");
        entry.name = extract_string("\"name\"");
        entry.shortcode = extract_string("\"shortcode\"");

        std::string tags_str = extract_string("\"tags\"");
        if (!tags_str.empty()) entry.tags.push_back(tags_str);

        std::string vi_str = extract_string("\"vi\"");
        if (!vi_str.empty()) entry.vi_tags.push_back(vi_str);

        if (!entry.character.empty())
            db->entries.push_back(entry);

        pos++;
    }

    return reinterpret_cast<XKEmojiDB*>(db);
}

void xk_emoji_free(XKEmojiDB* db) {
    if (db) delete reinterpret_cast<XKEmojiDBImpl*>(db);
}

int xk_emoji_search(XKEmojiDB* db, const char* query,
                    char** chars, char** names, int max_results) {
    if (!db || !query || !chars || !names || max_results <= 0) return 0;

    auto* d = reinterpret_cast<XKEmojiDBImpl*>(db);
    std::string q(query);
    for (char& c : q) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

    struct Match { int idx; int score; };
    std::vector<Match> matches;

    for (size_t i = 0; i < d->entries.size(); ++i) {
        const auto& e = d->entries[i];
        int score = 0;

        if (fuzzy_match(q, e.name)) score += 10;
        if (fuzzy_match(q, e.shortcode)) score += 8;
        for (const auto& t : e.tags) if (fuzzy_match(q, t)) score += 5;
        for (const auto& t : e.vi_tags) if (fuzzy_match(q, t)) score += 7;

        if (score > 0) matches.push_back({static_cast<int>(i), score});
    }

    std::sort(matches.begin(), matches.end(),
              [](const Match& a, const Match& b) { return a.score > b.score; });

    int count = std::min(max_results, static_cast<int>(matches.size()));
    for (int i = 0; i < count; ++i) {
        const auto& e = d->entries[matches[i].idx];
        chars[i] = strdup(e.character.c_str());
        names[i] = strdup(e.name.c_str());
    }
    return count;
}

} /* extern "C" */
