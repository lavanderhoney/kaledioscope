#ifndef LEXER_H
#define LEXER_H

#include <string>

enum Token {
    tok_eof = -1,

    // commands
    tok_def = -2,
    tok_extern = -3,

    // primary dtypes
    tok_identifier = -4,
    tok_number = -5,
};

// Global variables for lexer
extern std::string IdentifierStr; // Filled in if tok_identifier
extern double NumVal;             // Filled in if tok_number
extern int CurTok;

// Lexer functions
int gettok();
int getNextToken();

#endif // LEXER_H