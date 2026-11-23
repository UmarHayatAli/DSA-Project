// ---------------------------------------------------------------
// lexicon.cpp
//
// Builds, saves and loads the LEXICON (see lexicon.h).
//
// Binary file layout (index/lexicon.bin), all numbers little-endian:
//   [magic u32 = 0x4958454C "LEXI"] [number of words u32]
//   then for every wordID in order 0, 1, 2, ...:
//     [word: len u16 + bytes][df u32][cf u32]
// The wordID is not written: it is the word's position in the file.
// ---------------------------------------------------------------

#include "lexicon.h"

#include <fstream>

#include "binary_io.h"

static const uint32_t LEXICON_MAGIC = 0x4958454C;   // the letters "LEXI"

uint32_t Lexicon::addOccurrence(const std::string& word, uint32_t docID) {
    uint32_t wordID;
    auto found = ids_.find(word);

    if (found == ids_.end()) {
        // New word: give it the next wordID.
        wordID = static_cast<uint32_t>(words_.size());
        ids_[word] = wordID;
        WordInfo info;
        info.word = word;
        words_.push_back(info);
        lastDoc_.push_back(UINT32_MAX);    // "not seen in any case yet"
    } else {
        wordID = found->second;
    }

    WordInfo& info = words_[wordID];
    info.cf++;                             // one more occurrence in total
    if (lastDoc_[wordID] != docID) {       // first time in this case?
        info.df++;                         // then one more case contains it
        lastDoc_[wordID] = docID;
    }
    return wordID;
}

bool Lexicon::lookup(const std::string& word, uint32_t& wordID) const {
    auto found = ids_.find(word);
    if (found == ids_.end()) return false;
    wordID = found->second;
    return true;
}

bool Lexicon::saveBinary(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    writeU32(out, LEXICON_MAGIC);
    writeU32(out, static_cast<uint32_t>(words_.size()));
    for (const WordInfo& w : words_) {
        writeString(out, w.word);
        writeU32(out, w.df);
        writeU32(out, w.cf);
    }
    return static_cast<bool>(out);
}

bool Lexicon::saveText(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);   // binary: '\n' line endings on every system
    if (!out) return false;

    out << "# wordID\tword\tdf\tcf\n";
    for (size_t id = 0; id < words_.size(); id++) {
        const WordInfo& w = words_[id];
        out << id << '\t' << w.word << '\t' << w.df << '\t' << w.cf << '\n';
    }
    return static_cast<bool>(out);
}

bool Lexicon::loadBinary(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    uint32_t magic = 0, count = 0;
    if (!readU32(in, magic) || magic != LEXICON_MAGIC) return false;   // not our file
    if (!readU32(in, count)) return false;

    words_.clear();
    ids_.clear();
    lastDoc_.clear();
    words_.reserve(count);
    ids_.reserve(count);

    for (uint32_t id = 0; id < count; id++) {
        WordInfo w;
        if (!readString(in, w.word) || !readU32(in, w.df) || !readU32(in, w.cf)) return false;
        ids_[w.word] = id;
        words_.push_back(w);
    }
    lastDoc_.assign(count, UINT32_MAX);
    return true;
}
