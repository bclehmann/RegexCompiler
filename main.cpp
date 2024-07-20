#include <cstdio>
#include <llvm-c/Core.h>


int main() {
    char char_to_match = getchar();
    // create context and define getchar and puts
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef  module = LLVMModuleCreateWithNameInContext("RegexCompiler", context);
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);

    LLVMTypeRef int_8_type = LLVMInt8TypeInContext(context);
    LLVMTypeRef int_8_type_ptr = LLVMPointerType(int_8_type, 0);
    LLVMTypeRef int_32_type = LLVMInt32TypeInContext(context);

    LLVMTypeRef  getchar_function_type = LLVMFunctionType(int_32_type, nullptr, 0, false);
    LLVMValueRef getchar_function = LLVMAddFunction(module, "getchar", getchar_function_type);

    LLVMTypeRef puts_function_args_type[] {
        int_8_type_ptr
    };

    LLVMTypeRef  puts_function_type = LLVMFunctionType(int_32_type, puts_function_args_type, 1, false);
    LLVMValueRef puts_function = LLVMAddFunction(module, "puts", puts_function_type);

    // main function
    LLVMTypeRef  main_function_type = LLVMFunctionType(int_32_type, nullptr, 0, false);
    LLVMValueRef main_function = LLVMAddFunction(module, "main", main_function_type);

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

    return 0;
}
