#include "ConstantProvider.h"

ConstantProvider::ConstantProvider(TypeProvider& type_provider)
: type_provider{ type_provider }
{ }

llvm::ConstantInt* ConstantProvider::getBit(uint8_t value) {
	return llvm::ConstantInt::get(type_provider.getBit(), value);
}

llvm::ConstantInt* ConstantProvider::getByte(uint8_t value, bool is_signed) {
	return llvm::ConstantInt::get(type_provider.getByte(), value, is_signed);
}

llvm::ConstantInt* ConstantProvider::getInt32(uint32_t value, bool is_signed) {
	return llvm::ConstantInt::get(type_provider.getInt32(), value, is_signed);
}