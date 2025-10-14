cmake_minimum_required(VERSION 3.20)
project(Kaleidoscope LANGUAGES CXX)

# Path to your LLVM build (change if different)
set(LLVM_DIR "C:/build/lib/cmake/llvm")

# Find LLVM package configuration
find_package(LLVM REQUIRED CONFIG)

# Include LLVM headers
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Create your executable
add_executable(kaledio_lang kaledio_lang.cpp)

# Link with LLVM libraries
llvm_map_components_to_libnames(LLVM_LIBS core support native orcjit irreader)

target_link_libraries(kaledio_lang ${LLVM_LIBS})
target_compile_features(kaledio_lang PRIVATE cxx_std_17)

# Optional: show some debug info
message(STATUS "Using LLVM from: ${LLVM_DIR}")
message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM libs: ${LLVM_LIBS}")
