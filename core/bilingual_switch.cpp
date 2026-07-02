#include "vietkey.h"
#include "vietkey_internal.h"
#include <unordered_set>
#include <fstream>
#include <string>
#include <cstring>

static std::unordered_set<std::string> english_whitelist;

static void init_english_whitelist() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    static const char* words[] = {
        "the", "and", "for", "with", "from", "this", "that", "have", "has",
        "had", "not", "but", "what", "all", "were", "when", "your", "can",
        "said", "there", "use", "each", "which", "their", "time", "will",
        "about", "if", "up", "out", "many", "then", "them", "these", "so",
        "some", "her", "would", "make", "like", "into", "him", "has", "two",
        "more", "go", "no", "way", "could", "my", "than", "first", "been",
        "call", "who", "its", "now", "find", "long", "down", "day", "did",
        "get", "come", "made", "may", "part", "over", "new", "sound", "take",
        "only", "little", "work", "know", "place", "year", "live", "me",
        "back", "give", "most", "very", "after", "thing", "our", "just",
        "name", "good", "sentence", "man", "think", "say", "great", "where",
        "help", "through", "much", "before", "line", "right", "too", "mean",
        "old", "any", "same", "tell", "boy", "follow", "came", "want", "show",
        "also", "around", "form", "three", "small", "set", "put", "end",
        "why", "again", "turn", "here", "off", "went", "old", "number", "no",
        "way", "could", "people", "water", "than", "call", "first", "been",
        "hello", "world", "computer", "software", "hardware", "internet",
        "email", "password", "login", "logout", "user", "admin", "system",
        "file", "folder", "directory", "path", "name", "type", "class",
        "function", "method", "variable", "string", "integer", "boolean",
        "array", "list", "map", "set", "object", "null", "true", "false",
        "return", "import", "export", "module", "package", "library",
        "framework", "database", "server", "client", "network", "protocol",
        "http", "https", "api", "json", "xml", "html", "css", "javascript",
        "python", "java", "rust", "golang", "linux", "windows", "macos",
        "docker", "kubernetes", "git", "github", "gitlab", "commit", "push",
        "pull", "merge", "branch", "repository", "issue", "pull request",
        "test", "debug", "build", "deploy", "release", "version", "update",
        "install", "configure", "settings", "options", "preferences",
        "keyboard", "mouse", "screen", "display", "window", "application",
        "program", "process", "thread", "memory", "disk", "cpu", "gpu",
        "error", "warning", "info", "log", "trace", "exception", "stack",
        "buffer", "cache", "queue", "stack", "heap", "pointer", "reference",
        "value", "key", "index", "offset", "length", "size", "count",
        "open", "close", "read", "write", "create", "delete", "update",
        "search", "find", "replace", "copy", "paste", "cut", "undo", "redo",
        "save", "load", "print", "scan", "send", "receive", "connect",
        "disconnect", "start", "stop", "pause", "resume", "cancel", "confirm",
        "accept", "reject", "approve", "deny", "allow", "block", "enable",
        "disable", "show", "hide", "expand", "collapse", "minimize",
        "maximize", "restore", "fullscreen", "zoom", "scroll", "navigate",
        "forward", "backward", "next", "previous", "first", "last", "top",
        "bottom", "left", "right", "center", "middle", "above", "below",
        "inside", "outside", "between", "among", "within", "without", "during",
        "before", "after", "while", "until", "since", "because", "although",
        "however", "therefore", "moreover", "furthermore", "nevertheless",
        "meanwhile", "otherwise", "instead", "besides", "except", "including",
        "excluding", "regarding", "concerning", "according", "based", "using",
        "via", "per", "plus", "minus", "times", "divided", "equal", "greater",
        "less", "between", "among", "percent", "degree", "meter", "liter",
        "gram", "kilogram", "milliliter", "celsius", "fahrenheit", "mile",
        "kilometer", "inch", "foot", "yard", "pound", "ounce", "gallon",
        "quart", "pint", "cup", "tablespoon", "teaspoon", "second", "minute",
        "hour", "day", "week", "month", "year", "decade", "century",
        "monday", "tuesday", "wednesday", "thursday", "friday", "saturday",
        "sunday", "january", "february", "march", "april", "may", "june",
        "july", "august", "september", "october", "november", "december",
        "spring", "summer", "autumn", "winter", "morning", "afternoon",
        "evening", "night", "today", "tomorrow", "yesterday", "now", "later",
        "soon", "always", "never", "sometimes", "often", "rarely", "usually",
        "maybe", "perhaps", "probably", "certainly", "definitely", "absolutely",
        "completely", "totally", "entirely", "partially", "slightly", "barely",
        "hardly", "nearly", "almost", "exactly", "approximately", "roughly",
        "about", "around", "over", "under", "above", "below", "beyond",
        "across", "along", "through", "throughout", "within", "without",
        "against", "toward", "towards", "into", "onto", "upon", "off", "out",
        "up", "down", "in", "on", "at", "by", "for", "with", "without",
        "about", "between", "among", "during", "before", "after", "since",
        "until", "while", "because", "although", "unless", "whether", "if",
        "when", "where", "why", "how", "what", "which", "who", "whom", "whose",
        "yes", "no", "ok", "okay", "please", "thanks", "thank", "sorry",
        "welcome", "goodbye", "hello", "hi", "hey", "bye", "see", "look",
        "watch", "listen", "hear", "speak", "talk", "say", "tell", "ask",
        "answer", "reply", "respond", "explain", "describe", "define",
        "mean", "understand", "know", "learn", "teach", "study", "read",
        "write", "type", "edit", "modify", "change", "fix", "repair",
        "break", "damage", "destroy", "create", "make", "build", "design",
        "develop", "implement", "execute", "run", "compile", "interpret",
        "translate", "convert", "transform", "process", "handle", "manage",
        "control", "monitor", "track", "record", "store", "retrieve", "fetch",
        "query", "filter", "sort", "group", "aggregate", "calculate",
        "compute", "estimate", "predict", "analyze", "evaluate", "compare",
        "contrast", "match", "differ", "similar", "different", "same", "equal",
        "unique", "common", "rare", "special", "normal", "standard", "custom",
        "default", "optional", "required", "mandatory", "automatic", "manual",
        "public", "private", "protected", "internal", "external", "local",
        "global", "static", "dynamic", "constant", "variable", "mutable",
        "immutable", "readonly", "writable", "visible", "hidden", "active",
        "inactive", "enabled", "disabled", "valid", "invalid", "correct",
        "incorrect", "success", "failure", "pass", "fail", "pending", "done",
        "complete", "incomplete", "finished", "unfinished", "ready", "busy",
        "idle", "waiting", "running", "stopped", "paused", "cancelled",
        "scheduled", "delayed", "expired", "new", "old", "recent", "latest",
        "earliest", "current", "previous", "next", "future", "past", "present"
    };

    for (const char* w : words) english_whitelist.insert(w);
}

extern "C" {

int vk_is_english_word(const char* word) {
    if (!word) return 0;
    init_english_whitelist();
    std::string w(word);
    for (char& c : w) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (english_whitelist.count(w)) return 1;
    if (!vk_is_valid_syllable(word)) return 1;
    return 0;
}

void vk_bilingual_set_enabled(VKState* state, int enabled) {
    if (!state) return;
    auto* s = reinterpret_cast<vietkey::VKStateImpl*>(state);
    s->enabled = enabled != 0;
}

int vk_bilingual_is_enabled(VKState* state) {
    if (!state) return 1;
    auto* s = reinterpret_cast<vietkey::VKStateImpl*>(state);
    return s->enabled ? 1 : 0;
}

void vk_bilingual_load_state(VKState* state, const char* state_path) {
    if (!state || !state_path) return;
    auto* s = reinterpret_cast<vietkey::VKStateImpl*>(state);
    std::ifstream f(state_path);
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.find("\"enabled\"") != std::string::npos) {
            s->enabled = (line.find("true") != std::string::npos);
        }
    }
}

void vk_bilingual_save_state(VKState* state, const char* state_path) {
    if (!state || !state_path) return;
    auto* s = reinterpret_cast<vietkey::VKStateImpl*>(state);
    std::ofstream f(state_path);
    if (!f.is_open()) return;
    f << "{\n  \"enabled\": " << (s->enabled ? "true" : "false") << "\n}\n";
}

int vk_bilingual_app_disabled(const char* wm_class, const char* overrides_path) {
    if (!wm_class || !overrides_path) return 0;
    std::ifstream f(overrides_path);
    if (!f.is_open()) return 0;
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    return content.find(wm_class) != std::string::npos ? 1 : 0;
}

} /* extern "C" */
