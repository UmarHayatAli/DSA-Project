# Caselaw Search Engine

CS-250 Data Structures and Algorithms semester project (NUST SEECS, BESE-2025 A).

A search engine based on the paper *The Anatomy of a Large-Scale Hypertextual Web
Search Engine* (Brin and Page, 1998). It searches 54,359 Arkansas court cases from the
Caselaw Access Project. The core engine is written in C++17, and all index data is stored
in files (no database).

**Team:** M. Aizaz Wajid (548772), Umar Hayat Ali (561900), Tahreem Zafar (547588)

This README has three parts:

1. [What is done, what is left, next step](#part-1-what-is-done-what-is-left-next-step)
2. [How to reach the current level](#part-2-how-to-reach-the-current-level)
3. [The work done, explained](#part-3-the-work-done-explained)

---

# Part 1: What is done, what is left, next step

### Done ✅

| Part | What it does |
|---|---|
| Dataset download | Downloads all Arkansas cases and keeps the 54,359 with at least 100 opinion words |
| Document index | docID → title, citation, date, court, PageRank, where the case is in the file |
| Tokenizer | Cleans text into words (also used later for search queries) |
| Lexicon | word → wordID, plus how often each word appears |
| Forward index | case → its words and their positions |

Each part is explained in [Part 3](#part-3-the-work-done-explained).

### Next step ⏳ Inverted index (Part 1 deadline: 29 Nov)

The inverted index is the forward index "turned around":

```
Forward index (done):   case 0     -> goings, against, mills, court, ...
Inverted index (next):  "court"    -> case 0, case 1, case 5, ...   (with positions)
```

This is what makes search fast: to search "negligence", the engine finds its wordID in the
lexicon and reads only that word's list of cases.

What it must do:

- **Input:** `forward.bin`, `forward_off.bin` and `lexicon.bin` (already built).
- **Output:** for every wordID, its list of cases (docIDs sorted), with the number of hits
  and the hits for each case, plus an offset table (wordID → where its list starts in the file).
- **Memory:** stay well under 2 GB while building (the full dataset has 104 million hits).
- **Later:** easy to split into barrels (groups of wordIDs) for 6 Dec.

Files to add, following the pattern of the finished parts:
`include/inverted_index.h`, `src/inverted_index.cpp`, `tests/test_inverted_index.cpp`,
a check in `src/build_index.cpp`, the new files in `CMakeLists.txt`, and a new section in
Part 3 of this README.

### Left (later deadlines)

| Part | Requirement from the course description | Deadline |
|---|---|---|
| Barrels | Split the inverted index into barrels | 6 Dec |
| Single-word search | Results in under 500 ms | 16 Dec |
| Multi-word search | All words must match (also "and"), under 1.5 s for 5 words | 16 Dec |
| Ranking | Word frequency, position, and at least 2 metadata fields (date, PageRank, court) | 16 Dec |
| Autocomplete | Trie, suggestions in under 100 ms | 16 Dec |
| Semantic search | GloVe 6B 100d word vectors, our own cosine similarity, no LLMs | 16 Dec |
| Adding documents | New cases as JSON / text / from the UI, indexed in under 1 minute, search keeps working | 16 Dec |
| Web UI and server | cpp-httplib backend, HTML/CSS/JS frontend | 16 Dec |
| Deployment | Docker on Render | 16 Dec |
| Limits | At most 2 GB RAM; at least 20 Git commits, from every member | 16 Dec |

Viva: **18 Dec**.

---

# Part 2: How to reach the current level

Do these steps once, in order. Type the commands in the VS Code terminal (PowerShell)
unless a step says otherwise.

### Step 1: Install Git and Python

Check that both are installed:

```
git --version
python --version        (must be 3.10 or newer)
```

If not, install them from git-scm.com and python.org.

### Step 2: Install the C++ tools (g++, CMake, Ninja), about 20 minutes

1. Install MSYS2 (in PowerShell):
   ```
   winget install --id MSYS2.MSYS2 -e
   ```
2. Open the **MSYS2 UCRT64** terminal from the Start menu and run:
   ```
   pacman -Syu
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-gdb
   ```
   If `pacman -Syu` closes the window, open it again and run `pacman -Syu` once more.
3. Add `C:\msys64\ucrt64\bin` to your `Path` (Start → search "Edit the system environment
   variables" → Environment Variables → Path). If `C:\MinGW\bin` is listed, remove it or
   move the new folder above it (the old g++ 6.x does not support C++17 fully).
4. Restart VS Code and check in a new terminal:
   ```
   g++ --version      (should show 16.x)
   cmake --version    (should show 4.x)
   ```

| Tool | What it is for |
|---|---|
| g++ | C++17 compiler |
| CMake | Reads `CMakeLists.txt` (which `.cpp` files make which program) and runs the compiler |
| Ninja | Fast build runner used by CMake |

### Step 3: Get the code

First time:

```
git clone https://github.com/UmarHayatAli/DSA-Project.git
cd DSA-Project
```

Already cloned before:

```
git switch main
git pull
```

Open **the `DSA-Project` folder itself** in VS Code (File → Open Folder), so every terminal
starts in the right place. Then make your own branch with your name:

```
git switch -c TahreemZafar
```

### Step 4: Compile and run the tests

```
cmake -S . -B build -G Ninja      (only the first time)
cmake --build build               (again after every code change or git pull)
ctest --test-dir build
```

Expected: no errors, and `100% tests passed out of 3`.

### Step 5: Build the indexes on the 200-case sample (under 1 second)

The sample is already in the repository, so this works without downloading anything:

```
build/build_index sample/sample.jsonl index_sample
```

Expected: three lines starting with `CHECK OK`. The folder `index_sample/` now holds the
index files; open the `.txt` files in VS Code to look at them.

**For writing and testing code, the sample is enough.** The full dataset is only needed for
final testing and for measuring speed and memory.

### Step 6: Download the full dataset (one-time, about 30-45 minutes)

```
python scripts/download_dataset.py
```

Expected at the end: `KEPT (in cases.jsonl) ... 54,359` and `RESULT: OK`.

While it runs, it prints one line per volume, e.g. `[ark 63/375] volume 63: downloaded, 120 cases`.
To check progress, run `python scripts/check_download.py` in a **second** terminal.

| What you see | Meaning | What to do |
|---|---|---|
| New `[ark N/375]` lines keep appearing | Running | Wait |
| `attempt 1: timed out` | Server is slow, the script retries by itself | Wait |
| No new line for minutes, `check_download.py` says `PROBABLY PAUSED/STOPPED` | Stopped (internet, sleep, terminal closed) | Run the download command again |
| `volume N: FAILED` | That volume failed after all retries | At the end, run the download command again |
| `SUMMARY` and `RESULT: OK` | Finished | Go to Step 7 |

Re-running is always safe: finished volumes are skipped. Do not let the laptop sleep while it runs.

### Step 7: Build the indexes on the full dataset (about 1 minute)

```
build/build_index data/cases.jsonl index
```

Expected: `cases indexed : 54359`, `distinct words : 221742`, `hits stored : 104633367`,
and three lines starting with `CHECK OK`.

**✅ You are now at the current level.** The next step is the inverted index (see Part 1).

### Everyday workflow

```
git pull                        get the latest code
cmake --build build             recompile
ctest --test-dir build          run the tests
git add <files>
git commit -m "What you changed"
git push                        (first time on a new branch: git push -u origin <branch>)
```

Git rules:

- Work on your own branch; merge into `main` when your part works and all tests pass.
- Commit often under your own name (the course needs 20+ commits, from every member).
- Write commit messages yourself and do **not** add `Co-Authored-By` lines: only the three
  team members should appear as contributors.
- Never commit `data/`, `index/`, `index_sample/` or `build/` (`.gitignore` blocks them).

### Common problems

| Problem | Fix |
|---|---|
| `can't open file ...\scripts\download_dataset.py` | Wrong folder. Run `cd DSA-Project`, or open the `DSA-Project` folder in VS Code. |
| `cmake` or `g++` is "not recognized" | `C:\msys64\ucrt64\bin` is not on `Path`, or VS Code was not restarted. |
| `g++ --version` shows 6.x | Old MinGW comes first on `Path`. Remove `C:\MinGW\bin` and restart VS Code. |
| `Error: cannot open data/cases.jsonl` | Full dataset not downloaded yet (Step 6). Use the sample. |
| Terminal looks frozen | You clicked inside it ("Select" mode). Press `Esc`. |
| A test or `CHECK` fails after `git pull` | Recompile (`cmake --build build`), then build the indexes again. |

### Project structure

```
DSA-Project/
  CMakeLists.txt          build instructions for CMake (C++17, -O2)
  .gitignore              keeps data/, index/ and build/ out of Git
  .gitattributes          keeps .jsonl files byte-for-byte identical on every machine
  scripts/
    download_dataset.py     downloads the dataset and builds data/cases.jsonl
    check_download.py       shows if the download is running, paused or finished
    test_one_volume.py      quick check that one volume can be downloaded
  include/                C++ header files
    binary_io.h             helpers to write/read numbers in binary files
    docindex.h              document index
    tokenizer.h             tokenizer
    lexicon.h               lexicon
    forward_index.h         forward index
  src/
    build_index.cpp         main program: builds and checks all index files
    docindex.cpp, tokenizer.cpp, lexicon.cpp, forward_index.cpp
  tests/
    test_tokenizer.cpp, test_lexicon.cpp, test_forward_index.cpp
  third_party/
    json.hpp                nlohmann/json 3.11.3 (MIT licence)
  sample/
    sample.jsonl            first 200 cases (in Git)
  data/                   (not in Git) downloaded zips + cases.jsonl
  index/                  (not in Git) index files of the full dataset
  index_sample/           (not in Git) index files of the sample
  build/                  (not in Git) compiled programs
```

---

# Part 3: The work done, explained

All index parts are built by **one program**, `build_index`, in **one pass** over the cases.
It reads one case at a time (the whole dataset is never in memory):

```
case (one line of cases.jsonl)
  -> document index : gets its docID, stores title, date, court, ...
  -> tokenizer      : cleans name + head matter + opinions into words
  -> lexicon        : every word gets a wordID, its counts go up
  -> forward index  : the case's words and positions are written to disk
```

At the end it checks its own output and prints one `CHECK OK` line per part.
`.bin` files are used by the program; `.txt` files show the same data as readable text.

## 1. Dataset download

**What it is:** a Python script (standard library only) that downloads the Arkansas cases
from the Caselaw Access Project and saves them as one file, `data/cases.jsonl`, with one
case per line.

**How it is done:**

- Downloads all 479 volume zips (375 Supreme Court `ark` + 104 Court of Appeals `ark-app`)
  into `data/raw/`. Already downloaded zips are skipped, and failed downloads are retried.
- Drops cases whose opinion text has fewer than 100 words.
- Saves the first 200 kept cases as `sample/sample.jsonl` (stored in Git).
- Prints a summary of what was downloaded, skipped and failed.

**Commands:**

```
python scripts/test_one_volume.py               optional: check the source works (1 volume)
python scripts/download_dataset.py --limit 2    optional: test run, 2 volumes per court
python scripts/download_dataset.py              full download
python scripts/check_download.py                progress, from a second terminal
```

**Result:**

| | |
|---|---|
| Volumes downloaded | 479 / 479 |
| Cases seen / skipped / kept | 55,102 / 743 / **54,359** |
| `data/cases.jsonl` size | 744 MB |

## 2. Document index

**What it is:** a list of all cases with their basic details. Each case gets a number, its
**docID** (0, 1, 2, ... in file order). For each docID it stores the CAP id, title, citation,
date, court, PageRank percentile, number of opinion words, and **where the case is** in
`cases.jsonl` (byte offset and length). This is the *Document Index* from the Google paper.

It is used to show search results (title, date, court), to give ranking its metadata, and
to open the full text of one case without reading the whole file.

**How it is done:** when `build_index` reads a case, it stores its details and the byte
position where the case's line starts.
Code: `include/docindex.h`, `src/docindex.cpp`.

**Commands:** built by `build_index` (Step 5 / Step 7). It must print:

```
CHECK OK: docindex loads back identically and offsets point to the right cases
```

The check reloads `docindex.bin`, compares every case, and jumps to the stored offsets of the
first, middle and last case to confirm the right case is there.

**Result:** files `docindex.bin` and `docindex.txt`. Example lines from `docindex.txt`:

```
# docID  capId    offset  length  date      court    pagerank  words  citation   title
0        8728242  0       20056   18370100  Supreme  0.9507    2447   1 Ark. 11  Goings v. Mills
1        8728246  20057   2329    18370100  Supreme  -1.0000   198    1 Ark. 19  McKee v. Murphy
```

(`date` is `YYYYMMDD`, `00` = unknown month or day; `pagerank -1` = no value.)

| | |
|---|---|
| Cases | 54,359 (48,463 Supreme Court, 5,896 Court of Appeals) |
| Cases without PageRank | 1,531 |
| `docindex.bin` / `docindex.txt` | 4.0 MB / 5.2 MB |

## 3. Tokenizer

**What it is:** cleans text into a list of words (tokens). Each word gets a position and a
flag saying whether it started with a capital letter. The same tokenizer will clean search
queries, so query words always match the words in the index.

```
Input : The appellant's motion for a new trial was DENIED. See Smith v. State (1968).
Tokens: the@0^ appellant@1 motion@2 for@3 a@4 new@5 trial@6 was@7 denied@8^
        see@9^ smith@10^ v@11 state@12^ 1968@13          (^ = started with a capital)
```

**How it is done:** these rules:

| # | Rule | Example |
|---|---|---|
| 1 | Join a word broken by a hyphen at a line end | `negli-` + new line + `gence` → `negligence` |
| 2 | Anything that is not a letter or digit separates words | `41-2203` → `41`, `2203` |
| 3 | Remember if the word started with a capital, then lowercase it | `DENIED` → `denied` (capital) |
| 4 | Remove a possessive 's (straight `'` and curly `’`) | `appellant’s` → `appellant` |
| 5 | Drop words over 30 characters and numbers over 4 digits (years are kept) | `123456` dropped, `1968` kept |
| 6 | Keep stop words (`the`, `and`, ...), because multi-word search must match all words | `data structures and algorithms` → 4 words |
| 7 | No stemming | `motions` and `motion` stay different |
| 8 | Positions continue across the name, head matter and opinions of one case | |

Code: `include/tokenizer.h`, `src/tokenizer.cpp`.

**Commands:**

```
build/test_tokenizer                      20 unit tests, must end with "20 passed, 0 failed"
build/test_tokenizer data/cases.jsonl     optional: tokenize the whole dataset, print statistics
```

**Result:** 104,633,367 tokens and 221,742 distinct words in the full dataset.

## 4. Lexicon

**What it is:** a dictionary of every distinct word. Each word gets a number, its **wordID**,
in the order words first appear. For each word it also stores:

| Count | Meaning | Example: `negligence` |
|---|---|---|
| **df** (document frequency) | in how many cases the word appears | 6,635 cases |
| **cf** (collection frequency) | how many times it appears in total | 42,941 times |

The indexes store 4-byte wordIDs instead of whole words. A search turns its words into
wordIDs here, and ranking uses df (a rare word says more about a case than "the").
This is the *Lexicon* from the Google paper.

**How it is done:** for every word of every case: a new word gets the next free wordID; cf
goes up by 1; df goes up by 1 only the first time the word is seen in that case. A hash map
(`std::unordered_map`, word → wordID) finds a word's ID in constant time on average.
Code: `include/lexicon.h`, `src/lexicon.cpp`.

**Commands:**

```
build/test_lexicon        10 unit tests, must end with "10 passed, 0 failed"
```

and `build_index` (Step 5 / Step 7) must print `CHECK OK: lexicon ...`. The check confirms
that the file loads back identically, the sum of all cf values equals the total number of
tokens, every word has 1 ≤ df ≤ cf, and every word maps back to its own wordID.

**Result:** files `lexicon.bin` and `lexicon.txt`. Example lines from `lexicon.txt`:

```
# wordID  word        df      cf
0         goings      20      24
1         against     38761   152661
1319      negligence  6635    42941
11184     theft       1767    5197
```

| | |
|---|---|
| Distinct words | 221,742 |
| Words in only one case | 120,502 (mostly names, typos, scanning errors) |
| Most frequent words | the (8.9 million times), of, to, and, in, that, a, was, for, is |
| `lexicon.bin` / `lexicon.txt` | 3.8 MB / 4.3 MB |

## 5. Forward index

**What it is:** for every case, which words it contains and where. For each docID it stores
the number of words in the case and a list of `wordID → hits`. A **hit** is one occurrence of
a word. Example from `forward.txt`:

```
docID 0  tokens 3377  words 639  Goings v. Mills
  0   goings    4    0^T 3^ 1416^ 3366^
  1   against   5    1T 4 757 950 1914
  7   court     70   10^ 135^ 153 220^ ...
```

The `goings` line means: wordID 0, 4 hits, at positions 0, 3, 1416 and 3366
(`^` = capital, `T` = in the case title). Each hit is packed into 4 bytes:

| Bits | Meaning |
|---|---|
| 31 ... 2 | position of the word in the case |
| 1 | title flag: the word is in the case name (counts more in ranking) |
| 0 | capital flag: the word started with a capital letter |

It is the input for the inverted index, and its positions allow searching for words close
together. This is the *Forward Index* from the Google paper.

**How it is done:** for each case, every token gets its wordID; the tokens are sorted by
wordID (this groups each word's hits together, in position order); and the case's record is
**written to `forward.bin` immediately**, with its start position saved in `forward_off.bin`.
So only one case is in memory at a time, and any case can be read directly.
Code: `include/forward_index.h`, `src/forward_index.cpp`.

**Commands:**

```
build/test_forward_index    19 unit tests, must end with "19 passed, 0 failed"
```

and `build_index` (Step 5 / Step 7) must print `CHECK OK: forward index ...`. The check
confirms that every word of every case is stored exactly once, lists are sorted, df and cf
counted from the forward index equal the lexicon for every word, and the first, middle and
last records match their case text tokenized again.

**Result:** files `forward.bin` (all cases), `forward_off.bin` (docID → record position) and
`forward.txt` (first 20 cases only; all cases as text would be over 1 GB).

Layout of `forward.bin` (4-byte little-endian numbers):

```
["FWDI"] [number of cases]
for every case:  [docID] [numTokens] [numWords]
                 numWords times: [wordID] [numHits] [hit] [hit] ...
```

| | |
|---|---|
| Hits stored | 104,633,367 (= total tokens) |
| `forward.bin` / `forward_off.bin` | 649 MB / 435 KB |
| Full build time (all parts together) | about 45-90 s, depending on the laptop |

---

## Data source and licences

- Case data: [Caselaw Access Project](https://case.law/) (Harvard Law School Library),
  reporters [ark](https://static.case.law/ark/) and [ark-app](https://static.case.law/ark-app/).
  Released under **CC0 1.0 Universal** (public domain). Not redistributed here (only the
  200-case sample is included).
- [nlohmann/json](https://github.com/nlohmann/json) 3.11.3, MIT licence
  (licence text at the top of `third_party/json.hpp`).
