// ---------------------------------------------------------------
// test_inverted_index.cpp
//
// Unit tests for the inverted index: building it from a small forward index
// (in one batch and in many small batches), the content of the lists,
// reading back (in order and by jumping to one word), and refusing a
// lexicon that does not match the forward index.
// ---------------------------------------------------------------

#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "forward_index.h"
#include "inverted_index.h"
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

static std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// docIDs of a list, e.g. {0, 2}.
static std::vector<uint32_t> docsOf(const PostingList& list) {
    std::vector<uint32_t> ids;
    for (const Posting& p : list.docs) ids.push_back(p.docID);
    return ids;
}

// Positions of the hits of one posting, e.g. {2, 4, 9}.
static std::vector<uint32_t> positionsOf(const Posting& p) {
    std::vector<uint32_t> pos;
    for (uint32_t hit : p.hits) pos.push_back(hitPosition(hit));
    return pos;
}

int main() {
    std::cout << "Inverted index unit tests\n";

    // ----- a small forward index with 4 cases -----
    //   0: smith@0^T v@1T state@2^T | the@3^ state@4 appealed@5 smith@6^ lost@7 the@8 state@9
    //   1: (no words)
    //   2: jones@0^T v@1T smith@2^T | jones@3^ appealed@4 the@5 order@6 of@7 the@8 court@9
    //   3: state@0^T v@1T brown@2^T | brown@3^ appealed@4
    Lexicon lex;
    std::vector<ForwardDoc> docs;
    docs.push_back(makeDoc(0, "Smith v. State", "The state appealed. Smith lost. the state", lex));
    docs.push_back(makeDoc(1, "", "", lex));
    docs.push_back(makeDoc(2, "Jones v. Smith", "Jones appealed the order of the court.", lex));
    docs.push_back(makeDoc(3, "State v. Brown", "Brown appealed.", lex));

    const std::string fwdBin = "test_inv_fwd_tmp.bin", fwdOff = "test_inv_fwd_tmp_off.bin";
    const std::string invBin = "test_inv_tmp.bin", invOff = "test_inv_tmp_off.bin";
    const std::string invBin2 = "test_inv_tmp2.bin", invOff2 = "test_inv_tmp2_off.bin";
    const std::string badBin = "test_inv_bad_tmp.bin", badOff = "test_inv_bad_tmp_off.bin";

    ForwardIndexWriter writer;
    bool written = writer.open(fwdBin);
    for (const ForwardDoc& d : docs) written = written && writer.add(d);
    written = written && writer.close(fwdOff);
    check("write the forward index used by the tests", written);
    uint64_t totalTokens = writer.totalHits();

    {   // inner block: readers close their files at the end, so they can be deleted (Windows)
    ForwardIndexReader forward;
    bool fwdOpened = forward.open(fwdBin, fwdOff);

    // ----- build in one batch -----
    InvertedBuildStats stats;
    std::string error;
    bool built = fwdOpened &&
                 buildInvertedIndex(forward, lex, invBin, invOff, 1 << 20, stats, error);
    check("build with a large budget (one batch)", built && stats.passes == 1);
    check("stats count all hits and (word, case) pairs",
          stats.totalHits == totalTokens && stats.totalPostings == 18);   // 6 + 0 + 8 + 4 distinct words

    // ----- build in many tiny batches: same files -----
    InvertedBuildStats stats2;
    bool built2 = fwdOpened &&
                  buildInvertedIndex(forward, lex, invBin2, invOff2, 1, stats2, error);
    check("build with a tiny budget (one word per batch)",
          built2 && stats2.passes == lex.size());
    check("both builds give byte-identical files",
          built && built2 && readFile(invBin) == readFile(invBin2) &&
              readFile(invOff) == readFile(invOff2));

    uint64_t expectedSize = 8;
    for (uint32_t w = 0; w < lex.size(); w++) {
        expectedSize += postingListBytes(lex.get(w).df, lex.get(w).cf);
    }
    check("file size = header + sum of list sizes", readFile(invBin).size() == expectedSize);

    // ----- content of the lists -----
    InvertedIndexReader reader;
    bool opened = reader.open(invBin, invOff);
    check("open inverted index", opened && reader.size() == lex.size());

    uint32_t idSmith = 0, idState = 0, idAppealed = 0, idThe = 0;
    lex.lookup("smith", idSmith);
    lex.lookup("state", idState);
    lex.lookup("appealed", idAppealed);
    lex.lookup("the", idThe);
    PostingList list;

    bool ok = opened && reader.read(idSmith, list);
    check("'smith' is in cases 0 and 2", ok && docsOf(list) == std::vector<uint32_t>{0, 2});
    check("'smith' hits: case 0 at 0 and 6, case 2 at 2",
          ok && list.docs.size() == 2 && positionsOf(list.docs[0]) == std::vector<uint32_t>{0, 6} &&
              positionsOf(list.docs[1]) == std::vector<uint32_t>{2});
    check("title and capital flags are kept",
          ok && list.docs.size() == 2 && hitInTitle(list.docs[0].hits[0]) &&
              !hitInTitle(list.docs[0].hits[1]) && hitCapitalized(list.docs[0].hits[1]) &&
              hitInTitle(list.docs[1].hits[0]));

    ok = opened && reader.read(idState, list);
    check("'state': case 0 at 2, 4, 9 and case 3 at 0",
          ok && docsOf(list) == std::vector<uint32_t>{0, 3} &&
              positionsOf(list.docs[0]) == std::vector<uint32_t>{2, 4, 9} &&
              positionsOf(list.docs[1]) == std::vector<uint32_t>{0});

    ok = opened && reader.read(idAppealed, list);
    check("'appealed' cases are sorted by docID: 0, 2, 3",
          ok && docsOf(list) == std::vector<uint32_t>{0, 2, 3});

    ok = opened && reader.read(idThe, list);
    check("'the': case 0 at 3, 8 and case 2 at 5, 8",
          ok && docsOf(list) == std::vector<uint32_t>{0, 2} &&
              positionsOf(list.docs[0]) == std::vector<uint32_t>{3, 8} &&
              positionsOf(list.docs[1]) == std::vector<uint32_t>{5, 8});

    // ----- reading every list in order -----
    bool inOrder = opened;
    bool emptyCaseAbsent = true;
    bool countsMatch = true;
    uint64_t hitsRead = 0;
    reader.rewind();
    for (uint32_t w = 0; inOrder && w < lex.size(); w++) {
        inOrder = reader.next(list) && list.wordID == w;
        if (!inOrder) break;
        uint32_t cf = 0;
        for (const Posting& p : list.docs) {
            if (p.docID == 1) emptyCaseAbsent = false;
            cf += static_cast<uint32_t>(p.hits.size());
        }
        countsMatch &= list.docs.size() == lex.get(w).df && cf == lex.get(w).cf;
        hitsRead += cf;
    }
    inOrder = inOrder && !reader.next(list);
    check("reading in order gives wordIDs 0, 1, 2, ..., then stops", inOrder);
    check("df and cf of every list match the lexicon", inOrder && countsMatch);
    check("total hits read = total tokens", hitsRead == totalTokens);
    check("the case with no words is in no list", emptyCaseAbsent);

    // ----- jumping -----
    uint32_t last = static_cast<uint32_t>(lex.size() - 1);
    check("jump to the last word", opened && reader.read(last, list) && list.wordID == last);
    check("jump back to the first word", opened && reader.read(0, list) && list.wordID == 0);
    check("reading a wordID that does not exist fails", !reader.read(last + 1, list));

    InvertedIndexReader wrong;
    check("opening with a missing offset file fails", !wrong.open(invBin, "no_such_file.bin"));
    check("opening the forward index as inverted fails", !wrong.open(fwdBin, fwdOff));

    // ----- a lexicon that does not match the forward index -----
    Lexicon bad = lex;
    bad.addOccurrence("ghost", 9);     // a word that is not in the forward index
    std::string badError;
    InvertedBuildStats badStats;
    bool refused = fwdOpened &&
                   !buildInvertedIndex(forward, bad, badBin, badOff, 1 << 20, badStats, badError);
    check("a lexicon that does not match the forward index is refused",
          refused && badError.find("ghost") != std::string::npos);
    }

    for (const std::string& f : {fwdBin, fwdOff, invBin, invOff, invBin2, invOff2, badBin, badOff}) {
        std::remove(f.c_str());
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
