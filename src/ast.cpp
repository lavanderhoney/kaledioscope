#include "ast.h"
#include "codegen.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Verifier.h"
#include <algorithm>

// Forward declaration
llvm::Function *getFunction(std::string Name);

llvm::Value *NumberExprAST::codegen() {
    return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*TheContext), llvm::APFloat(Val));
}

llvm::Value *VariableExprAST::codegen() {
    // find the variable name in the symbol table. We assume that the variable has already been emitted somewhere and its value is available
    llvm::Value *V = NamedValues[Name];
    if (!V) {
        LogErrorV("Unknown variable name");
    }
    return V;
}

llvm::Value *BinaryExprAST::codegen() {
    llvm::Value *L = LHS->codegen();
    llvm::Value *R = RHS->codegen();

    if (!L || !R) {
        return nullptr;
    }

    switch (Op) {
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
        return Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext), "booltmp");
    default:
        return LogErrorV("invalid binary operator");
    }
}

llvm::Value *CallExprAST::codegen() {
     // Look up the name in the global module table.
    llvm::Function *CalleeF = getFunction(Callee);
    if (!CalleeF)
        return LogErrorV("Unknown function referenced");

    // If argument mismatch error.
    if (CalleeF->arg_size() != Args.size())
        return LogErrorV("Incorrect # arguments passed");

    std::vector<llvm::Value *> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
        ArgsV.push_back(Args[i]->codegen());
        if (!ArgsV.back())
            return nullptr;
    }
    return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

llvm::Function *PrototypeAST::codegen() {
    // Make the function type: double(double,double) etc.
    std::vector<llvm::Type*> Doubles(Args.size(), llvm::Type::getDoubleTy(*TheContext)); // N LLVM Double types for N args
    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), Doubles, false); // creates a function type with N doubles as args

    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, TheModule.get()); // actually creates the IR Function corresponding to the Prototype

    // Set names for all arguments.
    unsigned Idx = 0;
    for (auto &Arg : F->args())
        Arg.setName(Args[Idx++]);

    return F;
}

llvm::Function *getFunction(std::string Name){
    // First, see if the function has already been added to the current module.
    if(auto *F = TheModule->getFunction(Name)){
        return F;
    }

    // If not, check whether we can codegen the declaration from some existing prototype.
    auto FI = FunctionProtos.find(Name);
    if (FI != FunctionProtos.end()){
        return FI->second->codegen(); // this will create the function in the new modu
    }

    // If no existing prototype exists, return null.
    return nullptr;
}

llvm::Function *FunctionAST::codegen() {
     // Transfer ownership of the prototype to the FunctionProtos map, but keep a reference to it for use below.
    auto &P = *Proto;
    FunctionProtos[Proto->getName()] = std::move(Proto);

    // First, check for an existing function from a previous 'extern' declaration.
    llvm::Function *TheFunction = getFunction(P.getName());
    if (!TheFunction){
        return nullptr;
    }
    if (!TheFunction->empty()) {
        return (llvm::Function*)LogErrorV("Function cannot be redefined.");
    }
    
    // Create a new basic block named entry to start insertion into.
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    NamedValues.clear();
    for (auto &Arg : TheFunction->args()) {
        NamedValues[std::string(Arg.getName())] = &Arg;
    }

    if (llvm::Value *RetVal = Body->codegen()) {
        // Finish off the function.
        Builder->CreateRet(RetVal);

        // Validate the generated code, checking for consistency.
        llvm::verifyFunction(*TheFunction);
        return TheFunction;
    }

    // Error reading body, remove function.
    TheFunction->eraseFromParent();
    return nullptr;
}