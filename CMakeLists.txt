project(RegexCompiler)

find_package(LLVM 14 REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

message(STATUS "LLVM VERSION : ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_executable(RegexCompiler "main.cpp")

llvm_map_components_to_libnames(llvm_libs support core irreader)

target_link_libraries(RegexCompiler ${llvm_libs})
