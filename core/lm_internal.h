/* The language model reader's C++ side, for the core's own modules
 * (accent restoration, the English/Vietnamese choice). The C API is in
 * xrnikey.h. */
#ifndef XRNIKEY_LM_INTERNAL_H
#define XRNIKEY_LM_INTERNAL_H

#include "lm_format.h"
#include "xrnikey.h"

#include <cstdint>
#include <mutex>
#include <string_view>

namespace xrnikey {

/* Syllables by ASCII shape, for accent restoration (restore.cpp). */
struct ShapeIndex;
void shape_index_free(ShapeIndex* s);

struct Lm {
    void* map = nullptr;      /* the whole mapping, for munmap */
    uint64_t map_size = 0;
    const char* base = nullptr;  /* the model's first byte */
    uint64_t size = 0;
    const lmfmt::Header* hdr = nullptr;
    /* Built on first use, so opening a model stays cheap. */
    mutable std::once_flag shapes_once;
    mutable ShapeIndex* shapes = nullptr;
};

/* Map `length` bytes of `fd` from `offset` and check them; nullptr if they
 * are not a model this reader understands. The descriptor may be closed
 * afterwards. */
Lm* lm_map(int fd, uint64_t offset, uint64_t length);
void lm_unmap(Lm* m);

/* The word with this id (nullptr past the vocabulary), and the id of a
 * word (lmfmt::kUnk when the model doesn't know it). */
const char* lm_word(const Lm& m, uint32_t id);
uint32_t lm_id(const Lm& m, std::string_view word);

/* Index of the n-gram ids[0..k-1] in order k, or -1. */
int64_t lm_find(const Lm& m, const uint32_t* ids, int k);
float lm_prob_at(const Lm& m, int k, uint64_t idx);
float lm_bo_at(const Lm& m, int k, uint64_t idx);

/* log10 P(w | ctx[0..n-1]), the context oldest first, with backoff. For
 * w = <unk> this is the mass of every unknown word together. */
float lm_logprob(const Lm& m, const uint32_t* ctx, int n, uint32_t w);

/* What to subtract from <unk>'s log10 probability for one particular
 * unknown word: the unknown mass shared among the word types that training
 * folded into <unk>. Without it a word the model never saw would outrank
 * every rare word it did see. */
float lm_unk_share(const Lm& m);

inline const Lm& lm_of(const XKLm* lm) { return *reinterpret_cast<const Lm*>(lm); }

}  // namespace xrnikey

#endif
