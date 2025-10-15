#include "parser.h"
#include "lexer.h"
#include <cstdio>

std::unique_ptr<ExprAST> LogError(const char* Str) {
    fprintf(stderr, "LogError: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char* Str) {
    LogError(Str);
    return nullptr;
}

/// numberexpr ::= number
std::unique_ptr<ExprAST> ParseNumberExpr() {
    auto Result = std::make_unique<NumberExprAST>(NumVal); // similar to new keyword
    getNextToken();
    return std::move(Result); // change ownership of this pointer to the function calling it
}

/// parenexpr ::= '(' expression ')'
std::unique_ptr<ExprAST> ParseParenExpr() {
    getNextToken(); // consume '('
    auto V = ParseExpression();
    if (!V) {
        return nullptr;
    }

    if (CurTok != ')') {
        return LogError("expected ')' ");
    }
    getNextToken(); // consume ')'
    return V;
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
std::unique_ptr<ExprAST> ParseIdentifierOrCallExpr() {
    std::string IdName = IdentifierStr;

    getNextToken(); // consume the identifier token

    if (CurTok == '(') {
        getNextToken(); // eat (
        std::vector<std::unique_ptr<ExprAST>> Args;
        while (true) {
            auto Arg = ParseExpression();
            if (Arg) {
                Args.push_back(std::move(Arg));
            } else {
                return nullptr;
            }

            if (CurTok == ')') {
                getNextToken(); // eat )
                break;
            } else if (CurTok == ',') {
                getNextToken(); // means there're multiple args, so eat "," and continue
                continue;
            } else {
                return LogError("Expected ')' or ',' in argument list");
            }
        }

        return std::make_unique<CallExprAST>(IdName, std::move(Args));

    } else {
        return std::make_unique<VariableExprAST>(IdName); // means its just an identifier, not a function call
    }
}

/// primary (for unary operators)
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
std::unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok) {
        case tok_identifier:
            return ParseIdentifierOrCallExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
        default:
            return LogError("unknown token when expecting an expression");
    }
}

int GetTokPrecedence() {
    switch (CurTok) {
        case '<':
        case '>':
            return 10;
        case '+':
        case '-':
            return 20;
        case '*':
        case '/':
            return 40;
        default:
            return -1;
    }
}

std::unique_ptr<ExprAST> ParseBinOpRHS(
    int ExprPrec, // precedence number
    std::unique_ptr<ExprAST> LHS) {

        while (true) {
            int TokPrec = GetTokPrecedence(); // precedence of the current token

            if (TokPrec < ExprPrec) {
                return LHS;
            } else {
                int BinOp = CurTok;
                getNextToken(); // eat the binop
                auto RHS = ParsePrimary();
                if (RHS) {
                    int NextPrec = GetTokPrecedence();
                    if (TokPrec < NextPrec) {
                        RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
                        if (!RHS) {
                            return nullptr;
                        }
                    }
                    LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
                } else {
                    return nullptr;
                }
            }
        }
}

/// expression
///   ::= primary binoprhs
///
std::unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParsePrimary();
    if (LHS) {
        return ParseBinOpRHS(0, std::move(LHS));
    }

    return nullptr;
}

/// prototype (function signature)
///   ::= id '(' id* ')'
std::unique_ptr<PrototypeAST> ParsePrototype() {
    if (CurTok != tok_identifier) {
        return LogErrorP("Expected function name in prototype");
    }

    std::string FnName = IdentifierStr;
    getNextToken(); // eat identifier

    if (CurTok != '(') {
        return LogErrorP("Expected '(' in prototype");
    }
    std::vector<std::string> ArgNames;
    while (getNextToken() == tok_identifier) {
        ArgNames.push_back(IdentifierStr);
    }
    if (CurTok != ')') {
        return LogErrorP("Expected ')' in prototype");
    }

    getNextToken();

    return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

/// definition ::= 'def'
std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken(); // eat 'def'
    auto Proto = ParsePrototype();
    if (!Proto) {
        return nullptr;
    }

    auto E = ParseExpression();
    if (E) {
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    } else {
        return nullptr;
    }
}

/// external ::= 'extern' prototype
std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken(); // eat 'extern'
    return ParsePrototype();
}

/// toplevelexpr ::= expression
std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseExpression()) {
        // Make an anonymous proto.
        auto Proto = std::make_unique<PrototypeAST>("__anon_expr",
                                                    std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}