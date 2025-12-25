cmake_minimum_required(VERSION 3.16)

project(weave_bootstrap C)

# Runtime path - no longer needed (Weave defines main() directly)
# Kept for backward compatibility if external runtime is provided
if(NOT DEFINED WEAVE_RUNTIME)
  set(WEAVE_RUNTIME "")
endif()

# Build Stage0 via its CMake
add_subdirectory(stage0)

# Build Stage1 via its CMake (depends on weavec0 from stage0)
add_subdirectory(stage1)

# Build Stage2 via its CMake (depends on weavec1 from stage1)
add_subdirectory(stage2)

# Enable CTest at root to aggregate tests from subdirectories
enable_testing()
