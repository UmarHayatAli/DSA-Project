// ---------------------------------------------------------------
// tokenizer.cpp
//
// Implementation of the tokenizer (rules are described in tokenizer.h).
//
// How it works: we walk through the text one byte at a time and build the
// current word in `current`. A letter or digit is added to the word; any
// other character ends the word ("finishes" it). Two characters get special
// treatment before they end a word: a hyphen at the end of a line (rule 1)
// and an apostrophe followed by s (rule 4).
//
// The text is UTF-8. Every non-ASCII character is made of bytes >= 128,
// so treating all bytes >= 128 as separators turns characters like “ ” — §
// into separators, as rule 2 requires.
// ---------------------------------------------------------------

#include "tokenizer.h"

#include <cctype>

// ----- small character helpers (ASCII only, safe for bytes >= 128) -----

static bool isAsciiLetterOrDigit(unsigned char c) {
    return c < 128 && std::isalnum(c);
}

static bool isAsciiUpper(unsigned char c) {
    return c >= 'A' && c <= 'Z';
}

static bool isAsciiLower(unsigned char c) {
    return c >= 'a' && c <= 'z';
}

static bool isAllDigits(const std::string& word) {
    for (unsigned char c : word) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

// Length of an apostrophe starting at text[i]:
//   1 for the straight apostrophe '      (byte 0x27)
//   3 for the curly apostrophe  ’        (UTF-8 bytes E2 80 99)
//   0 if there is no apostrophe at i
static size_t apostropheLength(const std::string& text, size_t i) {
    if (text[i] == '\'') return 1;
    if (i + 2 < text.size() &&
        static_cast<unsigned char>(text[i]) == 0xE2 &&
        static_cast<unsigned char>(text[i + 1]) == 0x80 &&
        static_cast<unsigned char>(text[i + 2]) == 0x99) {
        return 3;
    }
    return 0;
}

// Rule 1: is the hyphen at text[i] at the end of a line, with the word
// continuing in lowercase on the next line? If yes, return the index of the
// first letter on the next line; otherwise return 0 (meaning "no join").
static size_t lineEndHyphenJoin(const std::string& text, size_t i) {
    size_t j = i + 1;
    // allow spaces / tabs / '\r' between the hyphen and the newline
    while (j < text.size() && (text[j] == ' ' || text[j] == '\t' || text[j] == '\r')) j++;
    if (j >= text.size() || text[j] != '\n') return 0;    // not at the end of a line
    j++;
    // allow indentation at the start of the next line
    while (j < text.size() && (text[j] == ' ' || text[j] == '\t' || text[j] == '\r')) j++;
    if (j < text.size() && isAsciiLower(static_cast<unsigned char>(text[j]))) return j;
    return 0;
}

// ----- the tokenizer -----

// Rules 5 and 8: decide whether the finished word is kept, give it a position.
static void finishWord(std::string& current, bool capitalized,
                       uint32_t& nextPosition, std::vector<Token>& out) {
    if (current.empty()) return;

    bool tooLong = current.size() > MAX_WORD_LENGTH;
    bool longNumber = isAllDigits(current) && current.size() > MAX_NUMBER_DIGITS;
    if (!tooLong && !longNumber) {
        out.push_back(Token{current, nextPosition, capitalized});
        nextPosition++;                 // only kept words use up a position
    }
    current.clear();
}

void tokenize(const std::string& text, uint32_t& nextPosition, std::vector<Token>& out) {
    std::string current;        // the word being built
    bool capitalized = false;   // did `current` start with a capital letter?
    size_t i = 0;

    while (i < text.size()) {
        unsigned char c = static_cast<unsigned char>(text[i]);

        // Letter or digit: add it (lowercased) to the current word.
        if (isAsciiLetterOrDigit(c)) {
            if (current.empty()) capitalized = isAsciiUpper(c);   // rule 3
            current += static_cast<char>(std::tolower(c));
            i++;
            continue;
        }

        // Rule 1: hyphen at a line end inside a word -> skip it and keep building.
        if (c == '-' && !current.empty() &&
            isAsciiLower(static_cast<unsigned char>(current.back()))) {
            size_t next = lineEndHyphenJoin(text, i);
            if (next != 0) {
                i = next;
                continue;
            }
        }

        // Rule 4: apostrophe + s at the end of a word -> drop the 's.
        size_t apos = apostropheLength(text, i);
        if (apos > 0 && !current.empty()) {
            size_t s = i + apos;                       // index of the letter after the apostrophe
            bool isS = s < text.size() && (text[s] == 's' || text[s] == 'S');
            bool wordEndsAfterS = (s + 1 >= text.size()) ||
                                  !isAsciiLetterOrDigit(static_cast<unsigned char>(text[s + 1]));
            if (isS && wordEndsAfterS) {
                finishWord(current, capitalized, nextPosition, out);
                i = s + 1;                             // skip the apostrophe and the s
                continue;
            }
        }

        // Rule 2: any other character is a separator.
        finishWord(current, capitalized, nextPosition, out);
        i++;
    }
    finishWord(current, capitalized, nextPosition, out);   // last word of the text
}

std::vector<std::string> tokenizeQuery(const std::string& text) {
    std::vector<Token> tokens;
    uint32_t position = 0;
    tokenize(text, position, tokens);

    std::vector<std::string> words;
    for (const Token& t : tokens) words.push_back(t.word);
    return words;
}
