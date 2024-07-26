#include <string>
#include <memory>
#include <vector>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include "Negate.h"
#include "AcceptDecision.h"
#include "TypeProvider.h"
#include "ConstantProvider.h"

Negate::Negate(Atom* to_negate, llvm::LLVMContext* context, llvm::Module* module, llvm::IRBuilder<>* builder)
: to_negate{to_negate}, context{context}, module{module}, builder{builder}
{
	TypeProvider type_provider(*context);
	generated_function_type = llvm::FunctionType::get(
		type_provider.getInt32(), // AcceptDecision
		std::vector<llvm::Type*> { type_provider.getBytePtr(), type_provider.getInt32(), type_provider.getInt32() }, // buf, index, and len
		false
	);
}

llvm::Function* Negate::codegen() {
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
	llvm::Value* len = (generated_function->args().begin() + 2);

	llvm::Function* function_to_negate = to_negate->codegen();

	builder->SetInsertPoint(entry);
	llvm::Value* unnegated = builder->CreateCall(
		function_to_negate->getFunctionType(),
		function_to_negate,
		std::vector<llvm::Value*> { buf, index, len }
	);

	llvm::Value* negated = builder->CreateXor(unnegated, constant_provider.getInt32(static_cast<int32_t>(AcceptDecision::Accept)));
	builder->CreateRet(negated);

	return generated_function;
}

llvm::FunctionType* Negate::get_generated_function_type() const {
	return generated_function_type;
}

std::string Negate::get_generated_function_name() const {
	return module->getModuleIdentifier() + "_metacharacter__negate__" + to_negate->get_generated_function_name();
}