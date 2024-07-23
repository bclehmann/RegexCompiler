#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include "Atom.h"
#include "Literal.h"
#include "StringStartMetacharacter.h"
#include "StringEndMetacharacter.h"
#include "Digit.h"
#include "AcceptDecision.h"
#include "TypeProvider.h"
#include "ConstantProvider.h"

llvm::Function* buildPanic(llvm::LLVMContext& context, llvm::IRBuilder<>& builder, llvm::Module& module) {
	TypeProvider type_provider(context);
	ConstantProvider constant_provider(type_provider);

	std::vector<llvm::Type*> puts_args_type(1, type_provider.getBytePtr());
	llvm::FunctionType* puts_type = llvm::FunctionType::get(type_provider.getInt32(), llvm::ArrayRef(puts_args_type), false);
	llvm::Function* existing_puts = module.getFunction("puts");
	llvm::Function* puts_function = existing_puts
		? existing_puts
		: llvm::Function::Create(puts_type, llvm::Function::ExternalLinkage, "puts", &module);

	llvm::FunctionType* exit_type = llvm::FunctionType::get(type_provider.getVoid(), std::vector<llvm::Type*> { type_provider.getInt32() }, false);
	llvm::Function* existing_exit = module.getFunction("exit");
	llvm::Function* exit_function = existing_exit
		? existing_exit
		: llvm::Function::Create(exit_type, llvm::Function::ExternalLinkage, "exit", &module);

	llvm::FunctionType* panic_type = llvm::FunctionType::get(type_provider.getVoid(), llvm::ArrayRef(puts_args_type), false);
	llvm::Function* existing_panic = module.getFunction("panic");
	if (existing_panic) {
		return existing_panic;
	}

	llvm::Function* panic = llvm::Function::Create(panic_type, llvm::Function::PrivateLinkage, "panic", &module);

	llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", panic);
	builder.SetInsertPoint(entry);
	builder.CreateCall(puts_type, puts_function, std::vector<llvm::Value*> { (panic->args().begin()) });
	builder.CreateCall(exit_type, exit_function, std::vector<llvm::Value*> { constant_provider.getInt32(127) });
	builder.CreateUnreachable();

	return panic;
}

llvm::BasicBlock* buildReadToBuf(llvm::LLVMContext& context, llvm::IRBuilder<>& builder, llvm::Module& module, llvm::Function* function, llvm::BasicBlock* after_block, llvm::AllocaInst*& buf_out, llvm::Value*& len_out) {
	TypeProvider type_provider(context);
	ConstantProvider constant_provider(type_provider);

	std::vector<llvm::Type*> getchar_args_type;
	llvm::FunctionType* getchar_type = llvm::FunctionType::get(type_provider.getInt32(), getchar_args_type, false);
	llvm::Function* existing_getchar = module.getFunction("getchar");
	llvm::Function* getchar_function = existing_getchar
		? existing_getchar
		: llvm::Function::Create(getchar_type, llvm::Function::ExternalLinkage, "getchar", module);

	llvm::BasicBlock* pre_init_loop = llvm::BasicBlock::Create(context, "pre_init_loop", function);
	builder.SetInsertPoint(pre_init_loop);

	const int buf_size = 1024;
	buf_out = builder.CreateAlloca(type_provider.getByte(), constant_provider.getInt32(buf_size), "buf");
	llvm::Value* start_index =  constant_provider.getInt32(0);

	llvm::BasicBlock* init_array_loop = llvm::BasicBlock::Create(context, "init_array_loop", function);
	llvm::BasicBlock* after_init_loop = llvm::BasicBlock::Create(context, "after_init_loop", function);
	builder.CreateBr(init_array_loop);

	builder.SetInsertPoint(init_array_loop);

	llvm::PHINode* loop_index = builder.CreatePHI(type_provider.getInt32(), 2);
	llvm::Value* next_index = builder.CreateAdd(loop_index, constant_provider.getInt32(1));
	loop_index->addIncoming(start_index, pre_init_loop);
	loop_index->addIncoming(next_index, init_array_loop);

	llvm::Value* input = builder.CreateCall(getchar_type, getchar_function);
	llvm::Value* was_eof = builder.CreateICmpEQ(input, constant_provider.getInt32(-1, true));
	builder.CreateStore(
		builder.CreateIntCast(input, type_provider.getByte(), true),
		builder.CreateGEP(type_provider.getByte(), buf_out, std::vector<llvm::Value*> { loop_index })
	);

	llvm::Value* next_iter_out_of_bounds = builder.CreateICmpEQ(next_index, constant_provider.getInt32(buf_size + 2));
	llvm::Value* should_exit = builder.CreateOr(was_eof, next_iter_out_of_bounds);
	builder.CreateCondBr(should_exit, after_init_loop, init_array_loop);

	builder.SetInsertPoint(after_init_loop);
	builder.CreateStore(
		constant_provider.getByte(0),
		builder.CreateGEP(type_provider.getByte(), buf_out, std::vector<llvm::Value*> { loop_index })
	);

	llvm::BasicBlock* panic_block = llvm::BasicBlock::Create(context, "panic_block", function);
	builder.CreateCondBr(next_iter_out_of_bounds, panic_block, after_block);

	llvm::Function* panic = buildPanic(context, builder, module);
	builder.SetInsertPoint(panic_block);
	builder.CreateCall(panic->getFunctionType(), panic, std::vector<llvm::Value*> { builder.CreateGlobalStringPtr("Error: Could not read string, buffer was too small.") });
	builder.CreateUnreachable();

	len_out = loop_index;
	return pre_init_loop;
}

int main(int argc, char* argv[]) {
	if(argc < 2) {
		std::cout << "Specify a regex\n";
		return 1;
	}
	std::string regex = argv[1];

	llvm::LLVMContext context;
	llvm::IRBuilder Builder(context);
	llvm::Module module("RegexCompiler", context);

	TypeProvider type_provider(context);
	ConstantProvider constant_provider(type_provider);

	std::vector<std::unique_ptr<Atom>> atoms;
	bool prev_was_escape = false;
	for(char c : regex) {
		if (c == '^' && !prev_was_escape) { // TODO: Escape characters
			std::unique_ptr<StringStartMetacharacter> metachar = std::make_unique<StringStartMetacharacter>(&context, &module, &Builder);
			atoms.push_back(std::move(metachar));
		} else if (c == '$' && !prev_was_escape) {
			std::unique_ptr<StringEndMetacharacter> metachar = std::make_unique<StringEndMetacharacter>(&context, &module, &Builder);
			atoms.push_back(std::move(metachar));
		} else if (c == 'd' && prev_was_escape) {
			std::unique_ptr<Digit> digit = std::make_unique<Digit>(&context, &module, &Builder);
			atoms.push_back(std::move(digit));
		} else if (c != '\\' || (c == '\\' && prev_was_escape)) {
			std::unique_ptr<Literal> literal = std::make_unique<Literal>(c, &context, &module, &Builder);
			atoms.push_back(std::move(literal));
		}

		prev_was_escape = c == '\\' && !prev_was_escape;
	}

	if (prev_was_escape) {
		std::cout << "Invalid regex: Trailing unescaped \\\n";
		return 1;
	}

	std::vector<llvm::Type*> main_args_type;
	llvm::FunctionType* main_type = llvm::FunctionType::get(type_provider.getInt32(), main_args_type, false);
	llvm::Function* main_function = llvm::Function::Create(main_type, llvm::Function::ExternalLinkage, "main", &module);

	llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", main_function);
	Builder.SetInsertPoint(entry);

	llvm::AllocaInst* buf;
	llvm::Value* input_len;
	llvm::BasicBlock* post_loop = llvm::BasicBlock::Create(context, "post_loop", main_function);
	llvm::BasicBlock* readToBuf = buildReadToBuf(context, Builder, module, main_function, post_loop, buf, input_len);

	Builder.SetInsertPoint(entry);
	Builder.CreateBr(readToBuf);

	llvm::BasicBlock* eval_loop = llvm::BasicBlock::Create(context, "eval_loop", main_function);
	llvm::BasicBlock* eval_consume_and_retry = llvm::BasicBlock::Create(context, "eval_consume_and_retry", main_function);
	llvm::BasicBlock* eval_reject_char = llvm::BasicBlock::Create(context, "eval_reject_char", main_function);

	llvm::BasicBlock* end = llvm::BasicBlock::Create(context, "end", main_function);
	Builder.SetInsertPoint(end);
	llvm::PHINode* resolved_is_accept = Builder.CreatePHI(type_provider.getBit(), regex.size() + 1);
	resolved_is_accept->addIncoming(constant_provider.getBit(0), eval_reject_char);

	Builder.SetInsertPoint(post_loop);
	Builder.CreateBr(eval_loop);

	Builder.SetInsertPoint(eval_loop);
	llvm::PHINode* string_start_index = Builder.CreatePHI(type_provider.getInt32(), 0);
	string_start_index->addIncoming(constant_provider.getInt32(0), post_loop);
	string_start_index->addIncoming(Builder.CreateAdd(string_start_index, constant_provider.getInt32(1)), eval_consume_and_retry);

	llvm::BasicBlock* first_atom_iter = llvm::BasicBlock::Create(context, "first_atom_iter", main_function);

	Builder.CreateBr(first_atom_iter);

	Builder.SetInsertPoint(eval_consume_and_retry);
	Builder.CreateBr(eval_loop);

	Builder.SetInsertPoint(first_atom_iter);
	llvm::Value* num_chars_consumed = constant_provider.getInt32(0);

	for(size_t i = 0; i < atoms.size(); i++) {
		std::unique_ptr<Atom>& atom = atoms[i];
		llvm::Value* input_index = Builder.CreateAdd(num_chars_consumed, string_start_index);

		llvm::BasicBlock* atom_iter_body = llvm::BasicBlock::Create(context, "atom_iter_body", main_function);

		Builder.CreateCondBr(Builder.CreateICmpULT(input_index, input_len), atom_iter_body, eval_reject_char);
		Builder.SetInsertPoint(atom_iter_body);

		auto insert_block = Builder.GetInsertBlock();
		auto insert_point = Builder.GetInsertPoint();
		llvm::Function* atom_function = atom->codegen();
		Builder.SetInsertPoint(insert_block, insert_point);

		llvm::Value* decision = Builder.CreateCall(
			atom_function->getFunctionType(),
			atom_function,
			std::vector<llvm::Value*> { buf, input_index, input_len }
		);
		llvm::Value* is_accept = Builder.CreateICmpNE(
			Builder.CreateAnd(decision, constant_provider.getInt32(static_cast<int32_t>(AcceptDecision::Accept))),
			constant_provider.getInt32(0)
		);
		llvm::Value* num_chars_to_consume = Builder.CreateAnd(decision, constant_provider.getInt32(static_cast<int32_t>(AcceptDecision::ConsumeMask)));
		num_chars_consumed = Builder.CreateAdd(num_chars_consumed, num_chars_to_consume);

		llvm::BasicBlock* next_atom_iter = llvm::BasicBlock::Create(context, "next_atom_iter", main_function);
		Builder.CreateCondBr(is_accept, next_atom_iter, eval_consume_and_retry);

		Builder.SetInsertPoint(next_atom_iter);
	}

	llvm::BasicBlock* completed = llvm::BasicBlock::Create(context, "completed", main_function);
	Builder.CreateBr(completed);

	Builder.SetInsertPoint(completed);
	Builder.CreateBr(end);

	resolved_is_accept->addIncoming(constant_provider.getBit(1), completed);

	Builder.SetInsertPoint(eval_reject_char);
	Builder.CreateBr(end);

	Builder.SetInsertPoint(end);
	if (regex.size()) {
		Builder.CreateRet(Builder.CreateIntCast(Builder.CreateNot(resolved_is_accept), type_provider.getInt32(), false));
	} else {
		Builder.CreateRet(constant_provider.getInt32(0));
	}

	std::error_code ec;
	llvm::raw_fd_ostream output("out.ll", ec);
	module.print(output, nullptr);

	return 0;
}
