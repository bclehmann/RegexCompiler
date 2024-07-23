#pragma once

#include <string>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include "Atom.h"

class StringStartMetacharacter : public Atom {
public:
	StringStartMetacharacter(llvm::LLVMContext* context, llvm::Module* module, llvm::IRBuilder<>* builder);
	llvm::Function* codegen() override;
	llvm::FunctionType* get_generated_function_type() const;
	std::string get_generated_function_name() const;

private:
	llvm::LLVMContext* context;
	llvm::Module* module;
	llvm::IRBuilder<>* builder;
	llvm::FunctionType* generated_function_type;
};