cmake_minimum_required(VERSION 3.20)
project(Kaleidoscope LANGUAGES CXX)

# Path to your LLVM build (change if different)
set(LLVM_DIR "C:/build/lib/cmake/llvm")

# Find LLVM package configuration
find_package(LLVM REQUIRED CONFIG)

# --- CHANGE 1: Use 'SYSTEM' here ---
# This tells the compiler "These are external library headers, do not show warnings for them."
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS}) 

add_definitions(${LLVM_DEFINITIONS})

# Create your executable
add_executable(kaledio_lang
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/ast.cpp
    src/codegen.cpp
)

# Link with LLVM libraries
llvm_map_components_to_libnames(LLVM_LIBS core support native orcjit irreader)

target_link_libraries(kaledio_lang ${LLVM_LIBS})
target_compile_features(kaledio_lang PRIVATE cxx_std_17)

# --- CHANGE 2: Add Warning Flags ---
if(MSVC)
    # Visual Studio Compiler settings
    # /W3 = Enable standard warnings
    # /we4715 = Treat "not all control paths return a value" as an ERROR (force crash at compile time)
    target_compile_options(kaledio_lang PRIVATE /W3 /we4715)
else()
    # Clang / GCC settings
    # -Wall = Enable all standard warnings
    # -Wreturn-type = Specifically catch missing returns
    target_compile_options(kaledio_lang PRIVATE -Wall -Wreturn-type)
endif()

# Optional: show some debug info
message(STATUS "Using LLVM from: ${LLVM_DIR}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM libs: ${LLVM_LIBS}")