// ---------------------------------------------------------------
// docindex.h
//
// DOCUMENT INDEX: a list of all cases with their basic details.
//
//   docID -> CAP id, where the case is in cases.jsonl (byte offset + length),
//            decision date, court, PageRank percentile, word count,
//            title and citation.
//
// docID = the line number of the case in cases.jsonl, starting at 0.
// The other indexes refer to cases only by this small number; when a search
// finds a docID, we look it up here to show the title/date/court, use the
// metadata for ranking, and jump straight to the full case in cases.jsonl.
//
// Files written:
//   index/docindex.bin   binary, used by the search engine
//   index/docindex.txt   the same data as readable text (for checking by eye)
// ---------------------------------------------------------------
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "json.hpp"

// Court level, stored as one small number.
enum CourtCode : uint8_t {
    COURT_SUPREME = 0,    // Arkansas Supreme Court
    COURT_APPEALS = 1,    // Arkansas Court of Appeals
    COURT_OTHER   = 2     // anything else (e.g. documents added later)
};

// Everything we store about one case.
struct DocInfo {
    uint64_t capId = 0;       // the case's id in the Caselaw Access Project
    uint64_t offset = 0;      // byte position where the case's line starts in cases.jsonl
    uint32_t length = 0;      // length of that line in bytes (without the '\n')
    uint32_t date = 0;        // decision date as YYYYMMDD, e.g. 19810225 (missing parts = 00)
    uint8_t  court = COURT_OTHER;
    float    pagerank = -1;   // PageRank percentile 0..1, or -1 if the case has none
    uint32_t wordCount = 0;   // number of words in the opinion text
    std::string title;        // e.g. "Tolley v. State"
    std::string citation;     // e.g. "1 Ark. App. 1"
};

class DocIndex {
public:
    // Add one case (already parsed from JSON) and return its new docID.
    // offset/length = where its line is in cases.jsonl.
    uint32_t add(const nlohmann::json& caseJson, uint64_t offset, uint32_t length);

    size_t size() const { return docs_.size(); }
    const DocInfo& get(uint32_t docID) const { return docs_[docID]; }

    bool saveBinary(const std::string& path) const;
    bool saveText(const std::string& path) const;
    bool loadBinary(const std::string& path);

private:
    std::vector<DocInfo> docs_;   // docs_[docID] = information about that case
};

// Helper functions (also used by tests and later by dynamic document addition).
uint32_t parseDate(const std::string& date);          // "1981-02-25" -> 19810225
uint8_t  courtCodeFromName(const std::string& name);  // "Arkansas Supreme Court" -> COURT_SUPREME
const char* courtName(uint8_t code);                  // COURT_SUPREME -> "Supreme"
