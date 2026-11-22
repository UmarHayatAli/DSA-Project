// ---------------------------------------------------------------
// test_tokenizer.cpp
//
// Unit tests for the tokenizer. Each test gives an input text and the
// tokens we expect; the program prints PASS or FAIL for each one.
//
// Usage:
//   test_tokenizer                      run the unit tests
//   test_tokenizer <cases.jsonl>        run the tests, then tokenize every case in
//                                       the file and print statistics (real-data check)
// ---------------------------------------------------------------

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "json.hpp"
#include "tokenizer.h"

using json = nlohmann::json;

static int passed = 0;
static int failed = 0;

// Turn tokens into one readable string, e.g. "the@0^ appellant@1"
// (^ marks a capitalized word). Comparing strings keeps the tests short.
static std::string show(const std::vector<Token>& tokens) {
    std::string s;
    for (const Token& t : tokens) {
        if (!s.empty()) s += ' ';
        s += t.word + "@" + std::to_string(t.position) + (t.capitalized ? "^" : "");
    }
    return s;
}

static void check(const std::string& testName, const std::string& input,
                  const std::string& expected) {
    std::vector<Token> tokens;
    uint32_t position = 0;
    tokenize(input, position, tokens);
    std::string got = show(tokens);

    if (got == expected) {
        passed++;
        std::cout << "  PASS  " << testName << "\n";
    } else {
        failed++;
        std::cout << "  FAIL  " << testName << "\n"
                  << "        expected: " << expected << "\n"
                  << "        got     : " << got << "\n";
    }
}

static void runUnitTests() {
    std::cout << "Tokenizer unit tests\n";

    // Example sentence from our project brief (rules 2, 3, 4, 8).
    check("full example sentence",
          "The appellant's motion for a new trial was DENIED. See Smith v. State (1968).",
          "the@0^ appellant@1 motion@2 for@3 a@4 new@5 trial@6 was@7 denied@8^ "
          "see@9^ smith@10^ v@11 state@12^ 1968@13");

    // Rule 1: hyphen at the end of a line.
    check("hyphen join at line end", "negli-\ngence", "negligence@0");
    check("hyphen join with spaces and \\r\\n", "de- \r\n   cisions", "decisions@0");
    check("normal hyphen splits words", "well-known", "well@0 known@1");
    check("no join before a capital letter", "Smith-\nJones", "smith@0^ jones@1^");

    // Rule 2: punctuation, quotes, dashes and § are separators.
    check("punctuation and section sign", "Ark. Stat. Ann. \xC2\xA7 41-2203 (Repl. 1977)",
          "ark@0^ stat@1^ ann@2^ 41@3 2203@4 repl@5^ 1977@6");
    check("curly quotes and long dash",
          "\xE2\x80\x9CGuilty\xE2\x80\x9D\xE2\x80\x94he said",
          "guilty@0^ he@1 said@2");

    // Rule 3: capital flag.
    check("capital flag", "DENIED denied Denied", "denied@0^ denied@1 denied@2^");

    // Rule 4: possessive 's with both apostrophes.
    check("possessive with straight apostrophe", "the court's order", "the@0 court@1 order@2");
    check("possessive with curly apostrophe", "the court\xE2\x80\x99s order",
          "the@0 court@1 order@2");
    check("plural possessive", "the appellants' brief", "the@0 appellants@1 brief@2");
    check("apostrophe not followed by s", "O'Brien", "o@0^ brien@1^");

    // Rule 5: long numbers and long words are dropped (and use no position).
    check("long number dropped, year kept", "paid 123456 dollars in 1968",
          "paid@0 dollars@1 in@2 1968@3");
    check("word over 30 characters dropped",
          "ok abcdefghijklmnopqrstuvwxyzabcde ok", "ok@0 ok@1");
    check("mixed letters and digits kept", "Form 12a", "form@0^ 12a@1");

    // Rule 6: stop words are kept.
    check("stop words kept", "data structures and algorithms",
          "data@0 structures@1 and@2 algorithms@3");

    // Edge cases.
    check("empty text", "", "");
    check("only punctuation", "... !!! ---", "");

    // Rule 8: positions continue across fields of the same document.
    {
        std::vector<Token> tokens;
        uint32_t position = 0;
        tokenize("Goings v. Mills", position, tokens);     // NAME field
        tokenize("The court held", position, tokens);      // OPINION field
        std::string expected = "goings@0^ v@1 mills@2^ the@3^ court@4 held@5";
        std::string got = show(tokens);
        if (got == expected) {
            passed++;
            std::cout << "  PASS  positions continue across fields\n";
        } else {
            failed++;
            std::cout << "  FAIL  positions continue across fields\n"
                      << "        expected: " << expected << "\n        got     : " << got << "\n";
        }
    }

    // Queries are cleaned the same way.
    {
        std::vector<std::string> q = tokenizeQuery("Appellant's  MOTION, denied!");
        bool ok = q.size() == 3 && q[0] == "appellant" && q[1] == "motion" && q[2] == "denied";
        (ok ? passed : failed)++;
        std::cout << (ok ? "  PASS" : "  FAIL") << "  query cleaned like documents\n";
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
}

// Real-data check: tokenize the name, head matter and every opinion of each case.
static void tokenizeFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cout << "Cannot open " << path << "\n";
        return;
    }
    std::cout << "\nTokenizing every case in " << path << "\n";

    auto start = std::chrono::steady_clock::now();
    std::unordered_set<std::string> distinct;
    std::vector<Token> tokens;
    long long cases = 0, totalTokens = 0, capitalizedTokens = 0;
    std::string line;

    while (std::getline(in, line)) {
        json c = json::parse(line, nullptr, false);
        if (c.is_discarded()) continue;

        tokens.clear();
        uint32_t position = 0;
        if (c.contains("name") && c["name"].is_string()) tokenize(c["name"], position, tokens);
        const json& body = c["casebody"];
        if (body.contains("head_matter") && body["head_matter"].is_string())
            tokenize(body["head_matter"], position, tokens);
        if (body.contains("opinions") && body["opinions"].is_array())
            for (const json& op : body["opinions"])
                if (op.contains("text") && op["text"].is_string()) tokenize(op["text"], position, tokens);

        for (const Token& t : tokens) {
            distinct.insert(t.word);
            if (t.capitalized) capitalizedTokens++;
        }
        totalTokens += static_cast<long long>(tokens.size());

        if (cases == 0) {   // show the start of the first case
            std::cout << "  first case, first 25 tokens:\n   ";
            for (size_t k = 0; k < tokens.size() && k < 25; k++)
                std::cout << " " << tokens[k].word << (tokens[k].capitalized ? "^" : "");
            std::cout << "\n";
        }
        cases++;
    }
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    std::cout << "  cases            : " << cases << "\n"
              << "  total tokens     : " << totalTokens << "\n"
              << "  capitalized      : " << capitalizedTokens << "\n"
              << "  distinct words   : " << distinct.size() << "\n"
              << "  time             : " << seconds << " s\n";
}

int main(int argc, char* argv[]) {
    runUnitTests();
    if (argc > 1) tokenizeFile(argv[1]);
    return failed == 0 ? 0 : 1;     // non-zero exit code = some test failed
}
