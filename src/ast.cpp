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

///Create an alloca instruction in the entry block of the function
llvm::AllocaInst* CreateEntryBlockAlloca(llvm::Function *TheFunction, llvm::StringRef VarName){
    llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),TheFunction->getEntryBlock().begin());

    return TmpB.CreateAlloca(llvm::Type::getDoubleTy(*TheContext), nullptr, VarName);
}

llvm::Value *NumberExprAST::codegen() {
    return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*TheContext), llvm::APFloat(Val));
}

llvm::Value *VariableExprAST::codegen() {
    // find the variable name in the symbol table. We assume that the variable has already been emitted somewhere and its value is available
    llvm::AllocaInst *A = NamedValues[Name];
    if (!A) {
        LogErrorV("Unknown variable name");
    }
    // Load the value
    return Builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());
}

llvm::Value *BinaryExprAST::codegen() {
    // Special case '=' because we don't want to emit the LHS as an expression.
    if (Op == '='){
        VariableExprAST *LHSE = static_cast<VariableExprAST*>(LHS.get());
        if (!LHSE){
            return LogErrorV("destination of '=' must be a variable");
        }
        
        // Codegen the RHS
        llvm::Value *Val = RHS->codegen();
        if (!Val)
            return nullptr;

        llvm::Value *Variable = NamedValues[LHSE->getName()];
        if (!Variable)
            return LogErrorV("Unknown variable name");
        Builder->CreateStore(Val, Variable);
        return Val;
    }
    llvm::Value *L = LHS->codegen();
    llvm::Value *R = RHS->codegen();

    if (!L || !R) {
        llvm::errs() << "DEBUG---BinaryExprAST::codegen failed to generate LHS or RHS\n";
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

llvm::Value *VarExprAST::codegen(){
    std::vector<llvm::AllocaInst*> OldBindings;
    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

    // Register all variables and emit their initializer
    for (unsigned i=0, e = VarNames.size(); i!=e; i++){
        const std::string &VarName = VarNames[i].first;
        ExprAST *Init = VarNames[i].second.get();

        llvm::Value *InitVal;
        if (Init){
            InitVal = Init->codegen();
            if (!InitVal)
                return nullptr;
        } else {
            InitVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0));
        }

        llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
        Builder->CreateStore(InitVal, Alloca);

        OldBindings.push_back(NamedValues[VarName]);
        NamedValues[VarName] = Alloca;
    }
    // Codegen the body, now that all vars are in scope.
    llvm::Value *BodyVal = Body->codegen();
    if (!BodyVal)
      return nullptr;
    
    // restore the previous variable bindings
    for(unsigned i=0, e = VarNames.size(); i!=e; i++){
        NamedValues[VarNames[i].first] = OldBindings[i];
    }

    // Return the body computation.
    return BodyVal;
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
    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
    
    // Create an alloca for the variable in the entry block
    llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
    
    // Emit the start code first, without 'variable' in scope.
    llvm::Value *StartVal = Start->codegen(); 
    if (!StartVal){
        llvm::errs() << "DEBUG---Error generating start value for for-loop variable: " << VarName << "\n";
        return nullptr;
    }

    // Store the value into the alloca, i.e, the destined register
    Builder->CreateStore(StartVal, Alloca);

    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*TheContext, "loop", TheFunction);
    Builder->CreateBr(LoopBB);

    Builder->SetInsertPoint(LoopBB);
    
    // allow shadowing of the counter variable
    llvm::AllocaInst *OldVal = NamedValues[VarName];
    NamedValues[VarName] = Alloca;

    // emit the body value
    llvm::Value *BodyV = Body->codegen();
    if (!BodyV){
        llvm::errs() << "DEBUG---Error generating body for for-loop variable: " << VarName << "\n";
        return nullptr;
    }

    //emit the step value
    llvm::Value *StepV = nullptr;
    if (Step){
        StepV = Step->codegen();
        if (!StepV){
            llvm::errs() << "DEBUG---Error generating step for for-loop variable: " << VarName << "\n";
            return nullptr;
        }
    } else {
        // step not specified, so default is 1.0
        StepV = llvm::ConstantFP::get(*TheContext, llvm::APFloat(1.0));
    }

    // value of the counter variable in next iteration
    llvm::Value *CurVal = Builder->CreateLoad(Alloca->getAllocatedType(), Alloca, VarName.c_str());
    llvm::Value *NextVar = Builder->CreateFAdd(CurVal, StepV, "nextvar"); 
    // The addIncoming of the phi node is replaced by storing the inc value of counter back to its register
    Builder->CreateStore(NextVar, Alloca);

    llvm::Value *EndCond = End->codegen();
    if (!EndCond){
        llvm::errs() << "DEBUG---Error generating end condition for for-loop variable: " << VarName << "\n";
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
        // Create alloca for the arguments
        llvm::AllocaInst *ArgsAlloca = CreateEntryBlockAlloca(TheFunction, Arg.getName()); 

        // Store their initial value into the register
        Builder->CreateStore(&Arg, ArgsAlloca);

        // Add to symbol table
        NamedValues[std::string(Arg.getName())] = ArgsAlloca;
    }

    llvm::Value *RetVal = Body->codegen();
    if (RetVal) {
        // Finish off the function.
        Builder->CreateRet(RetVal);

        // Validate the generated code, checking for consistency.
        std::string Str;
        llvm::raw_string_ostream OS(Str);
        if (llvm::verifyFunction(*TheFunction, &OS)) {
            llvm::errs() << "\n\nDEBUG---VERIFIER ERROR FOUND:\n";
            llvm::errs() << OS.str() << "\n";
            llvm::errs() << "-----------------------------\n";

            // Setup a safe exit
            TheFunction->eraseFromParent();
            return nullptr;
        }
        // Run the optimizer on the function
        TheFPM->run(*TheFunction, *TheFAM);
        return TheFunction;
    } 
    llvm::errs() << "DEBUG---Error generating function body, removing function: " << P.getName() << "\n";
    // Error reading body, remove function.
    TheFunction->eraseFromParent();
    return nullptr;
}