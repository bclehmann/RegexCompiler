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
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_os_ostream.h>
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

llvm::Function* buildReadInput(llvm::LLVMContext& context, llvm::IRBuilder<>& builder, llvm::Module& module) {
	TypeProvider type_provider(context);
	ConstantProvider constant_provider(type_provider);
	llvm::FunctionType* read_input_function_type = llvm::FunctionType::get(
		type_provider.getBytePtr(), // buf
		std::vector<llvm::Type*> {},
		false
	);

	std::string read_input_function_name = "read_input";
	llvm::Function* existing = module.getFunction(read_input_function_name);

	if (existing) {
		return existing;
	}

	llvm::Function* panic_function = buildPanic(context, builder, module);

	llvm::Function* read_input = llvm::Function::Create(read_input_function_type, llvm::Function::PrivateLinkage, read_input_function_name, module);
	llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", read_input);

	llvm::FunctionType* realloc_type = llvm::FunctionType::get(
		type_provider.getBytePtr(),
		std::vector<llvm::Type*> { type_provider.getBytePtr(), type_provider.getInt32() }, // ptr, new_size,
		false
	);
	llvm::Function* existing_realloc = module.getFunction("realloc");
	llvm::Function* realloc_function = existing_realloc
		? existing_realloc
		: llvm::Function::Create(realloc_type, llvm::Function::ExternalLinkage, "realloc", module);

	llvm::FunctionType* getchar_type = llvm::FunctionType::get(type_provider.getInt32(), std::vector<llvm::Type*> {}, false);
	llvm::Function* existing_getchar = module.getFunction("getchar");
	llvm::Function* getchar_function = existing_getchar
		? existing_getchar
		: llvm::Function::Create(getchar_type, llvm::Function::ExternalLinkage, "getchar", module);

	llvm::BasicBlock* panic = llvm::BasicBlock::Create(context, "panic", read_input);
	llvm::BasicBlock* line_start = llvm::BasicBlock::Create(context, "line_start", read_input);
	llvm::BasicBlock* line_end = llvm::BasicBlock::Create(context, "line_end", read_input);
	llvm::BasicBlock* loop_start = llvm::BasicBlock::Create(context, "loop_start", read_input);
	llvm::BasicBlock* loop_body = llvm::BasicBlock::Create(context, "loop_body", read_input);
	llvm::BasicBlock* loop_end = llvm::BasicBlock::Create(context, "loop_end", read_input);

	llvm::BasicBlock* first_alloc = llvm::BasicBlock::Create(context, "first_alloc", read_input);
	llvm::BasicBlock* alloc = llvm::BasicBlock::Create(context, "alloc", read_input);
	llvm::BasicBlock* needs_realloc = llvm::BasicBlock::Create(context, "needs_realloc", read_input);

	builder.SetInsertPoint(entry);

	llvm::Value* line_start_ref = builder.CreateAlloca(type_provider.getInt32(), constant_provider.getInt32(1), "line_start_ref");
	builder.CreateStore(
		constant_provider.getInt32(0),
		builder.CreateGEP(
			type_provider.getInt32(),
			line_start_ref,
			std::vector<llvm::Value*> { constant_provider.getInt32(0) }
		)
	);

	builder.CreateBr(first_alloc);

	builder.SetInsertPoint(first_alloc);
	llvm::Value* buf_ref = builder.CreateAlloca(type_provider.getBytePtr(), constant_provider.getInt32(1), "buf_ref");
	llvm::Value* buf_size_ref = builder.CreateAlloca(type_provider.getInt32(), constant_provider.getInt32(1), "buf_size_ref");
	const int init_size = 1024;
	llvm::Value* first_buf = builder.CreateCall(realloc_type, realloc_function, std::vector<llvm::Value*> {
		llvm::ConstantPointerNull::get(type_provider.getBytePtr()),
		constant_provider.getInt32(init_size)
	});
	builder.CreateStore(
		first_buf,
		builder.CreateGEP(
			type_provider.getBytePtr(),
			buf_ref,
			std::vector<llvm::Value*> { constant_provider.getInt32(0) }
		)
	);
	builder.CreateStore(
		constant_provider.getInt32(init_size),
		builder.CreateGEP(
			type_provider.getInt32(),
			buf_size_ref,
			std::vector<llvm::Value*> { constant_provider.getInt32(0) }
		)
	);

	builder.CreateBr(line_start);

	builder.SetInsertPoint(needs_realloc);
	builder.CreateBr(alloc);

	builder.SetInsertPoint(alloc);

	llvm::Value* old_buf_size = builder.CreateLoad(
		type_provider.getInt32(),
		builder.CreateGEP(
			type_provider.getInt32(),
			buf_size_ref,
			std::vector<llvm::Value*> { constant_provider.getInt32(0) }
		)
	);
	llvm::Value* new_buf_size = builder.CreateUDiv(
		builder.CreateMul(old_buf_size, constant_provider.getInt32(3)),
		constant_provider.getInt32(2)
	);
	builder.CreateStore(
		new_buf_size,
		builder.CreateGEP(
			type_provider.getInt32(),
			buf_size_ref,
			std::vector<llvm::Value*> { constant_provider.getInt32(0) }
		)
	);

	llvm::Value* new_buf = builder.CreateCall(realloc_type, realloc_function, std::vector<llvm::Value*> {
		builder.CreateLoad(
			type_provider.getBytePtr(),
			builder.CreateGEP(
				type_provider.getBytePtr(),
				buf_ref,
				std::vector<llvm::Value*> { constant_provider.getInt32(0) }
			)
		),
		new_buf_size
	});
	builder.CreateStore(
		new_buf,
		builder.CreateGEP(
			type_provider.getBytePtr(),
			buf_ref,
			std::vector<llvm::Value*> { constant_provider.getInt32(0) }
		)
	);

	llvm::BasicBlock* after_alloc = llvm::BasicBlock::Create(context, "after_alloc", read_input);
	builder.CreateBr(after_alloc);

	builder.SetInsertPoint(after_alloc);
	builder.CreateCondBr(builder.CreateIsNull(new_buf), panic, loop_start);

	builder.SetInsertPoint(line_start);
	llvm::Value* line_start_index = builder.CreateLoad(
		type_provider.getInt32(),
		builder.CreateGEP(
			type_provider.getInt32(),
			line_start_ref,
			std::vector<llvm::Value*> { constant_provider.getInt32(0) }
		)
	);

	builder.CreateBr(loop_start);

	builder.SetInsertPoint(loop_start);
	llvm::PHINode* index = builder.CreatePHI(type_provider.getInt32(), 3);
	index->addIncoming(constant_provider.getInt32(sizeof(int32_t)), line_start);
	index->addIncoming(index, after_alloc);
	index->addIncoming(builder.CreateAdd(index, constant_provider.getInt32(1)), loop_end);

	llvm::Value* true_index = builder.CreateAdd(line_start_index, index);

	llvm::Value* buf_size = builder.CreateLoad(
		type_provider.getInt32(),
		builder.CreateGEP(
			type_provider.getInt32(),
			buf_size_ref,
			std::vector<llvm::Value*> { constant_provider.getInt32(0) }
		)
	);

	builder.CreateCondBr(
		builder.CreateICmpUGE(true_index, builder.CreateSub(buf_size, constant_provider.getInt32(16))), // Since we may need to terminate a line, which uses up a lot of space
		needs_realloc,
		loop_body
	);

	builder.SetInsertPoint(loop_body);
	llvm::Value* buf = builder.CreateLoad(
		type_provider.getBytePtr(),
		builder.CreateGEP(
			type_provider.getBytePtr(),
			buf_ref,
			std::vector<llvm::Value*> { constant_provider.getInt32(0) }
		)
	);

	llvm::BasicBlock* was_eof = llvm::BasicBlock::Create(context, "was_eof", read_input);
	llvm::BasicBlock* was_newline = llvm::BasicBlock::Create(context, "was_newline", read_input);
	llvm::BasicBlock* was_not_newline = llvm::BasicBlock::Create(context, "was_not_newline", read_input);
	llvm::Value* input = builder.CreateCall(getchar_type, getchar_function);

	llvm::Value* input_equals_eof = builder.CreateICmpEQ(input, constant_provider.getInt32(EOF, false));
	llvm::Value* line_len = builder.CreateSub(true_index, builder.CreateAdd(line_start_index, constant_provider.getInt32(sizeof(int32_t))));

	builder.CreateCondBr(builder.CreateICmpEQ(input, constant_provider.getInt32('\n')), was_newline, was_not_newline);

	builder.SetInsertPoint(was_not_newline);
	builder.CreateStore(
		builder.CreateIntCast(input, type_provider.getByte(), true),
		builder.CreateGEP(
			type_provider.getByte(),
			buf,
			std::vector<llvm::Value*> { true_index }
		)
	);

	builder.CreateCondBr(input_equals_eof, was_eof, loop_end);

	builder.SetInsertPoint(was_newline);
	builder.CreateStore(
		constant_provider.getByte(0),
		builder.CreateGEP(
			type_provider.getByte(),
			buf,
			std::vector<llvm::Value*> { true_index }
		)
	);
	builder.CreateBr(line_end);

	builder.SetInsertPoint(was_eof);
	builder.CreateStore(
		builder.CreateNeg(line_len),
		builder.CreateGEP(
			type_provider.getInt32(),
			builder.CreatePointerCast(buf, type_provider.getInt32Ptr()),
			std::vector<llvm::Value*> { builder.CreateUDiv(line_start_index, constant_provider.getInt32(sizeof(int32_t))) }
		)
	);
	builder.CreateRet(buf);

	builder.SetInsertPoint(loop_end);
	builder.CreateBr(loop_start);

	builder.SetInsertPoint(line_end);
	llvm::raw_os_ostream ostream(std::cerr);
	index->getType()->print(ostream, true);
	ostream << '\n';
	builder.CreateAdd(line_start_index, constant_provider.getInt32(sizeof(int32_t)))->getType()->print(ostream, true);
	ostream << '\n';
	builder.CreateSub(index, builder.CreateAdd(line_start_index, constant_provider.getInt32(sizeof(int32_t))))->getType()->print(ostream, true);
	ostream << '\n';

	ostream.flush();
	builder.CreateStore(
		line_len,
		builder.CreateGEP(
			type_provider.getInt32(),
			builder.CreatePointerCast(buf, type_provider.getInt32Ptr()),
			std::vector<llvm::Value*> { builder.CreateUDiv(line_start_index, constant_provider.getInt32(sizeof(int32_t))) }
		)
	);

	builder.CreateStore(
		builder.CreateAdd(true_index, constant_provider.getInt32(1)),
		builder.CreateGEP(
			type_provider.getInt32(),
			line_start_ref,
			std::vector<llvm::Value*> { constant_provider.getInt32(0) }
		)
	);
	llvm::BasicBlock* align_loop = llvm::BasicBlock::Create(context, "align_loop", read_input);
	llvm::BasicBlock* goto_next_line = llvm::BasicBlock::Create(context, "goto_next_line", read_input);
	llvm::BasicBlock* moar_align = llvm::BasicBlock::Create(context, "moar_align", read_input);

	builder.CreateBr(align_loop);

	builder.SetInsertPoint(align_loop);
	llvm::Value* align_index = builder.CreateLoad(
		type_provider.getInt32(),
		builder.CreateGEP(
			type_provider.getInt32(),
			line_start_ref,
			std::vector<llvm::Value*> { constant_provider.getInt32(0) }
		)
	);

	builder.CreateStore(
		constant_provider.getByte(0),
		builder.CreateGEP(
			type_provider.getByte(),
			buf,
			std::vector<llvm::Value*> { align_index }
		)
	);

	builder.CreateCondBr(
		builder.CreateICmpEQ(builder.CreateURem(align_index, constant_provider.getInt32(8)), constant_provider.getInt32(0)),
		goto_next_line,
		moar_align
	);

	builder.SetInsertPoint(moar_align);
	builder.CreateStore(
		builder.CreateAdd(align_index, constant_provider.getInt32(1)),
		builder.CreateGEP(
			type_provider.getInt32(),
			line_start_ref,
			std::vector<llvm::Value*> { constant_provider.getInt32(0) }
		)
	);

	builder.CreateBr(align_loop);

	builder.SetInsertPoint(goto_next_line);
	builder.CreateBr(line_start);

	builder.SetInsertPoint(panic);
	builder.CreateCall(panic_function->getFunctionType(), panic_function, std::vector<llvm::Value*> { builder.CreateGlobalStringPtr("Error: Could not allocate buffer") });
	builder.CreateUnreachable();

	return read_input;
}

llvm::Function* buildEvaluateLine(llvm::LLVMContext& context, llvm::IRBuilder<>& builder, llvm::Module& module, const std::vector<std::unique_ptr<Atom>>& atoms) {
	TypeProvider type_provider(context);
	ConstantProvider constant_provider(type_provider);
	llvm::FunctionType* eval_line_function_type = llvm::FunctionType::get(
		type_provider.getBit(), // Accept/Reject
		std::vector<llvm::Type*> { type_provider.getBytePtr(), type_provider.getInt32() }, // buf and len
		false
	);

	std::string eval_line_function_name = "eval_line";
	llvm::Function* existing = module.getFunction(eval_line_function_name);

	if (existing) {
		return existing;
	}

	llvm::Function* eval_line = llvm::Function::Create(eval_line_function_type, llvm::Function::PrivateLinkage, eval_line_function_name, module);
	llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", eval_line);
	builder.SetInsertPoint(entry);

	llvm::Value* buf = eval_line->args().begin();
	llvm::Value* input_len = (eval_line->args().begin() + 1);

	llvm::BasicBlock* eval_loop = llvm::BasicBlock::Create(context, "eval_loop", eval_line);
	builder.CreateBr(eval_loop);
	llvm::BasicBlock* eval_consume_and_retry = llvm::BasicBlock::Create(context, "eval_consume_and_retry", eval_line);
	llvm::BasicBlock* eval_reject_char = llvm::BasicBlock::Create(context, "eval_reject_char", eval_line);

	llvm::BasicBlock* end = llvm::BasicBlock::Create(context, "end", eval_line);
	builder.SetInsertPoint(end);
	llvm::PHINode* resolved_is_accept = builder.CreatePHI(type_provider.getBit(), atoms.size() + 1);
	resolved_is_accept->addIncoming(constant_provider.getBit(0), eval_reject_char);

	builder.SetInsertPoint(eval_loop);
	llvm::PHINode* string_start_index = builder.CreatePHI(type_provider.getInt32(), 0);
	string_start_index->addIncoming(constant_provider.getInt32(0), entry);
	string_start_index->addIncoming(builder.CreateAdd(string_start_index, constant_provider.getInt32(1)), eval_consume_and_retry);

	llvm::BasicBlock* first_atom_iter = llvm::BasicBlock::Create(context, "first_atom_iter", eval_line);

	builder.CreateBr(first_atom_iter);

	builder.SetInsertPoint(eval_consume_and_retry);
	builder.CreateBr(eval_loop);

	builder.SetInsertPoint(first_atom_iter);
	llvm::Value* num_chars_consumed = constant_provider.getInt32(0);

	for(size_t i = 0; i < atoms.size(); i++) {
		const std::unique_ptr<Atom>& atom = atoms[i];
		llvm::Value* input_index = builder.CreateAdd(num_chars_consumed, string_start_index);

		llvm::BasicBlock* atom_iter_body = llvm::BasicBlock::Create(context, "atom_iter_body", eval_line);

		builder.CreateCondBr(builder.CreateICmpULT(input_index, input_len), atom_iter_body, eval_reject_char);
		builder.SetInsertPoint(atom_iter_body);

		auto insert_block = builder.GetInsertBlock();
		auto insert_point = builder.GetInsertPoint();
		llvm::Function* atom_function = atom->codegen();
		builder.SetInsertPoint(insert_block, insert_point);

		llvm::Value* decision = builder.CreateCall(
			atom_function->getFunctionType(),
			atom_function,
			std::vector<llvm::Value*> { buf, input_index, input_len }
		);
		llvm::Value* is_accept = builder.CreateICmpNE(
			builder.CreateAnd(decision, constant_provider.getInt32(static_cast<int32_t>(AcceptDecision::Accept))),
			constant_provider.getInt32(0)
		);
		llvm::Value* num_chars_to_consume = builder.CreateAnd(decision, constant_provider.getInt32(static_cast<int32_t>(AcceptDecision::ConsumeMask)));
		num_chars_consumed = builder.CreateAdd(num_chars_consumed, num_chars_to_consume);

		llvm::BasicBlock* next_atom_iter = llvm::BasicBlock::Create(context, "next_atom_iter", eval_line);
		builder.CreateCondBr(is_accept, next_atom_iter, eval_consume_and_retry);

		builder.SetInsertPoint(next_atom_iter);
	}

	llvm::BasicBlock* completed = llvm::BasicBlock::Create(context, "completed", eval_line);
	builder.CreateBr(completed);

	builder.SetInsertPoint(completed);
	builder.CreateBr(end);

	resolved_is_accept->addIncoming(constant_provider.getBit(1), completed);

	builder.SetInsertPoint(eval_reject_char);
	builder.CreateBr(end);

	builder.SetInsertPoint(end);
	if (atoms.size()) {
		builder.CreateRet(resolved_is_accept);
	} else {
		builder.CreateRet(constant_provider.getInt32(0));
	}

	return eval_line;
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

	llvm::Function* read_input = buildReadInput(context, Builder, module);

	Builder.SetInsertPoint(entry);

	llvm::Value* cooler_buf = Builder.CreateCall(read_input->getFunctionType(), read_input, std::vector<llvm::Value*>{});

#if 1
	llvm::FunctionType* trap_intrinsic_type = llvm::FunctionType::get(
		type_provider.getVoid(),
		std::vector<llvm::Type*> {},
		false
	);
	llvm::Function* trap_intrinsic = llvm::Function::Create(trap_intrinsic_type, llvm::Function::ExternalLinkage, "llvm.debugtrap", module);
	Builder.CreateCall(trap_intrinsic->getFunctionType(), trap_intrinsic, std::vector<llvm::Value*> { });
#endif

	llvm::Function* existing_puts = module.getFunction("puts");

	Builder.CreateCall(existing_puts->getFunctionType(), existing_puts, std::vector<llvm::Value*>{cooler_buf});
	Builder.CreateRet(constant_provider.getInt32(0));

#if 0
	llvm::AllocaInst* buf;
	llvm::Value* input_len;
	llvm::BasicBlock* post_loop = llvm::BasicBlock::Create(context, "post_loop", main_function);
	llvm::BasicBlock* readToBuf = buildReadToBuf(context, Builder, module, main_function, post_loop, buf, input_len);

	Builder.SetInsertPoint(entry);
	Builder.CreateBr(readToBuf);

	llvm::Function* eval_line = buildEvaluateLine(context, Builder, module, atoms);

	Builder.SetInsertPoint(post_loop);
	llvm::Value* accept = Builder.CreateCall(eval_line->getFunctionType(), eval_line, std::vector<llvm::Value*> { buf, input_len });

	Builder.CreateRet(Builder.CreateIntCast(Builder.CreateNot(accept), type_provider.getInt32(), false));
#endif
	std::error_code ec;
	llvm::raw_fd_ostream output("out.ll", ec);
	module.print(output, nullptr);

	return 0;
}
