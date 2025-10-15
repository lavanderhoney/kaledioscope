#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "llvm/IR/Module.h"
#include "KaleidoscopeJIT.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

using namespace llvm;
using namespace llvm::orc;

//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//
static ExitOnError ExitOnErr;

static void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        if (auto *FnIR = FnAST->codegen()) {
            fprintf(stderr, "Read function definition:");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");
            // Print the full module IR after the definition
            TheModule->print(llvm::errs(), nullptr);
            ExitOnErr(TheJIT->addModule(ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
            InitializeModule();
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
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");
            // Print the full module IR after the definition
            TheModule->print(llvm::errs(), nullptr);
            
            //register the function prototype
            FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);

        }
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}
std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;

static void HandleTopLevelExpression() {
    // Evaluate a top-level expression into an anonymous function.
    if (auto FnAST = ParseTopLevelExpr()) {
        if (auto *FnIR = FnAST->codegen()) {
            fprintf(stderr, "Read top-level expression:");
            FnIR->print(llvm::errs());
            fprintf(stderr, "\n");
            // Print the full module IR after the definition
            TheModule->print(llvm::errs(), nullptr);
    
            auto RT = TheJIT->getMainJITDylib().createResourceTracker();
            auto TSM = ThreadSafeModule(std::move(TheModule), std::move(TheContext));
            ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
            
            InitializeModule();

            // Search the JIT for the __anon_expr symbol.
            auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));
            assert(ExprSymbol.getAddress() && "Function not found");

             // Remove the anonymous expression.
            // FnIR->eraseFromParent();

            // Get the symbol's address and cast it to the right type (takes no
            // arguments, returns a double) so we can call it as a native function.
            double (*FP)() = ExprSymbol.getAddress().toPtr<double (*)()>();
            fprintf(stderr, "Evaluated to %f\n", FP());

            // Delete the anonymous expression module from the JIT.
            ExitOnErr(RT->remove());
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

int main() {

    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    
    // Prime the first token.
    fprintf(stderr, "ready> ");
    getNextToken();

    TheJIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create()); // TODO: understand how using the codegen.h this being globally availabe in this file and codgen.cpp
    
    // Make the module, which holds all the code.
    InitializeModule();

    // Run the main "interpreter loop" now.
    MainLoop();

    // Print out all of the generated code.
    TheModule->print(llvm::errs(), nullptr);
    return 0;
}