# Caselaw Search Engine

CS-250 Data Structures and Algorithms semester project (NUST SEECS, BESE-2025 A).

A search engine based on the paper *The Anatomy of a Large-Scale Hypertextual Web
Search Engine* (Brin and Page, 1998), built over 54,359 Arkansas court opinions
from the Caselaw Access Project. The core engine is written in C++17; all index data is
stored in files (no database).

**Team:** M. Aizaz Wajid (548772), Umar Hayat Ali (561900), Tahreem Zafar (547588)

## Quick start for team members

Follow these steps once, in order, to get from a fresh clone to the current state of the
project. All commands are typed in the VS Code terminal (PowerShell) unless stated otherwise.

### Progress so far

| Step | Status |
|---|---|
| Dataset download (54,359 cases) | ✅ Done |
| Document index | ✅ Done |
| Tokenizer | ✅ Done |
| Lexicon | ✅ Done |
| Forward index | ⏳ Next |
| Inverted index, checks | ⏳ To do (Part 1, due 29 Nov) |
| Barrels | ⏳ To do (due 6 Dec) |
| Search, ranking, autocomplete, semantic search, document addition, UI, deployment | ⏳ To do (due 16 Dec) |

### Step 1: Install the tools (one-time, about 20 minutes)

- **Git** and **Python 3.10 or newer** (check with `git --version` and `python --version`).
- **C++ tools (g++, CMake, Ninja):** follow
  [Installing the C++ tools on Windows](#installing-the-c-tools-on-windows-one-time) below.
  Do not use an old MinGW compiler (g++ 6.x); it does not support what we need.

### Step 2: Get the code

```
git clone https://github.com/UmarHayatAli/DSA-Project.git
cd DSA-Project
```

**Open the `DSA-Project` folder itself in VS Code** (File, Open Folder), so that every new
terminal starts in the right place. All commands below are run from this folder.

To work on your own changes, make your own branch (use your name), for example:

```
git switch -c AizazWajid
```

### Step 3: Compile the C++ code

```
cmake -S . -B build -G Ninja
cmake --build build
```

Expected: no errors, and the programs `build/build_index.exe`, `build/test_tokenizer.exe`
and `build/test_lexicon.exe` exist.

### Step 4: Run the unit tests

```
ctest --test-dir build
```

Expected: `100% tests passed`.

### Step 5: Build the indexes on the 200-case sample (works immediately)

The sample (`sample/sample.jsonl`) is already in the repository, so you can start working
without downloading anything:

```
build/build_index sample/sample.jsonl index_sample
```

Expected: two lines starting with `CHECK OK`, and the folder `index_sample/` now contains
`docindex.bin/.txt` and `lexicon.bin/.txt`. Open the `.txt` files in VS Code to look at them.

**For writing and testing code, the sample is enough.** The full dataset is only needed for
final testing and for measuring speed and memory.

### Step 6: Download the full dataset (one-time, about 30-45 minutes)

```
python scripts/download_dataset.py
```

Expected at the end: `KEPT (in cases.jsonl) ... 54,359` and `RESULT: OK`.
If it stops or some volumes fail, run the same command again; it continues where it
stopped. Details: [Getting the dataset](#getting-the-dataset).

### Step 7: Build the indexes on the full dataset

```
build/build_index data/cases.jsonl index
```

Expected (about 25 seconds): `cases indexed : 54359`, `distinct words : 221742`, and two
lines starting with `CHECK OK`.

**You are now at the current state of the project.**

### Everyday workflow

```
git pull                        # get the latest code from GitHub
cmake --build build             # recompile after pulling or changing code
ctest --test-dir build          # run the tests
git add <files>                 # stage your changes
git commit -m "What you changed"
git push                        # first time on a new branch: git push -u origin <branch>
```

Never commit `data/`, `index/`, `index_sample/` or `build/` (`.gitignore` already blocks them).

### Common problems

| Problem | Cause and fix |
|---|---|
| `can't open file ...\scripts\download_dataset.py` | The terminal is in the wrong folder. Run `cd DSA-Project` first, or open the `DSA-Project` folder in VS Code. |
| `cmake` or `g++` is "not recognized" | The C++ tools are not on `Path`, or VS Code was not restarted after adding them. |
| `g++ --version` shows 6.x | An old MinGW is found first on `Path`. Remove `C:\MinGW\bin` from `Path` or move `C:\msys64\ucrt64\bin` above it, then restart VS Code. |
| `Error: cannot open data/cases.jsonl` | The full dataset is not downloaded yet (Step 6). Use the sample instead. |

## Project structure

```
DSA-Project/
  CMakeLists.txt        build instructions for CMake (C++17, -O2)
  README.md             this file
  .gitignore            keeps data/, index/ and build/ out of Git
  .gitattributes        keeps .jsonl files byte-for-byte identical on every machine
  scripts/
    download_dataset.py   downloads the dataset and builds data/cases.jsonl
    test_one_volume.py    quick check that one volume can be downloaded and read
  include/              our C++ header files (.h)
    binary_io.h           helpers to write/read numbers in binary files
    docindex.h            document index: what is stored for each case
    tokenizer.h           tokenizer: the text-cleaning rules
    lexicon.h             lexicon: word -> wordID, df, cf
  src/
    build_index.cpp       main program: reads cases.jsonl and builds the index files
    docindex.cpp          document index: build, save, load
    tokenizer.cpp         tokenizer: turns text into clean words
    lexicon.cpp           lexicon: build, save, load, look up words
  tests/
    test_tokenizer.cpp    unit tests for the tokenizer
    test_lexicon.cpp      unit tests for the lexicon
  third_party/
    json.hpp              nlohmann/json 3.11.3 (MIT licence), JSON parser
  sample/
    sample.jsonl          first 200 cases, small test data (committed)
  data/                 (not in Git) downloaded zips + cases.jsonl, full dataset
  index/                (not in Git) index files built from the full dataset
  index_sample/         (not in Git) index files built from the 200-case sample
  build/                (not in Git) compiled programs
```

## Requirements

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
3. Add `C:\msys64\ucrt64\bin` to your `Path` environment variable (Start, search
   "Edit the system environment variables", Environment Variables, Path). If an older
   compiler folder such as `C:\MinGW\bin` is listed, remove it or move the new folder above it.
4. Restart VS Code, then check in a new terminal:
   ```
   g++ --version      (should show 16.x)
   cmake --version    (should show 4.x)
   ```

## Getting the dataset

The dataset is **not** stored in this repository (about 750 MB). Download it with the
script below. Run all commands from the project folder (`DSA-Project`).

### 1. (Optional) quick check that the data source works

```
python scripts/test_one_volume.py
```

Downloads one volume (`ark-app` volume 1) and prints the first case
(expected: *Tolley v. State*, 1981-02-25, Arkansas Court of Appeals).

### 2. Test the download script on a few volumes

```
python scripts/download_dataset.py --limit 2
```

Downloads only the first 2 volumes of each reporter (about 260 cases, under a minute).

### 3. Download the full dataset

```
python scripts/download_dataset.py
```

- Downloads all 479 volume zips (375 `ark` + 104 `ark-app`), roughly 30-45 minutes.
- Drops cases whose judgment (opinion) text has fewer than 100 words.
- At the end it prints a summary table: volumes downloaded / skipped / failed and
  cases kept / skipped. Expected result: **54,359 cases kept, 743 skipped**.
- If the download stops or some volumes fail, run the same command again. Zips that
  are already downloaded are skipped, so it continues where it left off.

### Files produced

| Path | Content | In Git? |
|---|---|---|
| `data/raw/ark/<N>.zip`, `data/raw/ark-app/<N>.zip` | Downloaded volume zips (cache) | No |
| `data/cases.jsonl` | All kept cases, one JSON object per line | No |
| `sample/sample.jsonl` | First 200 kept cases, small test data | Yes |

## Building the C++ code

The C++ code is like a recipe: it has to be compiled into a program before it can run.
Run these from the project folder:

```
cmake -S . -B build -G Ninja      # Step 1: set up the build (only the first time)
cmake --build build               # Step 2: compile (again after every code change)
```

This creates the program `build/build_index.exe`.

## Document index

### What it is

The document index is a **list of all cases with their basic details**, one line per case.
Each case gets a number (its **docID**: 0, 1, 2, ...), and for each docID we store:

- the case's CAP id, title (e.g. *Goings v. Mills*) and citation (e.g. *1 Ark. 11*)
- decision date, court (Supreme Court / Court of Appeals) and PageRank percentile
- number of words in the opinion text
- **where the case is stored** in `cases.jsonl` (byte offset and length)

It is used to:

1. refer to cases by a short number in the other indexes,
2. show the title, date and court of each search result,
3. give the ranking its metadata (date, court, PageRank),
4. open the full text of a case directly, without reading the whole file.

This is the *Document Index* from the Google paper. Code: `include/docindex.h`,
`src/docindex.cpp`, used by `src/build_index.cpp`.

### How to build it

The index files are **not in Git**, so everyone creates them on their own computer.
`build_index` builds the document index and the lexicon together in one run.

1. Make sure the program is compiled (see *Building the C++ code* above).
2. Build the index for the **200-case sample** (under 1 second):
   ```
   build/build_index sample/sample.jsonl index_sample
   ```
3. Build the index for the **full dataset** (about 25 seconds; needs `data/cases.jsonl`,
   see *Getting the dataset*):
   ```
   build/build_index data/cases.jsonl index
   ```
4. Check the output. Under *Checks*, the program must print:
   ```
   CHECK OK: docindex loads back identically and offsets point to the right cases
   ```
   Expected for the full dataset: `cases indexed : 54359`.

### Files created

| File | Content |
|---|---|
| `docindex.bin` | Binary version, used by the search engine (about 4 MB for the full dataset) |
| `docindex.txt` | Same data as readable text, one line per case; open it in VS Code to check by eye |

Example lines from `docindex.txt`:

```
# docID  capId    offset  length  date      court    pagerank  words  citation   title
0        8728242  0       20056   18370100  Supreme  0.9507    2447   1 Ark. 11  Goings v. Mills
1        8728246  20057   2329    18370100  Supreme  -1.0000   198    1 Ark. 19  McKee v. Murphy
```

- `date` is stored as `YYYYMMDD`; missing month or day is `00` (e.g. `1837-01` becomes `18370100`).
- `pagerank` is `-1` when the case has no PageRank value (1,531 cases).

### Results on the full dataset

| | |
|---|---|
| Cases indexed | 54,359 (48,463 Supreme Court, 5,896 Court of Appeals) |
| Build time | about 12 s |
| `docindex.bin` size | 4.0 MB |
| `docindex.txt` size | 5.2 MB |

## Tokenizer

### What it is

The tokenizer **cleans text into a list of words** (tokens). Every word gets a position
number and a flag saying whether it started with a capital letter. The lexicon and the
indexes are built from these words, and the **same tokenizer cleans the user's search
query**, so a query word always matches the same word in the cases.

Example:

```
Input : The appellant's motion for a new trial was DENIED. See Smith v. State (1968).
Tokens: the@0^ appellant@1 motion@2 for@3 a@4 new@5 trial@6 was@7 denied@8^
        see@9^ smith@10^ v@11 state@12^ 1968@13          (^ = started with a capital)
```

### Cleaning rules

| # | Rule | Example |
|---|---|---|
| 1 | Join a word broken by a hyphen at a line end | `negli-` + new line + `gence` → `negligence` |
| 2 | Anything that is not a letter or digit separates words (punctuation, quotes, dashes, §) | `41-2203` → `41`, `2203` |
| 3 | Remember if the word started with a capital, then lowercase it | `DENIED` → `denied` (capital) |
| 4 | Remove a possessive 's (straight `'` and curly `’` apostrophe) | `appellant’s` → `appellant` |
| 5 | Drop words over 30 characters and numbers over 4 digits (keep years) | `123456` dropped, `1968` kept |
| 6 | Keep stop words (`the`, `and`, `of`, ...), because multi-word search must match all words | `data structures and algorithms` → 4 words |
| 7 | No stemming | `motions` and `motion` stay different |
| 8 | Positions continue across the name, head matter and opinions of one case | |

Code: `include/tokenizer.h` (rules and interface), `src/tokenizer.cpp` (implementation).

### How to test it

1. Compile (see *Building the C++ code*).
2. Run the unit tests (20 tests, one per rule and edge case):
   ```
   build/test_tokenizer
   ```
   It must end with `20 passed, 0 failed`. (Alternatively: `ctest --test-dir build`.)
3. Optional: also tokenize a whole data file and print statistics:
   ```
   build/test_tokenizer sample/sample.jsonl
   build/test_tokenizer data/cases.jsonl
   ```

### Results on the full dataset

| | |
|---|---|
| Cases tokenized | 54,359 (name + head matter + all opinions) |
| Total tokens | 104,633,367 |
| Distinct words | 221,742 (this will be the size of the lexicon) |
| Time | about 21 s |

## Lexicon

### What it is

The lexicon is a **dictionary of every distinct word** in all the cases. Each word gets its
own number, the **wordID**:

```
goings  -> 0
against -> 1
mills   -> 2
...
negligence -> 1319
```

For each word it also stores two counts:

| Count | Meaning | Example: `negligence` |
|---|---|---|
| **df** (document frequency) | in how many cases the word appears | 6,635 cases |
| **cf** (collection frequency) | how many times it appears in all cases together | 42,941 times |

### Why we need it

1. **Short numbers instead of words.** The forward and inverted indexes store wordIDs
   (4 bytes each) instead of repeating words like "negligence" millions of times.
2. **Turning a search into numbers.** When a user searches "negligence", the engine looks the
   word up in the lexicon, gets wordID 1319, and then finds that word's list of cases in
   the inverted index. A word that is not in the lexicon appears in no case.
3. **Ranking.** df shows how rare a word is: a rare word (small df) like "negligence" says
   more about a case than a common word like "the" (in every case).

This is the *Lexicon* from the Google paper.

### How it is built

`build_index` reads `cases.jsonl` once. For each case it cleans the name, head matter and
opinions with the tokenizer, and for every word:

- if the word is new, it gets the next free wordID (so wordIDs follow the order in which
  words first appear: the first word of the first case is 0);
- its cf goes up by 1;
- its df goes up by 1 only the first time the word is seen in that case.

A **hash map** (`std::unordered_map`, word → wordID) finds a word's ID in constant time on
average, so building stays fast even with 221,742 words.

Code: `include/lexicon.h`, `src/lexicon.cpp`, used by `src/build_index.cpp`.

### How to build and test it

The lexicon is built together with the document index by the same command:

1. Compile (see *Building the C++ code*).
2. Run the lexicon unit tests (10 tests):
   ```
   build/test_lexicon
   ```
   It must end with `10 passed, 0 failed`. (`ctest --test-dir build` runs all tests.)
3. Build the indexes:
   ```
   build/build_index sample/sample.jsonl index_sample     # 200 cases, under 1 second
   build/build_index data/cases.jsonl index               # full dataset, about 25 seconds
   ```
4. The output must end with two lines starting with `CHECK OK` (document index and lexicon).
   The lexicon check confirms that:
   - the saved file loads back with exactly the same words and counts,
   - the sum of all cf values equals the total number of words in all cases,
   - every word has 1 ≤ df ≤ cf, and df is never more than the number of cases,
   - looking up each word gives back its own wordID (no word appears twice).

### Files created

| File | Content |
|---|---|
| `lexicon.bin` | Binary version, used by the search engine (3.8 MB for the full dataset) |
| `lexicon.txt` | Same data as readable text, one line per word |

Example lines from `lexicon.txt` (full dataset):

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
| Total words (tokens) | 104,633,367 |
| Words that appear in only one case | 120,502 (mostly names, typos and OCR errors) |
| Most frequent words | the (8.9 million times), of, to, and, in, that, a, was, for, is |
| Build time (document index + lexicon) | about 23 s |
| `lexicon.bin` size | 3.8 MB |

## Data source and licences

- Case data: [Caselaw Access Project](https://case.law/) (Harvard Law School Library),
  reporters [ark](https://static.case.law/ark/) and [ark-app](https://static.case.law/ark-app/).
  Released under **CC0 1.0 Universal** (public domain). Credited here and not redistributed
  in this repository (only the 200-case sample is included).
- [nlohmann/json](https://github.com/nlohmann/json) 3.11.3, MIT licence
  (licence text is at the top of `third_party/json.hpp`).
