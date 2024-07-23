#pragma once
#include <llvm/IR/Function.h>

class Atom {
public:
	virtual llvm::Function* codegen() = 0;
};