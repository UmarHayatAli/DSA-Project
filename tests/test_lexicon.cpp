// ---------------------------------------------------------------
// test_lexicon.cpp
//
// Unit tests for the lexicon: wordIDs in order of first appearance,
// correct df and cf, lookup, and save/load.
// ---------------------------------------------------------------

#include <cstdio>
#include <iostream>
#include <string>

#include "lexicon.h"

static int passed = 0;
static int failed = 0;

static void check(const std::string& testName, bool ok) {
    (ok ? passed : failed)++;
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << testName << "\n";
}

int main() {
    std::cout << "Lexicon unit tests\n";

    // Two small "cases":
    //   doc 0: "the court the order"
    //   doc 1: "the appeal"
    Lexicon lex;
    uint32_t idThe1   = lex.addOccurrence("the", 0);
    uint32_t idCourt  = lex.addOccurrence("court", 0);
    uint32_t idThe2   = lex.addOccurrence("the", 0);
    uint32_t idOrder  = lex.addOccurrence("order", 0);
    uint32_t idThe3   = lex.addOccurrence("the", 1);
    uint32_t idAppeal = lex.addOccurrence("appeal", 1);

    check("wordIDs in order of first appearance",
          idThe1 == 0 && idCourt == 1 && idOrder == 2 && idAppeal == 3);
    check("same word always gets the same wordID", idThe1 == idThe2 && idThe2 == idThe3);
    check("number of distinct words", lex.size() == 4);
    check("cf of 'the' = 3 (total occurrences)", lex.get(idThe1).cf == 3);
    check("df of 'the' = 2 (cases containing it)", lex.get(idThe1).df == 2);
    check("df and cf of 'appeal' = 1", lex.get(idAppeal).df == 1 && lex.get(idAppeal).cf == 1);

    uint32_t id = 999;
    check("lookup finds an existing word", lex.lookup("order", id) && id == idOrder);
    check("lookup fails for an unknown word", !lex.lookup("banana", id));

    // Save to a temporary file and load it back.
    const std::string path = "test_lexicon_tmp.bin";
    Lexicon loaded;
    bool saved = lex.saveBinary(path);
    bool ok = saved && loaded.loadBinary(path) && loaded.size() == lex.size();
    for (uint32_t w = 0; ok && w < lex.size(); w++) {
        ok = loaded.get(w).word == lex.get(w).word && loaded.get(w).df == lex.get(w).df &&
             loaded.get(w).cf == lex.get(w).cf;
    }
    check("save and load give the same lexicon", ok);
    check("lookup works after loading", loaded.lookup("court", id) && id == idCourt);
    std::remove(path.c_str());

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
