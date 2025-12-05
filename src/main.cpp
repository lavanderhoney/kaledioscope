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
#include <iostream>
//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//
static llvm::ExitOnError ExitOnErr;

static void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        if (auto *FnIR = FnAST->codegen()) {
            fprintf(stderr, "Read function definition:");
            fprintf(stderr, "\n");

            // Print the full module IR after the definition
            TheModule->print(llvm::errs(), nullptr);

            // Try to add module, capture error and print it (don't ExitOnErr)
            auto TSM = llvm::orc::ThreadSafeModule(std::move(TheModule), std::move(TheContext));
            if (auto Err = TheJIT->addModule(std::move(TSM))) {
                llvm::errs() << "Error adding module to JIT: " << Err;
                return;
            }
            llvm::errs() << "Module added to JIT.\n";

            InitializeModule();
        }else{
            llvm::errs() << "DEBUG---Codegen of function definition failed --- CurTok: " << CurTok << "\n";
        }
    } else {
        // Skip token for error recovery.
        llvm::errs() << "DEBUG---Parsing function definition failed --- CurTok: " << CurTok << "\n";
        getNextToken();
    }
}

static void HandleExtern() {
    if (auto ProtoAST = ParseExtern()) {
        if (auto *FnIR = ProtoAST->codegen()) {
            fprintf(stderr, "Read extern: ");
            // FnIR->print(llvm::errs());
            fprintf(stderr, "\n");
            // Print the full module IR after the definition
            TheModule->print(llvm::errs(), nullptr);
            
            // Register the function prototype
            FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
        }
    } else {
        // Skip token for error recovery.
        getNextToken();
    }
}

static void HandleTopLevelExpression() {
    // Evaluate a top-level expression into an anonymous function.
    auto FnAST = ParseTopLevelExpr();
    if (FnAST) {
        auto *FnIR = FnAST->codegen();
        if (FnIR) {
            fprintf(stderr, "Read top-level expression:");
            fprintf(stderr, "\n");
            TheModule->print(llvm::errs(), nullptr);

            auto RT = TheJIT->getMainJITDylib().createResourceTracker();
            auto TSM = llvm::orc::ThreadSafeModule(std::move(TheModule), std::move(TheContext));

            // Try to add module, capture error and print it (don't ExitOnErr)
            if (auto Err = TheJIT->addModule(std::move(TSM), RT)) {
                llvm::errs() << "Error adding module to JIT: " << Err;
                return;
            }
            llvm::errs() << "Module added to JIT.\n";

            InitializeModule();

            auto ExprSymbolExpected = TheJIT->lookup("__anon_expr");
            if (!ExprSymbolExpected) {
                llvm::errs() << "JIT Lookup Error: " << ExprSymbolExpected.takeError() << "\n";
                return; // Return to MainLoop
            }

            // Get the symbol's address and cast it to the right type (takes no arguments, returns a double) so we can call it as a native function.
            auto ExprSymbol = std::move(*ExprSymbolExpected);
            double (*FP)() = ExprSymbol.getAddress().toPtr<double (*)()>();
            fprintf(stderr, "Evaluated to %f\n", FP());

            // Delete the anonymous expression module from the JIT.
            if (auto Err = RT->remove()) {
                llvm::errs() << "Error removing module: " << Err << "\n";
                // We can continue even if removal fails
            }
        } else {
            llvm::errs() << "DEBUG---Codegen of top-level expression failed --- \n";
        }
    } else {
        llvm::errs() << "DEBUG---Parsing top-level expression failed --- CurTok: " << CurTok << "\n";
        // Skip token for error recovery.
        getNextToken();
    }
}

/// top ::= definition | external | expression | ';'
static void MainLoop() {
    while (true) {
        fprintf(stderr, "kaledioscope>>> ");
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

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main() {

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    
    // 1 is lowest precedence.
    BinopPrecedence['='] = 2;
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40; // highest.
    
    // Prime the first token.
    fprintf(stderr, "kaledioscope>>> ");
    getNextToken();

    auto JITOrErr = llvm::orc::KaleidoscopeJIT::Create();
    if (!JITOrErr) {
        llvm::errs() << "Failed to create JIT: ";
        llvm::errs() << JITOrErr.takeError();
        llvm::errs() << "\n";
        return 1; 
    }
    
    // On success, move the created JIT out:
    TheJIT = std::move(*JITOrErr); 
    
    // Make the module, which holds all the code.
    InitializeModule();

    // Run the main "interpreter loop" now.
    MainLoop();

    return 0;
}