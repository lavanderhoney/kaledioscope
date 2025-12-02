#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include <memory>
#include <map>

// Error logging for parser
std::unique_ptr<ExprAST> LogError(const char* Str);
std::unique_ptr<PrototypeAST> LogErrorP(const char* Str);

// Parsing functions
std::unique_ptr<ExprAST> ParseExpression();
std::unique_ptr<ExprAST> ParseNumberExpr();
std::unique_ptr<ExprAST> ParseParenExpr();
std::unique_ptr<ExprAST> ParseIdentifierOrCallExpr();
std::unique_ptr<ExprAST> ParsePrimary();
std::unique_ptr<ExprAST> ParseUnary();
std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS);
std::unique_ptr<PrototypeAST> ParsePrototype();
std::unique_ptr<FunctionAST> ParseDefinition();
std::unique_ptr<PrototypeAST> ParseExtern();
std::unique_ptr<IfExprAST> ParseIfExpr();
std::unique_ptr<ForExprAST> ParseForExpr();
std::unique_ptr<FunctionAST> ParseTopLevelExpr();

// Precedence helper
int GetTokPrecedence();
extern std::map<char, unsigned> BinopPrecedence;

#endif // PARSER_H