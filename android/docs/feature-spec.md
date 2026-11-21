# xrnikey for Android: feature spec

An Android keyboard for one owner's phone (Android 15, HyperOS) that replaces the closed-source
Vietnamese keyboard they have used for years. The owner's verdict on it: it is so good that
nothing open source has matched it. Three things make it worth replacing anyway: it is closed
source, it has internet access, and it syncs the words it learns to an online account. This
keyboard has **no INTERNET permission**, keeps everything it learns on the phone, and shares one
engine with the Fcitx5 input method in this repo, so an improvement reaches both the phone and
the laptop.

The layout and behaviour follow the reference keyboard. The name, icon, artwork and wording are
this project's own, and the reference is named only in the README's parity section. The
reference is described here in words. No screenshots of it are kept in the repo.

What the owner relies on every day, and so what decides whether this is "as good":

- typing without accents and getting them, in context: `khong` → `không`, `duoc` → `được`;
- English and Vietnamese mixed in one message, with no switching;
- auto-correct on space and punctuation, which is "really so good";
- the emoji panel and the clipboard panel;
- the number row, and the layout as a whole.

Swipe typing is not used.

## Terms

- **Key**: a letter, digit or symbol key, or a function key (Shift, Backspace, `?123`, Enter).
- **Word in progress**: the keys typed since the last word boundary, held by the engine. The app
  shows it as Android *composing text*, with no underline.
- **Telex result**: what the engine's Telex rules make of the keys (`dduowcj` → `được`,
  `khong` → `khong`). It is the same code the laptop runs.
- **ASCII shape**: a syllable with every mark and tone removed (`được` → `duoc`). The 5,840
  attested syllables have 1,445 shapes, with about 4 syllables per shape on average and up to 26
  (`do`, `dam`).
- **Accent restoration**: choosing the accented syllable for an ASCII shape typed without marks,
  using the words before it.
- **Look-back**: re-choosing the last one or two restored syllables once the next syllable is
  known (`co gang` first gives `có`, then `cố gắng`).
- **Pass-through**: a word committed exactly as typed, because it is English or an
  abbreviation, not Vietnamese.
- **Strip**: the row above the keys with three suggestion slots.
- **Centre pick**: the strip's middle slot. It is always what Space would commit.
- **Auto-correct**: Space or punctuation committing the centre pick instead of the keys as typed.
- **Revert**: Backspace right after an auto-correct, which puts back what was typed.
- **Tool row**: the strip's other face, a row of buttons: theme, settings, clipboard, cursor pad
  and emoji.
- **Panel**: a full-keyboard-height view in place of the keys: emoji, clipboard, cursor pad or
  the expanded candidate list.
- **Personal words**: words and word pairs learned from what the owner types. They are kept on
  the phone only.
- **Shortcut**: an abbreviation that expands at the end of a word (`ko` → `không`). The same as
  the laptop's macros.
- **Model**: the shipped language data: a Vietnamese syllable n-gram model, an English word
  model, and the lexicons and emoji index.

## Day one (must have)

1. Full Telex through the shared C++ core: `w` = ư, old-style tones (hòa, thúy), marks and tones
   allowed after the final consonant, a key typed twice undoes itself, and Backspace deletes one
   character. VNI and new-style tones are settings, since the core already has them.
2. Accent restoration of words typed without marks, ranked by a syllable n-gram model with the
   words before them as context, plus look-back over the last two syllables.
3. English and Vietnamese mixed with no language switch. Whether the keys mean English or
   Vietnamese is decided in context, which fixes the laptop's 19 English words that now type
   Vietnamese (`list` → `lít`, `turn` → `tủn`).
4. A strip with three slots: the centre pick, and alternatives, two-word phrases and an emoji
   suggestion. Next-word predictions appear after Space. An expanded list holds more.
5. Auto-correct on Space and punctuation at the "flexible" level, Revert on Backspace, and a full
   stop from a double Space.
6. Keys and gestures as the owner set them up: the number row, long-press on the top letter row
   for digits, a key-press popup, a 400 ms long-press, swiping the space bar to move the cursor,
   holding the space bar to move the cursor, and holding the comma for the keyboard picker.
7. Emoji panel: recents, categories, search, skin tones, and an emoji suggestion row.
8. Clipboard panel: history, pin, delete and paste.
9. Personal words learned on the phone, which can be viewed, deleted, exported and imported.
10. Shortcuts, with import and export as a file in the laptop's `macros.txt` format.
11. Settings for each of the above, plus the cursor pad and dark, light or system theme.
12. No INTERNET permission, and no other permission at all.

## Later (agreed, not now)

- Haptics and key sound. The owner has both off.
- Clipboard images, and a chip in the strip for a just-copied item.
- The fancy-text tab (`Aa`): typed text shown in Unicode styles (𝐛𝐨𝐥𝐝, 𝘪𝘵𝘢𝘭𝘪𝘤, 𝓈𝒸𝓇𝒾𝓅𝓉).
- Swipe on Backspace to delete a word.
- Auto-capitalise, smart space after punctuation, and moving the cursor to a word's end. The
  owner has these off in the reference.
- "Improved Telex" (cc=ch …) and quick initial and final consonants. The owner has these off.
- Split and one-handed layouts, and a tablet layout.
- Moving personal words between the phone and the laptop automatically. Day one only does it by
  hand, as files.

## Out of scope

- **Swipe (glide) typing**: not used.
- **Voice input**: no mic key; the keyboard stays fully offline.
- **Stickers, GIFs and a theme store**: they need the network and third-party artwork.
- **Drawing tone marks by hand**: the reference has it, but the owner doesn't use it.
- **Any account, sign-in, cloud sync or telemetry**: the point of the project is to have none.
- **Importing the reference keyboard's learned words**: they live in its online account, and no
  export is known. The new keyboard learns from scratch. The **shortcut table** does come over
  later, as a file.

---

## The keyboard (letters)

### What it shows

Top to bottom, in the dark slate theme by default. Keys have no borders, and their glyphs are
large, in MiSans Latin.

1. **Strip or tool row**, one row high (see "The strip" and "The tool row").
2. **Number row**: `1 2 3 4 5 6 7 8 9 0`, always shown in portrait. In landscape it follows the
   "Number row" setting (on, off or auto, where auto hides it in landscape).
3. **Letters**: QWERTY.
   - `q w e r t y u i o p`.
   - `a s d f g h j k l`, inset by half a key.
   - Shift (1.5 keys wide), `z x c v b n m`, then Backspace (1.5 keys wide).
4. **Bottom row**:
   - `?123`.
   - Comma, with a small globe above it, because holding it opens the keyboard picker.
   - The space bar, about 5 keys wide. It shows only a small, faint xrnikey tile, where the
     reference shows its name.
   - Full stop.
   - Enter, as a round key in the accent colour. Its icon follows the field's action: newline,
     send, search, go, next or done.

There is no navigation row, language key or hide key. Android's own navigation bar shows the
keyboard's hide button.

### What you can do

- **Tap a letter**: the key goes to the engine, and the word in progress is shown as composing
  text, without an underline where the app allows it. With "Key-press popup" on, a magnified
  copy of the key shows above the finger for as long as it is down.
- **Long-press a top-row letter** (400 ms): types its digit (`q` → 1 … `p` → 0). The popup also
  offers the letter's Vietnamese forms where it has any.
- **Long-press another letter**: a popup of its Vietnamese forms, `a` → â ă, `e` → ê, `o` → ô ơ,
  `u` → ư, `d` → đ. Slide to one and lift to type it. These go to the engine as finished
  letters, so the Telex rules still place the tone.
- **Shift**: one tap gives one capital, and a double tap locks capitals, which a third tap
  undoes. Capitals follow the keys that typed them (`Vieetj` → `Việt`), as on the laptop.
- **Backspace**:
  - with a word in progress, the engine deletes one character (`toán` → `tóa`, `được` → `đượ`);
  - otherwise it deletes before the cursor, or the selection;
  - right after an auto-correct, it reverts (see "Auto-correct");
  - holding it repeats after 400 ms, every 50 ms, and after 2 s it deletes a word at a time.
- **Space**: ends the word (see "Auto-correct"). A **horizontal swipe** that starts on the space
  bar moves the cursor one character per 12 dp. The word in progress is committed first.
- **Hold the space bar** (400 ms, no movement): the keys dim and the whole keyboard becomes a
  trackpad. Dragging moves the cursor in two dimensions, with lines as up and down. Lifting the
  finger leaves it.
- **Double Space**: replaces "␠␠" after a word with ". ". This happens only straight after a
  word or closing punctuation, and never in password, URL or email fields.
- **Comma**: types `,`, which ends the word like Space does, without a space. **Holding the
  comma** opens Android's keyboard picker, so the owner can switch to another keyboard and back.
- **Full stop**: types `.` and ends the word. **Holding it** gives `! ? ; : ' " - … ( )` in a
  popup.
- **Enter**: ends the word, then sends the field's action or a newline.
- **`?123`**: opens the symbols layout.
- **Rotate or resize**: the layout is never full-screen in landscape (no extract mode).

Fields change the keyboard:

- **Number and phone fields**: a 4×3 number pad.
- **Email and URL fields**: `@` and `.com`/`/` take the comma's and full stop's places, and there
  is no auto-correct.
- **Password fields** (any variation): every key is typed as itself. There is no Telex, no strip,
  no learning, and nothing goes to the clipboard history.
- **Fields that ask for no personalised learning**: everything works, but nothing is learned.

## The symbols layouts

### What it shows

- **Page 1**:
  - `1 2 3 4 5 6 7 8 9 0`;
  - `@ # ₫ _ & - + ( ) /`;
  - `=\<`, `* " ' : ; ! ?`, then Backspace;
  - the bottom row: `ABC`, comma, space, full stop and Enter.
- **Page 2**:
  - ``~ ` | • √ π ÷ × ¶ ∆``;
  - `£ € $ ¥ ^ ° = { } \`;
  - `?123`, `% © ® ™ ✓ [ ]`, then Backspace;
  - the same bottom row.
- Long-press popups hold related symbols: `-` → – — ·, `₫` → $ € £ ¥ ¢, `"` → “ ” „ « », and so
  on.

### What you can do

- **Tap a symbol**: it ends the word in progress, then types the symbol. Digits typed here are
  never Telex keys.
- **`ABC`**: back to letters. After a symbol followed by a space, the letters come back on their
  own, as on the reference.

## The strip

### What it shows

- On the left, a **menu button** (≡). It shows the tool row instead.
- **Three slots.**
  - The centre slot is the **centre pick**, with three small dots under it when there are more
    candidates.
  - The left and right slots hold the next-best candidates. A slot can hold an accented word, a
    two-word phrase (`không thể`, `dược phẩm`), the keys as typed, a shortcut's expansion, or an
    emoji.
  - The emoji suggestion takes the right slot when there is one and nothing better.
- On the right, an **emoji button**.

What the slots hold, by situation:

| Situation | Left | Centre (Space commits it) | Right |
|---|---|---|---|
| `khong` typed | `không thể` | `không` | `không có` |
| `duoc` typed after `không` | `dược phẩm` | `được` | 🙅 |
| `meeting` typed | `meetings` | `meeting` | `meet` |
| `ko` typed, with shortcut `ko` → `không` | `ko` | `không` | — |
| `list` typed after `the` | `lít` | `list` | `lists` |
| just after Space, following `không` | `có` | `được` | `phải` |
| field empty, nothing typed | the tool row instead |  |  |

### What you can do

- **Tap a slot**: commits that candidate and a space. A phrase commits both words. An emoji is
  inserted after the word, with one space.
- **Tap the dots, or swipe up on the strip**: opens the **expanded list**, a panel of up to 30
  candidates in a grid, in rank order. Tap one to commit it. Back, or the keyboard button,
  closes it.
- **Long-press a personal word in any slot**: a popup offers "remove". It is gone from the
  suggestions and from personal words.
- **Tap ≡**: the tool row replaces the strip until the next key.

The strip never shows a sentence of explanation, only candidates.

## The tool row

### What it shows

Five buttons, evenly spaced: theme, settings, clipboard, cursor pad and emoji. There is no mic:
the keyboard is fully offline and has no voice typing. The tool row shows whenever there is no word in
progress and no prediction to show, and whenever ≡ is tapped.

### What you can do

- **Theme**: a small panel offering dark slate (the default), light, true black, and follow the
  system.
- **Settings**: opens Settings.
- **Clipboard**: opens the clipboard panel.
- **Cursor pad**: opens the cursor pad.
- **Emoji**: opens the emoji panel.

## Auto-correct (Space and punctuation)

What Space, punctuation, Enter or a slot tap commits, by what was typed. The level is "flexible"
unless Settings says otherwise.

1. **A shortcut matches** (the keys, ignoring case): commit its expansion, cased like the keys
   (`ko` → `không`, `Ko` → `Không`, `KO` → `KHÔNG`).
2. **The keys used Telex marks or tones** (`dduowcj`, `khoong`): the owner is accenting by hand,
   so the Telex result is committed. There are three exceptions:
   - The Telex result is not a valid syllable (`đươc`: a stop coda with no tone). Auto-correct
     takes the best valid syllable that keeps every mark that was typed (`được`).
   - The keys are also an English word, and the context says English (see "Mixed English and
     Vietnamese"): the keys are committed as typed (`list`, not `lít`, after `the`).
   - The word can't be Vietnamese at all (`tẽt` from `text`): the keys as typed, as on the
     laptop today.
3. **The keys used no Telex effect** (all ASCII, such as `khong`, `ban`, `can`, `meeting`):
   - Candidates are:
     - every attested syllable with that ASCII shape;
     - the keys as typed, as English or as an unaccented Vietnamese syllable;
     - personal words;
     - for typos, syllables and English words one key away on the layout (see "Typos").
   - They are ranked by the bilingual model with the last three words as context.
   - The centre pick is the best. Space commits it when it is the shape's accented form, or the
     typed form itself.
   - A typo correction (a different ASCII shape) is committed only if it beats the keys as typed
     by the level's margin. Otherwise the keys stay.
4. **Neither a syllable nor a known word, and no close correction** (`dc`, `ns`, `vs`, a name):
   commit as typed. Short chat abbreviations are never "corrected" into words. A hand-written
   list of them is part of the project.

Levels:

| Level | Accent restoration on Space | Typo correction on Space |
|---|---|---|
| Off | no: the Telex result is committed | no |
| Flexible (default) | yes | edit distance 1, words of 3 or more letters, wide margin |
| Strong | yes | edit distance 2 for words of 7 or more letters, smaller margin |

- **Revert**: Backspace as the very next key after an auto-correct puts back what was typed and
  the separator (`không␠` → `khong␠`). A second Backspace deletes as usual. When the owner
  reverts the same typed form twice in a similar context, it is learned: that form is committed
  as typed from then on, and its auto-correct moves to the left slot.
- **Look-back** (decided: on, as the reference does it): after Space, the engine re-ranks the
  last three syllables typed without marks as one sequence. If the best sequence changes an
  earlier syllable, the keyboard rewrites it (`có gắng` → `cố gắng`). It does this only if all
  of these hold:
  - the earlier syllable was restored by the engine, not typed with marks, picked from the strip
    or reverted;
  - the cursor never left the run;
  - `getTextBeforeCursor` still shows exactly what the keyboard wrote.

  Otherwise it leaves the text alone, as the laptop's instant commit does. Backspace as the very
  next key after a look-back fix undoes that fix (the earlier syllable goes back to what was
  there), the same way Revert undoes an auto-correct.
- The cursor moving by the owner's own action (a tap in the text, the cursor pad, or a selection
  change the keyboard didn't cause) ends the word in progress as Space would, without
  auto-correct, and starts a new run.

## Accent restoration and ranking (the engine)

This is the part that has to match the reference. The work is done in the core (C++) and
measured by the eval harness ("Done means").

- **Candidates per syllable**: the attested syllables with the same ASCII shape, from the core's
  `vi-syllables.txt` extended with the model's vocabulary. `duoc` gives được, dược, đước, đuốc,
  duốc, and `ban` gives 14 syllables.
- **Score**: log P(syllable | the previous three tokens) from a 4-gram Vietnamese syllable model
  with modified Kneser–Ney smoothing and pruning, stored in a compact binary format.
  - Tokens are lowercase NFC syllables, with the tone placed the engine's way, plus English
    words and `<num>`.
  - English words that occur in Vietnamese text (online, check, file) are ordinary tokens, so
    the model knows them in Vietnamese context.
- **Context**: the last three tokens before the cursor. They come from the engine's own record
  of the run, or from `getTextBeforeCursor` when the word starts in existing text.
- **Phrases** for the side slots: the best next syllable when P(next | this) is high
  (`không` → `thể`, `có`). Phrases are never the centre pick.
- **Look-back**: a beam search (width 8) over the unaccented run of at most three syllables.
  This is cheap: at most 26 candidates per position.
- **Personal words** add their counts on top, as an interpolated user unigram and bigram model.
  This is how names, slang and the owner's own phrasing win after a couple of uses.
- **Capitals**: matching is on lowercase, and the commit gets the case of the typed keys back
  (`Khong` → `Không`).

## Mixed English and Vietnamese

There is one keyboard, with no language switch. For every word, the engine weighs both readings
of the keys in context:

- **Vietnamese reading**: the Telex result, or the accent-restored syllable, scored by the
  Vietnamese model.
- **English reading**: the keys as typed, scored by an English word model (unigrams and bigrams)
  and by the Vietnamese model, where the word occurs in Vietnamese text.
- **A switch cost**: P(language | the previous word's language), estimated from the corpora,
  then adapted to the owner's own typing on the phone.

The higher total wins. Ties go to Vietnamese, the laptop's rule that "typing Vietnamese never
turns into English".

**Why the laptop has 19 wrong words, and how this fixes them.** The core decides at the end of
each word with no context. If the Telex result is an *attested* syllable, Vietnamese always
wins; if not, and the keys spell a common English word, English wins. Commit f6a5021 added
everyday syllables to the attested list (from Tatoeba, plus a hand list): lít, tủn, víu, bốt,
kếp, líp, rơ and others. That fixed 13 Vietnamese words, and turned 19 English words into
Vietnamese everywhere: boost, boots, corn, cups, hangs, keeps, lifts, list, lips, lisp, lust,
photos, row, runs, sits, tips, turn, verse and virus.

A yes-or-no list has to settle each overlapping word once, for every context. No choice is right
in both `the list` and `hai lít nước`. The bilingual score uses what is around the word: `my list`
→ English, `2 lít` → Vietnamese. With no context at all, it uses each reading's frequency and the
owner's language mix. The same choice covers the 128 ASCII shapes that are both an unaccented
syllable and a common English word (`can`/`cần`, `chat`/`chát`, `do`/`đó`, `con`, `bang`, `day`).

This lands in the core, so the laptop gets it too: the Fcitx5 addon already keeps the last
couple of finished words per input context and will pass them in. Without the model loaded, the
core falls back to today's list rule.

"Stop adding tone marks to non-Vietnamese words" (on) is this feature. Turning it off commits
the Telex result whatever the context, like `AutoRestore=False` on the laptop.

## Typos

- Each tap is sent to the engine as its letter plus up to four neighbouring keys, weighted by
  the distance from the touch point to each key's centre. A tap between `n` and `m` counts for
  both.
- Correction candidates are the syllables (by ASCII shape) and English words reachable by one
  substitution, insertion, deletion or swap, where a substitution costs less for a neighbouring
  key than for a distant one. For words of 7 or more letters, two edits are allowed at the
  Strong level.
- A correction must beat the keys as typed by the level's margin (see "Auto-correct"). The core's
  existing `xk_autocorrect` (Damerau–Levenshtein plus the ch/tr, s/x, d/gi/r confusions) is kept
  and gains the key-distance cost.

## Personal words (learning, on the phone only)

### What is learned

- Every committed word and word pair, in fields that allow learning, adds to its count. This
  includes English words, names, and Vietnamese phrases the shipped model doesn't know. A word
  becomes a candidate after it has been typed twice and kept (not reverted).
- **Reverts**: a typed form reverted twice in a similar context is kept as typed from then on.
- **Strip picks**: picking a side slot over the centre pick raises that candidate in that
  context.
- Counts decay: they are halved every 90 days of typing, so old habits fade.

### Where it is kept

- In the app's credential-encrypted private storage, as two text files the core reads and writes
  atomically (a temporary file, then rename):
  - `user-dict.txt`: `word<TAB>count`, the same format as the laptop's
    `~/.local/share/xrnikey/user-dict.txt`;
  - `user-pairs.txt`: `word<TAB>word<TAB>count`.
- They are read only after the phone is first unlocked. Before that, the keyboard types with the
  shipped model alone.
- `allowBackup="false"`. Nothing is synced. Export and import are by hand (see Settings).

## Shortcuts

- **The table**: abbreviation → expansion, matched ignoring case, with the expansion's case
  following the keys. The same rules as the laptop's `Macro` and `CapitalizeMacro`. The matching
  code moves from the Fcitx5 addon (`macro_table.cpp`) into the core, so both devices behave the
  same.
- **When it expands**: on Space or punctuation, as auto-correct's first rule. The expansion also
  shows in the centre slot while the abbreviation is typed, so a tap works too.
- **Storage**: `shortcuts.txt` in private storage. Each line is `abbreviation<TAB>expansion`,
  exactly the laptop's `macros.txt` format, so one file can serve both devices.
- **Import** (Settings → Shortcuts → Import) reads a file chosen with the system picker, which
  needs no permission. Accepted lines:
  - `abbr<TAB>expansion`;
  - `abbr,expansion` (CSV, with quoted fields allowed);
  - `abbr=expansion`.

  The import shows the counts it found: new, same, changed and skipped. Nothing is written until
  the owner confirms. A changed entry replaces the old one.
- **Export** writes the table in the TAB format.
- The owner's own table is imported later, from their device. **It is never committed to this
  repo.** Tests use made-up tables.

## The emoji panel

### What it shows

The panel replaces the strip and the keys.

1. **A tab row**: back to the keyboard (`ABC`), emoji, emoticons `:-)` and symbols `Ω`. Fancy
   text `Aa` comes later. A messaging app's sticker tab is left out: its stickers are another
   company's artwork.
2. **An emoji suggestion row**: emoji that match the last words typed, from the Unicode CLDR
   keywords in Vietnamese and English (`không được` → 🙅 ❌).
3. **A grid**, 8 per row, scrolling vertically. It opens on recents (up to 48), or on smileys
   when recents are empty.
4. **A round search button**, floating at the bottom right of the grid.
5. **A category bar** at the bottom: recents, smileys and people, food, activities, travel,
   animals and nature, objects, symbols and flags, then Backspace.

### What you can do

- **Tap an emoji**: inserts it and moves it to the front of recents.
- **Long-press an emoji that takes skin tones**: a popup of the six tones. The choice is
  remembered for that emoji.
- **Tap a category**: jumps the grid to it. The bar follows as the grid scrolls.
- **Search**: a search field replaces the suggestion row, and the letter keys come back below it.
  Typing goes into the search field, not the app. Results match CLDR keywords in Vietnamese
  (with or without accents) and English, best first. Back closes the search.
- **Backspace** on the category bar deletes before the cursor in the app, one grapheme at a
  time, so a whole emoji goes at once.
- **Emoticons tab**: a grid of text emoticons and kaomoji. A hand-written list, part of the
  project.
- **Symbols tab**: Unicode symbols by group: arrows, maths, currency, bullets, boxes and stars.
- Only emoji that the phone's emoji font can draw are listed (`Paint.hasGlyph`). The emoji are
  drawn with the system font, and no emoji artwork ships with the app.

## The clipboard panel

### What it shows

- A list, newest first, of text the owner copied while this was the keyboard. Each item shows up
  to two lines.
- Pinned items sit above the rest, under a pin icon.
- A trash button at the top clears every unpinned item.
- When the history is empty, the panel shows only the clipboard icon, greyed out.

### What you can do

- **Tap an item**: pastes it at the cursor (`commitText`) and closes the panel.
- **Long-press an item**: pin or unpin it, or delete it.
- **What is kept**: plain text only, at most 50 unpinned items (the oldest goes first), with no
  time limit. Pinned items are kept until they are unpinned. These rules are settings.
- **What is never kept**:
  - a clip marked sensitive (`ClipDescription.EXTRA_IS_SENSITIVE`, which password managers set);
  - a clip copied from a password field;
  - anything copied while the clipboard setting is off.
- Storage is a JSON file in credential-encrypted private storage. It is read only after unlock.

## The cursor pad

### What it shows

The panel holds:

- arrows ← → ↑ ↓;
- Home and End;
- a Select toggle;
- Select all, Copy, Cut, Paste and Delete;
- `ABC` to go back.

### What you can do

- **Arrows**: move the cursor (`setSelection` when the extracted text is known, D-pad key events
  otherwise). With Select on, they extend the selection.
- **Copy, Cut and Paste**: `performContextMenuAction`.

## Settings

A plain Compose activity, opened from the tool row's gear and from the app's launcher icon. It
follows the family's look: the xrnikey tile is the title, the text is MiSans Latin, and the
labels are short with no explanatory sentences.

### First run

Two steps, the only text being their labels: **Enable** opens Android's keyboard settings, and
**Choose** opens the keyboard picker. Each step gets a tick when done. After that, the screen
opens on the settings list.

### What it shows and does

| Section | Setting | Default (the owner's) |
|---|---|---|
| Typing | Method: Telex / VNI | Telex |
| | Tone style: old (hòa) / new (hoà) | old |
| Suggestions | Show suggestions | on |
| | Emoji suggestions | on |
| | Personal words: the list, search, add, delete one, clear all (with confirm), export, import | — |
| Auto-correct | Level: off / flexible / strong | flexible |
| | Keep non-Vietnamese words as typed | on |
| | Double space for full stop | on |
| | Look-back | on |
| Keys | Number row: on / off / auto | auto |
| | Long-press top row for digits | on |
| | Key-press popup | on |
| | Long-press delay | 400 ms |
| | Swipe space bar to move cursor | on |
| | Hold space bar to move cursor | on |
| | Hold comma to switch keyboard | on |
| | Keyboard height: 80–120 % | 100 % |
| Clipboard | Keep history | on |
| | History size | 50 |
| Shortcuts | The table: add, edit, delete, import, export | — |
| Theme | Dark slate / light / true black / system | dark slate |
| About | Version, model version, licences of the code and the model data | — |

- **Export** of personal words or shortcuts writes a `.txt` file through the system picker.
- **Import** reads one, and shows its counts before anything is written.
- The About page also shows the latency figures (p50, p95 and p99 of key-to-text since the
  keyboard started) as numbers only. They are kept in memory and never written down.

## The engine route: JNI to the shared C++ core

**Decision: JNI.** The keyboard calls `libxrnikey` through a thin JNI layer. There is no Kotlin
port.

Why:

- **One engine.** The Telex rules, the word replay, tone placement, the syllable validator and
  auto-restore are about 1,600 lines, with 66 core test cases and 35 addon tests tuned across
  many fixes. A port would be a second copy that drifts, and every later fix would have to be
  made twice. The owner's rule is that improvements reach both devices.
- **It already fits.** The core is standard C++20 plus POSIX `mmap`, behind a C ABI of plain
  structs. Strings that cross the boundary are documented `malloc`'d copies. Its keys are XKB
  keysyms, which equal the character code for every key a phone sends, plus BackSpace
  (`0xFF08`). The engine's preedit mode is exactly Android's composing text: `preedit` →
  `setComposingText`, `commit` → `commitText`. Instant commit isn't needed on Android.
- **Speed.** A JNI call costs about a microsecond. The model's lookups and the beam search stay
  in C++ next to the memory-mapped data.

What it costs, and how each cost is handled:

- **An NDK build.**
  - `android/app/src/main/cpp/CMakeLists.txt` adds `../../../../../core` as a subdirectory, so
    there is one copy of the sources. It builds for `arm64-v8a` only, which is the owner's phone.
  - The core asks for CMake 3.27, but the SDK has 3.22. Either install SDK CMake 3.31 or set
    `cmake.dir` to the system CMake in `local.properties`.
  - NDK 27 is already installed.
  - The core links `pthread` by name. It should use `Threads::Threads`, which works on the
    laptop and on Android.
- **A crash in native code takes the keyboard down.**
  - No C++ exception crosses the C ABI; each entry point catches.
  - Host tests run the C API under ASan and UBSan, with a fuzz test of random key streams.
  - The JNI layer holds no global references between calls.
- **JVM unit tests need the engine.** The same JNI library is also built for the host
  (linux-x86_64, against the JDK's `jni.h`). JVM tests load it with `System.load`, so they run the
  real engine with no phone and no Robolectric.

### Core additions (all in `core/`, used by both frontends)

| Module | What | Used by the laptop |
|---|---|---|
| `lm.cpp` | Binary n-gram model: memory-mapped, 8-bit quantised probabilities and backoffs, sorted arrays with binary search. Opened from a path or from a file descriptor plus offset. Replaces the 132-line toy `vi.arpa` and its ARPA loader. | yes |
| `restore.cpp` | Candidates per ASCII shape, scoring in context, the look-back beam | yes, as suggestions |
| `bilingual.cpp` | The English model, the switch cost, and the context-aware pass-through decision in `word_final` | yes: fixes the 19 words |
| `learn.cpp` | Personal unigrams and pairs, reverts, decay, atomic save | yes, replacing `xk_dict_add_word`'s plain list |
| `shortcuts.cpp` | Moved from `fcitx5-addon/macro_table.cpp` | yes |
| `touch.cpp` | Key-neighbour weights in the edit cost | no |
| `emoji.cpp` | Rewritten over a CLDR-built index (Vietnamese and English keywords) | yes |

The new C API is a *session*: one opened model, the state, the context, and a ranked candidate
list carrying each candidate's kind (accented, as typed, phrase, correction, shortcut, emoji,
personal) and an "auto" flag for the centre pick. Calls commit a candidate, revert the last
commit, and teach. The existing `xk_process_key` contract stays as it is, and the Fcitx5 tests
keep passing.

## The Android app

- **Stack**:
  - Kotlin, with an `InputMethodService`.
  - The keys and the strip are one custom `View` drawn on a `Canvas`. That is the hot path, as in
    AOSP's LatinIME, with no recomposition per key.
  - The panels (emoji grid, clipboard, cursor pad, expanded list) and Settings are Jetpack
    Compose.
  - The Compose views in the IME window get the lifecycle and saved-state owners set on the
    window's decor view.
- **Project**:
  - The package is `com.xern.xrnikey`. The system lists the keyboard as "xrnikey", because the
    OS needs text.
  - `android/` is a Gradle project copied from the family's smallest app setup (`xrn-android-app`
    skill): AGP 9, Kotlin 2.4, compileSdk and targetSdk 35, minSdk 26, Java 17, and release builds
    with R8.
- **Font**: MiSans Latin VF from `res/font/`, credited in the README. The key glyphs use regular
  weight.
- **Before first unlock**: the service is `directBootAware`, so it types with the shipped model.
  Personal files load on `ACTION_USER_UNLOCKED`.
- **HyperOS**: an IME is bound by the system, so it needs no Autostart. Check that HyperOS's
  "secure keyboard" replacing third-party keyboards in password fields does no harm.
- **Tests**:
  - JUnit 4 on the JVM, against the host-built engine. The Telex corpus rows of
    `tests/test_corpus.cpp` move to a shared data file that both the Catch2 suite and a Kotlin
    test read, so the two can't disagree.
  - The InputConnection logic (composing, revert, look-back guards, cursor-moved detection) runs
    against a fake text buffer, like the laptop addon's fake client.
- **CI** (the repo is public, so minutes are free): a second job builds the debug APK, runs the
  JVM tests, and fails if the merged manifest names any `uses-permission`. CI uses a small test
  model built from the made-up test text, so it downloads no corpora.

## The model data

All of it ships in the APK and is never downloaded.

### Sources (redistributable, credited in `data/README.md` and Settings → About)

| Source | Used for | Licence |
|---|---|---|
| Leipzig Corpora Collection, Vietnamese news and web corpora (the downloadable sentence files) | the Vietnamese 4-gram model, phrases, the switch cost | CC BY |
| Leipzig Corpora Collection, English news corpus | English unigrams and bigrams | CC BY |
| Tatoeba Vietnamese sentences (already a source of `vi-syllables.txt`) | short conversational sentences for the model; part held out | CC BY 2.0 FR |
| Mozilla Common Voice Vietnamese sentences (already a source) | short sentences; part held out | CC0 |
| Wikisource public-domain texts (already a source) | the syllable inventory only; too old-fashioned for ranking | public domain |
| Unihan readings (already a source) | the syllable inventory | Unicode License v3 |
| SCOWL (already the source of `english.txt`) | the English vocabulary | SCOWL's permissive notice |
| Unicode CLDR annotations, `vi` and `en`, and `emoji-test.txt` | emoji names, keywords, order and groups | Unicode License v3 |
| A made-up chat and test set, written for this project | tuning the switch cost and abbreviations; the eval set C | MIT, with the code |

Rules:

- **Not used**:
  - Wikipedia, whose CC BY-SA would make the model BY-SA;
  - anything non-commercial (OPUS's TED and QED);
  - anything with no clear licence: subtitles, scraped news dumps, the Common Crawl text in
    OSCAR.
- The Leipzig terms page must be read and its exact wording recorded in `data/README.md` before
  a model ships.
- The shipped model is under **CC BY 4.0**, with the attributions above. The code stays MIT.

### Building it

`data/lm/README.md` has the commands. In short:

- `data/lm/sources.json` lists each source's URL, SHA-256 and licence.
- `data/lm/build.py` (Python standard library) works in three steps:
  - **fetch** downloads each source into its own folder and refuses any other hash;
  - **prepare** tokenizes them, streaming the archives without unpacking them: NFC, the engine's
    tone placement, lowercase, numbers as `<num>`. It also cuts the held-out sets by a hash of
    each sentence;
  - **train** runs `tools/lm/xk-lm-build`, this repo's own trainer: interpolated modified
    Kneser–Ney, count pruning, renormalised backoffs, written straight to the binary format. It
    needs no outside tool.
- The outputs (`vi.xklm`, `en.xklm`) are **not in git**. They are rebuilt from the pinned
  sources, and are proposed as release assets of this repo, with `data/lm/model.lock` pinning
  their SHA-256 for the Android and Arch builds.
- CI builds small models from made-up text (`data/lm/test`) at build time.

### Size on the phone

| File | Budget |
|---|---|
| `vi.lm` (about 4 M n-grams after pruning) | ≤ 25 MB |
| `en.lm` | ≤ 6 MB |
| `lexicon.bin`, `emoji.bin` | ≤ 3 MB |
| `libxrnikey.so` + JNI (arm64) | ≤ 3 MB |
| APK total | ≤ 45 MB |

- The model files are stored uncompressed in the APK (`noCompress`). They are memory-mapped
  straight from it through the asset's file descriptor and offset, so nothing is copied and only
  the pages in use take RAM.
- If that fails on some phone, the files are copied once to `noBackupFilesDir`.

## Data and privacy

- **Permissions**: none, and **no `android.permission.INTERNET`**. A CI test fails the build if
  the merged manifest declares any `uses-permission`. The service only *requires*
  `BIND_INPUT_METHOD` of the system that binds it.
- **On the phone** (credential-encrypted private storage):
  - personal words and pairs;
  - the revert memory;
  - the shortcut table;
  - the clipboard history and pins;
  - emoji recents and skin tones;
  - settings.

  These are kept until deleted in Settings, or the app is uninstalled. `allowBackup="false"`.
- **Never stored**:
  - anything typed in a password field;
  - text from fields that ask for no personalised learning;
  - sensitive clips;
  - whole messages. Only word counts and word-pair counts are learned, never sentences.
- **Leaves the phone**: nothing, except files the owner exports by hand through the system
  picker.
- **No microphone**: there is no voice input, and nothing is ever recorded.

## Sync

There is none. The keyboard has no network. Personal words and shortcuts move between the phone
and the laptop only as files the owner exports and imports. Both files use the laptop IME's
formats (`user-dict.txt`, `macros.txt`), so the same file works on both devices.

## Updates

A keyboard with no internet can't update itself, so the owner's hub app (a separate app of
theirs that already has network access and lists every project) does it. The keyboard never
gets INTERNET.

- The release APK is published to the owner's own server next to the hub app's other release
  files.
- The hub app checks the server, and when a newer keyboard is there it shows **Keyboard update
  ready**. The owner taps **Install**.
- It installs the APK with a `PackageInstaller` session, the same way the owner's rules app
  already updates itself. After its first install the hub app is the installer of record, so
  later updates need no extra confirmation (`USER_ACTION_NOT_REQUIRED`).
- Android installs an update only if it is signed with the keyboard's key, so a compromised
  server can't push a different keyboard.
- An IME update keeps the keyboard enabled and chosen as the default.
- Until the hub app has this, a release is installed with adb from the laptop.

## Signing key

The keyboard uses **the owner's release keystore**, the one their apps that share a signature
permission already use. The build reads it from `../xrn-signing.properties`, outside the repo, so
release builds are made from the main checkout. It is never the shared debug key. The choice is
made once and never changes.

Why:

- An input method sees every key typed in every app, so it gets the password-protected release
  key, which the owner's key backup already covers.
- Every debug build on the laptop is signed with the debug keystore, and its password is the
  well-known default. Anything that ever trusted "same signature" would trust them all.
- A signature permission may be needed later between the keyboard and an updater or a sibling
  app, which is the family's rule for using the release key.

Nothing about the key enters this public repo except that relative path.

## Done means

All the checks below are measured by the eval harness `tools/xk-eval`. It is part of the first
PR, prints a table, and runs in CI on the small test model, plus on the laptop against the real
one. The thresholds are first targets. The parity run (last item) replaces them with the
reference's own numbers where it beats them.

### Evaluation sets

| Set | What | Licence |
|---|---|---|
| **H1** | 2,000 held-out sentences from the Leipzig news and web corpora, never used in training, near-duplicates removed | CC BY |
| **H2** | 1,000 held-out short sentences from Tatoeba and Common Voice | CC BY 2.0 FR / CC0 |
| **C** | 300 made-up chat messages: casual register, abbreviations (`ko`, `dc`, `r`), about 1,000 English tokens mixed in, each with its intended text | MIT, written for this project |
| **E** | the 10,972 words of `english.txt` | as that file |
| **V** | the 5,840 syllables of `vi-syllables.txt` | as that file |
| **T** | set C with deterministic adjacent-key typos at 3 % per letter (seeded, using the real layout geometry) | MIT |

### Targets

1. **Accent restoration**, typed syllable by syllable without marks, scored per syllable with
   left context and look-back. The report also gives the figures without look-back.
   - H1: top-1 ≥ 95.0 %, and in the strip (top-3) ≥ 99.0 %.
   - H2: top-1 ≥ 94.0 %.
   - C: top-1 ≥ 93.0 %.
2. **English pass-through.**
   - E, each word typed with Telex after `the`: ≥ 99.5 % committed as typed.
   - C: ≥ 98.0 % of English tokens as typed.
   - V typed with Telex after a Vietnamese syllable, plus the Vietnamese of C: ≤ 0.5 % wrongly
     turned English.
   - Each of the 19 words gets two test rows: it stays English after an English word
     (`my list`), and stays Vietnamese after a Vietnamese word it goes with (`hai lít`,
     `giành cúp`, `ngắn tủn`). All 38 rows pass, on the laptop's Fcitx5 tests too.
3. **Telex parity**: every existing core and addon test passes. The shared corpus rows pass
   through JNI with the same results.
4. **Typos** (T): ≥ 85 % of corrupted words fixed by Space, and ≤ 1 % of correct words changed.
5. **Suggestions** (C and H2):
   - next-word top-3 hit rate ≥ 30 %;
   - after the first two letters, top-3 ≥ 70 %;
   - keystroke savings when the strip is used whenever the word is in it: ≥ 25 %.
6. **Latency**, on the owner's phone, release build, from 1,000 taps typed by script:
   - from the touch event to the commit or composing call returning: p95 ≤ 20 ms, p99 ≤ 35 ms;
   - the strip updated: p95 ≤ 30 ms after the key;
   - the engine call alone: p99 ≤ 5 ms;
   - the keyboard shown after the process was killed: ≤ 300 ms;
   - janky frames in a 200-key burst (`dumpsys gfxinfo`): ≤ 1 %.
7. **Memory and size**:
   - PSS ≤ 80 MB with the model mapped and in use (`dumpsys meminfo`);
   - APK ≤ 45 MB;
   - model files within the budgets above.
8. **Privacy**: `aapt2 dump permissions` lists nothing. The clipboard test shows that a
   sensitive clip and a password-field copy are not kept. A password field gets every key as
   typed and learns nothing.
9. **Parity with the reference** (decided: yes; the owner's side runs it on the phone later):
   - The run uses 300 sentences from H2 and C, with no private text, typed by adb taps at key
     coordinates into a plain text field.
   - Each keyboard types them with its own defaults, and the reference with its learning turned
     off.
   - Result: this keyboard's top-1 restoration and English pass-through are ≥ the reference's on
     the same sentences.
   - This is recorded in `android/docs/status.md` before the reference is uninstalled.
10. **Daily use**: the owner types with it as their only keyboard for 7 days, and the gaps they
    hit are listed in `android/docs/status.md` and fixed or agreed.

## Build order (each step one PR, with its tests)

1. **Model pipeline and eval harness.** `data/lm` (sources, cleaning, training), the binary
   `lm.cpp` and its reader tests, `tools/xk-eval`, and sets H1, H2, C, E, V and T. Baseline
   figures in the PR.
2. **Accent restoration.** `restore.cpp`, the session API, look-back, phrases. Eval target 1.
3. **Context-aware English decision.** `bilingual.cpp`, and the Fcitx5 addon passing its context.
   The 38 rows of target 2. This ships to the laptop first.
4. **Learning, shortcuts in the core, key-distance typos, CLDR emoji index.** Targets 4 and 5.
5. **`android/` skeleton.**
   - Gradle and the JNI build (arm64 plus the host);
   - host JNI tests and the shared corpus file;
   - the service, with letters, the number row, symbols and Telex as composing text;
   - Settings' first run and the no-permission CI check.

   The first build the owner can install.
6. **The strip and auto-correct.** Candidates, Space and punctuation, Revert, the expanded list,
   next-word predictions, double Space, field types.
7. **Gestures and popups.** Moving the cursor with the space bar and the trackpad, long-press
   digits and variants, comma to the picker, the key preview, Backspace repeat.
8. **The emoji panel** and emoji suggestions.
9. **The clipboard panel, cursor pad, tool row, themes and height.**
10. **Settings in full.** Personal words, shortcut import and export, About with licences and
    latency.
11. **A pass on the phone.**
    - latency, memory and the parity run;
    - `android/docs/status.md`;
    - the README's Android section and parity section;
    - registering the project with the owner's other apps.

## Risks

- **Training text.**
  - The reference's maker also runs a huge messaging app, so its model has very likely seen far
    more casual Vietnamese chat than any openly licensed corpus holds.
  - Leipzig's news and web text is formal. Accuracy on chat (set C) is the number most at risk,
    and the one the owner feels.
  - Mitigations: phrases learned on the phone, the made-up chat set for tuning, and the parity
    run to show the real gap.
- **The parity run** happens on the phone, by the owner's side, before the reference is removed.
  Until then the targets stay as written, with no direct comparison.
- **Typo correction** is where mature keyboards are strongest. A neighbour-weighted edit distance
  is simpler than a full touch model, so set T may show a gap.
- **App quirks.** Composing text, `deleteSurroundingText` and `getTextBeforeCursor` behave
  differently across messaging apps and browsers. Look-back and Revert must check the text before
  touching it, and otherwise do nothing, as the laptop's instant commit does.
- **The shared engine.** A change made for the phone can break the laptop. Every core PR runs the
  Catch2 suite, the Fcitx5 tests and the JVM tests.
- **Model hosting.** The model lives as a release asset of a public repo. Losing the asset means
  retraining from `sources.json`, which takes hours, not days.
- **Licences.** Leipzig's CC BY wording has to be confirmed on its own terms page before
  anything ships.

## Decisions (2026-11-21)

The owner answered the spec's open questions:

1. **No mic key at all.** The keyboard is fully offline, with no voice typing.
2. **The parity run** against the reference happens later, on the phone, run by the owner's side.
3. **Updates come through the hub app**: it shows "Keyboard update ready" and installs on a tap.
   The keyboard itself never gets INTERNET.
4. **Look-back is on**: the previous word is fixed in place, as the reference does, and
   Backspace right after undoes the fix.
5. **The emoji, `:-)` and `Ω` tabs stay.** Fancy text comes later. The sticker tab is left out.

No questions are open.
