#ifndef CODEGEN_H
#define CODEGEN_H

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include <memory>
#include <map>
#include <string>
#include "ast.h"

// Forward declaration for KaleidoscopeJIT
namespace llvm {
namespace orc {
class KaleidoscopeJIT;
}
}

// Codegen globals
extern std::unique_ptr<llvm::LLVMContext> TheContext;
extern std::unique_ptr<llvm::IRBuilder<>> Builder;
extern std::unique_ptr<llvm::Module> TheModule;
extern std::map<std::string, llvm::Value *> NamedValues;
extern std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
extern std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;
// Error logging for codegen
llvm::Value *LogErrorV(const char *Str);

// Module initialization
void InitializeModule();

#endif // CODEGEN_H