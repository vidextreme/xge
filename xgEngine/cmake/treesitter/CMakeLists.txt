# Copyright (c) 2026 John David Uy
# Licensed under the MIT License. See LICENSE for details.
#
# Builds the tree-sitter libraries xgReflectGen loads (libtree-sitter, libtree-sitter-cpp) into XGE_TOOLS_DIR.
# Used outside Windows; Windows uses the DLLs in 3rdparty/TreeSitter. The TreeSitter binding needs core 0.25+.
cmake_minimum_required(VERSION 3.25)
project(xge_treesitter LANGUAGES C)
include(FetchContent)

FetchContent_Declare(treesitter
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
    GIT_TAG v0.25.10
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _none)
FetchContent_Declare(treesitter_cpp
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-cpp.git
    GIT_TAG v0.23.4
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _none)
FetchContent_MakeAvailable(treesitter treesitter_cpp)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${XGE_TOOLS_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${XGE_TOOLS_DIR})

add_library(tree-sitter SHARED ${treesitter_SOURCE_DIR}/lib/src/lib.c)
target_include_directories(tree-sitter PRIVATE ${treesitter_SOURCE_DIR}/lib/include ${treesitter_SOURCE_DIR}/lib/src)
target_compile_definitions(tree-sitter PRIVATE _POSIX_C_SOURCE=200112L _DEFAULT_SOURCE _DARWIN_C_SOURCE)
set_target_properties(tree-sitter PROPERTIES C_STANDARD 11)

add_library(tree-sitter-cpp SHARED ${treesitter_cpp_SOURCE_DIR}/src/parser.c ${treesitter_cpp_SOURCE_DIR}/src/scanner.c)
target_include_directories(tree-sitter-cpp PRIVATE ${treesitter_cpp_SOURCE_DIR}/src)
set_target_properties(tree-sitter-cpp PROPERTIES C_STANDARD 11)
