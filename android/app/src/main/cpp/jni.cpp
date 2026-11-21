// The JNI bridge between the keyboard (com.xern.xrnkey.engine.Native) and the shared core's
// C API (core/xrnkey.h). Text crosses as UTF-8 byte arrays, not as Java strings: JNI's
// "UTF" functions use modified UTF-8, which mangles emoji and NUL. Handles are the core's
// pointers as jlong. Nothing here throws: the core's C API does not, and a null handle or
// array is answered with an empty result.
#include <jni.h>

#include <xrnkey.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

template <typename T>
T* as(jlong h) {
    return reinterpret_cast<T*>(static_cast<intptr_t>(h));
}

template <typename T>
jlong handle(T* p) {
    return static_cast<jlong>(reinterpret_cast<intptr_t>(p));
}

std::string text_of(JNIEnv* env, jbyteArray a) {
    if (!a) return {};
    const jsize n = env->GetArrayLength(a);
    std::string s(static_cast<size_t>(n), '\0');
    env->GetByteArrayRegion(a, 0, n, reinterpret_cast<jbyte*>(s.data()));
    return s;
}

jbyteArray bytes(JNIEnv* env, const char* s) {
    const jsize n = s ? static_cast<jsize>(std::strlen(s)) : 0;
    jbyteArray a = env->NewByteArray(n);
    if (a && n) env->SetByteArrayRegion(a, 0, n, reinterpret_cast<const jbyte*>(s));
    return a;
}

jobjectArray byte_arrays(JNIEnv* env, const std::vector<std::string>& v) {
    jclass cls = env->FindClass("[B");
    jobjectArray out = env->NewObjectArray(static_cast<jsize>(v.size()), cls, nullptr);
    for (size_t i = 0; i < v.size(); ++i) {
        jbyteArray b = bytes(env, v[i].c_str());
        env->SetObjectArrayElement(out, static_cast<jsize>(i), b);
        env->DeleteLocalRef(b);
    }
    return out;
}

std::vector<std::string> strings(JNIEnv* env, jobjectArray a) {
    std::vector<std::string> v;
    if (!a) return v;
    const jsize n = env->GetArrayLength(a);
    for (jsize i = 0; i < n; ++i) {
        auto b = static_cast<jbyteArray>(env->GetObjectArrayElement(a, i));
        v.push_back(text_of(env, b));
        env->DeleteLocalRef(b);
    }
    return v;
}

std::vector<const char*> pointers(const std::vector<std::string>& v) {
    std::vector<const char*> p;
    for (const auto& s : v) p.push_back(s.c_str());
    return p;
}

/* A key's result: [commit, preedit], with consumed and delete_before in `out`. */
jobjectArray result(JNIEnv* env, const XKResult& r, jintArray out) {
    if (out && env->GetArrayLength(out) >= 2) {
        const jint ints[2] = {r.consumed, r.delete_before};
        env->SetIntArrayRegion(out, 0, 2, ints);
    }
    return byte_arrays(env, {r.commit, r.preedit});
}

}  // namespace

#define FN(ret, name) extern "C" JNIEXPORT ret JNICALL Java_com_xern_xrnkey_engine_Native_##name

FN(jlong, lmOpenPath)(JNIEnv* env, jobject, jbyteArray path) {
    return handle(xk_lm_open(text_of(env, path).c_str()));
}

FN(jlong, lmOpenFd)(JNIEnv*, jobject, jint fd, jlong offset, jlong length) {
    return handle(xk_lm_open_fd(fd, offset, length));
}

FN(void, lmFree)(JNIEnv*, jobject, jlong lm) { xk_lm_free(as<XKLm>(lm)); }

FN(jlong, modelsNew)(JNIEnv*, jobject, jlong vi, jlong en) {
    return handle(xk_models_new(as<XKLm>(vi), as<XKLm>(en)));
}

FN(void, modelsFree)(JNIEnv*, jobject, jlong m) { xk_models_free(as<XKModels>(m)); }

FN(void, modelsSetCorrection)(JNIEnv*, jobject, jlong m, jint level) {
    xk_models_set_correction(as<XKModels>(m), level);
}

FN(jlong, stateNew)(JNIEnv*, jobject, jint mode) { return handle(xk_state_new(mode)); }

FN(void, stateFree)(JNIEnv*, jobject, jlong s) { xk_state_free(as<XKState>(s)); }

FN(void, stateSetModels)(JNIEnv*, jobject, jlong s, jlong m) {
    xk_state_set_models(as<XKState>(s), as<XKModels>(m));
}

FN(void, stateSetOption)(JNIEnv*, jobject, jlong s, jint option, jint value) {
    xk_state_set_option(as<XKState>(s), option, value);
}

FN(void, stateReset)(JNIEnv*, jobject, jlong s) { xk_state_reset(as<XKState>(s)); }

FN(jobjectArray, stateKey)(JNIEnv* env, jobject, jlong s, jint keysym, jintArray out) {
    return result(env, xk_process_key(as<XKState>(s), static_cast<uint32_t>(keysym), 0), out);
}

FN(jobjectArray, stateFlush)(JNIEnv* env, jobject, jlong s, jintArray out) {
    return result(env, xk_state_flush(as<XKState>(s)), out);
}

FN(jbyteArray, stateKeys)(JNIEnv* env, jobject, jlong s) {
    char* k = xk_state_get_keys(as<XKState>(s));
    jbyteArray a = bytes(env, k);
    std::free(k);
    return a;
}

FN(void, stateSetContext)(JNIEnv* env, jobject, jlong s, jobjectArray words) {
    const auto w = strings(env, words);
    const auto p = pointers(w);
    xk_state_set_context(as<XKState>(s), p.data(), static_cast<int>(p.size()));
}

FN(jobjectArray, restoreRun)(JNIEnv* env, jobject, jlong m, jobjectArray context, jobjectArray typed,
                             jbyteArray next) {
    const auto c = strings(env, context), t = strings(env, typed);
    const auto cp = pointers(c), tp = pointers(t);
    const std::string nx = text_of(env, next);
    std::vector<char*> got(t.size(), nullptr);
    const int n = xk_restore_run(as<XKModels>(m), cp.data(), static_cast<int>(cp.size()), tp.data(),
                                 static_cast<int>(tp.size()), next ? nx.c_str() : nullptr, got.data());
    if (n <= 0) return nullptr;
    std::vector<std::string> out;
    for (int i = 0; i < n; ++i) {
        out.emplace_back(got[static_cast<size_t>(i)]);
        std::free(got[static_cast<size_t>(i)]);
    }
    return byte_arrays(env, out);
}

FN(jobjectArray, restoreWord)(JNIEnv* env, jobject, jlong m, jobjectArray context, jbyteArray typed,
                              jint max, jintArray types) {
    const auto c = strings(env, context);
    const auto cp = pointers(c);
    const std::string t = text_of(env, typed);
    std::vector<XKCandidate> cands(static_cast<size_t>(max > 0 ? max : 0));
    const int n = max > 0 ? xk_restore_word(as<XKModels>(m), cp.data(), static_cast<int>(cp.size()), t.c_str(),
                                            cands.data(), max)
                          : 0;
    std::vector<std::string> out;
    std::vector<jint> kinds;
    for (int i = 0; i < n; ++i) {
        out.emplace_back(cands[static_cast<size_t>(i)].text);
        kinds.push_back(cands[static_cast<size_t>(i)].type);
    }
    if (types && env->GetArrayLength(types) >= n && n > 0) env->SetIntArrayRegion(types, 0, n, kinds.data());
    return byte_arrays(env, out);
}

// ---- emoji (core/emoji.cpp) --------------------------------------------------------------------

FN(jlong, emojiLoad)(JNIEnv* env, jobject, jbyteArray text) {
    const std::string t = text_of(env, text);
    return handle(xk_emoji_load_text(t.data(), t.size()));
}

FN(void, emojiFree)(JNIEnv*, jobject, jlong db) { xk_emoji_free(as<XKEmojiDB>(db)); }

FN(jint, emojiCount)(JNIEnv*, jobject, jlong db) { return xk_emoji_count(as<XKEmojiDB>(db)); }

/* [emoji, group, name]; skin[0] = 1 when it takes skin tones. */
FN(jobjectArray, emojiAt)(JNIEnv* env, jobject, jlong db, jint i, jintArray skin) {
    const char *e = nullptr, *g = nullptr, *n = nullptr;
    int s = 0;
    if (!xk_emoji_at(as<XKEmojiDB>(db), i, &e, &g, &n, &s)) return nullptr;
    if (skin && env->GetArrayLength(skin) >= 1) env->SetIntArrayRegion(skin, 0, 1, &s);
    return byte_arrays(env, {e, g, n});
}

FN(jobjectArray, emojiSearch)(JNIEnv* env, jobject, jlong db, jbyteArray query, jint max) {
    const std::string q = text_of(env, query);
    std::vector<char*> chars(static_cast<size_t>(max > 0 ? max : 0)), names(chars.size());
    const int n = max > 0 ? xk_emoji_search(as<XKEmojiDB>(db), q.c_str(), chars.data(), names.data(), max) : 0;
    std::vector<std::string> out;
    for (int i = 0; i < n; ++i) {
        out.emplace_back(chars[static_cast<size_t>(i)]);
        std::free(chars[static_cast<size_t>(i)]);
        std::free(names[static_cast<size_t>(i)]);
    }
    return byte_arrays(env, out);
}

FN(jobjectArray, emojiSuggest)(JNIEnv* env, jobject, jlong db, jobjectArray words, jint max) {
    const auto w = strings(env, words);
    const auto p = pointers(w);
    std::vector<char*> got(static_cast<size_t>(max > 0 ? max : 0));
    const int n = max > 0 ? xk_emoji_suggest(as<XKEmojiDB>(db), p.data(), static_cast<int>(p.size()), got.data(), max) : 0;
    std::vector<std::string> out;
    for (int i = 0; i < n; ++i) {
        out.emplace_back(got[static_cast<size_t>(i)]);
        std::free(got[static_cast<size_t>(i)]);
    }
    return byte_arrays(env, out);
}

// ---- personal words (core/learn.cpp) -----------------------------------------------------------

FN(jlong, userNew)(JNIEnv*, jobject) { return handle(xk_user_new()); }

FN(void, userFree)(JNIEnv*, jobject, jlong u) { xk_user_free(as<XKUser>(u)); }

FN(jint, userLoad)(JNIEnv* env, jobject, jlong u, jbyteArray path) {
    return xk_user_load(as<XKUser>(u), text_of(env, path).c_str());
}

FN(jint, userSave)(JNIEnv* env, jobject, jlong u, jbyteArray path) {
    return xk_user_save(as<XKUser>(u), text_of(env, path).c_str());
}

FN(void, userLearn)(JNIEnv* env, jobject, jlong u, jbyteArray prev, jbyteArray word) {
    const std::string p = prev ? text_of(env, prev) : std::string();
    xk_user_learn(as<XKUser>(u), prev ? p.c_str() : nullptr, text_of(env, word).c_str());
}

FN(jdouble, userCount)(JNIEnv* env, jobject, jlong u, jbyteArray prev, jbyteArray word) {
    const std::string p = prev ? text_of(env, prev) : std::string();
    return xk_user_count(as<XKUser>(u), prev ? p.c_str() : nullptr, text_of(env, word).c_str());
}

FN(void, userForget)(JNIEnv* env, jobject, jlong u, jbyteArray word) {
    xk_user_forget(as<XKUser>(u), text_of(env, word).c_str());
}

FN(void, userDecay)(JNIEnv*, jobject, jlong u, jdouble factor) { xk_user_decay(as<XKUser>(u), factor); }

FN(void, modelsSetUser)(JNIEnv*, jobject, jlong m, jlong u) { xk_models_set_user(as<XKModels>(m), as<XKUser>(u)); }

// ---- shortcuts (core/shortcuts.cpp) ------------------------------------------------------------

FN(jlong, shortcutsNew)(JNIEnv*, jobject) { return handle(xk_shortcuts_new()); }

FN(void, shortcutsFree)(JNIEnv*, jobject, jlong sc) { xk_shortcuts_free(as<XKShortcuts>(sc)); }

FN(jint, shortcutsLoad)(JNIEnv* env, jobject, jlong sc, jbyteArray path) {
    return xk_shortcuts_load(as<XKShortcuts>(sc), text_of(env, path).c_str());
}

FN(jint, shortcutsSave)(JNIEnv* env, jobject, jlong sc, jbyteArray path) {
    return xk_shortcuts_save(as<XKShortcuts>(sc), text_of(env, path).c_str());
}

/* counts: added, same, changed, skipped. */
FN(jint, shortcutsImport)(JNIEnv* env, jobject, jlong sc, jbyteArray text, jboolean apply, jintArray counts) {
    XKShortcutCounts c{};
    const int n = xk_shortcuts_import(as<XKShortcuts>(sc), text_of(env, text).c_str(), apply ? 1 : 0, &c);
    const jint out[4] = {c.added, c.same, c.changed, c.skipped};
    if (counts && env->GetArrayLength(counts) >= 4) env->SetIntArrayRegion(counts, 0, 4, out);
    return n;
}

FN(jboolean, shortcutsSet)(JNIEnv* env, jobject, jlong sc, jbyteArray abbr, jbyteArray expansion) {
    return xk_shortcuts_set(as<XKShortcuts>(sc), text_of(env, abbr).c_str(), text_of(env, expansion).c_str()) ? JNI_TRUE : JNI_FALSE;
}

FN(void, shortcutsRemove)(JNIEnv* env, jobject, jlong sc, jbyteArray abbr) {
    xk_shortcuts_remove(as<XKShortcuts>(sc), text_of(env, abbr).c_str());
}

/* Every entry, as abbr, expansion, abbr, expansion... */
FN(jobjectArray, shortcutsAll)(JNIEnv* env, jobject, jlong sc) {
    std::vector<std::string> out;
    const char *a = nullptr, *e = nullptr;
    for (int i = 0; xk_shortcuts_at(as<XKShortcuts>(sc), i, &a, &e); ++i) {
        out.emplace_back(a);
        out.emplace_back(e);
    }
    return byte_arrays(env, out);
}

FN(jbyteArray, shortcutsExpand)(JNIEnv* env, jobject, jlong sc, jbyteArray typed) {
    char* e = xk_shortcuts_expand(as<XKShortcuts>(sc), text_of(env, typed).c_str(), 1);
    if (!e) return nullptr;
    jbyteArray out = bytes(env, e);
    std::free(e);
    return out;
}
