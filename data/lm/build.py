#!/usr/bin/env python3
"""Build the language models from the sources in sources.json.

  build.py fetch DOWNLOADS
      Download every source into its own new folder under DOWNLOADS and check
      its SHA-256 against sources.json.
  build.py prepare DOWNLOADS WORK --lib LIBXRNIKEY [--jobs N]
      Tokenize every source (xklm_text.py), streaming: archives are read in
      place, never unpacked. Writes WORK/vi.train.txt and WORK/en.train.txt
      (one sentence of tokens per line), the held-out evaluation sets
      WORK/H1.txt (news and web) and WORK/H2.txt (short sentences), the rest
      of the held-out text as WORK/dev.txt for tuning, and WORK/MANIFEST.json.
  build.py train WORK OUT --tool XK_LM_BUILD
      Train OUT/vi.xklm (order 4, for the laptop), OUT/vi-phone.xklm (pruned
      for the phone) and OUT/en.xklm (order 3) with xk-lm-build.
  build.py tokenize --lib LIBXRNIKEY OUT IN...
      Tokenize plain-text files into one (the made-up test corpora in data/lm/test).

A sentence is held out when a hash of its tokens falls in the first 4 % of
the range, whatever its source, so a sentence that two sources share is held
out from both. H1 is the 2,000 held-out news and web sentences with the
smallest hashes, H2 the 1,000 such Tatoeba and Common Voice sentences; only
sentences of 4 to 40 tokens with no "<unk>" are picked.

Run it with python3 -I: it reads downloaded files.
"""

import argparse
import bz2
import hashlib
import json
import os
import subprocess
import sys
import tarfile
import urllib.request
from concurrent.futures import ProcessPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xklm_text  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
HELDOUT_PER_MILLE = 40
H1_SIZE, H2_SIZE, DEV_SIZE = 2000, 1000, 3000


def load_sources():
    with open(os.path.join(HERE, "sources.json"), encoding="utf-8") as f:
        return json.load(f)["sources"]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def download_path(downloads, src):
    return os.path.join(downloads, src["name"], os.path.basename(src["url"]))


def cmd_fetch(args):
    for src in load_sources():
        path = download_path(args.downloads, src)
        if not os.path.exists(path):
            os.makedirs(os.path.dirname(path), exist_ok=True)
            print("fetch", src["url"], flush=True)
            part = path + ".part"
            with urllib.request.urlopen(src["url"], timeout=60) as r, open(part, "wb") as out:
                while chunk := r.read(1 << 20):
                    out.write(chunk)
            os.replace(part, path)
        got = sha256(path)
        if got != src["sha256"]:
            sys.exit(f"{path}: sha256 {got}, sources.json says {src['sha256']}")
        print("ok", src["name"])


def raw_lines(downloads, src):
    """The sentences of one source, as text."""
    path = download_path(downloads, src)
    if src["format"] == "leipzig":
        with tarfile.open(path, "r:gz") as tar:
            for member in tar:
                if member.isfile() and member.name.endswith("-sentences.txt"):
                    f = tar.extractfile(member)
                    for raw in f:
                        line = raw.decode("utf-8", "replace").rstrip("\n")
                        yield line.split("\t", 1)[-1]
                    return
        sys.exit(f"{path}: no -sentences.txt inside")
    elif src["format"] == "tatoeba":
        with bz2.open(path, "rt", encoding="utf-8", errors="replace") as f:
            for line in f:
                yield line.rstrip("\n").split("\t", 2)[-1]
    elif src["format"] == "lines":
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                yield line.rstrip("\n")
    else:
        sys.exit(f"unknown format {src['format']}")


def bucket(tokens):
    """(per-mille bucket, sort key) of a sentence, from its tokens alone."""
    digest = hashlib.blake2b(" ".join(tokens).encode(), digest_size=8).digest()
    value = int.from_bytes(digest, "big")
    return value % 1000, digest.hex()


def prepare_one(job):
    downloads, work, lib, src = job
    engine = xklm_text.Engine(lib)
    train_path = os.path.join(work, src["name"] + ".train.txt")
    held_path = os.path.join(work, src["name"] + ".heldout.tsv")
    n_sent = n_tok = n_held = 0
    with open(train_path + ".part", "w", encoding="utf-8") as train, \
         open(held_path + ".part", "w", encoding="utf-8") as held:
        for text in raw_lines(downloads, src):
            for toks in xklm_text.sentences(engine, text):
                b, key = bucket(toks)
                if b < HELDOUT_PER_MILLE:
                    held.write(f"{key}\t{' '.join(toks)}\n")
                    n_held += 1
                else:
                    train.write(" ".join(toks) + "\n")
                    n_sent += 1
                    n_tok += len(toks)
    os.replace(train_path + ".part", train_path)
    os.replace(held_path + ".part", held_path)
    return src["name"], n_sent, n_tok, n_held


def eligible(tokens):
    return 4 <= len(tokens) <= 40 and "<unk>" not in tokens


def cmd_prepare(args):
    os.makedirs(args.work, exist_ok=True)
    sources = load_sources()
    for src in sources:
        if sha256(download_path(args.downloads, src)) != src["sha256"]:
            sys.exit(f"{src['name']}: not the file sources.json pins (run fetch)")
    jobs = [(args.downloads, args.work, os.path.abspath(args.lib), s) for s in sources]
    stats = {}
    with ProcessPoolExecutor(max_workers=args.jobs) as pool:
        for name, n_sent, n_tok, n_held in pool.map(prepare_one, jobs):
            stats[name] = {"train_sentences": n_sent, "train_tokens": n_tok, "heldout": n_held}
            print(name, stats[name], flush=True)
    for lang in ("vi", "en"):
        out = os.path.join(args.work, f"{lang}.train.txt")
        with open(out + ".part", "w", encoding="utf-8") as dst:
            for src in sources:
                if src["lang"] != lang:
                    continue
                with open(os.path.join(args.work, src["name"] + ".train.txt"), encoding="utf-8") as f:
                    for line in f:
                        dst.write(line)
        os.replace(out + ".part", out)
    pools = {"H1": [], "H2": []}
    for src in sources:
        if not src["heldout"]:
            continue
        with open(os.path.join(args.work, src["name"] + ".heldout.tsv"), encoding="utf-8") as f:
            for line in f:
                key, toks = line.rstrip("\n").split("\t", 1)
                pools[src["heldout"]].append((key, toks))
    picked = {}
    dev = []
    for name, size in (("H1", H1_SIZE), ("H2", H2_SIZE)):
        seen, chosen = set(), []
        for key, toks in sorted(pools[name]):
            if toks in seen:
                continue
            seen.add(toks)
            if len(chosen) < size and eligible(toks.split()):
                chosen.append(toks)
            elif eligible(toks.split()):
                dev.append((key, toks))
        picked[name] = chosen
        with open(os.path.join(args.work, f"{name}.txt"), "w", encoding="utf-8") as f:
            f.writelines(t + "\n" for t in chosen)
    dev.sort()
    with open(os.path.join(args.work, "dev.txt"), "w", encoding="utf-8") as f:
        f.writelines(t + "\n" for _, t in dev[:DEV_SIZE])
    manifest = {
        "sources": {s["name"]: {"sha256": s["sha256"], "licence": s["licence"], **stats[s["name"]]}
                    for s in sources},
        "H1": len(picked["H1"]), "H2": len(picked["H2"]), "dev": min(len(dev), DEV_SIZE),
        "heldout_per_mille": HELDOUT_PER_MILLE,
    }
    with open(os.path.join(args.work, "MANIFEST.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
    print(json.dumps({k: v for k, v in manifest.items() if k != "sources"}))


# name, training text, xk-lm-build options. vi-phone is pruned harder to fit
# the phone's 25 MB budget (android/docs/feature-spec.md); with the pinned
# sources it is 22.6 MB against vi's 81 MB, and restores 0.2-0.7 points less
# (data/lm/README.md).
MODELS = [
    ("vi", "vi", ["--order", "4", "--min-count", "3", "--prune", "0,1,2"]),
    ("vi-phone", "vi", ["--order", "4", "--min-count", "3", "--prune", "3,6,6"]),
    ("en", "en", ["--order", "3", "--min-count", "3", "--prune", "1,1"]),
]


def cmd_train(args):
    os.makedirs(args.out, exist_ok=True)
    for name, lang, opts in MODELS:
        corpus = os.path.join(args.work, f"{lang}.train.txt")
        out = os.path.join(args.out, f"{name}.xklm")
        subprocess.run([args.tool, *opts, "-o", out, corpus], check=True)
        print(name, out, os.path.getsize(out), sha256(out))


def cmd_tokenize(args):
    engine = xklm_text.Engine(os.path.abspath(args.lib))
    with open(args.output + ".part", "w", encoding="utf-8") as out:
        for path in args.inputs:
            with open(path, encoding="utf-8") as f:
                for line in f:
                    if line.startswith("#"):
                        continue
                    for toks in xklm_text.sentences(engine, line):
                        out.write(" ".join(toks) + "\n")
    os.replace(args.output + ".part", args.output)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("fetch")
    p.add_argument("downloads")
    p.set_defaults(func=cmd_fetch)
    p = sub.add_parser("prepare")
    p.add_argument("downloads")
    p.add_argument("work")
    p.add_argument("--lib", required=True)
    p.add_argument("--jobs", type=int, default=4)
    p.set_defaults(func=cmd_prepare)
    p = sub.add_parser("train")
    p.add_argument("work")
    p.add_argument("out")
    p.add_argument("--tool", required=True)
    p.set_defaults(func=cmd_train)
    p = sub.add_parser("tokenize")
    p.add_argument("output")
    p.add_argument("inputs", nargs="+")
    p.add_argument("--lib", required=True)
    p.set_defaults(func=cmd_tokenize)
    args = ap.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
