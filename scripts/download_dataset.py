"""
download_dataset.py
-------------------
Downloads the Caselaw Access Project (CAP) data for Arkansas and turns it
into one file that our C++ indexer can read line by line.

Two reporters (a "reporter" is a series of law books):
    ark      -> Arkansas Supreme Court     (375 volumes)
    ark-app  -> Arkansas Court of Appeals  (104 volumes)

Each volume is one zip file, e.g. https://static.case.law/ark/1.zip
Inside the zip, every case is one JSON file in the folder "json/".

What the script does:
    STEP 1  Get the list of volumes for each reporter (VolumesMetadata.json).
    STEP 2  Download every volume zip into data/raw/<reporter>/<N>.zip
            (skips zips that are already downloaded, so it can be re-run).
    STEP 3  Read the cases out of the saved zips, drop cases whose judgment
            (opinion) text has fewer than 100 words, and write the rest to:
                data/cases.jsonl      (all kept cases, one JSON per line)
                sample/sample.jsonl   (first 200 kept cases, for testing)
    STEP 4  Print a summary.

Usage (run from the project folder):
    python scripts/download_dataset.py             # full dataset (~479 zips)
    python scripts/download_dataset.py --limit 2   # test: first 2 volumes of each reporter

Only the Python standard library is used.
"""

import argparse
import io
import json
import os
import time
import urllib.error
import urllib.request
import zipfile

# ---------------------------------------------------------------
# Settings
# ---------------------------------------------------------------

BASE_URL = "https://static.case.law"
REPORTERS = ["ark", "ark-app"]                     # order matters: ark first, then ark-app
FALLBACK_VOLUME_COUNT = {"ark": 375, "ark-app": 104}

# The server refuses Python's default name (HTTP 403), so we pretend to be a browser.
HEADERS = {"User-Agent": "Mozilla/5.0"}
TIMEOUT_SECONDS = 60
MAX_ATTEMPTS = 3              # try each download up to 3 times
RETRY_WAIT_SECONDS = 5        # wait between failed attempts
POLITE_WAIT_SECONDS = 1       # wait between downloads so we don't overload the server

MIN_WORDS = 100               # cases shorter than this are dropped
SAMPLE_SIZE = 200             # number of cases written to sample/sample.jsonl
EXPECTED_KEPT = 54359         # what we expect for the full dataset (from our proposal)

# All paths are relative to the project folder (the folder above scripts/).
PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW_DIR = os.path.join(PROJECT_DIR, "data", "raw")
CASES_FILE = os.path.join(PROJECT_DIR, "data", "cases.jsonl")
SAMPLE_FILE = os.path.join(PROJECT_DIR, "sample", "sample.jsonl")


# ---------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------

def fetch_bytes(url):
    """Download a URL and return its content as bytes (raises an error on failure)."""
    request = urllib.request.Request(url, headers=HEADERS)
    with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as response:
        return response.read()


def volume_sort_key(volume):
    """Sort volumes as numbers ("2" before "10"). Non-numeric names go last."""
    if volume.isdigit():
        return (0, int(volume), volume)
    return (1, 0, volume)


def is_valid_zip(path):
    """True if the file exists and is a complete, readable zip."""
    if not os.path.exists(path):
        return False
    try:
        with zipfile.ZipFile(path) as z:
            return z.testzip() is None      # testzip() returns None when nothing is broken
    except (zipfile.BadZipFile, OSError):
        return False


def case_entries(zip_file):
    """Names of the case files inside a volume zip, in sorted order."""
    names = [n for n in zip_file.namelist() if n.startswith("json/") and n.endswith(".json")]
    return sorted(names)


def opinion_word_count(case):
    """
    Number of words in all opinions (judgment texts) of a case.
    A word = a piece of text between spaces/newlines. Missing parts count as 0.
    """
    casebody = case.get("casebody") or {}
    opinions = casebody.get("opinions") or []
    return sum(len((opinion.get("text") or "").split()) for opinion in opinions)


def new_stats():
    """Counters we keep for each reporter, printed in the final summary."""
    return {
        "volumes_listed": 0,
        "downloaded_now": 0,      # downloaded during this run
        "already_had": 0,         # valid zip was already on disk, so skipped
        "failed": [],             # volume numbers that could not be downloaded
        "cases_seen": 0,
        "cases_kept": 0,
        "cases_short": 0,         # dropped: opinion text has fewer than MIN_WORDS words
        "cases_broken": 0,        # dropped: the JSON file could not be read
    }


# ---------------------------------------------------------------
# STEP 1: list of volumes
# ---------------------------------------------------------------

def get_volume_list(reporter):
    """
    Read VolumesMetadata.json to find which volumes exist.
    If that fails, fall back to 1..N (missing ones will just fail with 404 later).
    """
    url = f"{BASE_URL}/{reporter}/VolumesMetadata.json"
    try:
        metadata = json.loads(fetch_bytes(url))
        volumes = [str(v["volume_folder"]) for v in metadata]
        print(f"  {reporter}: {len(volumes)} volumes listed in VolumesMetadata.json")
    except Exception as error:
        count = FALLBACK_VOLUME_COUNT[reporter]
        print(f"  {reporter}: could not read VolumesMetadata.json ({error}); assuming volumes 1..{count}")
        volumes = [str(n) for n in range(1, count + 1)]
    return sorted(volumes, key=volume_sort_key)


# ---------------------------------------------------------------
# STEP 2: download the zips
# ---------------------------------------------------------------

def download_volume(reporter, volume, zip_path):
    """
    Download one volume zip to zip_path.
    Returns "already had", "downloaded" or "FAILED".
    """
    if is_valid_zip(zip_path):
        return "already had"

    url = f"{BASE_URL}/{reporter}/{volume}.zip"
    temp_path = zip_path + ".part"      # write here first, rename only when complete

    for attempt in range(1, MAX_ATTEMPTS + 1):
        try:
            data = fetch_bytes(url)
            zipfile.ZipFile(io.BytesIO(data))           # raises BadZipFile if the data is broken
            with open(temp_path, "wb") as f:
                f.write(data)
            os.replace(temp_path, zip_path)             # the rename makes the zip appear "all at once"
            return "downloaded"
        except urllib.error.HTTPError as error:
            print(f"    attempt {attempt}: HTTP {error.code} {error.reason}")
            if error.code == 404:                       # the volume does not exist, retrying won't help
                break
        except Exception as error:                      # timeout, network error, broken zip ...
            print(f"    attempt {attempt}: {error}")
        if attempt < MAX_ATTEMPTS:
            time.sleep(RETRY_WAIT_SECONDS)

    if os.path.exists(temp_path):
        os.remove(temp_path)
    return "FAILED"


def download_all(volume_lists, stats):
    """Download every volume of every reporter, updating stats."""
    for reporter in REPORTERS:
        volumes = volume_lists[reporter]
        os.makedirs(os.path.join(RAW_DIR, reporter), exist_ok=True)

        for i, volume in enumerate(volumes, start=1):
            zip_path = os.path.join(RAW_DIR, reporter, f"{volume}.zip")
            status = download_volume(reporter, volume, zip_path)
            label = f"  [{reporter} {i}/{len(volumes)}] volume {volume}"

            if status == "FAILED":
                print(f"{label}: FAILED")
                stats[reporter]["failed"].append(volume)
                continue

            with zipfile.ZipFile(zip_path) as z:
                count = len(case_entries(z))
            print(f"{label}: {status}, {count} cases")

            if status == "downloaded":
                stats[reporter]["downloaded_now"] += 1
                time.sleep(POLITE_WAIT_SECONDS)         # only wait after a real download
            else:
                stats[reporter]["already_had"] += 1


# ---------------------------------------------------------------
# STEP 3: build cases.jsonl and sample.jsonl from the saved zips
# ---------------------------------------------------------------

def build_dataset(volume_lists, stats):
    """
    Read every case out of the saved zips (ark first, then ark-app, volumes in
    ascending order, case files sorted by name) and write the kept cases.
    The order is fixed, so the line number of a case in cases.jsonl
    (which later becomes its docID) is the same every time we run this.
    """
    total_kept = 0
    os.makedirs(os.path.dirname(CASES_FILE), exist_ok=True)
    os.makedirs(os.path.dirname(SAMPLE_FILE), exist_ok=True)

    # newline="\n" stops Windows from writing "\r\n"; later the C++ indexer
    # will store byte offsets into this file, so line endings must be predictable.
    with open(CASES_FILE, "w", encoding="utf-8", newline="\n") as cases_out, \
         open(SAMPLE_FILE, "w", encoding="utf-8", newline="\n") as sample_out:

        for reporter in REPORTERS:
            s = stats[reporter]
            for volume in volume_lists[reporter]:
                zip_path = os.path.join(RAW_DIR, reporter, f"{volume}.zip")
                if volume in s["failed"] or not os.path.exists(zip_path):
                    continue

                with zipfile.ZipFile(zip_path) as z:
                    for name in case_entries(z):
                        s["cases_seen"] += 1
                        try:
                            case = json.loads(z.read(name))
                        except (json.JSONDecodeError, UnicodeDecodeError):
                            print(f"    unreadable case: {reporter}/{volume}.zip {name}")
                            s["cases_broken"] += 1
                            continue

                        # Count the words of the actual judgment (opinion) text.
                        # We do NOT use analysis.word_count: it also counts headnotes,
                        # party and lawyer names, so some cases with almost no judgment
                        # text would pass the filter.
                        if opinion_word_count(case) < MIN_WORDS:
                            s["cases_short"] += 1
                            continue

                        # One case per line. ensure_ascii=False keeps characters like § as they are.
                        line = json.dumps(case, ensure_ascii=False) + "\n"
                        cases_out.write(line)
                        if total_kept < SAMPLE_SIZE:
                            sample_out.write(line)
                        total_kept += 1
                        s["cases_kept"] += 1
            print(f"  {reporter}: {s['cases_kept']} cases written")


# ---------------------------------------------------------------
# STEP 4: summary
# ---------------------------------------------------------------

def print_summary(stats, is_test_run, minutes):
    line = "=" * 62
    print("\n" + line)
    print("SUMMARY" + ("   (TEST RUN with --limit)" if is_test_run else ""))
    print(line)

    header = f"  {'':28}{'ark':>10}{'ark-app':>10}{'TOTAL':>12}"
    print(header)

    def row(title, key):
        values = [stats[r][key] if not isinstance(stats[r][key], list) else len(stats[r][key])
                  for r in REPORTERS]
        print(f"  {title:28}{values[0]:>10,}{values[1]:>10,}{sum(values):>12,}")

    print("  VOLUMES")
    row("  listed on the website", "volumes_listed")
    row("  downloaded in this run", "downloaded_now")
    row("  already on disk (skipped)", "already_had")
    row("  FAILED", "failed")
    print("  CASES")
    row("  found in the zips", "cases_seen")
    row(f"  skipped (text < {MIN_WORDS} words)", "cases_short")
    row("  skipped (unreadable JSON)", "cases_broken")
    row("  KEPT (in cases.jsonl)", "cases_kept")
    print(line)

    # Exactly which volumes failed, so they can be retried
    all_failed = [(r, v) for r in REPORTERS for v in stats[r]["failed"]]
    if all_failed:
        print("  FAILED VOLUMES (run the script again to retry them):")
        for reporter, volume in all_failed:
            print(f"    {BASE_URL}/{reporter}/{volume}.zip")
    else:
        print("  No failed volumes.")

    total_kept = sum(stats[r]["cases_kept"] for r in REPORTERS)
    size_mb = os.path.getsize(CASES_FILE) / (1024 * 1024)
    print(f"  data/cases.jsonl    : {total_kept:,} cases, {size_mb:,.1f} MB")
    print(f"  sample/sample.jsonl : {min(total_kept, SAMPLE_SIZE)} cases")
    print(f"  time taken          : {minutes:.1f} minutes")

    if is_test_run:
        print("  This was a test. Run without --limit to get the full dataset.")
    elif abs(total_kept - EXPECTED_KEPT) <= 100:
        print(f"  RESULT: OK (expected about {EXPECTED_KEPT:,} kept cases)")
    else:
        print(f"  RESULT: CHECK! Expected about {EXPECTED_KEPT:,} kept cases, got {total_kept:,}.")
    print(line)


# ---------------------------------------------------------------
# Main program
# ---------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Download the CAP Arkansas dataset.")
    parser.add_argument("--limit", type=int, default=0,
                        help="only use the first N volumes of each reporter (for testing)")
    args = parser.parse_args()

    start_time = time.time()
    stats = {reporter: new_stats() for reporter in REPORTERS}

    print("STEP 1: reading volume lists")
    volume_lists = {}
    for reporter in REPORTERS:
        volumes = get_volume_list(reporter)
        if args.limit > 0:
            volumes = volumes[:args.limit]
        volume_lists[reporter] = volumes
        stats[reporter]["volumes_listed"] = len(volumes)
    total_volumes = sum(len(v) for v in volume_lists.values())

    print(f"\nSTEP 2: downloading {total_volumes} volumes into {RAW_DIR}")
    download_all(volume_lists, stats)

    print(f"\nSTEP 3: building {CASES_FILE}")
    build_dataset(volume_lists, stats)

    minutes = (time.time() - start_time) / 60
    print_summary(stats, args.limit > 0, minutes)


if __name__ == "__main__":
    main()
