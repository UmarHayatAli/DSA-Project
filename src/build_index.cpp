// ---------------------------------------------------------------
// build_index.cpp
//
// Main program of the indexer. Reads cases.jsonl one line (= one case)
// at a time and builds the index files in the output folder.
//
// Done so far:
//   Task 3  document index  -> docindex.bin, docindex.txt
//   Task 4  tokenizer       (cleans the text of each case into words)
//   Task 5  lexicon         -> lexicon.bin,  lexicon.txt
// Coming next: forward index, inverted index.
//
// Usage:
//   build_index <cases.jsonl> [output folder]
//   e.g. build_index sample/sample.jsonl index_sample
// ---------------------------------------------------------------

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "docindex.h"
#include "json.hpp"
#include "lexicon.h"
#include "tokenizer.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

// ---------------------------------------------------------------
// Tokenize the three indexed fields of one case, in this order:
//   name (full case name), head matter (headnotes/summary), every opinion text.
// Positions continue across the fields (tokenizer rule 8).
// ---------------------------------------------------------------
static void tokenizeCase(const json& c, std::vector<Token>& tokens) {
    tokens.clear();
    uint32_t position = 0;

    if (c.contains("name") && c["name"].is_string()) {
        tokenize(c["name"].get_ref<const std::string&>(), position, tokens);
    }
    if (!c.contains("casebody") || !c["casebody"].is_object()) return;
    const json& body = c["casebody"];

    if (body.contains("head_matter") && body["head_matter"].is_string()) {
        tokenize(body["head_matter"].get_ref<const std::string&>(), position, tokens);
    }
    if (body.contains("opinions") && body["opinions"].is_array()) {
        for (const json& opinion : body["opinions"]) {
            if (opinion.contains("text") && opinion["text"].is_string()) {
                tokenize(opinion["text"].get_ref<const std::string&>(), position, tokens);
            }
        }
    }
}

// ---------------------------------------------------------------
// Check the saved document index:
//  1. load docindex.bin back and compare it with what we built in memory,
//  2. for a few docIDs, jump to the stored byte offset in cases.jsonl and
//     check that the case found there has the same CAP id.
// ---------------------------------------------------------------
static bool checkDocIndex(const DocIndex& built, const std::string& binPath,
                          const std::string& casesPath) {
    DocIndex loaded;
    if (!loaded.loadBinary(binPath) || loaded.size() != built.size()) {
        std::cout << "  CHECK FAILED: could not load " << binPath << " correctly\n";
        return false;
    }
    for (uint32_t id = 0; id < built.size(); id++) {
        const DocInfo& a = built.get(id);
        const DocInfo& b = loaded.get(id);
        if (a.capId != b.capId || a.offset != b.offset || a.length != b.length ||
            a.date != b.date || a.court != b.court || a.pagerank != b.pagerank ||
            a.wordCount != b.wordCount || a.title != b.title || a.citation != b.citation) {
            std::cout << "  CHECK FAILED: docID " << id << " differs after loading\n";
            return false;
        }
    }

    std::ifstream cases(casesPath, std::ios::binary);
    uint32_t n = static_cast<uint32_t>(built.size());
    uint32_t samples[] = {0, n / 2, n - 1};          // first, middle and last case
    for (uint32_t id : samples) {
        const DocInfo& d = built.get(id);
        std::string line(d.length, '\0');
        cases.clear();
        cases.seekg(static_cast<std::streamoff>(d.offset));   // jump straight to the case
        cases.read(&line[0], d.length);
        json c = json::parse(line, nullptr, false);
        if (c.is_discarded() || c.value("id", 0ULL) != d.capId) {
            std::cout << "  CHECK FAILED: offset of docID " << id << " is wrong\n";
            return false;
        }
        std::cout << "  docID " << id << " -> offset " << d.offset << " -> \""
                  << d.title << "\" (" << d.citation << ", " << d.date << ", "
                  << courtName(d.court) << ")\n";
    }
    std::cout << "  CHECK OK: docindex loads back identically and offsets point to the right cases\n";
    return true;
}

// ---------------------------------------------------------------
// Check the saved lexicon:
//  1. load lexicon.bin back and compare every word, df and cf,
//  2. the sum of all cf values must equal the total number of tokens,
//  3. for every word: 1 <= df <= cf and df <= number of cases,
//  4. looking up each word gives back its own wordID (no duplicates).
// ---------------------------------------------------------------
static bool checkLexicon(const Lexicon& built, const std::string& binPath,
                         uint64_t totalTokens, size_t numDocs) {
    Lexicon loaded;
    if (!loaded.loadBinary(binPath) || loaded.size() != built.size()) {
        std::cout << "  CHECK FAILED: could not load " << binPath << " correctly\n";
        return false;
    }
    uint64_t sumCf = 0;
    for (uint32_t id = 0; id < built.size(); id++) {
        const WordInfo& a = built.get(id);
        const WordInfo& b = loaded.get(id);
        if (a.word != b.word || a.df != b.df || a.cf != b.cf) {
            std::cout << "  CHECK FAILED: wordID " << id << " differs after loading\n";
            return false;
        }
        if (a.df < 1 || a.df > a.cf || a.df > numDocs) {
            std::cout << "  CHECK FAILED: impossible df/cf for word \"" << a.word << "\"\n";
            return false;
        }
        uint32_t foundID;
        if (!loaded.lookup(a.word, foundID) || foundID != id) {
            std::cout << "  CHECK FAILED: lookup of \"" << a.word << "\" gives the wrong wordID\n";
            return false;
        }
        sumCf += a.cf;
    }
    if (sumCf != totalTokens) {
        std::cout << "  CHECK FAILED: sum of cf (" << sumCf << ") != total tokens ("
                  << totalTokens << ")\n";
        return false;
    }
    std::cout << "  CHECK OK: lexicon loads back identically, sum of cf = total tokens, "
                 "df/cf values valid, every word maps to its own wordID\n";
    return true;
}

// Print the most frequent words (by cf), to see that the counts make sense.
static void printTopWords(const Lexicon& lex, size_t howMany) {
    std::vector<uint32_t> ids(lex.size());
    for (uint32_t i = 0; i < ids.size(); i++) ids[i] = i;
    howMany = std::min(howMany, ids.size());
    std::partial_sort(ids.begin(), ids.begin() + howMany, ids.end(),
                      [&](uint32_t a, uint32_t b) { return lex.get(a).cf > lex.get(b).cf; });
    std::cout << "  most frequent words (word: cf / df):\n   ";
    for (size_t i = 0; i < howMany; i++) {
        const WordInfo& w = lex.get(ids[i]);
        std::cout << " " << w.word << ": " << w.cf << "/" << w.df << (i + 1 < howMany ? "," : "");
    }
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: build_index <cases.jsonl> [output folder]\n";
        return 1;
    }
    std::string inputPath = argv[1];
    std::string outDir = (argc > 2) ? argv[2] : "index";

    std::ifstream in(inputPath, std::ios::binary);   // binary: byte offsets must be exact
    if (!in) {
        std::cerr << "Error: cannot open " << inputPath << "\n";
        return 1;
    }
    fs::create_directories(outDir);

    auto start = std::chrono::steady_clock::now();

    DocIndex docIndex;
    Lexicon lexicon;
    std::vector<Token> tokens;      // reused for every case to avoid re-allocating
    std::string line;
    uint64_t offset = 0;            // byte position of the current line in the file
    uint64_t totalTokens = 0;
    long long badLines = 0;

    // ONE pass over the file: read one line (= one case) at a time;
    // the whole file is never in memory.
    while (std::getline(in, line)) {
        uint64_t lineStart = offset;
        offset += line.size() + 1;            // +1 for the '\n' that getline removed

        if (line.empty()) continue;
        json c = json::parse(line, nullptr, false);   // no exception on broken JSON
        if (c.is_discarded()) {
            badLines++;
            continue;
        }

        // Task 3: add the case to the document index; this gives its docID.
        uint32_t docID = docIndex.add(c, lineStart, static_cast<uint32_t>(line.size()));

        // Tasks 4 + 5: clean the text into words and record every word in the lexicon.
        tokenizeCase(c, tokens);
        for (const Token& t : tokens) {
            lexicon.addOccurrence(t.word, docID);
        }
        totalTokens += tokens.size();

        if (docIndex.size() % 10000 == 0) {
            std::cout << "  " << docIndex.size() << " cases processed...\n";
        }
    }

    // Save everything.
    std::string docBin = outDir + "/docindex.bin", docTxt = outDir + "/docindex.txt";
    std::string lexBin = outDir + "/lexicon.bin",  lexTxt = outDir + "/lexicon.txt";
    if (!docIndex.saveBinary(docBin) || !docIndex.saveText(docTxt)) {
        std::cerr << "Error: could not write the document index to " << outDir << "\n";
        return 1;
    }
    if (!lexicon.saveBinary(lexBin) || !lexicon.saveText(lexTxt)) {
        std::cerr << "Error: could not write the lexicon to " << outDir << "\n";
        return 1;
    }

    double seconds = std::chrono::duration<double>(
                         std::chrono::steady_clock::now() - start).count();

    size_t singleCaseWords = 0;     // words that appear in only one case
    for (uint32_t id = 0; id < lexicon.size(); id++) {
        if (lexicon.get(id).df == 1) singleCaseWords++;
    }

    std::cout << "\nIndex build finished in " << seconds << " s\n";
    std::cout << "\nDocument index\n";
    std::cout << "  cases indexed    : " << docIndex.size() << "\n";
    std::cout << "  unreadable lines : " << badLines << "\n";
    std::cout << "  " << docBin << " : " << fs::file_size(docBin) << " bytes\n";
    std::cout << "  " << docTxt << " : " << fs::file_size(docTxt) << " bytes\n";

    std::cout << "\nLexicon\n";
    std::cout << "  total tokens     : " << totalTokens << "\n";
    std::cout << "  distinct words   : " << lexicon.size() << "\n";
    std::cout << "  words in 1 case  : " << singleCaseWords << "\n";
    printTopWords(lexicon, 10);
    std::cout << "  " << lexBin << " : " << fs::file_size(lexBin) << " bytes\n";
    std::cout << "  " << lexTxt << " : " << fs::file_size(lexTxt) << " bytes\n";

    std::cout << "\nChecks\n";
    if (docIndex.size() == 0) return 1;
    if (!checkDocIndex(docIndex, docBin, inputPath)) return 1;
    if (!checkLexicon(lexicon, lexBin, totalTokens, docIndex.size())) return 1;
    return 0;
}
