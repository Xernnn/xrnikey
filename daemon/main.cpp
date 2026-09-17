#include "socket_server.h"
#include <vietkey.h>
#include <iostream>
#include <csignal>
#include <atomic>
#include <vector>
#include <deque>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

static std::atomic<bool> g_running{true};

static void signal_handler(int) { g_running = false; }

struct DaemonState {
    VKDict* dict;
    VKNgram* ngram;
    VKEmojiDB* emoji;
    std::deque<std::string> recent_words;
    static constexpr size_t MAX_RECENT = 500;
    uint64_t keystrokes_saved;
    uint64_t language_switches;
};

static DaemonState g_state{};

static void add_recent_word(const std::string& word) {
    g_state.recent_words.push_back(word);
    if (g_state.recent_words.size() > DaemonState::MAX_RECENT)
        g_state.recent_words.pop_front();
}

static std::string handle_complete(const std::string& prefix,
                                    const std::vector<std::string>& context,
                                    int max) {
    char* results[32];
    int n = vk_dict_complete(g_state.dict, prefix.c_str(), results, max);

    std::string json = "{\"candidates\":[";
    for (int i = 0; i < n; ++i) {
        if (i > 0) json += ",";
        int freq = vk_dict_lookup(g_state.dict, results[i]);
        float score = freq > 0 ? static_cast<float>(freq) / 10000.0f : 0.5f;

        if (g_state.ngram && context.size() >= 2) {
            const char* ctx[] = {context[context.size()-2].c_str(),
                                  context[context.size()-1].c_str()};
            float ng = vk_ngram_score(g_state.ngram, ctx, 2, results[i]);
            score = score * 0.6f + (ng + 10.0f) / 10.0f * 0.4f;
        }

        json += "{\"text\":\"" + std::string(results[i]) +
                "\",\"score\":" + std::to_string(score) +
                ",\"type\":\"dict\"}";
        free(results[i]);
    }
    json += "]}";
    return json;
}

static std::string handle_emoji(const std::string& query) {
    char* chars[16];
    char* names[16];
    int n = vk_emoji_search(g_state.emoji, query.c_str(), chars, names, 16);

    std::string json = "{\"emoji\":[";
    for (int i = 0; i < n; ++i) {
        if (i > 0) json += ",";
        json += "{\"char\":\"" + std::string(chars[i]) +
                "\",\"name\":\"" + std::string(names[i]) +
                "\",\"shortcode\":\":emoji:\"}";
        free(chars[i]);
        free(names[i]);
    }
    json += "]}";
    return json;
}

static std::string handle_stats() {
    return "{\"keystrokes_saved\":" + std::to_string(g_state.keystrokes_saved) +
           ",\"language_switches\":" + std::to_string(g_state.language_switches) +
           ",\"recent_words\":" + std::to_string(g_state.recent_words.size()) + "}";
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    const char* data_dir = VIETKEY_DATA_DIR;
    std::string dict_path = std::string(data_dir) + "/dict/vi.dict";
    std::string ngram_path = std::string(data_dir) + "/ngram/vi.arpa";
    std::string emoji_path = std::string(data_dir) + "/emoji/emoji.json";

    g_state.dict = vk_dict_load(dict_path.c_str());
    g_state.ngram = vk_ngram_load(ngram_path.c_str());
    g_state.emoji = vk_emoji_load(emoji_path.c_str());
    g_state.keystrokes_saved = 0;
    g_state.language_switches = 0;

    uid_t uid = getuid();
    std::string socket_path = "/run/user/" + std::to_string(uid) + "/vietkey.sock";

    SocketServer server(socket_path, [](const std::string& request) -> std::string {
        if (request.find("\"type\":\"complete\"") != std::string::npos) {
            std::string prefix;
            size_t p = request.find("\"prefix\":\"");
            if (p != std::string::npos) {
                p += 10;
                size_t end = request.find('"', p);
                prefix = request.substr(p, end - p);
            }
            return handle_complete(prefix, {}, 5);
        }
        if (request.find("\"type\":\"emoji\"") != std::string::npos) {
            std::string query;
            size_t p = request.find("\"query\":\"");
            if (p != std::string::npos) {
                p += 9;
                size_t end = request.find('"', p);
                query = request.substr(p, end - p);
            }
            return handle_emoji(query);
        }
        if (request.find("\"type\":\"stats\"") != std::string::npos) {
            return handle_stats();
        }
        return "{\"error\":\"unknown request\"}";
    });

    std::cerr << "vkd: listening on " << socket_path << std::endl;
    server.run(g_running);

    if (g_state.dict) vk_dict_free(g_state.dict);
    if (g_state.ngram) vk_ngram_free(g_state.ngram);
    if (g_state.emoji) vk_emoji_free(g_state.emoji);

    return 0;
}
