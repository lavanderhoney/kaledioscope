#include "parser.h"
#include "lexer.h"
#include <cstdio>
#include <iostream>
#include <cctype>

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
///   ::= ifexpr
///   ::= forexpr
std::unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok) {
        case tok_identifier:
            return ParseIdentifierOrCallExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
        case tok_if:
            return ParseIfExpr();
        case tok_for:
            return ParseForExpr();
        default:
            std::cout << "CurTok: " << CurTok << std::endl;
            return LogError("unknown token when expecting an expression");
    }
}

std::map<char, unsigned> BinopPrecedence;
int GetTokPrecedence() {
    if (!__isascii(CurTok)){
        return -1;
    }
    // Make sure the operator is a declared binop.
    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0){
        return -1;
    }
    return TokPrec;
}

/// unary
///   ::= primary
///   ::= '!' unary -> !!x
std::unique_ptr<ExprAST> ParseUnary() {
    //base case, if its not an operator anymore, it must be a primary expr
    if (!__isascii(CurTok) || CurTok == '(' || CurTok == ','){
        return ParsePrimary();
    }

    // recursion
    int Opc = CurTok;
    getNextToken();
    auto Operand = ParseUnary();
    if (Operand){
        return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));
    }
    return nullptr;
}

/// binoprhs
///   ::= ('+' unary)*
std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, // precedence number
    std::unique_ptr<ExprAST> LHS) {    
        while (true) {
            int TokPrec = GetTokPrecedence(); // precedence of the current token
            if (TokPrec < ExprPrec) {
                return LHS;
            } 
            int BinOp = CurTok;
            getNextToken(); // eat the binop
            // Parse the unary expression after the binary operator.
            auto RHS = ParseUnary();
            if (RHS) {
                int NextPrec = GetTokPrecedence();
                if (TokPrec < NextPrec) {
                    RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
                    if (!RHS) {
                        return nullptr;
                    }
                }
                // Merge LHS/RHS
                LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
            } else {
                return nullptr;
            }
            
        }
}

/// expression
///   ::= unary binoprhs
///
std::unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParseUnary();
    if (LHS) {
        return ParseBinOpRHS(0, std::move(LHS));
    }
    return nullptr;
}

/// prototype (function signature)
///   ::= id '(' id* ')'
///   ::= binary LETTER number? (id, id)
///   ::= unary LETTER (id)
std::unique_ptr<PrototypeAST> ParsePrototype() {
    std::string FnName;

    unsigned Kind = 0; // 0 = identifier, 1 = unary, 2 = binary
    unsigned BinaryPrecedence = 20;

    switch (CurTok){
        case tok_identifier:
            FnName = IdentifierStr;
            Kind = 0;
            getNextToken();
            break;
        case tok_binary:
            getNextToken();
            if (!__isascii(CurTok)){
                return LogErrorP("Expected binary operator");
            }
            Kind = 2;
            FnName = "binary";
            FnName += (char)CurTok;
            getNextToken();

            // read the precedence number
            if (CurTok == tok_number){
                if (NumVal < 1 || NumVal > 100){
                    return LogErrorP("Precedence value must be in range 1...100");
                }
                BinaryPrecedence = NumVal;
            }
            getNextToken();
            break;
        case tok_unary:
            getNextToken();
            if (!__isascii(CurTok)){
                return LogErrorP("Expected unary operator");
            }
            FnName = "unary";
            FnName += (char)CurTok;
            Kind = 1;
            getNextToken();
            break;
        default:
            return LogErrorP("Expected function name in prototype");
            break;
    }

    if (CurTok != '(') {
        return LogErrorP("Expected '(' in prototype");
    }
    std::vector<std::string> ArgNames;
    while (getNextToken() == tok_identifier){
        ArgNames.push_back(IdentifierStr);
    }
    if (CurTok != ')') {
        return LogErrorP("Expected ')' in prototype");
    }
    getNextToken(); //eat ')'

     // Verify right number of names for operator.
    if (Kind && ArgNames.size() != Kind){
        return LogErrorP("Invalid number of operands for operator");
    }

    return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames), Kind!=0, BinaryPrecedence);
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

/// ifexpr ::= 'if' expression 'then' expression 'else' expression
std::unique_ptr<IfExprAST> ParseIfExpr(){
    getNextToken(); // eat 'if'

    auto condn = ParseExpression(); // evaluate the expression
    if (!condn){
        return nullptr;
    }

    if (CurTok != tok_then){
        LogError("Expected 'then' !");
        return nullptr;
    }
    getNextToken(); // eat 'then'
    auto then_body = ParseExpression();
    if (!then_body){
        return nullptr;
    }

    if (CurTok != tok_else){
        LogError("'else' expected after 'if - then' !");
        return nullptr;
    }
    getNextToken(); // eat 'else'
    auto else_body = ParseExpression();
    if (!else_body){
        return nullptr;
    }

    return std::make_unique<IfExprAST>(std::move(condn), std::move(then_body), std::move(else_body));
}

/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
std::unique_ptr<ForExprAST> ParseForExpr(){
    getNextToken();
    if (CurTok != tok_identifier){
        LogError("expected identifier after for");
        return nullptr;
    }
    std::string IdName = IdentifierStr;
    getNextToken(); //eat identifier

    if (CurTok != '='){
        LogError("expected '=' after for ");
        return nullptr;
    }
    getNextToken(); //eat '='
    auto Start = ParseExpression();
    if (!Start)
        return nullptr;

    if (CurTok != ','){
        LogError("expected ',' after for start value");
        return nullptr;
    }
    getNextToken();

    auto End = ParseExpression();
    if (!End)
        return nullptr;

    // The step value is optional
    std::unique_ptr<ExprAST> Step;
    if (CurTok == ',') {
        getNextToken();
        Step = ParseExpression();
        if (!Step)
            return nullptr;
    }
    if (CurTok != tok_in){
        LogError("expected 'in' after for");
        return nullptr;
    }
    getNextToken();  // eat 'in'.

    auto Body = ParseExpression();
    if (!Body)
      return nullptr;

    return std::make_unique<ForExprAST>(IdName, std::move(Start), std::move(End), std::move(Step), std::move(Body));
    
}

/// toplevelexpr ::= expression
std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    auto E = ParseExpression();
    if (E) {
        // Make an anonymous proto.
        auto Proto = std::make_unique<PrototypeAST>("__anon_expr",
                                                    std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}