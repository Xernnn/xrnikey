#!/usr/bin/env bash
# build_dict.sh — compile seed wordlist to binary double-array trie
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/../../build/data/dict}"
SEED="${SCRIPT_DIR}/seed_wordlist.txt"
OUT="${BUILD_DIR}/vi.dict"

mkdir -p "$BUILD_DIR"

if [ ! -f "$SEED" ]; then
    echo "Error: seed wordlist not found at $SEED" >&2
    exit 1
fi

# Validate UTF-8
if ! iconv -f UTF-8 -t UTF-8 "$SEED" >/dev/null 2>&1; then
    echo "Error: seed wordlist is not valid UTF-8" >&2
    exit 1
fi

echo "Building dictionary from $SEED..."

# Use Python builder for cross-platform trie compilation
python3 - "$SEED" "$OUT" <<'PYEOF'
import sys
import struct
from collections import deque

seed_path, out_path = sys.argv[1], sys.argv[2]

words = []
with open(seed_path, encoding="utf-8") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        word = parts[0]
        freq = int(parts[2]) if len(parts) > 2 else 100
        words.append((word, freq))

words.sort(key=lambda x: x[0])

## Plain trie first, then lay it into the double array in one BFS pass.
##
## Every word gets a trailing 0x00 edge to a leaf, and the frequency lives on
## that leaf as a negative base. Terminal-ness CANNOT be marked on the node
## the word itself ends at: base[] on an interior node is its children offset,
## so overwriting it with -freq orphans every child. 88 of the seed words are
## proper prefixes of another word ("ba" / "bàn", "ca" / "cam"), and marking
## those in place silently truncated the trie to ~6 reachable words.
children = [{}]          # plain node -> {byte: plain node}
leaf_freq = {}           # plain leaf node -> frequency


def insert(word, freq):
    cur = 0
    for b in word.encode("utf-8") + b"\x00":
        nxt = children[cur].get(b)
        if nxt is None:
            children.append({})
            nxt = len(children) - 1
            children[cur][b] = nxt
        cur = nxt
    leaf_freq[cur] = freq


for word, freq in words:
    insert(word, freq)

size = 4096
base = [0] * size
check = [-1] * size


def ensure(idx):
    global size, base, check
    while idx >= size:
        base.extend([0] * size)
        check.extend([-1] * size)
        size *= 2


ROOT = 1
check[ROOT] = 0
da = {0: ROOT}
search_from = 2

## A node's base must be chosen so that base+c is free for EVERY child byte c
## at once -- picking a slot per child and rewriting base as we go (the old
## approach) invalidates the siblings already placed under the previous base.
queue = deque([0])
while queue:
    pn = queue.popleft()
    s = da[pn]
    kids = children[pn]
    if not kids:
        base[s] = -leaf_freq[pn]
        continue
    cs = sorted(kids)
    b = search_from
    while True:
        ensure(b + cs[-1])
        if all(check[b + c] == -1 for c in cs):
            break
        b += 1
    base[s] = b
    for c in cs:
        ci = b + c
        check[ci] = s
        da[kids[c]] = ci
        queue.append(kids[c])
    while search_from < size and check[search_from] != -1:
        search_from += 1

num_nodes = max(i for i in range(size) if check[i] != -1) + 1

with open(out_path, "wb") as out:
    out.write(struct.pack("<i", num_nodes))
    out.write(struct.pack("<%di" % num_nodes, *base[:num_nodes]))
    out.write(struct.pack("<%di" % num_nodes, *check[:num_nodes]))

print(f"Compiled {len(words)} words -> {out_path} ({num_nodes} nodes)")
PYEOF

# Copy abbreviations and en-vi stubs
cp "$OUT" "${BUILD_DIR}/en-vi.dict" 2>/dev/null || true
cp "$OUT" "${BUILD_DIR}/abbrev.dict" 2>/dev/null || true

echo "Dictionary build complete."
