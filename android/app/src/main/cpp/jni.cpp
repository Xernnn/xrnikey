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

std::string text(JNIEnv* env, jbyteArray a) {
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
        v.push_back(text(env, b));
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
    return handle(xk_lm_open(text(env, path).c_str()));
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
    const std::string nx = text(env, next);
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
    const std::string t = text(env, typed);
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
