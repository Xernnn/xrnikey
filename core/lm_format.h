/* The on-disk layout of an xrnikey language model (.xklm), shared by the
 * reader (lm.cpp) and the builder (tools/lm/xk-lm-build.cpp).
 *
 * A backoff n-gram model, as in an ARPA file, laid out as a forward trie so
 * it can be memory-mapped and read in place:
 *
 *   - Words are ids 0..vocab-1: <unk>, <s>, </s>, then the rest by frequency.
 *     Ids fit in 16 bits.
 *   - Order 1 is indexed by id: float log10 probability, float log10 backoff,
 *     and the start of the word's children in order 2.
 *   - Order k (2..order) is sorted by (w1, ..., wk), so the children of an
 *     order k-1 entry are one contiguous range, searched by word id. Each
 *     entry has a word id, an 8-bit quantised probability and backoff (see
 *     the per-order codebooks), and, below the top order, the start of its
 *     children in order k+1. Child arrays have one extra sentinel entry.
 *
 * Everything is little-endian and every section starts 8-byte aligned.
 * Offsets are from the start of the model (which need not be the start of
 * the file: a model can live inside an APK). */
#ifndef XRNIKEY_LM_FORMAT_H
#define XRNIKEY_LM_FORMAT_H

#include <cstdint>

namespace xrnikey::lmfmt {

constexpr char kMagic[8] = {'X', 'K', 'L', 'M', '0', '0', '0', '1'};
constexpr uint32_t kMaxOrder = 5;
constexpr uint32_t kMaxVocab = 65536;
constexpr uint32_t kUnk = 0;
constexpr uint32_t kBos = 1;
constexpr uint32_t kEos = 2;
/* log10 probability of a word the model never predicts (<s>). */
constexpr float kNever = -99.0f;

struct OrderSection {
    uint64_t prob_code;  /* float[256] */
    uint64_t bo_code;    /* float[256], 0 at the top order */
    uint64_t word;       /* uint16[count] */
    uint64_t prob;       /* uint8[count] */
    uint64_t bo;         /* uint8[count], 0 at the top order */
    uint64_t child;      /* uint32[count + 1], 0 at the top order */
};

struct Header {
    char magic[8];
    uint32_t order;
    uint32_t vocab;
    uint64_t count[kMaxOrder + 1];  /* entries per order; count[1] == vocab */
    uint64_t total_size;            /* bytes of the whole model */
    uint64_t vocab_blob;            /* NUL-terminated strings, in id order */
    uint64_t vocab_blob_size;
    uint64_t vocab_index;           /* uint32[vocab]: blob offset of each id */
    uint64_t vocab_sorted;          /* uint32[vocab]: ids in byte order of strings */
    uint64_t uni_prob;              /* float[vocab] */
    uint64_t uni_bo;                /* float[vocab] */
    uint64_t uni_child;             /* uint32[vocab + 1], 0 when order == 1 */
    OrderSection sec[kMaxOrder + 1];  /* sec[2..order] */
};

}  // namespace xrnikey::lmfmt

#endif
