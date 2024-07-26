#pragma once

#include <string>
#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include "Atom.h"

class Negate : public Atom {
public:
	Negate(Atom* to_negate, llvm::LLVMContext* context, llvm::Module* module, llvm::IRBuilder<>* builder);
	llvm::Function* codegen() override;
	llvm::FunctionType* get_generated_function_type() const override;
	std::string get_generated_function_name() const override;

private:
	Atom* to_negate;
	llvm::LLVMContext* context;
	llvm::Module* module;
	llvm::IRBuilder<>* builder;
	llvm::FunctionType* generated_function_type;
};