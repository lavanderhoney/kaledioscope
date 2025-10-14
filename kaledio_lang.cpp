#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;

enum Token{
    tok_eof = -1,

    // commands
    tok_def = -2,
    tok_extern = -3,

    //primary dtypes
    tok_identifier = -4,
    tok_number = -5,
};

//global variables
static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number

static int gettok(){
    static int LastChar = ' ';

    //consume white spaces
    while(isspace(LastChar)){
        LastChar = getchar(); //getchar() reads one char from the input stream
    }

    //identifier => [a-zA-Z][a-zA-Z0-9]*
    if (isalpha(LastChar)){
        IdentifierStr = LastChar;

        while (isalnum((LastChar = getchar()))){
            IdentifierStr += LastChar;
        }

        if (IdentifierStr == "def"){
            return tok_def;
        }
        if (IdentifierStr == "extern"){
            return tok_extern;
        }
        return tok_identifier;
    }
    
    // read the number/digit (floating val) as a str, then convert to double
    if (isdigit(LastChar) || LastChar == '.'){
        std::string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        }while(isdigit(LastChar) || LastChar == '.');
        NumVal = strtod(NumStr.c_str(), 0);
        return tok_number;
    }

    //handle comments
    if (LastChar == '#'){
        do{
            LastChar = getchar();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        // skip the comment and look for new token
        if (LastChar != EOF ){
            return gettok();
        }
    }

    if (LastChar == EOF){
        return tok_eof;
    }

    // returns the ASCII
    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}

//---------- Parser --------------//


class ExprAST{
public:
    virtual ~ExprAST() = default;
    virtual Value *codegen() = 0;
};

class NumberExprAST : public ExprAST {
    double Val;
public:
    NumberExprAST(double V) : Val(V) {} // list initializer, like this.Val = V
    Value *codegen() override;
}; 


/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;
public:
  VariableExprAST(const std::string &N) : Name(N) {}
  Value *codegen() override;
};


class BinaryExprAST : public ExprAST {
    char Op; // + - * / <>
    std::unique_ptr<ExprAST> LHS, RHS;
public:
    BinaryExprAST(
        char op,
        std::unique_ptr<ExprAST> lhs,
        std::unique_ptr<ExprAST> rhs
    ) : Op(op), LHS(std::move(lhs)), RHS(std::move(rhs)) {}
    Value *codegen() override;
};


/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
    std::string Callee; // name of the function
    std::vector<std::unique_ptr<ExprAST>> Args;
public:
    CallExprAST(const std::string &Callee,
                            std::vector<std::unique_ptr<ExprAST>> Args)
            : Callee(Callee), Args(std::move(Args)) {}
    Value *codegen() override;
};


/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number  of arguments the function takes).
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
public:
    PrototypeAST(const std::string &Name, std::vector<std::string> Args)
      : Name(Name), Args(std::move(Args)) {}    
    Function *codegen();
    const std::string &getName() const { return Name; }
};


/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;
public:
    FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
    : Proto(std::move(Proto)), Body(std::move(Body)) {}
    Function *codegen();
};


static int CurTok;

static int getNextToken(){
    return CurTok = gettok();
}

std::unique_ptr<ExprAST> LogError(const char* Str){
    fprintf(stderr, "LogError: %s\n",Str);
    return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char* Str){
    LogError(Str);
    return nullptr;
}

// ------------------ Grammar ----------------------
static std::unique_ptr<ExprAST> ParseExpression();

/// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr(){
    auto Result = std::make_unique<NumberExprAST>(NumVal); // similar to new keyword
    getNextToken();
    return std::move(Result); //change ownership of this pointer to the function calling it
}

/// parenexpr ::== '(' expression ')'
static std::unique_ptr<ExprAST> ParseParenExpr(){
    getNextToken(); // consume '(' 
    auto V = ParseExpression();
    if (!V)
    {
        return nullptr; 
    }

    if (CurTok != ')'){
        return LogError("expected ')' ");
    }
    getNextToken(); // consume ')'
    return V;   
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierOrCallExpr(){
    std::string IdName = IdentifierStr;

    getNextToken(); //consume the identifier token

    if (CurTok == '('){
        getNextToken(); // eat (
        std::vector<std::unique_ptr<ExprAST>> Args;
        while (true){
            auto Arg = ParseExpression();
            if (Arg){
                Args.push_back(std::move(Arg));
            }else{
                return nullptr;
            }

            if (CurTok == ')'){
                getNextToken(); // eat )
                break;
            }else if (CurTok == ','){
                getNextToken(); // means there're multiple args, so eat "," and continue
                continue;
            }else{
                return LogError("Expected ')' or ',' in argument list");
            }
        }

        return std::make_unique<CallExprAST>(IdName, std::move(Args));
        
    }else{
        return std::make_unique<VariableExprAST>(IdName); // means its just an identifier, not a function call
    }
}

/// primary (for unary operators)
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
static std::unique_ptr<ExprAST> ParsePrimary(){
    switch (CurTok){
        case tok_identifier:
            return ParseIdentifierOrCallExpr();
        case tok_number:
            return ParseNumberExpr();
        case '(':
            return ParseParenExpr();
        default:
            return LogError("unkown token when expecting an expression");
    }
}

static int GetTokPrecedence(){
    switch (CurTok){
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

static std::unique_ptr<ExprAST> ParseBinOpRHS(
    int ExprPrec, //precedence number
    std::unique_ptr<ExprAST> LHS){
        
        while (true){
            int TokPrec = GetTokPrecedence();

            if (TokPrec < ExprPrec) {
                return LHS; 
            } else {
                int BinOp = CurTok;
                getNextToken(); // eat the binop
                auto RHS = ParsePrimary();
                if (RHS) { 
                    int NextPrec = GetTokPrecedence();
                    if (TokPrec < NextPrec){
                        RHS = ParseBinOpRHS(TokPrec+1, std::move(RHS));
                        if (!RHS){
                            return nullptr;
                        }
                    }
                    LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
                } else{
                    return nullptr;
                }
            }
        }
}


/// expression
///   ::= primary binoprhs
///
static std::unique_ptr<ExprAST> ParseExpression(){
    auto LHS = ParsePrimary();
    if (LHS){
        return ParseBinOpRHS(0, std::move(LHS));
    }
    
    return nullptr;
}

/// prototype (function signature)
///   ::= id '(' id* ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
    if (CurTok != tok_identifier){
        return LogErrorP("Expected function name in prototype");
    }

    std::string FnName = IdentifierStr;
    getNextToken(); // eat identifier

    if (CurTok != '('){
        return LogErrorP("Expected '(' in prototype");
    }
    std::vector<std::string> ArgNames;
    while (getNextToken() == tok_identifier){
        ArgNames.push_back(IdentifierStr);
    } 
    if (CurTok != ')'){
        return LogErrorP("Expected ')' in prototype");
    }
    
    getNextToken();

    return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

/// definition ::= 'def' 
static std::unique_ptr<FunctionAST> ParseDefinition(){
    getNextToken(); // eat 'def'
    auto Proto = ParsePrototype();
    if (!Proto ){
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
static std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken(); // eat 'extern'
    return ParsePrototype();
}

/// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    // Make an anonymous proto.
    auto Proto = std::make_unique<PrototypeAST>("__anon_expr",
                                                std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//
static std::unique_ptr<LLVMContext> TheContext;
static std::unique_ptr<IRBuilder<>> Builder;
static std::unique_ptr<Module> TheModule;
static std::map<std::string, Value *> NamedValues; // a symboltable, value->llvm ir
Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}

Value *NumberExprAST::codegen(){
    return ConstantFP::get(Type::getDoubleTy(*TheContext), APFloat(Val));
}

Value *VariableExprAST::codegen(){
    // find the variable name in the symbol table. We assume that the variable has already been emitted somewhere and its value is available
    Value *V = NamedValues[Name];
    if (!V){
        LogErrorV("Unknown variable name");
    }
    return V;
}

Value *BinaryExprAST::codegen(){
    Value *L = LHS->codegen();
    Value *R = RHS->codegen();

    if (!L||!R){
        return nullptr;
    }

    switch (Op){
    case '+':
        return Builder->CreateFAdd(L, R, "addtmp");
    case '-':
        return Builder->CreateFSub(L, R, "subtmp");
    case '*':
        return Builder->CreateFMul(L, R, "multmp");
    case '/':
        return Builder->CreateFDiv(L, R, "divtmp");
    case '<':
        L = Builder->CreateFCmpULT(L, R, "cmptmp");
        // Convert bool 0/1 to double 0.0 or 1.0
        return Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext),"booltmp");
    default:
        return LogErrorV("invalid binary operator");
    }
}

Value *CallExprAST::codegen(){
    Function *CalleeF = TheModule->getFunction(Callee);
    if (!CalleeF)
        return LogErrorV("Unknown function referenced");

    // If argument mismatch error.
    if (CalleeF->arg_size() != Args.size())
        return LogErrorV("Incorrect # arguments passed");

    std::vector<Value *> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
      ArgsV.push_back(Args[i]->codegen());
      if (!ArgsV.back())
        return nullptr;
    }
    return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

Function *PrototypeAST::codegen() {
  // Make the function type:  double(double,double) etc.
    std::vector<Type*> Doubles(Args.size(), Type::getDoubleTy(*TheContext)); // N LLVM Double types for N args
    FunctionType *FT = FunctionType::get(Type::getDoubleTy(*TheContext), Doubles, false); // creates a function type with N doubles as args

    Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get()); //actually creates the IR Function corresponding to the Prototype

    // Set names for all arguments.
    unsigned Idx = 0;
    for (auto &Arg : F->args())
      Arg.setName(Args[Idx++]);

    return F;
}

Function *FunctionAST::codegen() {
    // First, check for an existing function from a previous 'extern' declaration.
    Function *TheFunction = TheModule->getFunction(Proto->getName());   
    if (!TheFunction)
      TheFunction = Proto->codegen();   
    if (!TheFunction)
      return nullptr;   
    if (!TheFunction->empty())
      return (Function*)LogErrorV("Function cannot be redefined.");
    // Create a new basic block named entry to start insertion into.
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    NamedValues.clear();
    for (auto &Arg : TheFunction->args()){
        NamedValues[std::string(Arg.getName())] = &Arg;
    }
    if (Value *RetVal = Body->codegen()) {
        // Finish off the function.
        Builder->CreateRet(RetVal);

        // Validate the generated code, checking for consistency.
        verifyFunction(*TheFunction);

        return TheFunction;
    }
}

//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//

static void InitializeModule() {
  // Open a new context and module.
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("my cool jit", *TheContext);

  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*TheContext);
}

static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read function definition:");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      // Print the full module IR after the definition
      TheModule->print(errs(), nullptr);
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      fprintf(stderr, "Read extern: ");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      // Print the full module IR after the definition
      TheModule->print(errs(), nullptr);
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = ParseTopLevelExpr()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read top-level expression:");
      FnIR->print(errs());
      fprintf(stderr, "\n");
      // Print the full module IR after the definition
      TheModule->print(errs(), nullptr);

      // Remove the anonymous expression.
      FnIR->eraseFromParent();
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
static void MainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons.
      getNextToken();
      break;
    case tok_def:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}


//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main(){
    
    // Prime the first token.
    fprintf(stderr, "ready> ");
    getNextToken(); 

    // Make the module, which holds all the code.
    InitializeModule(); 

    // Run the main "interpreter loop" now.
    MainLoop(); 

    // Print out all of the generated code.
    TheModule->print(errs(), nullptr);
    return 0;
}