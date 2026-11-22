// ---------------------------------------------------------------
// build_index.cpp
//
// Main program of the indexer. For now (Task 2: project skeleton) it only
// checks that the build works and that we can read the dataset:
//   - opens a .jsonl file (one case per line),
//   - parses each line with nlohmann/json,
//   - prints how many cases it read and the first few titles.
// Later tasks turn this into the real indexer (document index, lexicon,
// forward index, inverted index).
//
// Usage:
//   build_index <cases.jsonl>            e.g. build_index sample/sample.jsonl
// ---------------------------------------------------------------

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

#include "json.hpp"

using json = nlohmann::json;

int main(int argc, char* argv[]) {
    // Input file: first command-line argument, or the sample by default.
    std::string inputPath = (argc > 1) ? argv[1] : "sample/sample.jsonl";

    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        std::cerr << "Error: cannot open " << inputPath << "\n";
        return 1;
    }

    auto start = std::chrono::steady_clock::now();

    std::string line;
    long long caseCount = 0;
    long long badLines = 0;

    // Read the file one line (= one case) at a time, so the whole
    // dataset is never in memory at once.
    while (std::getline(in, line)) {
        if (line.empty()) continue;

        // parse(..., nullptr, false) returns a "discarded" value instead of
        // throwing an exception when the JSON is broken.
        json c = json::parse(line, nullptr, false);
        if (c.is_discarded()) {
            badLines++;
            continue;
        }

        // Print the first 3 cases as a quick check.
        if (caseCount < 3) {
            std::cout << "  docID " << caseCount << ": "
                      << c.value("name_abbreviation", "(no title)") << " | "
                      << c.value("decision_date", "?") << "\n";
        }
        caseCount++;
    }

    auto end = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();

    std::cout << "Read " << caseCount << " cases from " << inputPath
              << " (" << badLines << " unreadable lines) in "
              << seconds << " s\n";
    return 0;
}
