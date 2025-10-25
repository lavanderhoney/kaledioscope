#ifndef AST_H
#define AST_H

#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include <memory>
#include <vector>
#include <string>

// Forward declarations
class ExprAST;
class PrototypeAST;
class FunctionAST;

// Base class for all expression nodes
class ExprAST {
public:
    virtual ~ExprAST() = default;
    virtual llvm::Value *codegen() = 0;
};

class NumberExprAST : public ExprAST {
    double Val;
public:
    NumberExprAST(double V) : Val(V) {}
    llvm::Value *codegen() override;
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
    std::string Name;
public:
    VariableExprAST(const std::string &N) : Name(N) {}
    llvm::Value *codegen() override;
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
    llvm::Value *codegen() override;
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
    std::string Callee; // name of the function
    std::vector<std::unique_ptr<ExprAST>> Args;
public:
    CallExprAST(const std::string &Callee,
                            std::vector<std::unique_ptr<ExprAST>> Args)
            : Callee(Callee), Args(std::move(Args)) {}
    llvm::Value *codegen() override;
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number of arguments the function takes).
class PrototypeAST {
    std::string Name;
    std::vector<std::string> Args;
public:
    PrototypeAST(const std::string &Name, std::vector<std::string> Args)
      : Name(Name), Args(std::move(Args)) {}
    llvm::Function *codegen();
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
    llvm::Function *codegen();
};

// IfElse block AST, it just stores pointers to cond, else, then blocks
class IfExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Cond, Then, Else;
public:
    IfExprAST(
        std::unique_ptr<ExprAST> cond,
        std::unique_ptr<ExprAST> then,
        std::unique_ptr<ExprAST> else_st
    ) : Cond(std::move(cond)), Then(std::move(then)), Else(std::move(else_st)) {}
    llvm::Value *codegen() override;
}; 

// expression class for for/in
// Start: expr for initialization of cntr, End: ending condition expression, Step: cntr incrementing expression,  
class ForExprAST : public ExprAST {
    std::string VarName;
    std::unique_ptr<ExprAST> Start, End, Step, Body;
public:
    ForExprAST(
        std::string &varname,
        std::unique_ptr<ExprAST> start,
        std::unique_ptr<ExprAST> end, 
        std::unique_ptr<ExprAST> step,
        std::unique_ptr<ExprAST> body
    ) : VarName(varname), Start(std::move(start)), End(std::move(end)), 
    Step(std::move(step)), Body(std::move(body)) {}
    llvm::Value *codegen() override;
};
#endif // AST_H