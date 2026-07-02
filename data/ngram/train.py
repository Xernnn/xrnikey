#!/usr/bin/env python3
"""Train trigram language model from Vietnamese corpus."""

import argparse
import math
from collections import Counter, defaultdict
from pathlib import Path

def read_corpus(path):
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                yield line.split()

def train(corpus_path, output_path, max_ngrams=10000):
    unigrams = Counter()
    bigrams = Counter()
    trigrams = Counter()

    for tokens in read_corpus(corpus_path):
        for t in tokens:
            unigrams[t] += 1
        for i in range(len(tokens) - 1):
            bigrams[(tokens[i], tokens[i+1])] += 1
        for i in range(len(tokens) - 2):
            trigrams[(tokens[i], tokens[i+1], tokens[i+2])] += 1

    total_unigrams = sum(unigrams.values())
    total_bigrams = sum(bigrams.values())
    total_trigrams = sum(trigrams.values())

    with open(output_path, "w", encoding="utf-8") as f:
        f.write("\\data\\\n")
        f.write(f"ngram 1: type=unigrams\tcount={len(unigrams)}\n")
        f.write(f"ngram 2: type=bigrams\tcount={len(bigrams)}\n")
        f.write(f"ngram 3: type=trigrams\tcount={len(trigrams)}\n\n")

        f.write("\\1-grams:\n")
        for word, count in unigrams.most_common(max_ngrams):
            log_prob = math.log10(count / total_unigrams)
            f.write(f"{log_prob:.4f}\t{word}\n")
        f.write("\n")

        f.write("\\2-grams:\n")
        for (w1, w2), count in bigrams.most_common(max_ngrams):
            log_prob = math.log10(count / total_bigrams)
            f.write(f"{log_prob:.4f}\t{w1} {w2}\n")
        f.write("\n")

        f.write("\\3-grams:\n")
        for (w1, w2, w3), count in trigrams.most_common(max_ngrams):
            log_prob = math.log10(count / total_trigrams)
            f.write(f"{log_prob:.4f}\t{w1} {w2} {w3}\n")
        f.write("\n")

        f.write("\\end\\\n")

    print(f"Wrote ARPA model to {output_path}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("corpus", help="Plain text corpus file")
    parser.add_argument("-o", "--output", default="vi.arpa")
    parser.add_argument("--max", type=int, default=10000)
    args = parser.parse_args()
    train(args.corpus, args.output, args.max)

if __name__ == "__main__":
    main()
