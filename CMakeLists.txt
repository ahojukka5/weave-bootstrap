cmake_minimum_required(VERSION 3.16)

project(weave_bootstrap C)

# Runtime path - configurable, defaults to slim internal runtime
if(NOT DEFINED WEAVE_RUNTIME)
  set(WEAVE_RUNTIME ${CMAKE_SOURCE_DIR}/stage0/runtime/stdlib_runtime.c)
endif()

if(NOT EXISTS ${WEAVE_RUNTIME})
  message(WARNING "Runtime not found at ${WEAVE_RUNTIME}. You can:")
  message(WARNING "  1. Set -DWEAVE_RUNTIME=/path/to/runtime.c when running cmake")
  message(WARNING "  2. Use internal slim runtime at stage0/runtime/stdlib_runtime.c (default)")
  message(WARNING "  Note: stage0/runtime/runtime.c is a minimal shim; helpers live in stdlib_runtime.c")
endif()

# Build Stage0 via its CMake
add_subdirectory(stage0)

# Build Stage1 via its CMake (depends on weavec0 from stage0)
add_subdirectory(stage1)

# Build Stage2 via its CMake (depends on weavec1 from stage1)
add_subdirectory(stage2)

# Enable CTest at root to aggregate tests from subdirectories
enable_testing()
