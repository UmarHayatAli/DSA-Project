// ---------------------------------------------------------------
// docindex.cpp
//
// Builds, saves and loads the DOCUMENT INDEX (see docindex.h).
//
// Binary file layout (index/docindex.bin), all numbers little-endian:
//   [magic u32 = 0x58444944 "DIDX"] [number of documents u32]
//   then for every docID in order 0, 1, 2, ...:
//     [capId u64][offset u64][length u32][date u32][court u8]
//     [pagerank f32][wordCount u32][title: len u16 + bytes][citation: len u16 + bytes]
// ---------------------------------------------------------------

#include "docindex.h"

#include <cctype>
#include <fstream>
#include <iomanip>

#include "binary_io.h"

using json = nlohmann::json;

static const uint32_t DOCINDEX_MAGIC = 0x58444944;   // the letters "DIDX", marks our file type

// ---------------------------------------------------------------
// Safe JSON helpers: some fields can be missing or null in the dataset,
// so we check the type before reading instead of crashing.
// ---------------------------------------------------------------

static std::string getString(const json& obj, const char* key) {
    if (obj.is_object() && obj.contains(key) && obj[key].is_string()) {
        return obj[key].get<std::string>();
    }
    return "";
}

static const json& getObject(const json& obj, const char* key) {
    static const json empty = json::object();
    if (obj.is_object() && obj.contains(key) && obj[key].is_object()) {
        return obj[key];
    }
    return empty;
}

// Count words in the opinion text: a word = a run of non-space characters.
static uint32_t countOpinionWords(const json& caseJson) {
    const json& casebody = getObject(caseJson, "casebody");
    if (!casebody.contains("opinions") || !casebody["opinions"].is_array()) return 0;

    uint32_t count = 0;
    for (const json& opinion : casebody["opinions"]) {
        std::string text = getString(opinion, "text");
        bool insideWord = false;
        for (unsigned char ch : text) {
            if (std::isspace(ch)) {
                insideWord = false;
            } else if (!insideWord) {    // first character of a new word
                insideWord = true;
                count++;
            }
        }
    }
    return count;
}

// ---------------------------------------------------------------
// Small conversion helpers
// ---------------------------------------------------------------

// The date can be "YYYY", "YYYY-MM" or "YYYY-MM-DD". Missing parts become 00.
// Example: "1837-01" -> 18370100. Returns 0 if the date can't be read.
uint32_t parseDate(const std::string& date) {
    int year = 0, month = 0, day = 0;
    if (date.size() < 4) return 0;
    for (int i = 0; i < 4; i++) {
        if (!std::isdigit(static_cast<unsigned char>(date[i]))) return 0;
    }
    year = std::stoi(date.substr(0, 4));
    if (date.size() >= 7 && std::isdigit(static_cast<unsigned char>(date[5])) &&
        std::isdigit(static_cast<unsigned char>(date[6]))) {
        month = std::stoi(date.substr(5, 2));
    }
    if (date.size() >= 10 && std::isdigit(static_cast<unsigned char>(date[8])) &&
        std::isdigit(static_cast<unsigned char>(date[9]))) {
        day = std::stoi(date.substr(8, 2));
    }
    return static_cast<uint32_t>(year * 10000 + month * 100 + day);
}

uint8_t courtCodeFromName(const std::string& name) {
    if (name.find("Supreme") != std::string::npos) return COURT_SUPREME;
    if (name.find("Appeals") != std::string::npos) return COURT_APPEALS;
    return COURT_OTHER;
}

const char* courtName(uint8_t code) {
    switch (code) {
        case COURT_SUPREME: return "Supreme";
        case COURT_APPEALS: return "Appeals";
        default:            return "Other";
    }
}

// ---------------------------------------------------------------
// Building
// ---------------------------------------------------------------

uint32_t DocIndex::add(const json& caseJson, uint64_t offset, uint32_t length) {
    DocInfo d;

    if (caseJson.contains("id") && caseJson["id"].is_number_integer()) {
        d.capId = caseJson["id"].get<uint64_t>();
    }
    d.offset = offset;
    d.length = length;
    d.date = parseDate(getString(caseJson, "decision_date"));
    d.court = courtCodeFromName(getString(getObject(caseJson, "court"), "name"));

    // PageRank percentile is inside analysis -> pagerank -> percentile (may be missing).
    const json& pagerank = getObject(getObject(caseJson, "analysis"), "pagerank");
    if (pagerank.contains("percentile") && pagerank["percentile"].is_number()) {
        d.pagerank = pagerank["percentile"].get<float>();
    }

    d.wordCount = countOpinionWords(caseJson);

    // Short title ("Goings v. Mills"); fall back to the full name if it is missing.
    d.title = getString(caseJson, "name_abbreviation");
    if (d.title.empty()) d.title = getString(caseJson, "name");

    // First citation, e.g. "1 Ark. 11".
    if (caseJson.contains("citations") && caseJson["citations"].is_array() &&
        !caseJson["citations"].empty()) {
        d.citation = getString(caseJson["citations"][0], "cite");
    }

    docs_.push_back(d);
    return static_cast<uint32_t>(docs_.size() - 1);   // the new docID
}

// ---------------------------------------------------------------
// Saving and loading
// ---------------------------------------------------------------

bool DocIndex::saveBinary(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    writeU32(out, DOCINDEX_MAGIC);
    writeU32(out, static_cast<uint32_t>(docs_.size()));
    for (const DocInfo& d : docs_) {
        writeU64(out, d.capId);
        writeU64(out, d.offset);
        writeU32(out, d.length);
        writeU32(out, d.date);
        writeU8(out, d.court);
        writeF32(out, d.pagerank);
        writeU32(out, d.wordCount);
        writeString(out, d.title);
        writeString(out, d.citation);
    }
    return static_cast<bool>(out);
}

bool DocIndex::saveText(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);   // binary: write '\n' exactly, no '\r\n'
    if (!out) return false;

    out << "# docID\tcapId\toffset\tlength\tdate\tcourt\tpagerank\twords\tcitation\ttitle\n";
    for (size_t docID = 0; docID < docs_.size(); docID++) {
        const DocInfo& d = docs_[docID];
        out << docID << '\t' << d.capId << '\t' << d.offset << '\t' << d.length << '\t'
            << d.date << '\t' << courtName(d.court) << '\t'
            << std::fixed << std::setprecision(4) << d.pagerank << '\t'
            << d.wordCount << '\t' << d.citation << '\t' << d.title << '\n';
    }
    return static_cast<bool>(out);
}

bool DocIndex::loadBinary(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    uint32_t magic = 0, count = 0;
    if (!readU32(in, magic) || magic != DOCINDEX_MAGIC) return false;   // not our file
    if (!readU32(in, count)) return false;

    docs_.clear();
    docs_.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        DocInfo d;
        bool ok = readU64(in, d.capId) && readU64(in, d.offset) && readU32(in, d.length) &&
                  readU32(in, d.date) && readU8(in, d.court) && readF32(in, d.pagerank) &&
                  readU32(in, d.wordCount) && readString(in, d.title) &&
                  readString(in, d.citation);
        if (!ok) return false;   // file is cut short or broken
        docs_.push_back(d);
    }
    return true;
}
