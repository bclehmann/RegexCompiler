#include <iostream>
#include <string>
#include <memory>
#include <vector>
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

struct LLVMValueDeleter {
	void operator()(llvm::Value* p) {
		p->deleteValue();
	}
};

int main(int argc, char* argv[]) {
	if(argc < 2) {
		std::cout << "Specify a regex\n";
		return 1;
	}

	std::string regex = argv[1];

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

	std::vector<llvm::Value*> loop_values;
	std::vector<llvm::BasicBlock*> loop_blocks;
	llvm::BasicBlock* next_iter = nullptr;
	llvm::Value* is_equal = nullptr;
	llvm::BasicBlock* end = llvm::BasicBlock::Create(context, "end", main_function);

	llvm::BasicBlock* first_iter = llvm::BasicBlock::Create(context, "first_iter", main_function);
	loop_blocks.push_back(first_iter);

	Builder.CreateBr(first_iter);
	Builder.SetInsertPoint(first_iter);

	for(char c : regex) {
		llvm::Value* input = Builder.CreateCall(getchar_type, getchar_function);
		is_equal = Builder.CreateICmpEQ(input, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), c));
		loop_values.push_back(is_equal);

		next_iter = llvm::BasicBlock::Create(context, "next_iter", main_function);
		loop_blocks.push_back(next_iter);
		Builder.CreateCondBr(is_equal, next_iter, end);

		Builder.SetInsertPoint(next_iter);
	}

	Builder.CreateBr(end);
	loop_values.push_back(llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1));
	loop_blocks.push_back(next_iter);

	Builder.SetInsertPoint(end);
	if (regex.size()) {
		llvm::PHINode* resolved_is_equal = Builder.CreatePHI(llvm::Type::getInt1Ty(context), regex.size() + 1);
		for(int i = 0; i < loop_values.size(); i++) {
			resolved_is_equal->addIncoming(loop_values[i], loop_blocks[i]);
		}

		Builder.CreateRet(Builder.CreateIntCast(Builder.CreateNot(resolved_is_equal), llvm::Type::getInt32Ty(context), false));
	} else {
		Builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0, true));
	}

	std::error_code ec;
	llvm::raw_fd_ostream output("out.ll", ec);
	module.print(output, nullptr);

	return 0;
}
