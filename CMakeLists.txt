# ---------------------------------------------------------------
# Build file for the Caselaw Search Engine (C++17).
#
# Build (from the project folder):
#   cmake -S . -B build -G Ninja      (step 1: configure, only needed once)
#   cmake --build build               (step 2: compile; repeat after every change)
# ---------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)
project(CaselawSearchEngine LANGUAGES CXX)

# Use standard C++17 (no compiler-specific extensions).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build optimised code by default ("Release"); -O2 = optimisation level 2.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# Show helpful compiler warnings.
add_compile_options(-Wall -Wextra)

# Where the compiler looks for header files:
#   include/      our own headers (.h)
#   third_party/  external single-header libraries (json.hpp)
include_directories(include third_party)

# ---------------------------------------------------------------
# Programs
# ---------------------------------------------------------------

# build_index: reads cases.jsonl and builds the index files.
# More .cpp files (tokenizer, lexicon, ...) are added to this list as we write them.
add_executable(build_index
    src/build_index.cpp
    src/docindex.cpp
)

# On Windows (MinGW), link the C++ runtime into the .exe so it runs
# without needing extra DLL files next to it.
if(MINGW)
    target_link_options(build_index PRIVATE -static)
endif()
