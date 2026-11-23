"""
check_download.py

Shows how far the dataset download (download_dataset.py) has got.
Safe to run at any time, also in a second terminal WHILE the download runs:
it only looks at the files in data/, it changes nothing.

Usage (from the project folder):
    python scripts/check_download.py
"""

import os
import time

RAW_DIR = os.path.join("data", "raw")
CASES_FILE = os.path.join("data", "cases.jsonl")
EXPECTED = {"ark": 375, "ark-app": 104}     # volumes per reporter (full dataset)
STALLED_AFTER_MINUTES = 3                   # no new zip for this long -> probably paused


def main():
    total_have = 0
    newest = 0.0
    partial = []
    for reporter, expected in EXPECTED.items():
        folder = os.path.join(RAW_DIR, reporter)
        names = os.listdir(folder) if os.path.isdir(folder) else []
        zips = [n for n in names if n.endswith(".zip")]
        partial += [os.path.join(folder, n) for n in names if n.endswith(".part")]
        for n in zips:
            newest = max(newest, os.path.getmtime(os.path.join(folder, n)))
        total_have += len(zips)
        print(f"  {reporter:8} {len(zips):4} / {expected} volumes downloaded")

    total_expected = sum(EXPECTED.values())
    print(f"  {'total':8} {total_have:4} / {total_expected}  ({100 * total_have / total_expected:.0f}%)")

    if newest:
        minutes = (time.time() - newest) / 60
        print(f"  newest zip: {minutes:.1f} minutes ago")
    if partial:
        print(f"  unfinished download in progress: {', '.join(partial)}")

    print()
    if os.path.exists(CASES_FILE) and total_have >= total_expected:
        size_mb = os.path.getsize(CASES_FILE) / 1e6
        print(f"  STATUS: FINISHED. {CASES_FILE} exists ({size_mb:,.0f} MB).")
    elif total_have >= total_expected:
        print("  STATUS: all zips downloaded, cases.jsonl is being built (or the script was")
        print("          stopped just before). If nothing changes, run download_dataset.py again.")
    elif newest and (time.time() - newest) / 60 > STALLED_AFTER_MINUTES:
        print(f"  STATUS: PROBABLY PAUSED/STOPPED (no new zip for over {STALLED_AFTER_MINUTES} minutes).")
        print("          Run  python scripts/download_dataset.py  again; it continues where it stopped.")
    else:
        print("  STATUS: downloading...")


if __name__ == "__main__":
    main()
