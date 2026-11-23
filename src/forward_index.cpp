// ---------------------------------------------------------------
// forward_index.cpp
//
// Builds, writes and reads the FORWARD INDEX (see forward_index.h).
//
// Binary file layout, all numbers little-endian u32 unless noted:
//
//   forward.bin
//     [magic = 0x49445746 "FWDI"] [number of cases]
//     then for every docID in order 0, 1, 2, ...:
//       [docID] [numTokens] [numWords]
//       numWords times:  [wordID] [numHits] [hit] [hit] ...
//
//   forward_off.bin
//     [magic = 0x4F445746 "FWDO"] [number of cases]
//     then for every docID: [byte offset of its record in forward.bin, u64]
// ---------------------------------------------------------------

#include "forward_index.h"

#include <algorithm>
#include <utility>

#include "binary_io.h"

static const uint32_t FORWARD_MAGIC = 0x49445746;   // the letters "FWDI"
static const uint32_t OFFSETS_MAGIC = 0x4F445746;   // the letters "FWDO"

// ---------------------------------------------------------------
// Group the tokens of one case by wordID.
// ---------------------------------------------------------------
void buildForwardDoc(uint32_t docID, const std::vector<Token>& tokens,
                     const std::vector<uint32_t>& wordIDs, size_t titleTokens,
                     ForwardDoc& out) {
    out.docID = docID;
    out.numTokens = static_cast<uint32_t>(tokens.size());
    out.words.clear();

    // Pairs (wordID, token number). Sorting them puts all tokens of the same
    // word next to each other, and within one word keeps the tokens in text
    // order, so every hit list comes out sorted by position.
    std::vector<std::pair<uint32_t, uint32_t>> order(tokens.size());
    for (uint32_t i = 0; i < tokens.size(); i++) order[i] = {wordIDs[i], i};
    std::sort(order.begin(), order.end());

    for (const auto& [wordID, i] : order) {
        if (out.words.empty() || out.words.back().wordID != wordID) {
            out.words.push_back(ForwardEntry{wordID, {}});
        }
        const Token& t = tokens[i];
        uint32_t position = std::min(t.position, HIT_MAX_POSITION);
        out.words.back().hits.push_back(makeHit(position, t.capitalized, i < titleTokens));
    }
}

// ---------------------------------------------------------------
// Writer
// ---------------------------------------------------------------

// Add a u32 to a byte buffer (little-endian), so a whole case is written at once.
static void appendU32(std::string& buf, uint32_t value) {
    for (int i = 0; i < 4; i++) buf.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
}

bool ForwardIndexWriter::open(const std::string& binPath) {
    out_.open(binPath, std::ios::binary);
    if (!out_) return false;
    offsets_.clear();
    totalHits_ = 0;
    writeU32(out_, FORWARD_MAGIC);
    writeU32(out_, 0);              // number of cases: filled in by close()
    return static_cast<bool>(out_);
}

bool ForwardIndexWriter::add(const ForwardDoc& doc) {
    if (doc.docID != offsets_.size()) return false;   // must be added in docID order
    offsets_.push_back(static_cast<uint64_t>(out_.tellp()));

    std::string buf;
    buf.reserve(12 + doc.words.size() * 8 + doc.numTokens * 4);
    appendU32(buf, doc.docID);
    appendU32(buf, doc.numTokens);
    appendU32(buf, static_cast<uint32_t>(doc.words.size()));
    for (const ForwardEntry& e : doc.words) {
        appendU32(buf, e.wordID);
        appendU32(buf, static_cast<uint32_t>(e.hits.size()));
        for (uint32_t hit : e.hits) appendU32(buf, hit);
        totalHits_ += e.hits.size();
    }
    out_.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    return static_cast<bool>(out_);
}

bool ForwardIndexWriter::close(const std::string& offPath) {
    // Go back and write the real number of cases into the header.
    out_.seekp(4);
    writeU32(out_, static_cast<uint32_t>(offsets_.size()));
    out_.close();
    if (out_.fail()) return false;

    std::ofstream off(offPath, std::ios::binary);
    if (!off) return false;
    writeU32(off, OFFSETS_MAGIC);
    writeU32(off, static_cast<uint32_t>(offsets_.size()));
    for (uint64_t o : offsets_) writeU64(off, o);
    return static_cast<bool>(off);
}

// ---------------------------------------------------------------
// Reader
// ---------------------------------------------------------------

bool ForwardIndexReader::open(const std::string& binPath, const std::string& offPath) {
    offsets_.clear();
    std::ifstream off(offPath, std::ios::binary);
    uint32_t magic = 0, count = 0;
    if (!off || !readU32(off, magic) || magic != OFFSETS_MAGIC || !readU32(off, count)) {
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
    if (!in_ || !readU32(in_, magic) || magic != FORWARD_MAGIC || !readU32(in_, binCount)) {
        return false;
    }
    if (binCount != count) return false;   // the two files do not belong together
    nextDoc_ = 0;
    return true;
}

// Read one record at the current file position.
// The hits are read as one block of bytes (much faster than 4 bytes at a time).
bool ForwardIndexReader::readRecord(ForwardDoc& doc) {
    uint32_t numWords = 0;
    if (!readU32(in_, doc.docID) || !readU32(in_, doc.numTokens) || !readU32(in_, numWords)) {
        return false;
    }
    doc.words.resize(numWords);
    std::string bytes;
    for (ForwardEntry& e : doc.words) {
        uint32_t numHits = 0;
        if (!readU32(in_, e.wordID) || !readU32(in_, numHits)) return false;
        bytes.resize(static_cast<size_t>(numHits) * 4);
        if (numHits > 0 && !in_.read(&bytes[0], static_cast<std::streamsize>(bytes.size()))) {
            return false;
        }
        e.hits.resize(numHits);
        const unsigned char* b = reinterpret_cast<const unsigned char*>(bytes.data());
        for (uint32_t h = 0; h < numHits; h++, b += 4) {
            e.hits[h] = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
                        (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
        }
    }
    return true;
}

bool ForwardIndexReader::read(uint32_t docID, ForwardDoc& doc) {
    if (docID >= offsets_.size()) return false;
    in_.clear();
    in_.seekg(static_cast<std::streamoff>(offsets_[docID]));   // jump straight to the case
    if (!readRecord(doc) || doc.docID != docID) return false;
    nextDoc_ = docID + 1;
    return true;
}

void ForwardIndexReader::rewind() {
    nextDoc_ = 0;
    in_.clear();
    in_.seekg(8);   // just after the header
}

bool ForwardIndexReader::next(ForwardDoc& doc) {
    if (nextDoc_ >= offsets_.size()) return false;
    if (!readRecord(doc) || doc.docID != nextDoc_) return false;
    nextDoc_++;
    return true;
}
