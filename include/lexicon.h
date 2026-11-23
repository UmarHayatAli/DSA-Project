// ---------------------------------------------------------------
// lexicon.h
//
// LEXICON: the list of every distinct word in the dataset. Each word gets a
// unique number, its wordID. The other indexes store wordIDs (small numbers)
// instead of the words themselves.
//
//   wordID -> word, df, cf
//     df (document frequency)   = in how many cases the word appears
//     cf (collection frequency) = how many times it appears in all cases together
//
// wordIDs are given in order of first appearance: the first word of the
// first case gets 0, the next new word gets 1, and so on.
//
// While building, a hash map (word -> wordID) finds a word's ID in O(1) on
// average. Later, the search engine loads the lexicon to turn the words of a
// query into wordIDs.
//
// Files written:
//   index/lexicon.bin   binary, used by the search engine
//   index/lexicon.txt   the same data as readable text
// ---------------------------------------------------------------
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// What we store for one word.
struct WordInfo {
    std::string word;   // the cleaned word, e.g. "negligence"
    uint32_t df = 0;    // number of cases containing the word
    uint32_t cf = 0;    // total number of occurrences in all cases
};

class Lexicon {
public:
    // Record one occurrence of `word` in case `docID` and return its wordID.
    // A new word gets the next free wordID. cf always goes up by 1;
    // df goes up by 1 only the first time the word is seen in this case.
    // (Cases must be added in increasing docID order, which build_index does.)
    uint32_t addOccurrence(const std::string& word, uint32_t docID);

    // Find the wordID of a word. Returns false if the word is not in the lexicon.
    bool lookup(const std::string& word, uint32_t& wordID) const;

    size_t size() const { return words_.size(); }
    const WordInfo& get(uint32_t wordID) const { return words_[wordID]; }

    bool saveBinary(const std::string& path) const;
    bool saveText(const std::string& path) const;
    bool loadBinary(const std::string& path);

private:
    std::vector<WordInfo> words_;                      // words_[wordID] = info about that word
    std::unordered_map<std::string, uint32_t> ids_;    // word -> wordID (hash map, fast lookup)
    std::vector<uint32_t> lastDoc_;                    // last docID each word was seen in
                                                       // (only needed while building, for df)
};
