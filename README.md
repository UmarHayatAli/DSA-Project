# Caselaw Search Engine

CS-250 Data Structures and Algorithms semester project (NUST SEECS, BESE-2025 A).

A search engine based on the paper *The Anatomy of a Large-Scale Hypertextual Web
Search Engine* (Brin and Page, 1998). It searches 54,359 Arkansas court cases from the
Caselaw Access Project. The core engine is written in C++17, and all index data is stored
in files (no database).

**Team:** M. Aizaz Wajid (548772), Umar Hayat Ali (561900), Tahreem Zafar (547588)

## Contents

1. [Project status: what is done, what is next](#1-project-status-what-is-done-what-is-next)
2. [Getting started: reach the current state in 7 steps](#2-getting-started-reach-the-current-state-in-7-steps)
3. [Next task: inverted index](#3-next-task-inverted-index)
4. [Everyday workflow and Git rules](#4-everyday-workflow-and-git-rules)
5. [Common problems](#5-common-problems)
6. [Project structure](#6-project-structure)
7. [Requirements and installing the tools](#7-requirements-and-installing-the-tools)
8. [Getting the dataset](#8-getting-the-dataset)
9. [How the indexes are built (overview)](#9-how-the-indexes-are-built-overview)
10. [Document index](#10-document-index)
11. [Tokenizer](#11-tokenizer)
12. [Lexicon](#12-lexicon)
13. [Forward index](#13-forward-index)
14. [Data source and licences](#14-data-source-and-licences)

---

## 1. Project status: what is done, what is next

### Done ✅

| # | Part | What it does | Code |
|---|---|---|---|
| 1 | Dataset download | Downloads all cases and keeps those with at least 100 opinion words (54,359 cases) | `scripts/download_dataset.py` |
| 2 | Sample data | First 200 cases, stored in Git so everyone can test immediately | `sample/sample.jsonl` |
| 3 | Document index | docID → title, citation, date, court, PageRank, where the case is in the file | `docindex.h/.cpp` |
| 4 | Tokenizer | Cleans text into words (also used for search queries) | `tokenizer.h/.cpp` |
| 5 | Lexicon | word → wordID, plus df and cf counts | `lexicon.h/.cpp` |
| 6 | Forward index | docID → the words of the case and their positions (hits) | `forward_index.h/.cpp` |

All of these are built by one program, `build_index`, and have unit tests and self-checks.

### Next ⏳

| # | Part | Deadline |
|---|---|---|
| 7 | **Inverted index** (wordID → cases), see [section 3](#3-next-task-inverted-index) | Part 1, **29 Nov** |
| 8 | Final checks of Part 1, README results (build time, file sizes, memory) | 29 Nov |

### Later

| Part | Requirement from the course description | Deadline |
|---|---|---|
| Barrels | Split the inverted index into barrels (groups of wordIDs) | 6 Dec |
| Single-word search | Results in under 500 ms | 16 Dec |
| Multi-word search | All words must match (also "and"), under 1.5 s for 5 words | 16 Dec |
| Ranking | By word frequency, position, and at least 2 metadata fields (date, PageRank, court) | 16 Dec |
| Autocomplete | Trie, suggestions in under 100 ms | 16 Dec |
| Semantic search | GloVe 6B 100d word vectors, our own cosine similarity, no LLMs | 16 Dec |
| Adding documents | New cases as JSON / text / from the UI, indexed in under 1 minute without stopping search | 16 Dec |
| Web UI and server | cpp-httplib backend, HTML/CSS/JS frontend | 16 Dec |
| Deployment | Docker on Render | 16 Dec |
| Limits | Whole engine uses at most 2 GB RAM; at least 20 Git commits, from every member | 16 Dec |

The viva is on **18 Dec**.

---

## 2. Getting started: reach the current state in 7 steps

Do these steps once, in order. Type all commands in the VS Code terminal (PowerShell),
from the `DSA-Project` folder.

### Step 1: Install the tools (one-time, about 20 minutes)

- **Git** and **Python 3.10 or newer**. Check with `git --version` and `python --version`.
- **C++ tools (g++, CMake, Ninja)**: follow
  [Installing the C++ tools on Windows](#installing-the-c-tools-on-windows-one-time).
  Do not use the old MinGW compiler (g++ 6.x); it does not support C++17 fully.

### Step 2: Get the code

```
git clone https://github.com/UmarHayatAli/DSA-Project.git
cd DSA-Project
```

Open **the `DSA-Project` folder itself** in VS Code (File → Open Folder), so every new
terminal starts in the right place.

If you already cloned before, just get the latest code instead:

```
git switch main
git pull
```

Then make your own branch (use your name) to work on:

```
git switch -c TahreemZafar
```

### Step 3: Compile the C++ code

```
cmake -S . -B build -G Ninja
cmake --build build
```

Expected: no errors. The folder `build/` now has `build_index.exe`, `test_tokenizer.exe`,
`test_lexicon.exe` and `test_forward_index.exe`.

### Step 4: Run the unit tests

```
ctest --test-dir build
```

Expected: `100% tests passed out of 3`.

### Step 5: Build the indexes on the 200-case sample (under 1 second)

The sample is already in the repository, so this works without downloading anything:

```
build/build_index sample/sample.jsonl index_sample
```

Expected: **three lines starting with `CHECK OK`** (document index, lexicon, forward index).
The folder `index_sample/` now holds the index files; open the `.txt` files in VS Code to
look at them.

> **For writing and testing code, the sample is enough.** The full dataset is only needed
> for final testing and for measuring speed and memory.

### Step 6: Download the full dataset (one-time, about 30-45 minutes)

```
python scripts/download_dataset.py
```

Expected at the end: `KEPT (in cases.jsonl) ... 54,359` and `RESULT: OK`.
If it stops, run the same command again; it continues where it stopped.
More details: [Getting the dataset](#8-getting-the-dataset).

### Step 7: Build the indexes on the full dataset (about 1 minute)

```
build/build_index data/cases.jsonl index
```

Expected: `cases indexed : 54359`, `distinct words : 221742`,
`hits stored : 104633367`, and three lines starting with `CHECK OK`.

**✅ You are now at the current state of the project.** The next task is described below.

---

## 3. Next task: inverted index

### What it is

The inverted index is the forward index "turned around". Instead of
*case → words*, it stores *word → cases*:

```
Forward index (done):    docID 0   -> goings, against, mills, court, ...
Inverted index (next):   "court"   -> docID 0, docID 1, docID 5, ...  (with hits)
```

This is what makes search fast: to search "negligence", the engine looks up its wordID in
the lexicon and reads only that word's list of cases from the inverted index.

### What it must do

| | |
|---|---|
| **Input** | `forward.bin` + `forward_off.bin` (already built), `lexicon.bin` |
| **Output** | For every wordID: its list of cases (docIDs, **sorted**), and for each case the number of hits and the hits themselves |
| **Lookup** | An offset table: wordID → where its list starts in the file, so one word's list can be read without reading the whole file |
| **Memory** | Must stay well under 2 GB while building (the full dataset has 104 million hits) |
| **Later** | Must be easy to split into **barrels** (groups of wordIDs) for the 6 Dec deadline |

### Where to add the code

Follow the same pattern as the parts already done:

- `include/inverted_index.h` and `src/inverted_index.cpp`: writer and reader
- `tests/test_inverted_index.cpp`: unit tests
- `src/build_index.cpp`: build it after the forward index, and add a `checkInvertedIndex`
  (for example: df of every word = length of its list, total hits = total tokens,
  docIDs in each list are sorted, a few lists match the forward index)
- `CMakeLists.txt`: add the new `.cpp` files and the new test
- This README: a new "Inverted index" section with the same structure as the sections below

---

## 4. Everyday workflow and Git rules

```
git pull                        # get the latest code from GitHub
cmake --build build             # recompile after pulling or changing code
ctest --test-dir build          # run all tests
git add <files>                 # stage your changes
git commit -m "What you changed"
git push                        # first time on a new branch: git push -u origin <branch>
```

Rules:

- **Work on your own branch** and merge into `main` when your part works and all tests pass.
- **Commit often, under your own name.** The course needs at least 20 commits, from every member.
- **Write the commit message yourself** (what you changed). Do **not** add
  `Co-Authored-By` lines: only the three team members should appear as contributors on GitHub.
- **Never commit** `data/`, `index/`, `index_sample/` or `build/` (`.gitignore` already blocks them).

---

## 5. Common problems

| Problem | Cause and fix |
|---|---|
| `can't open file ...\scripts\download_dataset.py` | The terminal is in the wrong folder. Run `cd DSA-Project` first, or open the `DSA-Project` folder in VS Code. |
| `cmake` or `g++` is "not recognized" | The C++ tools are not on `Path`, or VS Code was not restarted after adding them. |
| `g++ --version` shows 6.x | An old MinGW is found first on `Path`. Remove `C:\MinGW\bin` from `Path` or move `C:\msys64\ucrt64\bin` above it, then restart VS Code. |
| `Error: cannot open data/cases.jsonl` | The full dataset is not downloaded yet (Step 6). Use the sample instead. |
| The download seems stuck | See [Is the download running, paused or finished?](#is-the-download-running-paused-or-finished) |
| A test or `CHECK` fails after `git pull` | Recompile first (`cmake --build build`), then build the indexes again. |

---

## 6. Project structure

```
DSA-Project/
  CMakeLists.txt          build instructions for CMake (C++17, -O2)
  README.md               this file
  .gitignore              keeps data/, index/ and build/ out of Git
  .gitattributes          keeps .jsonl files byte-for-byte identical on every machine
  scripts/
    download_dataset.py     downloads the dataset and builds data/cases.jsonl
    check_download.py       shows if the download is running, paused or finished
    test_one_volume.py      quick check that one volume can be downloaded and read
  include/                our C++ header files (.h)
    binary_io.h             helpers to write/read numbers in binary files
    docindex.h              document index
    tokenizer.h             tokenizer (text-cleaning rules)
    lexicon.h               lexicon (word -> wordID, df, cf)
    forward_index.h         forward index (docID -> wordIDs + hits)
  src/
    build_index.cpp         main program: reads cases.jsonl, builds and checks all index files
    docindex.cpp            document index: build, save, load
    tokenizer.cpp           tokenizer: turns text into clean words
    lexicon.cpp             lexicon: build, save, load, look up words
    forward_index.cpp       forward index: build, write, read
  tests/
    test_tokenizer.cpp      20 unit tests
    test_lexicon.cpp        10 unit tests
    test_forward_index.cpp  19 unit tests
  third_party/
    json.hpp                nlohmann/json 3.11.3 (MIT licence), JSON parser
  sample/
    sample.jsonl            first 200 cases, small test data (in Git)
  data/                   (not in Git) downloaded zips + cases.jsonl
  index/                  (not in Git) index files of the full dataset
  index_sample/           (not in Git) index files of the 200-case sample
  build/                  (not in Git) compiled programs
```

---

## 7. Requirements and installing the tools

| Tool | Version used | What it is for |
|---|---|---|
| Python | 3.10+ (we use 3.13) | Dataset download script (standard library only) |
| g++ | 16.2 (MSYS2 UCRT64) | C++17 compiler |
| CMake | 4.5 | Build system: reads `CMakeLists.txt` and runs the compiler |
| Ninja | 1.13 | Fast build runner used by CMake |
| Git | any recent | Version control |

### Installing the C++ tools on Windows (one-time)

1. Install MSYS2 (in PowerShell): `winget install --id MSYS2.MSYS2 -e`
2. Open the **MSYS2 UCRT64** terminal from the Start menu and run:
   ```
   pacman -Syu
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-gdb
   ```
   (If `pacman -Syu` closes the window, open it again and run `pacman -Syu` once more.)
3. Add `C:\msys64\ucrt64\bin` to your `Path` environment variable (Start → search
   "Edit the system environment variables" → Environment Variables → Path). If an older
   compiler folder such as `C:\MinGW\bin` is listed, remove it or move the new folder above it.
4. Restart VS Code, then check in a new terminal:
   ```
   g++ --version      (should show 16.x)
   cmake --version    (should show 4.x)
   ```

### What CMake does

C++ code must be compiled into a program before it can run. `CMakeLists.txt` lists which
`.cpp` files make up which program; CMake reads it and runs the compiler for us:

```
cmake -S . -B build -G Ninja      # set up the build (only the first time)
cmake --build build               # compile (again after every code change)
```

---

## 8. Getting the dataset

The dataset (about 750 MB) is **not** in Git. Everyone downloads it with the script.

### Commands

| Command | What it does | Time |
|---|---|---|
| `python scripts/test_one_volume.py` | Optional: downloads one volume and prints its first case (*Tolley v. State*) to check the source works | seconds |
| `python scripts/download_dataset.py --limit 2` | Optional: test run with only 2 volumes per reporter (about 260 cases) | under 1 minute |
| `python scripts/download_dataset.py` | **Full download**: all 479 volumes (375 `ark` + 104 `ark-app`) | 30-45 minutes |
| `python scripts/check_download.py` | Shows how far the download has got (run it in a second terminal) | instant |

What the full download does:

- downloads every volume zip into `data/raw/` (zips already downloaded are skipped),
- drops cases whose opinion text has fewer than 100 words,
- writes all kept cases to `data/cases.jsonl` (one case per line),
- prints a summary. Expected: **54,359 cases kept, 743 skipped, `RESULT: OK`**.

### Is the download running, paused or finished?

While it runs, the script prints one line per volume, e.g. `[ark 63/375] volume 63: downloaded, 120 cases`
(volume 63 of 375 of the `ark` reporter is done). After `ark` comes `ark-app`, then building
`cases.jsonl` and the summary.

| What you see | Meaning | What to do |
|---|---|---|
| New `[ark N/375]` lines keep appearing | **Running** | Wait |
| `attempt 1: timed out` | The server is slow; the script retries by itself | Wait |
| No new line for several minutes; `check_download.py` says `PROBABLY PAUSED/STOPPED` | **Stopped** (internet dropped, laptop slept, terminal closed, Ctrl+C) | Run `python scripts/download_dataset.py` again |
| `volume N: FAILED` | That volume failed after all retries; the script goes on | At the end, run the script again |
| `SUMMARY` and `RESULT: OK` | **Finished** | Go on with Step 7 |

Tips:

- Do not let the laptop sleep during the download (Settings → System → Power).
- If the terminal looks frozen, you may have clicked inside it ("select mode", title starts
  with `Select`). Press `Esc` to continue.
- Re-running is always safe; it continues where it stopped.

### Files produced

| Path | Content | In Git? |
|---|---|---|
| `data/raw/ark/<N>.zip`, `data/raw/ark-app/<N>.zip` | Downloaded volume zips | No |
| `data/cases.jsonl` | All 54,359 kept cases, one JSON object per line | No |
| `sample/sample.jsonl` | First 200 kept cases, small test data | Yes |

---

## 9. How the indexes are built (overview)

One program, `build_index`, builds **all** index files in **one pass** over `cases.jsonl`.
It reads one case at a time, so the whole dataset is never in memory.

```
build/build_index <cases file> <output folder>

build/build_index sample/sample.jsonl index_sample     # 200 cases, under 1 second
build/build_index data/cases.jsonl index               # full dataset, about 1 minute
```

For every case:

```
case (one line of cases.jsonl)
  -> document index : gets its docID, stores title, date, court, ...
  -> tokenizer      : cleans name + head matter + opinions into words
  -> lexicon        : every word gets its wordID, df and cf are counted
  -> forward index  : the case's words and their positions are written to disk
```

At the end, the program **checks its own output** and prints one `CHECK OK` line per part.
If any check fails, it prints `CHECK FAILED` with the reason.

| Output file | Part | Full dataset size |
|---|---|---|
| `docindex.bin`, `docindex.txt` | Document index | 4.0 MB, 5.2 MB |
| `lexicon.bin`, `lexicon.txt` | Lexicon | 3.8 MB, 4.3 MB |
| `forward.bin`, `forward_off.bin`, `forward.txt` | Forward index | 649 MB, 435 KB, first 20 cases only |

`.bin` files are used by the program; `.txt` files show the same data as readable text,
so you can check it by eye.

Full dataset build time: **about 45-90 seconds**, depending on the laptop.

---

## 10. Document index

### What it is

A **list of all cases with their basic details**. Each case gets a number, its **docID**
(0, 1, 2, ... in file order), and for each docID we store:

- CAP id, title (e.g. *Goings v. Mills*) and citation (e.g. *1 Ark. 11*)
- decision date, court (Supreme Court / Court of Appeals) and PageRank percentile
- number of words in the opinion text
- **where the case is** in `cases.jsonl` (byte offset and length)

This is the *Document Index* from the Google paper.

### Why we need it

1. The other indexes refer to cases by their short docID.
2. It gives the title, date and court shown for each search result.
3. It gives the ranking its metadata (date, court, PageRank).
4. It opens the full text of a case directly, without reading the whole file.

### How it is built

When `build_index` reads a case, it adds its details to the document index and remembers the
byte position where the case's line starts in `cases.jsonl`.

Code: `include/docindex.h`, `src/docindex.cpp`, used by `src/build_index.cpp`.

### How to test it

Run `build_index` (see [section 9](#9-how-the-indexes-are-built-overview)). It must print:

```
CHECK OK: docindex loads back identically and offsets point to the right cases
```

The check loads `docindex.bin` back and compares every case, then jumps to the stored offsets
of the first, middle and last case in `cases.jsonl` and confirms the right case is there.

### Files created

| File | Content |
|---|---|
| `docindex.bin` | Binary version, used by the search engine |
| `docindex.txt` | The same data as text, one line per case |

Example lines from `docindex.txt`:

```
# docID  capId    offset  length  date      court    pagerank  words  citation   title
0        8728242  0       20056   18370100  Supreme  0.9507    2447   1 Ark. 11  Goings v. Mills
1        8728246  20057   2329    18370100  Supreme  -1.0000   198    1 Ark. 19  McKee v. Murphy
```

- `date` is stored as `YYYYMMDD`; a missing month or day is `00` (`1837-01` → `18370100`).
- `pagerank` is `-1` when the case has no PageRank value.

### Results on the full dataset

| | |
|---|---|
| Cases | 54,359 (48,463 Supreme Court, 5,896 Court of Appeals) |
| Cases without PageRank | 1,531 |
| `docindex.bin` size | 4.0 MB |

---

## 11. Tokenizer

### What it is

The tokenizer **cleans text into a list of words** (tokens). Every word gets a position
number and a flag saying whether it started with a capital letter.

```
Input : The appellant's motion for a new trial was DENIED. See Smith v. State (1968).
Tokens: the@0^ appellant@1 motion@2 for@3 a@4 new@5 trial@6 was@7 denied@8^
        see@9^ smith@10^ v@11 state@12^ 1968@13          (^ = started with a capital)
```

### Why we need it

The lexicon and all indexes are built from these words. The **same tokenizer also cleans
the user's search query**, so a query word always matches the same word in the cases.

### How it works: cleaning rules

| # | Rule | Example |
|---|---|---|
| 1 | Join a word broken by a hyphen at a line end | `negli-` + new line + `gence` → `negligence` |
| 2 | Anything that is not a letter or digit separates words | `41-2203` → `41`, `2203` |
| 3 | Remember if the word started with a capital, then lowercase it | `DENIED` → `denied` (capital) |
| 4 | Remove a possessive 's (straight `'` and curly `’`) | `appellant’s` → `appellant` |
| 5 | Drop words over 30 characters and numbers over 4 digits (years are kept) | `123456` dropped, `1968` kept |
| 6 | Keep stop words (`the`, `and`, `of`, ...), because multi-word search must match all words | `data structures and algorithms` → 4 words |
| 7 | No stemming | `motions` and `motion` stay different |
| 8 | Positions continue across the name, head matter and opinions of one case | |

Code: `include/tokenizer.h`, `src/tokenizer.cpp`.

### How to test it

```
build/test_tokenizer                      # 20 unit tests, must end with "20 passed, 0 failed"
build/test_tokenizer data/cases.jsonl     # optional: tokenize a whole file and print statistics
```

### Results on the full dataset

| | |
|---|---|
| Total tokens | 104,633,367 |
| Distinct words | 221,742 |

---

## 12. Lexicon

### What it is

A **dictionary of every distinct word** in all the cases. Each word gets its own number,
the **wordID**, in the order the words first appear (the first word of the first case is 0).
For each word it also stores two counts:

| Count | Meaning | Example: `negligence` |
|---|---|---|
| **df** (document frequency) | in how many cases the word appears | 6,635 cases |
| **cf** (collection frequency) | how many times it appears in all cases together | 42,941 times |

This is the *Lexicon* from the Google paper.

### Why we need it

1. **Numbers instead of words.** The indexes store 4-byte wordIDs instead of repeating
   words like "negligence" millions of times.
2. **Turning a search into numbers.** A search for "negligence" looks up wordID 1319 here,
   then reads that word's cases from the inverted index. A word not in the lexicon is in no case.
3. **Ranking.** A rare word (small df) says more about a case than a common word like "the".

### How it is built

For every word of every case:

- if the word is new, it gets the next free wordID,
- its cf goes up by 1,
- its df goes up by 1 only the first time the word is seen in that case.

A **hash map** (`std::unordered_map`, word → wordID) finds a word's ID in constant time on average.

Code: `include/lexicon.h`, `src/lexicon.cpp`, used by `src/build_index.cpp`.

### How to test it

```
build/test_lexicon        # 10 unit tests, must end with "10 passed, 0 failed"
```

Then run `build_index` (see [section 9](#9-how-the-indexes-are-built-overview)). It must print
`CHECK OK: lexicon ...`. The check confirms that:

- the saved file loads back with exactly the same words and counts,
- the sum of all cf values equals the total number of words in all cases,
- every word has 1 ≤ df ≤ cf, and df is never more than the number of cases,
- looking up each word gives back its own wordID (no word appears twice).

### Files created

| File | Content |
|---|---|
| `lexicon.bin` | Binary version, used by the search engine |
| `lexicon.txt` | The same data as text, one line per word |

Example lines from `lexicon.txt`:

```
# wordID  word        df      cf
0         goings      20      24
1         against     38761   152661
1278      contract    15104   128554
1319      negligence  6635    42941
11184     theft       1767    5197
```

### Results on the full dataset

| | |
|---|---|
| Distinct words | 221,742 |
| Words that appear in only one case | 120,502 (mostly names, typos and scanning errors) |
| Most frequent words | the (8.9 million times), of, to, and, in, that, a, was, for, is |
| `lexicon.bin` size | 3.8 MB |

---

## 13. Forward index

### What it is

For **every case, which words it contains and where**. For each docID it stores the number
of words in the case, and a list of `wordID → hits`, sorted by wordID. A **hit** is one
occurrence of a word. Example from `forward.txt`:

```
docID 0  tokens 3377  words 639  Goings v. Mills
  0   goings    4    0^T 3^ 1416^ 3366^
  1   against   5    1T 4 757 950 1914
  7   court     70   10^ 135^ 153 220^ ...
```

Read the line for `goings` as: wordID 0, the word "goings", 4 hits, at positions 0, 3, 1416
and 3366. `^` means it started with a capital; `T` means it is in the case title.

Each hit is packed into 4 bytes:

| Bits | Meaning |
|---|---|
| 31 ... 2 | **position** of the word in the case (0, 1, 2, ...) |
| 1 | **title** flag (`T`): the word is in the case name |
| 0 | **capital** flag (`^`): the word started with a capital letter |

This is the *Forward Index* from the Google paper.

### Why we need it

1. **It is the input for the inverted index**, which is made by "turning it around".
2. **Positions** allow searching for words that appear close together.
3. **Title hits** count more in ranking (like the "fancy hits" in the Google paper).
4. **The number of words per case** is used in ranking (long cases contain more words by chance).

### How it is built

For each case, in the same pass as the other parts:

1. every token gets its wordID from the lexicon,
2. the tokens are sorted by wordID, which groups all hits of a word together, in position order,
3. the case's record is **written to `forward.bin` straight away**, and its start position is
   saved in `forward_off.bin`.

So only one case is in memory at a time (the full index is 649 MB), and
`forward_off.bin` lets the program jump directly to any case.

Code: `include/forward_index.h`, `src/forward_index.cpp`, used by `src/build_index.cpp`.

### How to test it

```
build/test_forward_index    # 19 unit tests, must end with "19 passed, 0 failed"
```

Then run `build_index` (see [section 9](#9-how-the-indexes-are-built-overview)). It must print
`CHECK OK: forward index ...`. The check confirms that:

- every word of every case is stored exactly once (no word lost, none twice),
- wordIDs in each case are sorted, and each hit list is sorted by position,
- counting from the forward index gives the same df and cf as the lexicon, for every word,
- for the first, middle and last case, the stored record matches the original case text
  tokenized again.

### Files created

| File | Content |
|---|---|
| `forward.bin` | Binary forward index, one record per case, in docID order |
| `forward_off.bin` | docID → where its record starts in `forward.bin` |
| `forward.txt` | Readable text for the **first 20 cases only** (all cases as text would be over 1 GB) |

Layout of `forward.bin` (all numbers 4-byte little-endian):

```
["FWDI"] [number of cases]
for every case:  [docID] [numTokens] [numWords]
                 numWords times: [wordID] [numHits] [hit] [hit] ...
```

### Results on the full dataset

| | |
|---|---|
| Hits stored | 104,633,367 (= total tokens) |
| `forward.bin` size | 649 MB |
| `forward_off.bin` size | 435 KB |

---

## 14. Data source and licences

- Case data: [Caselaw Access Project](https://case.law/) (Harvard Law School Library),
  reporters [ark](https://static.case.law/ark/) and [ark-app](https://static.case.law/ark-app/).
  Released under **CC0 1.0 Universal** (public domain). Credited here and not redistributed
  in this repository (only the 200-case sample is included).
- [nlohmann/json](https://github.com/nlohmann/json) 3.11.3, MIT licence
  (licence text is at the top of `third_party/json.hpp`).
