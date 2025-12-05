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

    // control 
    tok_if = -6,
    tok_then = -7,
    tok_else = -8,
    tok_for = -9,
    tok_in = -10,

    tok_unary = -11,
    tok_binary = -12,

    tok_var = -13
};

// Global variables for lexer
extern std::string IdentifierStr; // Filled in if tok_identifier
extern double NumVal;             // Filled in if tok_number
extern int CurTok;

// Lexer functions
int gettok();
int getNextToken();

#endif // LEXER_H