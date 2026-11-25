// ---------------------------------------------------------------
// inverted_index.h
//
// INVERTED INDEX: for every word (wordID), the list of cases that contain it
// and, for every such case, WHERE in the case the word appears.
//
//   wordID -> [ docID -> hits (position + flags), ... ]   (sorted by docID)
//
// It is the forward index "turned around":
//
//   forward index :  case 0   -> goings, against, mills, court, ...
//   inverted index:  "court"  -> case 0, case 1, case 5, ...   (with hits)
//
// This is what makes search fast: to search "negligence", the engine finds
// its wordID in the lexicon and reads ONLY that word's list from the file
// (the offset table says where it starts). This is the Inverted Index from
// the Google paper. Hits use the same 4-byte format as the forward index
// (see forward_index.h): position, title flag, capital flag.
//
// HOW IT IS BUILT (memory stays small, the full dataset has 104 million hits):
//   The lexicon already knows, for every word, df (number of cases) and
//   cf (number of hits). So the exact size of every word's list is known
//   BEFORE reading the forward index:
//       size = 8 + 8 * df + 4 * cf  bytes
//   The words are split into consecutive wordID ranges ("batches") whose
//   lists fit in a memory budget (256 MB in build_index). For each batch,
//   forward.bin is read once from start to end and every hit of a word in the
//   batch is copied straight to its final place in a buffer. Cases are read
//   in docID order, so every list comes out sorted by docID without sorting.
//   The buffer is then appended to inverted.bin. A word whose list alone is
//   bigger than the budget gets a batch of its own.
//
//   Because the lists are stored in wordID order with an offset table, the
//   file can later be split into barrels (groups of wordIDs) by cutting it
//   at wordID boundaries.
//
// Files written:
//   index/inverted.bin       binary, one posting list per word (in wordID order)
//   index/inverted_off.bin   wordID -> byte offset of its list in inverted.bin
//   index/inverted.txt       readable text for the FIRST few words only
// ---------------------------------------------------------------
#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "forward_index.h"
#include "lexicon.h"

// One case in a word's list: the case and the word's hits inside it.
struct Posting {
    uint32_t docID = 0;
    std::vector<uint32_t> hits;   // sorted by position
};

// Everything the inverted index stores for one word.
struct PostingList {
    uint32_t wordID = 0;
    std::vector<Posting> docs;    // sorted by docID, each docID once
};

// Size in bytes of one word's list in inverted.bin:
//   [wordID][numDocs]  +  df * [docID][numHits]  +  cf * [hit]
inline uint64_t postingListBytes(uint32_t df, uint32_t cf) {
    return 8 + 8ull * df + 4ull * cf;
}

// Numbers reported after building.
struct InvertedBuildStats {
    uint64_t totalPostings = 0;   // (word, case) pairs = sum of df
    uint64_t totalHits = 0;       // = sum of cf = total tokens
    uint32_t passes = 0;          // batches = passes over forward.bin
    uint64_t largestBatch = 0;    // biggest buffer used, in bytes
};

// Build inverted.bin and inverted_off.bin from the forward index.
//   `lex` must be the lexicon built together with the forward index
//   (its df and cf give the size of every list).
//   `memoryBudget` = maximum buffer size in bytes for one batch.
// Returns false (and a message in `error`) if a file cannot be written or the
// forward index does not match the lexicon.
bool buildInvertedIndex(ForwardIndexReader& forward, const Lexicon& lex,
                        const std::string& binPath, const std::string& offPath,
                        uint64_t memoryBudget, InvertedBuildStats& stats, std::string& error);

// ----- reading (used by the checks, the tests and later the search) -----

class InvertedIndexReader {
public:
    // Open inverted.bin and load the offset table.
    bool open(const std::string& binPath, const std::string& offPath);

    size_t size() const { return offsets_.size(); }   // number of words

    // Read the list of one word directly (jumps to its offset).
    bool read(uint32_t wordID, PostingList& list);

    // Read the lists one after another from the start (faster for a full scan).
    void rewind();
    bool next(PostingList& list);   // false at the end of the file

private:
    bool readRecord(PostingList& list);

    std::ifstream in_;
    std::vector<uint64_t> offsets_;
    uint32_t nextWord_ = 0;
};

// Write the first `maxWords` words as readable text, showing at most
// `maxDocs` cases per word and `maxHits` positions per case.
// `wordOf(wordID)` gives the word.
template <typename WordOf>
bool saveInvertedText(InvertedIndexReader& reader, const std::string& path, size_t maxWords,
                      size_t maxDocs, size_t maxHits, WordOf wordOf);

// ---------------------------------------------------------------
// Template implementation (must be in the header).
// ---------------------------------------------------------------
template <typename WordOf>
bool saveInvertedText(InvertedIndexReader& reader, const std::string& path, size_t maxWords,
                      size_t maxDocs, size_t maxHits, WordOf wordOf) {
    std::ofstream out(path, std::ios::binary);   // binary: '\n' line endings on every system
    if (!out) return false;

    out << "# Inverted index, first " << maxWords << " words (of " << reader.size() << ")\n"
        << "# One block per word:  wordID <id>  <word>  df <cases>  cf <hits>\n"
        << "# then one line per case (first " << maxDocs << " only):  docID  count  positions\n"
        << "# position flags:  ^ = started with a capital,  T = in the case name (title)\n";

    PostingList list;
    for (uint32_t id = 0; id < reader.size() && id < maxWords; id++) {
        if (!reader.read(id, list)) return false;
        uint64_t cf = 0;
        for (const Posting& p : list.docs) cf += p.hits.size();
        out << "\nwordID " << list.wordID << "  " << wordOf(list.wordID) << "  df "
            << list.docs.size() << "  cf " << cf << "\n";
        for (size_t d = 0; d < list.docs.size() && d < maxDocs; d++) {
            const Posting& p = list.docs[d];
            out << "  " << p.docID << '\t' << p.hits.size() << '\t';
            for (size_t h = 0; h < p.hits.size() && h < maxHits; h++) {
                uint32_t hit = p.hits[h];
                out << (h ? " " : "") << hitPosition(hit) << (hitCapitalized(hit) ? "^" : "")
                    << (hitInTitle(hit) ? "T" : "");
            }
            if (p.hits.size() > maxHits) out << " ...";
            out << '\n';
        }
        if (list.docs.size() > maxDocs) {
            out << "  ... " << (list.docs.size() - maxDocs) << " more cases\n";
        }
    }
    return static_cast<bool>(out);
}
