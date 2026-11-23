// ---------------------------------------------------------------
// forward_index.h
//
// FORWARD INDEX: for every case (docID), the list of words it contains and,
// for every word, WHERE in the case it appears (its "hit list").
//
//   docID -> number of tokens,
//            [ wordID -> hits (position + flags), ... ]   (sorted by wordID)
//
// This is the Forward Index from the Google paper. The inverted index is made
// from it by "turning it around" (wordID -> list of docIDs + hits).
//
// A HIT is one occurrence of a word, packed into 4 bytes (uint32_t):
//
//     bits 31 ........ 2 |   1    |    0
//          position      | title  | capital
//
//   position = word number inside the case (0, 1, 2, ...), from the tokenizer
//   title    = 1 if the word is in the case NAME (e.g. "Goings v. Mills").
//              Like the "fancy hits" in the Google paper: a match in the
//              title says more than a match deep inside the opinion text.
//   capital  = 1 if the word started with a capital letter in the text.
//
// The forward index is WRITTEN ONE CASE AT A TIME while build_index reads
// cases.jsonl, so it is never fully in memory (the full dataset has about
// 104 million hits, forward.bin = 650 MB). A small offset table remembers where each
// case's record starts, so one case can be read without reading the file.
//
// Files written:
//   index/forward.bin       binary, one record per case (used by the next steps)
//   index/forward_off.bin   docID -> byte offset of its record in forward.bin
//   index/forward.txt       readable text for the FIRST few cases only
//                           (the whole index as text would be over 1 GB)
// ---------------------------------------------------------------
#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "tokenizer.h"

// ----- hits -----

const uint32_t HIT_CAPITAL = 1u;          // bit 0
const uint32_t HIT_TITLE   = 2u;          // bit 1
const uint32_t HIT_MAX_POSITION = (1u << 30) - 1;   // positions must fit in 30 bits

inline uint32_t makeHit(uint32_t position, bool capitalized, bool inTitle) {
    return (position << 2) | (inTitle ? HIT_TITLE : 0u) | (capitalized ? HIT_CAPITAL : 0u);
}
inline uint32_t hitPosition(uint32_t hit)    { return hit >> 2; }
inline bool     hitCapitalized(uint32_t hit) { return (hit & HIT_CAPITAL) != 0; }
inline bool     hitInTitle(uint32_t hit)     { return (hit & HIT_TITLE) != 0; }

// ----- one case in the forward index -----

// One word of a case and all the places where it occurs.
struct ForwardEntry {
    uint32_t wordID = 0;
    std::vector<uint32_t> hits;   // sorted by position
};

// Everything the forward index stores for one case.
struct ForwardDoc {
    uint32_t docID = 0;
    uint32_t numTokens = 0;              // total number of words in the case
    std::vector<ForwardEntry> words;     // sorted by wordID, each wordID once
};

// Build the ForwardDoc of one case from its tokens.
//   tokens[i] has wordIDs[i] (looked up in the lexicon),
//   the first `titleTokens` tokens come from the case name.
void buildForwardDoc(uint32_t docID, const std::vector<Token>& tokens,
                     const std::vector<uint32_t>& wordIDs, size_t titleTokens,
                     ForwardDoc& out);

// ----- writing (used by build_index) -----

class ForwardIndexWriter {
public:
    // Open forward.bin for writing. Returns false if the file cannot be created.
    bool open(const std::string& binPath);

    // Append one case. Cases must be added in docID order 0, 1, 2, ...
    bool add(const ForwardDoc& doc);

    // Finish forward.bin and write the offset table to offPath.
    bool close(const std::string& offPath);

    uint64_t totalHits() const { return totalHits_; }

private:
    std::ofstream out_;
    std::vector<uint64_t> offsets_;   // offsets_[docID] = where its record starts
    uint64_t totalHits_ = 0;
};

// ----- reading (used by the checks, the tests and the inverted index) -----

class ForwardIndexReader {
public:
    // Open forward.bin and load the offset table.
    bool open(const std::string& binPath, const std::string& offPath);

    size_t size() const { return offsets_.size(); }

    // Read the record of one case directly (jumps to its offset).
    bool read(uint32_t docID, ForwardDoc& doc);

    // Read the cases one after another from the start (faster for a full scan).
    void rewind();
    bool next(ForwardDoc& doc);   // false at the end of the file

private:
    bool readRecord(ForwardDoc& doc);

    std::ifstream in_;
    std::vector<uint64_t> offsets_;
    uint32_t nextDoc_ = 0;
};

// Write the first `maxDocs` cases as readable text, with the word next to each wordID.
// `wordOf(wordID)` gives the word, `titleOf(docID)` gives the case title.
template <typename WordOf, typename TitleOf>
bool saveForwardText(ForwardIndexReader& reader, const std::string& path, size_t maxDocs,
                     WordOf wordOf, TitleOf titleOf);

// ---------------------------------------------------------------
// Template implementation (must be in the header).
// ---------------------------------------------------------------
template <typename WordOf, typename TitleOf>
bool saveForwardText(ForwardIndexReader& reader, const std::string& path, size_t maxDocs,
                     WordOf wordOf, TitleOf titleOf) {
    std::ofstream out(path, std::ios::binary);   // binary: '\n' line endings on every system
    if (!out) return false;

    out << "# Forward index, first " << maxDocs << " cases (of " << reader.size() << ")\n"
        << "# One block per case:  docID <id>  tokens <n>  words <distinct>  <title>\n"
        << "# then one line per word:  wordID  word  count  positions\n"
        << "# position flags:  ^ = started with a capital,  T = in the case name (title)\n";

    ForwardDoc doc;
    for (uint32_t id = 0; id < reader.size() && id < maxDocs; id++) {
        if (!reader.read(id, doc)) return false;
        out << "\ndocID " << doc.docID << "  tokens " << doc.numTokens << "  words "
            << doc.words.size() << "  " << titleOf(doc.docID) << "\n";
        for (const ForwardEntry& e : doc.words) {
            out << "  " << e.wordID << '\t' << wordOf(e.wordID) << '\t' << e.hits.size() << '\t';
            for (size_t h = 0; h < e.hits.size(); h++) {
                uint32_t hit = e.hits[h];
                out << (h ? " " : "") << hitPosition(hit) << (hitCapitalized(hit) ? "^" : "")
                    << (hitInTitle(hit) ? "T" : "");
            }
            out << '\n';
        }
    }
    return static_cast<bool>(out);
}
