#include "ast.h"
#include "codegen.h"
#include "parser.h"
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
        break;
    }

    // If it wasn't a builtin binary operator, it must be a user defined one. Emit a call to it.
    llvm::Function *F = getFunction(std::string("binary") + Op);
    assert(F && "binary operator not found!");

    llvm::Value *Ops[2] = { L, R };
    return Builder->CreateCall(F, Ops, "binop");
}

llvm::Value *UnaryExprAST::codegen(){
    llvm::Value *OperandV = Operand->codegen();
    if (!OperandV){
        return nullptr;
    }

    llvm::Function *F = getFunction(std::string("unary") + Opcode);
    if (!F){
        return LogErrorV("Unkown unary operator");
    }
    return Builder->CreateCall(F, OperandV, "unop");
}

llvm::Value *IfExprAST::codegen(){
    llvm::Value *CondV = Cond->codegen();
    if (!CondV){
        return nullptr;
    }
    // Convert condition to a bool by comparing non-equal to 0.0.
    CondV = Builder->CreateFCmpONE(CondV, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "ifcond");

    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

    //Create the BasicBlock for then and attach it to its parent function
    llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*TheContext, "then", TheFunction);

    // creates 'floating' basicblocks in memory
    llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*TheContext, "else");
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*TheContext, "ifcont");

    Builder->CreateCondBr(CondV, ThenBB, ElseBB);
    //Move the builder to point *inside* the 'then' block
    Builder->SetInsertPoint(ThenBB);
    // recursively  gen code for then section
    llvm::Value *ThenV = Then->codegen();
    if (!ThenV){
        return nullptr;
    }
    Builder->CreateBr(MergeBB); //creates unconditional branch from 'then' to 'merge'
    ThenBB = Builder->GetInsertBlock(); //updated ThenBB for Phi node

    // add the else block to the function
    TheFunction->insert(TheFunction->end(), ElseBB);
    // now move builder to 'else' block
    Builder->SetInsertPoint(ElseBB);
    llvm::Value *ElseV = Else->codegen();
    if (!ElseV){
        return nullptr;
    }
    Builder->CreateBr(MergeBB);
    ElseBB = Builder->GetInsertBlock();

    TheFunction->insert(TheFunction->end(), MergeBB);
    Builder->SetInsertPoint(MergeBB);
    llvm::PHINode *PN = Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, "iftmp");
    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
}

llvm::Value *ForExprAST::codegen(){
    llvm::Value *StartVal = Start->codegen(); // emit the IR for Start expr before the variable init, as that is inlined in the Phi node
    if (!StartVal){
        return nullptr;
    }
    llvm::BasicBlock *PreHeaderBB = Builder->GetInsertBlock(); // The BB just before the loop begins
    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*TheContext, "loop", TheFunction);
    Builder->CreateBr(LoopBB);

    Builder->SetInsertPoint(LoopBB);
    llvm::PHINode *Variable = Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, VarName);
    Variable->addIncoming(StartVal, PreHeaderBB); // if the control comes from PreheaderBB, the VarName takes value StartVal
    
    // allow shadowing of the counter variable
    llvm::Value *OldVal = NamedValues[VarName];
    NamedValues[VarName] = Variable;

    // emit the body value
    llvm::Value *BodyV = Body->codegen();
    if (!BodyV){
        return nullptr;
    }

    //emit the step value
    llvm::Value *StepV = nullptr;
    if (Step){
        StepV = Step->codegen();
        if (!StepV){
            return nullptr;
        }
    } else {
        // step not specified, so default is 1.0
        StepV = llvm::ConstantFP::get(*TheContext, llvm::APFloat(1.0));
    }

    // value of the counter variable in next iteration
    llvm::Value *NextVar = Builder->CreateFAdd(Variable, StepV, "nextvar"); 

    llvm::Value *EndCond = End->codegen();
    if (!EndCond){
        return nullptr;
    }   
    // Convert condition to a bool by comparing non-equal to 0.0.
    EndCond = Builder->CreateFCmpONE(EndCond, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "loopcond");

    // refers to where Builder currently is, which cud be some other nested block, or LoopBB itself. It points to the 'end' of the loop
    llvm::BasicBlock *LoopEndBB = Builder->GetInsertBlock(); 
    llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*TheContext, "afterloop", TheFunction);
    Builder->CreateCondBr(EndCond, LoopBB, AfterBB);    

    // Any new code will be inserted in AfterBB.
    Builder->SetInsertPoint(AfterBB);

    Variable->addIncoming(NextVar, LoopEndBB);

    //Restore the unshadowed variable
    if (OldVal){
        NamedValues[VarName] = OldVal;
    } else {
        NamedValues.erase(VarName);
    }

    // for loop will return 0.0
    return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*TheContext));
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
    
    if (P.isBinaryOp()){
        BinopPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();
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

        // Run the optimizer on the function
        TheFPM->run(*TheFunction, *TheFAM);
        return TheFunction;
    }

    // Error reading body, remove function.
    TheFunction->eraseFromParent();
    return nullptr;
}