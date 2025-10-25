#include "lexer.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>

// Global variables
std::string IdentifierStr; // Filled in if tok_identifier
double NumVal;             // Filled in if tok_number
int CurTok;

int gettok() {
    static int LastChar = ' ';

    // consume white spaces
    while (isspace(LastChar)) {
        LastChar = getchar(); // getchar() reads one char from the input stream
    }

    // identifier => [a-zA-Z][a-zA-Z0-9]*
    if (isalpha(LastChar)) {
        IdentifierStr = LastChar;

        while (isalnum((LastChar = getchar()))) {
            IdentifierStr += LastChar;
        }

        if (IdentifierStr == "def") {
            return tok_def;
        }
        if (IdentifierStr == "extern") {
            return tok_extern;
        }
        if (IdentifierStr == "if"){
            return tok_if;
        }
        if (IdentifierStr == "then"){
            return tok_then;
        }
        if (IdentifierStr == "else"){
            return tok_else;
        }
        if (IdentifierStr == "for"){
            return tok_for;
        }
        if (IdentifierStr == "in"){
            return tok_in;
        }
        return tok_identifier;
    }

    // read the number/digit (floating val) as a str, then convert to double
    if (isdigit(LastChar) || LastChar == '.') {
        std::string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar) || LastChar == '.');
        NumVal = strtod(NumStr.c_str(), 0);
        return tok_number;
    }

    // handle comments
    if (LastChar == '#') {
        do {
            LastChar = getchar();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        // skip the comment and look for new token
        if (LastChar != EOF) {
            return gettok();
        }
    }

    if (LastChar == EOF) {
        return tok_eof;
    }

    // returns the ASCII
    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}

int getNextToken() {
    return CurTok = gettok();
}