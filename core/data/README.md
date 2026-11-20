# Word lists for auto-restore

Both lists are compiled into libxrnikey (see `core/CMakeLists.txt`): one entry
per line, UTF-8, lines starting with `#` are comments. Auto-restore uses them
only when a word ends: if the keys typed spell a common English word and the
Vietnamese result is not an attested syllable, the keys are committed as
typed ("terms" stays "terms" rather than "tém"). An attested syllable always
wins, so typing Vietnamese is never turned into English ("of" stays "ò").

## english.txt

Common English words, lowercase, letters a–z only.

- SCOWL 2020.12.07 by Kevin Atkinson (<http://wordlist.aspell.net/>): the
  lists `english-words.10`, `english-words.20`, `american-words.10` and
  `american-words.20` (the most common words; levels 10 and 20 are built
  from public-domain sources: Moby Words II, Brian Kelk's UK English
  Wordlist with Frequency Classification, and 12dicts).
- The computing words this project used to keep in `bilingual_switch.cpp`.

SCOWL's licence asks for this notice:

> Copyright 2000-2018 by Kevin Atkinson
>
> Permission to use, copy, modify, distribute and sell these word lists, the
> associated scripts, the output created from the scripts, and its
> documentation for any purpose is hereby granted without fee, provided that
> the above copyright notice appears in all copies and that both that
> copyright notice and this permission notice appear in supporting
> documentation. Kevin Atkinson makes no representations about the
> suitability of this array for any purpose. It is provided "as is" without
> express or implied warranty.

## vi-syllables.txt

Vietnamese syllables seen in real use, lowercase, NFC, either tone style.
A syllable is listed only if `xk_is_valid_syllable` accepts it.

- Unicode Unihan database 18.0 (`Unihan_Readings.txt`, field `kVietnamese`:
  the Sino-Vietnamese and Nôm readings of Han characters), under the Unicode
  License v3, below.
- Mozilla Common Voice Vietnamese sentences
  (`server/data/vi/sentence-collector.txt`), released as CC0.
- Public-domain texts from vi.wikisource.org: Trần Trọng Kim, *Việt Nam sử
  lược*; Nguyễn Du, *Truyện Kiều* (1870 edition); Nguyễn Đình Chiểu, *Lục
  Vân Tiên*.
- Vietnamese sentences from Tatoeba (<https://tatoeba.org>), the export
  `vie_sentences.tsv` of 2026-11-20, written by Tatoeba's contributors and
  licensed under CC BY 2.0 FR
  (<https://creativecommons.org/licenses/by/2.0/fr/>). Only the syllables
  are kept, not the sentences, and not those of hyphenated words, which
  spell out foreign words ("pót-cát"). They bring everyday modern words
  (lít, cúp, bốt, típ …).
- `data/dict/seed_wordlist.txt` from this project.
- At the end, a short hand-written list, part of this project: interjections
  and spoken forms that written sources miss (ò, í, ừm …), and everyday
  words and loanwords they miss whose Telex keys spell English words, so
  that without them the English word won (cỏn, háng, líp, phốt, rún, sít,
  tủn, vế: "corn", "hangs", "lips", "photos", "runs", "sits", "turn",
  "verse"). More text was tried for these instead: novels of 1912–1950 on
  vi.wikisource (Hồ Biểu Chánh, Nam Cao, Vũ Trọng Phụng, Ngô Tất Tố …) have
  most of them, but also so many regional spellings and typesetting slips
  ("dụ dổ", "sum sê", "L ửa") that "her", "or", "see", "here" and "door"
  became Vietnamese, so they are not used.

> UNICODE LICENSE V3 — COPYRIGHT AND PERMISSION NOTICE
>
> Copyright © 1991-2026 Unicode, Inc.
>
> Permission is hereby granted, free of charge, to any person obtaining a
> copy of data files and any associated documentation (the "Data Files") or
> software and any associated documentation (the "Software") to deal in the
> Data Files or Software without restriction, including without limitation
> the rights to use, copy, modify, merge, publish, distribute, and/or sell
> copies of the Data Files or Software, and to permit persons to whom the
> Data Files or Software are furnished to do so, provided that either (a)
> this copyright and permission notice appear with all copies of the Data
> Files or Software, or (b) this copyright and permission notice appear in
> associated Documentation.
>
> THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
> KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
> MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
> THIRD PARTY RIGHTS.
>
> IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE
> BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES,
> OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
> WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
> ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE DATA
> FILES OR SOFTWARE.
>
> Except as contained in this notice, the name of a copyright holder shall
> not be used in advertising or otherwise to promote the sale, use or other
> dealings in these Data Files or Software without prior written
> authorization of the copyright holder.

## Rebuilding

`tools/fetch_wikisource.py OUTDIR "Việt Nam sử lược" "Truyện Kiều" "Lục Vân Tiên"` fetches
the Wikisource texts, and Tatoeba's export is
<https://downloads.tatoeba.org/exports/per_language/vie/vie_sentences.tsv.bz2> (unpacked with
`bunzip2`). `tools/make_lists.py` then writes both lists from SCOWL's `final/` folder, the old
hand list, Unihan's `Unihan_Readings.txt`, the Common Voice sentences, the Wikisource text, the
seed word list and the Tatoeba sentences, keeping only syllables that a built `libxrnikey.so`
accepts (its usage line gives the argument order).
