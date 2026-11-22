// ---------------------------------------------------------------
// tokenizer.h
//
// TOKENIZER: turns raw text into a list of clean words ("tokens").
// The SAME tokenizer is used for case text (when indexing) and for the
// user's search query, so both are cleaned in exactly the same way.
//
// Rules (in this order):
//   1. A word broken by a hyphen at the end of a line is joined again:
//        "negli-\ngence" -> "negligence"
//      (only when the next line continues with a lowercase letter)
//   2. Every character that is not an English letter or digit is a separator:
//      spaces, punctuation, quotes, dashes, the section sign (§), and any
//      non-ASCII character (curly quotes, bullets, accented letters, ...).
//   3. We remember whether the word started with a capital letter
//      (e.g. names like "Smith"), then convert it to lowercase.
//   4. A possessive 's is removed: "appellant's" / "appellant’s" -> "appellant"
//      (both the straight ' and the curly ’ apostrophe are handled).
//   5. Words longer than 30 characters are dropped (usually OCR garbage),
//      and numbers longer than 4 digits are dropped (years like 1968 are kept).
//   6. Stop words ("the", "and", "of", ...) are KEPT: the project requires a
//      query like "data structures and algorithms" to match all four words.
//   7. No stemming ("motions" and "motion" stay different words).
//   8. Every kept word gets a position number. Positions continue across
//      calls, so one document's name, head matter and opinions share one
//      continuous numbering (0, 1, 2, ...).
//
// Example:
//   "The appellant's motion ... DENIED."  ->  the@0(cap) appellant@1 motion@2 ... denied@8(cap)
// ---------------------------------------------------------------
#pragma once

#include <cstdint>
#include <string>
#include <vector>

// One cleaned word and where it was found.
struct Token {
    std::string word;       // lowercase word, e.g. "appellant"
    uint32_t position;      // position in the document (0, 1, 2, ...)
    bool capitalized;       // true if the original started with a capital letter
};

// Limits from rule 5.
const size_t MAX_WORD_LENGTH = 30;
const size_t MAX_NUMBER_DIGITS = 4;

// Tokenize `text` and APPEND the tokens to `out`.
// `nextPosition` is the position given to the first new token; it is
// increased for every kept token, so the caller can continue numbering
// in the next field of the same document.
void tokenize(const std::string& text, uint32_t& nextPosition, std::vector<Token>& out);

// Convenience version for search queries: returns only the words.
std::vector<std::string> tokenizeQuery(const std::string& text);
