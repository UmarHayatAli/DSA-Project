# Caselaw Search Engine

CS-250 Data Structures and Algorithms semester project (NUST SEECS, BESE-2025 A).

A search engine based on the paper *The Anatomy of a Large-Scale Hypertextual Web
Search Engine* (Brin and Page, 1998), built over about 54,359 Arkansas court opinions
from the Caselaw Access Project. The core engine is written in C++17; all index data is
stored in files.

**Team:** M. Aizaz Wajid (548772), Umar Hayat Ali (561900), Tahreem Zafar (547588)

## Requirements

- Python 3.10 or newer (standard library only, nothing to install)
- Internet connection for the dataset download

## Getting the dataset

The dataset is **not** stored in this repository (it is about 1 GB). Download it with the
script below. Run all commands from the project folder.

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
  cases kept / skipped. Expected result: **about 54,359 cases kept, 743 skipped**.
- If the download stops or some volumes fail, run the same command again. Zips that
  are already downloaded are skipped, so it continues where it left off.

### Files produced

| Path | Content | In Git? |
|---|---|---|
| `data/raw/ark/<N>.zip`, `data/raw/ark-app/<N>.zip` | Downloaded volume zips (cache) | No |
| `data/cases.jsonl` | All kept cases, one JSON object per line | No |
| `sample/sample.jsonl` | First 200 kept cases, small test data | Yes |

## Data source and licence

Case data comes from the [Caselaw Access Project](https://case.law/) (Harvard Law School
Library), reporters [ark](https://static.case.law/ark/) and
[ark-app](https://static.case.law/ark-app/). The data is released under
**CC0 1.0 Universal** (public domain). It is credited here and is not redistributed in
this repository.
