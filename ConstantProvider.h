#pragma once

#include <cstdint>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include "TypeProvider.h"

class ConstantProvider {
public:
	explicit ConstantProvider(TypeProvider& type_provider);
	llvm::ConstantInt* getBit(uint8_t value);
	llvm::ConstantInt* getByte(uint8_t value, bool is_signed=false);
	llvm::ConstantInt* getInt32(uint32_t value, bool is_signed=false);
	llvm::ConstantInt* getInt64(uint64_t value, bool is_signed=false);
private:
	TypeProvider& type_provider;
};