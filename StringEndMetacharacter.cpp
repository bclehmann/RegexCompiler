#include "StringEndMetacharacter.h"
#include "AcceptDecision.h"
#include "TypeProvider.h"
#include "ConstantProvider.h"

StringEndMetacharacter::StringEndMetacharacter(llvm::LLVMContext* context, llvm::Module* module, llvm::IRBuilder<>* builder)
: context{context}, module{module}, builder{builder}
{
	TypeProvider type_provider(*context);
	generated_function_type = llvm::FunctionType::get(
		type_provider.getInt32(), // AcceptDecision
		std::vector<llvm::Type*> { type_provider.getBytePtr(), type_provider.getInt32(), type_provider.getInt32() }, // buf, index, and len
		false
	);
}

llvm::Function* StringEndMetacharacter::codegen() {
	TypeProvider type_provider(*context);
	ConstantProvider constant_provider(type_provider);
	llvm::Function* existing = module->getFunction(get_generated_function_name());

	if (existing) {
		return existing;
	}

	llvm::Function* generated_function = llvm::Function::Create(generated_function_type, llvm::Function::PrivateLinkage, get_generated_function_name(), module);
	llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context, "entry", generated_function);
	builder->SetInsertPoint(entry);

	llvm::Value* index = (generated_function->args().begin() + 1);
	llvm::Value* len = (generated_function->args().begin() + 2);
	llvm::Value* is_at_end = builder->CreateICmpEQ(index, builder->CreateSub(len, constant_provider.getInt32(1)));

	llvm::BasicBlock* matches = llvm::BasicBlock::Create(*context, "matches", generated_function);
	llvm::BasicBlock* does_not_match = llvm::BasicBlock::Create(*context, "does_not_match", generated_function);

	builder->CreateCondBr(is_at_end, matches, does_not_match);

	builder->SetInsertPoint(matches);
	builder->CreateRet(constant_provider.getInt32(static_cast<int32_t>(AcceptDecision::Accept) | 0));

	builder->SetInsertPoint(does_not_match);
	builder->CreateRet(constant_provider.getInt32(1));

	return generated_function;
}

llvm::FunctionType* StringEndMetacharacter::get_generated_function_type() const {
	return generated_function_type;
}

std::string StringEndMetacharacter::get_generated_function_name() const {
	return module->getModuleIdentifier() + "_metacharacter__string_end";
}