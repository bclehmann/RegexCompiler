#include <cstdio>
#include <llvm-c/Core.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/IR/InstrTypes.h>

int main() {
    char char_to_match = getchar();
    llvm::LLVMContext context;
    llvm::IRBuilder Builder(context);
    llvm::Module module("RegexCompiler", context);

    std::vector<llvm::Type*> getchar_args_type;
    llvm::FunctionType* getchar_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), getchar_args_type, false);
    llvm::Function* getchar_function = llvm::Function::Create(getchar_type, llvm::Function::ExternalLinkage, "getchar", &module);

    std::vector<llvm::Type*> puts_args_type(1, llvm::Type::getInt8Ty(context)->getPointerTo());
    llvm::FunctionType* puts_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), llvm::ArrayRef(puts_args_type), false);
    llvm::Function* puts_function = llvm::Function::Create(puts_type, llvm::Function::ExternalLinkage, "puts", &module);

    // main function
    std::vector<llvm::Type*> main_args_type;
    llvm::FunctionType* main_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), main_args_type, false);
    llvm::Function* main_function = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage, "main", &module);

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", main_function);
    Builder.SetInsertPoint(entry);

    llvm::Value* input = Builder.CreateCall(getchar_type, getchar_function);
    llvm::Value* isEqual = Builder.CreateICmpEQ(input, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), char_to_match));

    llvm::BasicBlock* end = llvm::BasicBlock::Create(context, "end", main_function);

    llvm::BasicBlock* no_match = llvm::BasicBlock::Create(context, "no_match", main_function);
    Builder.SetInsertPoint(no_match);
    Builder.CreateCall(puts_type, puts_function, Builder.CreateGlobalStringPtr("Sorry, that didn't match."));
    Builder.CreateBr(end);

    llvm::BasicBlock* match = llvm::BasicBlock::Create(context, "match", main_function);
    Builder.SetInsertPoint(match);
    Builder.CreateCall(puts_type, puts_function, Builder.CreateGlobalStringPtr("Yay! That did match."));
    Builder.CreateBr(end);

    Builder.SetInsertPoint(entry);
    Builder.CreateCondBr(isEqual, match, no_match);

    Builder.SetInsertPoint(end);
    Builder.CreateRet(Builder.CreateIntCast(Builder.CreateNot(isEqual), llvm::Type::getInt32Ty(context), false));

    std::error_code ec;
    llvm::raw_fd_ostream output("out.ll", ec);
    module.print(output, nullptr);

    return 0;
}
