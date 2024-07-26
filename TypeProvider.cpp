#include "TypeProvider.h"

TypeProvider::TypeProvider(llvm::LLVMContext& context)
: context { context }
{ }

llvm::Type* TypeProvider::getVoid() {
	return llvm::Type::getVoidTy(context);
}

llvm::IntegerType* TypeProvider::getByte() {
	return llvm::Type::getInt8Ty(context);
}

llvm::IntegerType* TypeProvider::getBit() {
	return llvm::Type::getInt1Ty(context);
}

llvm::IntegerType* TypeProvider::getInt32() {
	return llvm::Type::getInt32Ty(context);
}

llvm::IntegerType* TypeProvider::getInt64() {
	return llvm::Type::getInt32Ty(context);
}

llvm::PointerType* TypeProvider::getVoidPtr() {
	return getVoid()->getPointerTo();
}

llvm::PointerType* TypeProvider::getBytePtr() {
	return getByte()->getPointerTo();
}

llvm::PointerType* TypeProvider::getBitPtr() {
	return getBit()->getPointerTo();
}

llvm::PointerType* TypeProvider::getInt32Ptr() {
	return getInt32()->getPointerTo();
}

llvm::PointerType* TypeProvider::getInt64Ptr() {
	return getInt64()->getPointerTo();
}