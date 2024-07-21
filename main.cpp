#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <llvm-c/Core.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/IR/InstrTypes.h>

enum class AcceptDecision : int32_t {
	Consume, // Not a match, but can try again with a later substring
	Accept, // Match
	Reject // Not a match
};

class Atom {
public:
	virtual llvm::Function* codegen() = 0;
};

class Literal : public Atom {
public:
	Literal(char to_match, llvm::LLVMContext* context, llvm::Module* module, llvm::IRBuilder<>* builder)
	: to_match{to_match}, context{context}, module{module}, builder{builder}
	{}
	llvm::Function* codegen() override {
		llvm::Function* existing = module->getFunction(get_generated_function_name());

		if (existing) {
			return existing;
		}

		llvm::FunctionType* generated_function_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(*context), false);
		llvm::Function* generated_function = llvm::Function::Create(generated_function_type, llvm::Function::PrivateLinkage, get_generated_function_name());

		llvm::FunctionType* getchar_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(*context), false);
		llvm::Function* getchar_function = llvm::Function::Create(getchar_type, llvm::Function::ExternalLinkage, "getchar", module);

		llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", generated_function);
		builder->SetInsertPoint(entry);
		llvm::Value* input = builder->CreateCall(getchar_type, getchar_function);
		llvm::Value* is_equal = builder->CreateICmpEQ(input, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), to_match));

		llvm::BasicBlock* matches = llvm::BasicBlock::Create(*context, "matches", generated_function);
		builder->SetInsertPoint(matches);
		builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), static_cast<int32_t>(AcceptDecision::Accept), true));

		llvm::BasicBlock* does_not_match = llvm::BasicBlock::Create(*context, "does_not_match", generated_function);
		builder->SetInsertPoint(does_not_match);
		builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context), static_cast<int32_t>(AcceptDecision::Consume), true));

		builder->SetInsertPoint(entry);
		builder->CreateCondBr(is_equal, matches, does_not_match);

		return generated_function;
	}
private:
	std::string get_generated_function_name() const {
		return module->getModuleIdentifier() + "literal_matching__" + to_match;
	}

	char to_match;
	llvm::LLVMContext* context;
	llvm::Module* module;
	llvm::IRBuilder<>* builder;
};

int main(int argc, char* argv[]) {
	if(argc < 2) {
		std::cout << "Specify a regex\n";
		return 1;
	}


	llvm::LLVMContext context;
	llvm::IRBuilder Builder(context);
	llvm::Module module("RegexCompiler", context);

	std::string regex = argv[1];
	std::vector<Atom*> atoms;
	for(char c : regex) {
		Literal literal(c, &context, &module, &Builder);
		atoms.push_back(&literal);
	}

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

	llvm::BasicBlock* pre_init_loop = llvm::BasicBlock::Create(context, "pre_init_loop", main_function);
	Builder.CreateBr(pre_init_loop);
	Builder.SetInsertPoint(pre_init_loop);

	llvm::AllocaInst* buf = Builder.CreateAlloca(llvm::Type::getInt8Ty(context), llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1024), "buf");
	llvm::Value* start_index = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);

	llvm::BasicBlock* init_array_loop = llvm::BasicBlock::Create(context, "init_array_loop", main_function);
	llvm::BasicBlock* after_init_loop = llvm::BasicBlock::Create(context, "after_init_loop", main_function);
	Builder.CreateBr(init_array_loop);

	Builder.SetInsertPoint(init_array_loop);

	llvm::PHINode* loop_index = Builder.CreatePHI(llvm::Type::getInt32Ty(context), 2);
	llvm::Value* next_index = Builder.CreateAdd(loop_index, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1));
	loop_index->addIncoming(start_index, pre_init_loop);
	loop_index->addIncoming(next_index, init_array_loop);
	Builder.CreateStore(
		Builder.CreateIntCast(Builder.CreateAdd(loop_index, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 'A')), llvm::Type::getInt8Ty(context), true),
		Builder.CreateGEP(llvm::Type::getInt8Ty(context), buf, std::vector<llvm::Value*> { loop_index })
	);
	Builder.CreateCondBr(Builder.CreateICmpEQ(loop_index, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 10)), after_init_loop, init_array_loop);

	Builder.SetInsertPoint(after_init_loop);
	Builder.CreateCall(puts_type, puts_function, buf);

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
