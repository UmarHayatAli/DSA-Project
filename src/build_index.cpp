// ---------------------------------------------------------------
// build_index.cpp
//
// Main program of the indexer. Reads cases.jsonl one line (= one case)
// at a time and builds the index files in the output folder.
//
// Done so far:
//   Task 3  document index  -> index/docindex.bin, index/docindex.txt
// Coming next: tokenizer, lexicon, forward index, inverted index.
//
// Usage:
//   build_index <cases.jsonl> [output folder]
//   e.g. build_index sample/sample.jsonl index
// ---------------------------------------------------------------

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "docindex.h"
#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

// Check the saved document index:
//  1. load docindex.bin back and compare it with what we built in memory,
//  2. for a few docIDs, jump to the stored byte offset in cases.jsonl and
//     check that the case found there has the same CAP id.
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
    return true;
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
    std::string line;
    uint64_t offset = 0;      // byte position of the current line in the file
    long long badLines = 0;

    // Read one line (= one case) at a time; the whole file is never in memory.
    while (std::getline(in, line)) {
        uint64_t lineStart = offset;
        offset += line.size() + 1;            // +1 for the '\n' that getline removed

        if (line.empty()) continue;
        json c = json::parse(line, nullptr, false);   // no exception on broken JSON
        if (c.is_discarded()) {
            badLines++;
            continue;
        }

        docIndex.add(c, lineStart, static_cast<uint32_t>(line.size()));

        if (docIndex.size() % 10000 == 0) {
            std::cout << "  " << docIndex.size() << " cases processed...\n";
        }
    }

    // Save the document index.
    std::string binPath = outDir + "/docindex.bin";
    std::string txtPath = outDir + "/docindex.txt";
    if (!docIndex.saveBinary(binPath) || !docIndex.saveText(txtPath)) {
        std::cerr << "Error: could not write the document index to " << outDir << "\n";
        return 1;
    }

    double seconds = std::chrono::duration<double>(
                         std::chrono::steady_clock::now() - start).count();

    std::cout << "\nDocument index built\n";
    std::cout << "  cases indexed   : " << docIndex.size() << "\n";
    std::cout << "  unreadable lines: " << badLines << "\n";
    std::cout << "  " << binPath << " : " << fs::file_size(binPath) << " bytes\n";
    std::cout << "  " << txtPath << " : " << fs::file_size(txtPath) << " bytes\n";
    std::cout << "  time            : " << seconds << " s\n";

    std::cout << "\nChecking the document index\n";
    if (docIndex.size() == 0 || !checkDocIndex(docIndex, binPath, inputPath)) return 1;
    std::cout << "  CHECK OK: saved file loads back identically and offsets point to the right cases\n";
    return 0;
}
