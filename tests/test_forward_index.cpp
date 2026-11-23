// ---------------------------------------------------------------
// test_forward_index.cpp
//
// Unit tests for the forward index: hit packing, grouping tokens by
// wordID, title/capital flags, and writing + reading back (in order and
// by jumping to one case).
// ---------------------------------------------------------------

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "forward_index.h"
#include "lexicon.h"
#include "tokenizer.h"

static int passed = 0;
static int failed = 0;

static void check(const std::string& testName, bool ok) {
    (ok ? passed : failed)++;
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << testName << "\n";
}

// Tokenize a name + body like build_index does, look up wordIDs, build the ForwardDoc.
static ForwardDoc makeDoc(uint32_t docID, const std::string& name, const std::string& body,
                          Lexicon& lex) {
    std::vector<Token> tokens;
    uint32_t position = 0;
    tokenize(name, position, tokens);
    size_t titleTokens = tokens.size();
    tokenize(body, position, tokens);
    std::vector<uint32_t> wordIDs;
    for (const Token& t : tokens) wordIDs.push_back(lex.addOccurrence(t.word, docID));
    ForwardDoc doc;
    buildForwardDoc(docID, tokens, wordIDs, titleTokens, doc);
    return doc;
}

static bool same(const ForwardDoc& a, const ForwardDoc& b) {
    if (a.docID != b.docID || a.numTokens != b.numTokens || a.words.size() != b.words.size()) {
        return false;
    }
    for (size_t i = 0; i < a.words.size(); i++) {
        if (a.words[i].wordID != b.words[i].wordID || a.words[i].hits != b.words[i].hits) {
            return false;
        }
    }
    return true;
}

int main() {
    std::cout << "Forward index unit tests\n";

    // ----- hit packing -----
    uint32_t hit = makeHit(12345, true, false);
    check("hit keeps position and capital flag",
          hitPosition(hit) == 12345 && hitCapitalized(hit) && !hitInTitle(hit));
    hit = makeHit(7, false, true);
    check("hit keeps title flag", hitPosition(hit) == 7 && !hitCapitalized(hit) && hitInTitle(hit));
    hit = makeHit(HIT_MAX_POSITION, true, true);
    check("largest position fits", hitPosition(hit) == HIT_MAX_POSITION);

    // ----- building one case -----
    //   name: "Smith v. State"      -> smith@0^T v@1T state@2^T
    //   body: "The state appealed. Smith lost. the state"
    //         -> the@3^ state@4 appealed@5 smith@6^ lost@7 the@8 state@9
    Lexicon lex;
    ForwardDoc d0 = makeDoc(0, "Smith v. State", "The state appealed. Smith lost. the state", lex);
    uint32_t idSmith, idState, idThe;
    lex.lookup("smith", idSmith);
    lex.lookup("state", idState);
    lex.lookup("the", idThe);

    check("numTokens = all words of name + body", d0.numTokens == 10);
    check("each distinct word appears once", d0.words.size() == 6);
    bool sorted = true;
    for (size_t i = 1; i < d0.words.size(); i++) sorted &= d0.words[i - 1].wordID < d0.words[i].wordID;
    check("words are sorted by wordID", sorted);

    const ForwardEntry* state = nullptr;
    const ForwardEntry* smith = nullptr;
    const ForwardEntry* the = nullptr;
    for (const ForwardEntry& e : d0.words) {
        if (e.wordID == idState) state = &e;
        if (e.wordID == idSmith) smith = &e;
        if (e.wordID == idThe) the = &e;
    }
    check("'state' has 3 hits at positions 2, 4, 9",
          state && state->hits.size() == 3 && hitPosition(state->hits[0]) == 2 &&
              hitPosition(state->hits[1]) == 4 && hitPosition(state->hits[2]) == 9);
    check("title flag only on hits from the name",
          state && hitInTitle(state->hits[0]) && !hitInTitle(state->hits[1]) &&
              !hitInTitle(state->hits[2]));
    check("capital flag follows the original text",
          smith && hitCapitalized(smith->hits[0]) && hitCapitalized(smith->hits[1]) && the &&
              hitCapitalized(the->hits[0]) && !hitCapitalized(the->hits[1]));

    ForwardDoc empty = makeDoc(1, "", "", lex);
    check("a case with no words gives an empty record", empty.numTokens == 0 && empty.words.empty());

    // ----- write and read back -----
    ForwardDoc d2 = makeDoc(2, "Jones v. Smith", "Jones appealed the order of the court.", lex);
    const std::string bin = "test_forward_tmp.bin", off = "test_forward_tmp_off.bin";
    ForwardIndexWriter writer;
    bool written = writer.open(bin) && writer.add(d0) && writer.add(empty) && writer.add(d2);
    check("adding a case out of docID order is refused", !writer.add(d0));
    written = written && writer.close(off);
    check("write forward index", written);
    check("writer counts all hits", writer.totalHits() == d0.numTokens + d2.numTokens);

    {   // inner block: the reader closes its files at the end, so they can be deleted (Windows)
    ForwardIndexReader reader;
    bool opened = reader.open(bin, off);
    check("open forward index", opened && reader.size() == 3);

    ForwardDoc r;
    bool inOrder = opened;
    reader.rewind();
    inOrder = inOrder && reader.next(r) && same(r, d0);
    inOrder = inOrder && reader.next(r) && same(r, empty);
    inOrder = inOrder && reader.next(r) && same(r, d2);
    inOrder = inOrder && !reader.next(r);
    check("reading in order gives the same cases, then stops", inOrder);

    check("jump to the last case", opened && reader.read(2, r) && same(r, d2));
    check("jump back to the first case", opened && reader.read(0, r) && same(r, d0));
    check("reading a docID that does not exist fails", !reader.read(3, r));

    ForwardIndexReader wrong;
    check("opening with a missing offset file fails", !wrong.open(bin, "no_such_file.bin"));
    }

    std::remove(bin.c_str());
    std::remove(off.c_str());

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
