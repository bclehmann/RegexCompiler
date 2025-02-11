cmake_minimum_required(VERSION 3.22)
if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

project(RegexCompiler)

set(CMAKE_CXX_STANDARD 17)
find_package(LLVM 14 REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

message(STATUS "LLVM VERSION : ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

SET(ENV "DEBUG" CACHE STRING "Either DEBUG or RELEASE")

if (MSVC)
	if("${ENV}" STREQUAL "RELEASE")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2")
	else()
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DEBUG")
	endif()
	add_compile_options(/W4)
else()
	if("${ENV}" STREQUAL "RELEASE")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
	else()
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
	endif()
	add_compile_options(-Wall -Wextra -Wpedantic)
endif()
message(STATUS "ENV: ${ENV}")
message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

add_executable(RegexCompiler
	main.cpp
	Literal.cpp
	TypeProvider.cpp
	ConstantProvider.cpp
	StringStartMetacharacter.cpp
	StringEndMetacharacter.cpp
	Digit.cpp
)

llvm_map_components_to_libnames(llvm_libs support core irreader)

target_link_libraries(RegexCompiler ${llvm_libs})
