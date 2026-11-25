// ---------------------------------------------------------------
// inverted_index.cpp
//
// Builds and reads the INVERTED INDEX (see inverted_index.h).
//
// Binary file layout, all numbers little-endian u32 unless noted:
//
//   inverted.bin
//     [magic = 0x49564E49 "INVI"] [number of words]
//     then for every wordID in order 0, 1, 2, ...:
//       [wordID] [numDocs]
//       numDocs times:  [docID] [numHits] [hit] [hit] ...
//
//   inverted_off.bin
//     [magic = 0x4F564E49 "INVO"] [number of words]
//     then for every wordID: [byte offset of its list in inverted.bin, u64]
// ---------------------------------------------------------------

#include "inverted_index.h"

#include <algorithm>

#include "binary_io.h"

static const uint32_t INVERTED_MAGIC = 0x49564E49;      // the letters "INVI"
static const uint32_t INV_OFFSETS_MAGIC = 0x4F564E49;   // the letters "INVO"

// Write many u32 values as little-endian bytes, a large block at a time
// (much faster than writing 4 bytes at a time).
static bool writeU32Block(std::ostream& out, const std::vector<uint32_t>& values) {
    const size_t CHUNK = 1 << 16;                 // 65,536 values = 256 KB per write
    std::string bytes;
    for (size_t start = 0; start < values.size(); start += CHUNK) {
        size_t count = std::min(CHUNK, values.size() - start);
        bytes.resize(count * 4);
        for (size_t i = 0; i < count; i++) {
            uint32_t v = values[start + i];
            bytes[4 * i]     = static_cast<char>(v & 0xFF);
            bytes[4 * i + 1] = static_cast<char>((v >> 8) & 0xFF);
            bytes[4 * i + 2] = static_cast<char>((v >> 16) & 0xFF);
            bytes[4 * i + 3] = static_cast<char>((v >> 24) & 0xFF);
        }
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    return static_cast<bool>(out);
}

// ---------------------------------------------------------------
// Builder
// ---------------------------------------------------------------
bool buildInvertedIndex(ForwardIndexReader& forward, const Lexicon& lex,
                        const std::string& binPath, const std::string& offPath,
                        uint64_t memoryBudget, InvertedBuildStats& stats, std::string& error) {
    stats = InvertedBuildStats();
    const uint32_t numWords = static_cast<uint32_t>(lex.size());

    // 1. The size of every list is known from the lexicon, so the offset
    //    table can be computed before reading the forward index.
    std::vector<uint64_t> offsets(numWords);
    uint64_t position = 8;                          // after the header
    for (uint32_t w = 0; w < numWords; w++) {
        offsets[w] = position;
        position += postingListBytes(lex.get(w).df, lex.get(w).cf);
        stats.totalPostings += lex.get(w).df;
        stats.totalHits += lex.get(w).cf;
    }

    std::ofstream out(binPath, std::ios::binary);
    if (!out) {
        error = "cannot create " + binPath;
        return false;
    }
    writeU32(out, INVERTED_MAGIC);
    writeU32(out, numWords);

    // 2. One batch of words at a time.
    std::vector<uint32_t> buffer;      // the lists of the batch, as u32 values
    std::vector<uint64_t> cursor;      // cursor[w - lo] = next free slot of word w in buffer
    std::vector<uint64_t> listEnd;     // listEnd[w - lo] = slot just after word w's list
    ForwardDoc doc;

    uint32_t lo = 0;
    while (lo < numWords) {
        // Take words lo, lo+1, ... while their lists fit in the budget
        // (always at least one word, even if its list alone is bigger).
        uint32_t hi = lo;
        uint64_t batchBytes = 0;
        while (hi < numWords) {
            uint64_t bytes = postingListBytes(lex.get(hi).df, lex.get(hi).cf);
            if (hi > lo && batchBytes + bytes > memoryBudget) break;
            batchBytes += bytes;
            hi++;
        }

        // Write the header [wordID][numDocs] of every list; the cursors point
        // just after it.
        const uint64_t base = offsets[lo];
        buffer.assign(batchBytes / 4, 0);
        cursor.resize(hi - lo);
        listEnd.resize(hi - lo);
        for (uint32_t w = lo; w < hi; w++) {
            uint64_t slot = (offsets[w] - base) / 4;
            buffer[slot] = w;
            buffer[slot + 1] = lex.get(w).df;
            cursor[w - lo] = slot + 2;
            listEnd[w - lo] = slot + postingListBytes(lex.get(w).df, lex.get(w).cf) / 4;
        }

        // Read the whole forward index. Each case's words are sorted by wordID,
        // so binary search finds the first word of the batch, and the loop stops
        // at the first word after the batch.
        forward.rewind();
        for (uint32_t d = 0; d < forward.size(); d++) {
            if (!forward.next(doc)) {
                error = "could not read the forward index record of docID " + std::to_string(d);
                return false;
            }
            auto it = std::lower_bound(
                doc.words.begin(), doc.words.end(), lo,
                [](const ForwardEntry& e, uint32_t w) { return e.wordID < w; });
            for (; it != doc.words.end() && it->wordID < hi; ++it) {
                uint64_t& c = cursor[it->wordID - lo];
                uint64_t needed = 2 + it->hits.size();
                if (c + needed > listEnd[it->wordID - lo]) {
                    error = "word \"" + lex.get(it->wordID).word +
                            "\" has more cases or hits in the forward index than in the lexicon";
                    return false;
                }
                buffer[c] = doc.docID;
                buffer[c + 1] = static_cast<uint32_t>(it->hits.size());
                std::copy(it->hits.begin(), it->hits.end(), buffer.begin() + c + 2);
                c += needed;
            }
        }

        // Every list must be exactly full; otherwise the lexicon and the
        // forward index do not belong together.
        for (uint32_t w = lo; w < hi; w++) {
            if (cursor[w - lo] != listEnd[w - lo]) {
                error = "word \"" + lex.get(w).word +
                        "\" has fewer cases or hits in the forward index than in the lexicon";
                return false;
            }
        }

        if (!writeU32Block(out, buffer)) {
            error = "could not write " + binPath;
            return false;
        }
        stats.passes++;
        stats.largestBatch = std::max(stats.largestBatch, batchBytes);
        lo = hi;
    }

    out.close();
    if (out.fail()) {
        error = "could not finish " + binPath;
        return false;
    }

    // 3. The offset table.
    std::ofstream off(offPath, std::ios::binary);
    if (!off) {
        error = "cannot create " + offPath;
        return false;
    }
    writeU32(off, INV_OFFSETS_MAGIC);
    writeU32(off, numWords);
    for (uint64_t o : offsets) writeU64(off, o);
    if (!off) {
        error = "could not write " + offPath;
        return false;
    }
    buffer.clear();
    buffer.shrink_to_fit();            // give the memory back
    return true;
}

// ---------------------------------------------------------------
// Reader
// ---------------------------------------------------------------

bool InvertedIndexReader::open(const std::string& binPath, const std::string& offPath) {
    offsets_.clear();
    std::ifstream off(offPath, std::ios::binary);
    uint32_t magic = 0, count = 0;
    if (!off || !readU32(off, magic) || magic != INV_OFFSETS_MAGIC || !readU32(off, count)) {
        return false;
    }
    offsets_.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        if (!readU64(off, offsets_[i])) return false;
    }

    in_.close();
    in_.clear();
    in_.open(binPath, std::ios::binary);
    uint32_t binCount = 0;
    if (!in_ || !readU32(in_, magic) || magic != INVERTED_MAGIC || !readU32(in_, binCount)) {
        return false;
    }
    if (binCount != count) return false;   // the two files do not belong together
    nextWord_ = 0;
    return true;
}

// Read one list at the current file position.
// The hits are read as one block of bytes (much faster than 4 bytes at a time).
bool InvertedIndexReader::readRecord(PostingList& list) {
    uint32_t numDocs = 0;
    if (!readU32(in_, list.wordID) || !readU32(in_, numDocs)) return false;
    list.docs.resize(numDocs);
    std::string bytes;
    for (Posting& p : list.docs) {
        uint32_t numHits = 0;
        if (!readU32(in_, p.docID) || !readU32(in_, numHits)) return false;
        bytes.resize(static_cast<size_t>(numHits) * 4);
        if (numHits > 0 && !in_.read(&bytes[0], static_cast<std::streamsize>(bytes.size()))) {
            return false;
        }
        p.hits.resize(numHits);
        const unsigned char* b = reinterpret_cast<const unsigned char*>(bytes.data());
        for (uint32_t h = 0; h < numHits; h++, b += 4) {
            p.hits[h] = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
                        (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
        }
    }
    return true;
}

bool InvertedIndexReader::read(uint32_t wordID, PostingList& list) {
    if (wordID >= offsets_.size()) return false;
    in_.clear();
    in_.seekg(static_cast<std::streamoff>(offsets_[wordID]));   // jump straight to the list
    if (!readRecord(list) || list.wordID != wordID) return false;
    nextWord_ = wordID + 1;
    return true;
}

void InvertedIndexReader::rewind() {
    nextWord_ = 0;
    in_.clear();
    in_.seekg(8);   // just after the header
}

bool InvertedIndexReader::next(PostingList& list) {
    if (nextWord_ >= offsets_.size()) return false;
    if (!readRecord(list) || list.wordID != nextWord_) return false;
    nextWord_++;
    return true;
}
