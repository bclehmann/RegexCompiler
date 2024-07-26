#pragma once

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>

class TypeProvider {
public:
	explicit TypeProvider(llvm::LLVMContext& context);
	llvm::Type* getVoid();
	llvm::IntegerType* getByte();
	llvm::IntegerType* getBit();
	llvm::IntegerType* getInt32();
	llvm::IntegerType* getInt64();
	llvm::PointerType* getVoidPtr();
	llvm::PointerType* getBytePtr();
	llvm::PointerType* getBitPtr();
	llvm::PointerType* getInt32Ptr();
	llvm::PointerType* getInt64Ptr();
private:
	llvm::LLVMContext& context;
};