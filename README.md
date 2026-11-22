# Caselaw Search Engine

CS-250 Data Structures and Algorithms semester project (NUST SEECS, BESE-2025 A).

A search engine based on the paper *The Anatomy of a Large-Scale Hypertextual Web
Search Engine* (Brin and Page, 1998), built over 54,359 Arkansas court opinions
from the Caselaw Access Project. The core engine is written in C++17; all index data is
stored in files (no database).

**Team:** M. Aizaz Wajid (548772), Umar Hayat Ali (561900), Tahreem Zafar (547588)

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
  src/
    build_index.cpp       main program: reads cases.jsonl and builds the index files
    docindex.cpp          document index: build, save, load
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

1. Make sure the program is compiled (see *Building the C++ code* above).
2. Build the index for the **200-case sample** (under 1 second):
   ```
   build/build_index sample/sample.jsonl index_sample
   ```
3. Build the index for the **full dataset** (about 12 seconds; needs `data/cases.jsonl`,
   see *Getting the dataset*):
   ```
   build/build_index data/cases.jsonl index
   ```
4. Check the output. The program prints a summary and must end with:
   ```
   CHECK OK: saved file loads back identically and offsets point to the right cases
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

## Data source and licences

- Case data: [Caselaw Access Project](https://case.law/) (Harvard Law School Library),
  reporters [ark](https://static.case.law/ark/) and [ark-app](https://static.case.law/ark-app/).
  Released under **CC0 1.0 Universal** (public domain). Credited here and not redistributed
  in this repository (only the 200-case sample is included).
- [nlohmann/json](https://github.com/nlohmann/json) 3.11.3, MIT licence
  (licence text is at the top of `third_party/json.hpp`).
