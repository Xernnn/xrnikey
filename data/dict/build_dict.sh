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

# Build double-array trie
BASE_SIZE = 65536
base = [0] * BASE_SIZE
check = [-1] * BASE_SIZE
tail_data = bytearray()
node_values = {}

def get_or_create_node():
    for i in range(1, BASE_SIZE):
        if check[i] == -1:
            return i
    raise RuntimeError("Trie full")

root = 1
check[root] = 0

def insert(word, freq):
    s = root
    for ch in word.encode("utf-8"):
        c = ch
        next_idx = base[s] + c
        if next_idx >= BASE_SIZE:
            base[s] = max(0, s)
            next_idx = base[s] + c
        if next_idx < BASE_SIZE and (check[next_idx] == s or check[next_idx] == -1):
            if check[next_idx] == -1:
                check[next_idx] = s
            s = next_idx
        else:
            new_s = get_or_create_node()
            base[s] = new_s - c
            check[new_s] = s
            s = new_s

    node_values[s] = freq

for word, freq in words:
    insert(word, freq)

# Mark terminal nodes with negative base
for node, freq in node_values.items():
    base[node] = -freq

num_nodes = max(i for i in range(BASE_SIZE) if check[i] != -1) + 1

with open(out_path, "wb") as out:
    out.write(struct.pack("<i", num_nodes))
    for i in range(num_nodes):
        out.write(struct.pack("<i", base[i]))
    for i in range(num_nodes):
        out.write(struct.pack("<i", check[i]))

print(f"Compiled {len(words)} words -> {out_path} ({num_nodes} nodes)")
PYEOF

# Copy abbreviations and en-vi stubs
cp "$OUT" "${BUILD_DIR}/en-vi.dict" 2>/dev/null || true
cp "$OUT" "${BUILD_DIR}/abbrev.dict" 2>/dev/null || true

echo "Dictionary build complete."
