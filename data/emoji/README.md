# Emoji index

`emoji-index.tsv` lists 1,914 emoji in Unicode's order, one per line: the emoji, its group,
whether it takes skin tones, and its English and Vietnamese names and keywords. Skin-tone
variants are folded into their base. The core reads it (`xk_emoji_*`), for the emoji panel,
emoji search, and the emoji suggested for the words just typed.

`generate.py` builds it from the files pinned in `sources.json`:

```bash
python3 -I data/emoji/generate.py fetch ~/.cache/xrnkey/downloads
python3 -I data/emoji/generate.py ~/.cache/xrnkey/downloads
```

## Sources and licence

- Unicode Emoji 17.0, `emoji-test.txt`: the emoji, their order and groups.
- Unicode CLDR 48, `annotations` and `annotationsDerived` for `en` and `vi`, from the
  `unicode-org/cldr-json` repository: names (`tts`) and keywords.

Both are Unicode data, under the Unicode License v3:

> UNICODE LICENSE V3 — COPYRIGHT AND PERMISSION NOTICE
>
> Copyright © 1991-2025 Unicode, Inc.
>
> NOTICE TO USER: Carefully read the following legal agreement. BY DOWNLOADING, INSTALLING,
> COPYING OR OTHERWISE USING DATA FILES, AND/OR SOFTWARE, YOU UNEQUIVOCALLY ACCEPT, AND AGREE
> TO BE BOUND BY, ALL OF THE TERMS AND CONDITIONS OF THIS AGREEMENT. IF YOU DO NOT AGREE, DO
> NOT DOWNLOAD, INSTALL, COPY, DISTRIBUTE OR USE THE DATA FILES OR SOFTWARE.
>
> Permission is hereby granted, free of charge, to any person obtaining a copy of data files
> and any associated documentation (the "Data Files") or software and any associated
> documentation (the "Software") to deal in the Data Files or Software without restriction,
> including without limitation the rights to use, copy, modify, merge, publish, distribute,
> and/or sell copies of the Data Files or Software, and to permit persons to whom the Data Files
> or Software are furnished to do so, provided that either (a) this copyright and permission
> notice appear with all copies of the Data Files or Software, or (b) this copyright and
> permission notice appear in associated Documentation.
>
> THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
> PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
>
> IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY
> CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
> FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
> TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE DATA
> FILES OR SOFTWARE.
>
> Except as contained in this notice, the name of a copyright holder shall not be used in
> advertising or otherwise to promote the sale, use or other dealings in these Data Files or
> Software without prior written authorization of the copyright holder.
