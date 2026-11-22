import urllib.request
import urllib.error
import zipfile
import io
import json

# ---------------------------------------------------------------
# Test script: download ONE volume (ark-app volume 1) and print
# the important fields of the first case, to check that we can
# read the Caselaw Access Project data correctly.
# ---------------------------------------------------------------

url = "https://static.case.law/ark-app/1.zip"
print("Downloading", url)

# Send a browser-like name, otherwise the site refuses Python (HTTP 403)
req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})

try:
    data = urllib.request.urlopen(req, timeout=60).read()
except urllib.error.HTTPError as e:
    print("Download failed. HTTP error code:", e.code, e.reason)
    print("403 = site blocked the request, 404 = wrong link")
    raise SystemExit(1)
except urllib.error.URLError as e:
    print("Download failed. Network problem:", e.reason)
    raise SystemExit(1)

print("Downloaded", len(data) // 1024, "KB")

# Open the zip directly from memory (no need to save it to disk)
z = zipfile.ZipFile(io.BytesIO(data))
names = z.namelist()
print("Files in zip:", len(names))
print("First 10 files:", names[:10])

# Find the case JSON files inside the zip.
# Inside the zip they live in a folder called "json/" (e.g. json/0001-01.json),
# NOT "cases/" (that name is only used on the website).
case_files = [n for n in names if n.startswith("json/") and n.endswith(".json")]
print("Case files:", len(case_files))

if not case_files:
    print("No case JSON files found. Send the file list above to check the zip layout.")
    raise SystemExit(1)

# Open the first case and print the important parts
case = json.loads(z.read(case_files[0]))

print()
print("Title       :", case.get("name_abbreviation"))
print("Date        :", case.get("decision_date"))
print("Court       :", case.get("court", {}).get("name"))
print("Words       :", case.get("analysis", {}).get("word_count"))
print("PageRank pct:", case.get("analysis", {}).get("pagerank", {}).get("percentile"))

opinions = case.get("casebody", {}).get("opinions", [])
print("Opinions    :", len(opinions))
if opinions:
    print("Text        :", opinions[0].get("text", "")[:300])
