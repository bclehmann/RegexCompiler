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

//TODO: This probably will need to add Ignore for optionals, as they can fail to match and not advance the cursor
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
	{
		generated_function_type = llvm::FunctionType::get(
			llvm::Type::getInt32Ty(*context),
			std::vector<llvm::Type*> { llvm::Type::getInt8Ty(*context)->getPointerTo(), llvm::Type::getInt32Ty(*context) }, // buf and index
			false
		);
	}
	llvm::Function* codegen() override {
		llvm::Function* existing = module->getFunction(get_generated_function_name());

		if (existing) {
			return existing;
		}

		llvm::Function* generated_function = llvm::Function::Create(generated_function_type, llvm::Function::PrivateLinkage, get_generated_function_name(), module);
		llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", generated_function);
		builder->SetInsertPoint(entry);

		llvm::Value* buf = generated_function->args().begin();
		llvm::Value* index = (generated_function->args().begin() + 1);

		llvm::Value* input = builder->CreateLoad(
			llvm::Type::getInt8Ty(*context),
			builder->CreateGEP(llvm::Type::getInt8Ty(*context), buf, std::vector<llvm::Value*> { index })
		);
		llvm::Value* is_equal = builder->CreateICmpEQ(input, llvm::ConstantInt::get(llvm::Type::getInt8Ty(*context), to_match));

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
	llvm::FunctionType* get_generated_function_type() {
		return generated_function_type;
	}
private:
	std::string get_generated_function_name() const {
		return module->getModuleIdentifier() + "_literal_matching__" + to_match;
	}

	char to_match;
	llvm::LLVMContext* context;
	llvm::Module* module;
	llvm::IRBuilder<>* builder;
	llvm::FunctionType* generated_function_type;
};

llvm::Function* buildPanic(llvm::LLVMContext& context, llvm::IRBuilder<>& builder, llvm::Module& module) {
	std::vector<llvm::Type*> puts_args_type(1, llvm::Type::getInt8Ty(context)->getPointerTo());
	llvm::FunctionType* puts_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), llvm::ArrayRef(puts_args_type), false);
	llvm::Function* existing_puts = module.getFunction("puts");
	llvm::Function* puts_function = existing_puts
		? existing_puts
		: llvm::Function::Create(puts_type, llvm::Function::ExternalLinkage, "puts", &module);

	llvm::FunctionType* exit_type = llvm::FunctionType::get(llvm::Type::getVoidTy(context), std::vector<llvm::Type*> { llvm::Type::getInt32Ty(context) }, false);
	llvm::Function* existing_exit = module.getFunction("exit");
	llvm::Function* exit_function = existing_exit
		? existing_exit
		: llvm::Function::Create(exit_type, llvm::Function::ExternalLinkage, "exit", &module);


	llvm::FunctionType* panic_type = llvm::FunctionType::get(llvm::Type::getVoidTy(context), llvm::ArrayRef(puts_args_type), false);
	llvm::Function* existing_panic = module.getFunction("panic");
	if (existing_panic) {
		return existing_panic;
	}

	llvm::Function* panic = llvm::Function::Create(panic_type, llvm::Function::PrivateLinkage, "panic", &module);

	llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", panic);
	builder.SetInsertPoint(entry);
	builder.CreateCall(puts_type, puts_function, std::vector<llvm::Value*> { (panic->args().begin()) });
	builder.CreateCall(exit_type, exit_function, std::vector<llvm::Value*> { llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1) });
	builder.CreateUnreachable();

	return panic;
}

llvm::BasicBlock* buildReadToBuf(llvm::LLVMContext& context, llvm::IRBuilder<>& builder, llvm::Module& module, llvm::Function* function, llvm::BasicBlock* after_block, llvm::AllocaInst*& buf_out, llvm::Value*& len_out) {
	std::vector<llvm::Type*> getchar_args_type;
	llvm::FunctionType* getchar_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), getchar_args_type, false);
	llvm::Function* existing_getchar = module.getFunction("getchar");
	llvm::Function* getchar_function = existing_getchar
		? existing_getchar
		: llvm::Function::Create(getchar_type, llvm::Function::ExternalLinkage, "getchar", module);

	llvm::BasicBlock* pre_init_loop = llvm::BasicBlock::Create(context, "pre_init_loop", function);
	builder.SetInsertPoint(pre_init_loop);

	const int buf_size = 1024;
	buf_out = builder.CreateAlloca(llvm::Type::getInt8Ty(context), llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), buf_size), "buf");
	llvm::Value* start_index = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);

	llvm::BasicBlock* init_array_loop = llvm::BasicBlock::Create(context, "init_array_loop", function);
	llvm::BasicBlock* after_init_loop = llvm::BasicBlock::Create(context, "after_init_loop", function);
	builder.CreateBr(init_array_loop);

	builder.SetInsertPoint(init_array_loop);

	llvm::PHINode* loop_index = builder.CreatePHI(llvm::Type::getInt32Ty(context), 2);
	llvm::Value* next_index = builder.CreateAdd(loop_index, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1));
	loop_index->addIncoming(start_index, pre_init_loop);
	loop_index->addIncoming(next_index, init_array_loop);
	llvm::Value* input = builder.CreateCall(getchar_type, getchar_function);
	llvm::Value* was_eof = builder.CreateICmpEQ(input, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), -1));
	builder.CreateStore(
		builder.CreateIntCast(input, llvm::Type::getInt8Ty(context), true),
		builder.CreateGEP(llvm::Type::getInt8Ty(context), buf_out, std::vector<llvm::Value*> { loop_index })
	);
	llvm::Value* next_iter_out_of_bounds = builder.CreateICmpEQ(next_index, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), buf_size + 2));
	llvm::Value* should_exit = builder.CreateOr(was_eof, next_iter_out_of_bounds);
	builder.CreateCondBr(should_exit, after_init_loop, init_array_loop);

	builder.SetInsertPoint(after_init_loop);
	builder.CreateStore(
		llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 0),
		builder.CreateGEP(llvm::Type::getInt8Ty(context), buf_out, std::vector<llvm::Value*> { loop_index })
	);

	len_out = loop_index;

	llvm::BasicBlock* panic_block = llvm::BasicBlock::Create(context, "panic_block", function);
	builder.CreateCondBr(next_iter_out_of_bounds, panic_block, after_block);

	llvm::Function* panic = buildPanic(context, builder, module);
	builder.SetInsertPoint(panic_block);
	builder.CreateCall(panic->getFunctionType(), panic, std::vector<llvm::Value*> { builder.CreateGlobalStringPtr("Error: Could not read string, buffer was too small.") });
	builder.CreateUnreachable();

	return pre_init_loop;
}

int main(int argc, char* argv[]) {
	if(argc < 2) {
		std::cout << "Specify a regex\n";
		return 1;
	}

	llvm::LLVMContext context;
	llvm::IRBuilder Builder(context);
	llvm::Module module("RegexCompiler", context);

	std::string regex = argv[1];

	/*
	std::vector<llvm::Type*> puts_args_type(1, llvm::Type::getInt8Ty(context)->getPointerTo());
	llvm::FunctionType* puts_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), llvm::ArrayRef(puts_args_type), false);
	llvm::Function* puts_function = llvm::Function::Create(puts_type, llvm::Function::ExternalLinkage, "puts", &module);
	*/

	// main function
	std::vector<llvm::Type*> main_args_type;
	llvm::FunctionType* main_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), main_args_type, false);
	llvm::Function* main_function = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage, "main", &module);

	llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", main_function);
	Builder.SetInsertPoint(entry);
	std::vector<std::unique_ptr<Atom>> atoms;
	for(char c : regex) {
		std::unique_ptr<Literal> literal = std::make_unique<Literal>(c, &context, &module, &Builder);
		atoms.push_back(std::move(literal));
	}

	llvm::AllocaInst* buf;
	llvm::Value* input_len;
	llvm::BasicBlock* post_loop = llvm::BasicBlock::Create(context, "post_loop", main_function);
	llvm::BasicBlock* readToBuf = buildReadToBuf(context, Builder, module, main_function, post_loop, buf, input_len);

	Builder.SetInsertPoint(entry);
	Builder.CreateBr(readToBuf);

	Builder.SetInsertPoint(post_loop);
	//Builder.CreateCall(puts_type, puts_function, buf);

	std::vector<llvm::Value*> loop_values;
	std::vector<llvm::BasicBlock*> loop_blocks;
	llvm::BasicBlock* next_iter = nullptr;
	llvm::Value* is_accept = nullptr;
	llvm::BasicBlock* end = llvm::BasicBlock::Create(context, "end", main_function);

	llvm::BasicBlock* first_iter = llvm::BasicBlock::Create(context, "first_iter", main_function);
	loop_blocks.push_back(first_iter);

	Builder.CreateBr(first_iter);
	Builder.SetInsertPoint(first_iter);

	for(size_t i = 0; i < atoms.size(); i++) {
		std::unique_ptr<Atom>& atom = atoms[i];
		llvm::Value* loop_index = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), i);

		auto insert_block = Builder.GetInsertBlock();
		auto insert_point = Builder.GetInsertPoint();
		llvm::Function* atom_function = atom->codegen();
		Builder.SetInsertPoint(insert_block, insert_point);

		llvm::Value* decision = Builder.CreateCall(atom_function->getFunctionType(), atom_function, std::vector<llvm::Value*> { buf, loop_index } );
		is_accept = Builder.CreateICmpEQ(decision, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), static_cast<int32_t>(AcceptDecision::Accept)));
		loop_values.push_back(is_accept);

		next_iter = llvm::BasicBlock::Create(context, "next_iter", main_function);
		loop_blocks.push_back(next_iter);
		Builder.CreateCondBr(is_accept, next_iter, end);

		Builder.SetInsertPoint(next_iter);
	}

	Builder.CreateBr(end);
	loop_values.push_back(llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1));
	loop_blocks.push_back(next_iter);

	Builder.SetInsertPoint(end);
	if (regex.size()) {
		llvm::PHINode* resolved_is_accept = Builder.CreatePHI(llvm::Type::getInt1Ty(context), regex.size() + 1);
		for(size_t i = 0; i < loop_values.size(); i++) {
			resolved_is_accept->addIncoming(loop_values[i], loop_blocks[i]);
		}

		Builder.CreateRet(Builder.CreateIntCast(Builder.CreateNot(resolved_is_accept), llvm::Type::getInt32Ty(context), false));
	} else {
		Builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0, true));
	}

	std::error_code ec;
	llvm::raw_fd_ostream output("out.ll", ec);
	module.print(output, nullptr);

	return 0;
}
