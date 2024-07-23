#include <string>
#include <memory>
#include <vector>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include "Literal.h"
#include "AcceptDecision.h"
#include "TypeProvider.h"
#include "ConstantProvider.h"

Literal::Literal(char to_match, llvm::LLVMContext* context, llvm::Module* module, llvm::IRBuilder<>* builder)
: to_match{to_match}, context{context}, module{module}, builder{builder}
{
	TypeProvider type_provider(*context);
	generated_function_type = llvm::FunctionType::get(
		type_provider.getInt32(), // AcceptDecision
		std::vector<llvm::Type*> { type_provider.getBytePtr(), type_provider.getInt32(), type_provider.getInt32() }, // buf, index, and len
		false
	);
}

llvm::Function* Literal::codegen() {
	TypeProvider type_provider(*context);
	ConstantProvider constant_provider(type_provider);
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
		type_provider.getByte(),
		builder->CreateGEP(type_provider.getByte(), buf, std::vector<llvm::Value*> { index })
	);
	llvm::Value* is_equal = builder->CreateICmpEQ(input, constant_provider.getByte(to_match));

	llvm::BasicBlock* matches = llvm::BasicBlock::Create(*context, "matches", generated_function);
	builder->SetInsertPoint(matches);
	builder->CreateRet(constant_provider.getInt32(static_cast<int32_t>(AcceptDecision::Accept) | 1));

	llvm::BasicBlock* does_not_match = llvm::BasicBlock::Create(*context, "does_not_match", generated_function);
	builder->SetInsertPoint(does_not_match);
	builder->CreateRet(constant_provider.getInt32(1));

	builder->SetInsertPoint(entry);
	builder->CreateCondBr(is_equal, matches, does_not_match);

	return generated_function;
}

llvm::FunctionType* Literal::get_generated_function_type() const {
	return generated_function_type;
}

std::string Literal::get_generated_function_name() const {
	return module->getModuleIdentifier() + "_literal_matching__" + to_match;
}