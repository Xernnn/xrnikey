# Language models

The engine ranks accented readings and decides between English and Vietnamese with two backoff
n-gram models:

- `vi.xklm`: Vietnamese, order 4, over syllables. English words seen in Vietnamese text are
  ordinary tokens.
- `vi-phone.xklm`: the same, pruned harder for the phone's 25 MB budget.
- `en.xklm`: English, order 3, over words.

Built from the pinned sources, measured with `tools/xk-eval` (restoration top-1 with look-back):

| Model | Size | H1 | H2 | C |
|---|---|---|---|---|
| `vi.xklm` (prune 0,1,2) | 81 MB | 97.80 % | 96.29 % | 95.85 % |
| prune 1,3,3 | 40 MB | 97.56 % | 95.93 % | 95.80 % |
| prune 2,4,4 | 31 MB | 97.44 % | 95.80 % | 95.45 % |
| `vi-phone.xklm` (prune 3,6,6) | 22.6 MB | 97.26 % | 95.59 % | 95.22 % |

`en.xklm` is 6.5 MB.

Both are `.xklm` files (`core/lm_format.h`), memory-mapped and read in place by `core/lm.cpp`.
`tools/lm/xk-lm-build` trains them with interpolated modified Kneser–Ney smoothing, then prunes
rare n-grams and renormalises the backoff weights.

## What is in git, and what is not

- **In git**: the scripts, `sources.json` (every source's URL, SHA-256 and licence), the made-up
  test corpora in `test/`, and the made-up evaluation set `eval/chat.txt`.
- **Not in git**: the downloaded corpora, the tokenized text, the held-out sets H1 and H2 (cut
  from the corpora by `build.py`), and the trained models. The models are tens of megabytes and
  are rebuilt from the pinned sources.

The small models that the tests and CI use are trained at build time from `test/vi.txt` and
`test/en.txt` (`data/CMakeLists.txt`, with `-DXRNKEY_ENABLE_TESTS=ON`). They say nothing about
quality.

## Building the real models

The tokenizer needs a built `libxrnkey`, because it asks the engine what a syllable is and where
its tone goes.

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
python3 -I data/lm/build.py fetch ~/.cache/xrnkey-lm/downloads          # about 800 MB
python3 -I data/lm/build.py prepare ~/.cache/xrnkey-lm/downloads ~/.cache/xrnkey-lm/work \
    --lib build/core/libxrnkey.so --jobs 4
python3 -I data/lm/build.py train ~/.cache/xrnkey-lm/work ~/.cache/xrnkey-lm/models \
    --tool build/tools/xk-lm-build
```

- **Downloads**: each source goes into its own folder, and `fetch` refuses any file whose SHA-256
  differs from `sources.json`.
- **Reading the archives**: `prepare` streams the sentence files straight out of them, without
  unpacking anything. It takes about two minutes with four jobs.
- **Held-out text**: a sentence is held out when a hash of its tokens falls in the first 4 % of
  the range, so a sentence that two sources share is held out from both. H1 and H2 are picked
  from that; the rest becomes `dev.txt`, for tuning.
- **Training**: `train` takes a few minutes and a few gigabytes of memory, mostly for counting
  the Vietnamese 4-grams.

To install the trained models with the Fcitx5 input method, configure with
`-DXRNKEY_LM_DIR=<folder with vi.xklm and en.xklm>`.

Then measure them:

```bash
python3 -I tools/xk-eval --lib build/core/libxrnkey.so --vi ~/.cache/xrnkey-lm/models/vi.xklm \
    --en ~/.cache/xrnkey-lm/models/en.xklm --work ~/.cache/xrnkey-lm/work
```

## Sources and licences

The models are derived from the sources below, and are distributed under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) with this attribution. The code that
builds them is MIT, like the rest of the repo.

- **Leipzig Corpora Collection**, Universität Leipzig, the corpora `vie_news_2022_1M`,
  `vie_news_2020_1M`, `vie-vn_web_2015_1M` and `eng_news_2023_300K`
  (<https://wortschatz.uni-leipzig.de/en/download>). The project's terms of usage
  (<https://wortschatz.uni-leipzig.de/en/usage>) offer its downloadable text corpora under CC BY.
  Cite: D. Goldhahn, T. Eckart, U. Quasthoff (2012), *Building Large Monolingual Dictionaries at
  the Leipzig Corpora Collection: From 100 to 200 Languages*, LREC 2012.
- **Tatoeba**, Vietnamese sentences (`vie_sentences.tsv`), written by Tatoeba's contributors,
  [CC BY 2.0 FR](https://creativecommons.org/licenses/by/2.0/fr/) (<https://tatoeba.org>).
- **Mozilla Common Voice**, Vietnamese sentence collection (`sentence-collector.txt`),
  [CC0](https://creativecommons.org/publicdomain/zero/1.0/).

Not used:

- Wikipedia, whose CC BY-SA would carry over to the models.
- Anything licensed for non-commercial use only.
- Anything without a clear licence: subtitles, scraped news dumps, and Common Crawl text.
