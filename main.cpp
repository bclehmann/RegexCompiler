#include <cstdio>
#include <llvm-c/Core.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/ArrayRef.h>

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

    Builder.CreateCall(puts_type, puts_function, Builder.CreateGlobalStringPtr("Hello world!"));
    Builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0, true));

#if 0

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, main_function, "entry");
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef input = LLVMBuildCall2(builder, getchar_function_type, getchar_function, nullptr, 0, "getchar");
    LLVMValueRef equals = LLVMBuildICmp(builder, LLVMIntEQ, input, LLVMConstInt(int_32_type, char_to_match, false), "equality check");

    LLVMBasicBlockRef no_match = LLVMAppendBasicBlockInContext(context, main_function, "no_match");
    LLVMBasicBlockRef match = LLVMAppendBasicBlockInContext(context, main_function, "match");
    LLVMBasicBlockRef end = LLVMAppendBasicBlockInContext(context, main_function, "end");
    LLVMBuildCondBr(builder, equals, match, no_match);

    LLVMPositionBuilderAtEnd(builder, no_match);
    LLVMValueRef no_match_args[] = {
        LLVMBuildPointerCast(builder, LLVMBuildGlobalString(builder, "Sorry, that didn't match", "no_match_str"), int_8_type_ptr, "0")
    };
    LLVMBuildCall2(builder, puts_function_type, puts_function, no_match_args, 1, "no_match_call");
    LLVMBuildBr(builder, end);

    LLVMPositionBuilderAtEnd(builder, match);
     LLVMValueRef match_args[] = {
        LLVMBuildPointerCast(builder, LLVMBuildGlobalString(builder, "Yay! that did match", "match_str"), int_8_type_ptr, "0")
    };
    LLVMBuildCall2(builder, puts_function_type, puts_function, match_args, 1, "match_call");
    LLVMBuildBr(builder, end);

    LLVMPositionBuilderAtEnd(builder, end);

    LLVMBuildRet(builder, LLVMBuildIntCast2(builder, LLVMBuildNot(builder, equals, ""), int_32_type, true, ""));

    //LLVMDumpModule(module); // dump module to STDOUT
    LLVMPrintModuleToFile(module, "out.ll", nullptr);

    // clean memory
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
    LLVMContextDispose(context);
#endif

    std::error_code ec;
    llvm::raw_fd_ostream output("out.ll", ec);
    module.print(output, nullptr);

    return 0;
}
