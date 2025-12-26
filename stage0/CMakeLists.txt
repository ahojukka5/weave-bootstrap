cmake_minimum_required(VERSION 3.16)

# Enable C++ for JIT support (even though main project is C)
enable_language(CXX OPTIONAL)

project(weave_stage0 C CXX)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Enable extensions to get POSIX functions like realpath() on Unix-like systems.
set(CMAKE_C_EXTENSIONS ON)

# Enable AddressSanitizer for memory corruption detection in Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()

# Option to automatically download and build LLVM if not found
option(WEAVE_AUTO_BUILD_LLVM "Automatically download and build LLVM if not found" OFF)

# Find LLVM (optional - falls back to clang if not found)
find_package(LLVM QUIET CONFIG)

# If LLVM not found and auto-build is enabled, download and build it
if(NOT LLVM_FOUND AND WEAVE_AUTO_BUILD_LLVM)
  message(STATUS "")
  message(STATUS "==========================================")
  message(STATUS "LLVM not found. Downloading and building LLVM")
  message(STATUS "WARNING: This will take 30-60+ minutes and requires:")
  message(STATUS "  - C++ compiler (C++17 support)")
  message(STATUS "  - ~10GB disk space")
  message(STATUS "  - 4+ GB RAM")
  message(STATUS "==========================================")
  message(STATUS "")
  
  include(ExternalProject)
  
  # Use a recent stable LLVM version
  set(LLVM_VERSION "18.1.0")
  set(LLVM_SOURCE_DIR "${CMAKE_BINARY_DIR}/llvm-src")
  set(LLVM_BINARY_DIR "${CMAKE_BINARY_DIR}/llvm-build")
  set(LLVM_INSTALL_DIR "${CMAKE_BINARY_DIR}/llvm-install")
  
  # Enable C++ language for LLVM build (even though this project is C-only)
  enable_language(CXX OPTIONAL)
  
  # Check for C++ compiler (required for building LLVM)
  if(NOT CMAKE_CXX_COMPILER)
    message(FATAL_ERROR "C++ compiler required to build LLVM. Install a C++ compiler or use system LLVM.")
  endif()
  
  message(STATUS "Using C++ compiler: ${CMAKE_CXX_COMPILER}")
  
  # Download and configure LLVM using Git (faster than tarball for development)
  # For production, consider using release tarball with URL/SHA256
  # Note: llvm-project contains multiple subprojects; we only need the llvm/ subdirectory
  ExternalProject_Add(
    llvm_external
    GIT_REPOSITORY "https://github.com/llvm/llvm-project.git"
    GIT_TAG "llvmorg-${LLVM_VERSION}"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SOURCE_DIR "${LLVM_SOURCE_DIR}"
    SOURCE_SUBDIR "llvm"
    BINARY_DIR "${LLVM_BINARY_DIR}"
    INSTALL_DIR "${LLVM_INSTALL_DIR}"
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX:PATH=${LLVM_INSTALL_DIR}
      -DCMAKE_BUILD_TYPE=Release
      -DLLVM_ENABLE_PROJECTS=
      -DLLVM_ENABLE_BINDINGS=OFF
      -DLLVM_ENABLE_OCAMLDOC=OFF
      -DLLVM_ENABLE_ZLIB=OFF
      -DLLVM_ENABLE_TERMINFO=OFF
      -DLLVM_TARGETS_TO_BUILD=host
      -DLLVM_BUILD_LLVM_DYLIB=ON
      -DLLVM_LINK_LLVM_DYLIB=ON
      -DLLVM_INCLUDE_TESTS=OFF
      -DLLVM_INCLUDE_EXAMPLES=OFF
      -DLLVM_INCLUDE_DOCS=OFF
      -DLLVM_INCLUDE_UTILS=OFF
      -DLLVM_ENABLE_ASSERTIONS=OFF
      -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
      -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
      -DCMAKE_CXX_STANDARD=17
    BUILD_COMMAND ${CMAKE_COMMAND} --build . --config Release -j${CMAKE_BUILD_PARALLEL_LEVEL}
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install --config Release
    LOG_DOWNLOAD ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    UPDATE_COMMAND ""  # Don't update on rebuild
  )
  
  # Set LLVM_DIR to the installed location
  set(LLVM_DIR "${LLVM_INSTALL_DIR}/lib/cmake/llvm")
  
  # Check if LLVM was already built (from a previous run)
  if(EXISTS "${LLVM_DIR}/LLVMConfig.cmake")
    message(STATUS "Found previously built LLVM at ${LLVM_DIR}")
    find_package(LLVM REQUIRED CONFIG PATHS ${LLVM_DIR} NO_DEFAULT_PATH)
    set(LLVM_FOUND TRUE)
  else()
    message(STATUS "LLVM will be built in: ${LLVM_BINARY_DIR}")
    message(STATUS "LLVM will be installed to: ${LLVM_INSTALL_DIR}")
    message(STATUS "")
    message(STATUS "Build process:")
    message(STATUS "  1. Run: cmake --build . --target llvm_external")
    message(STATUS "  2. After build completes, reconfigure: cmake ${CMAKE_SOURCE_DIR}")
    message(STATUS "")
    set(LLVM_FOUND FALSE)
  endif()
endif()

if(LLVM_FOUND)
  message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
  
  # Map LLVM components to library names
  # Since we built with LLVM_BUILD_LLVM_DYLIB=ON, we can use the dylib
  # But we still need individual components for some functionality
  llvm_map_components_to_libnames(LLVM_LIBS
    core
    executionengine
    orcjit
    mc
    mcparser
    support
    target
    transformutils
    analysis
    bitwriter
    linker
    irreader
    native
  )
  
  # If LLVM was built as a dylib, prefer using it
  if(LLVM_LINK_LLVM_DYLIB)
    set(LLVM_LIBS LLVM)
  endif()
  
  set(USE_LLVM_API 1)
  add_definitions(-DUSE_LLVM_API)
  
  # If we built LLVM externally, make sure it's built before weavec0
  if(WEAVE_AUTO_BUILD_LLVM AND TARGET llvm_external)
    message(STATUS "weavec0 will depend on llvm_external build")
  endif()
elseif(WEAVE_AUTO_BUILD_LLVM AND TARGET llvm_external AND NOT LLVM_FOUND)
  # LLVM is being built, but not available yet
  message(STATUS "LLVM build is configured but not yet available.")
  message(STATUS "Build LLVM with: cmake --build . --target llvm_external")
  message(STATUS "Then reconfigure: cmake ${CMAKE_SOURCE_DIR}")
  set(USE_LLVM_API 0)
else()
  message(STATUS "LLVM not found - will use clang system calls")
  message(STATUS "  To enable LLVM API integration:")
  message(STATUS "    1. Install LLVM development packages, or")
  message(STATUS "    2. Run cmake with -DWEAVE_AUTO_BUILD_LLVM=ON (takes 30-60+ minutes)")
  set(USE_LLVM_API 0)
endif()

add_executable(weavec0
  src/common.c
  src/diagnostics.c
  src/lexer.c
  src/sexpr.c
  src/fs.c
  src/ir.c
  src/fn_table.c
  src/type_env.c
  src/types.c
  src/env.c
  src/value.c
  src/builtins.c
  src/stats.c
  src/cgutils.c
  src/expr.c
  src/stmt.c
  src/program.c
  src/main.c
)

# Test program for JIT (optional)
if(USE_LLVM_API AND CMAKE_CXX_COMPILER)
  add_executable(test_jit src/test_jit.c src/llvm_jit.cpp)
  target_include_directories(test_jit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_include_directories(test_jit PRIVATE ${LLVM_INCLUDE_DIRS})
  target_link_libraries(test_jit ${LLVM_LIBS})
  target_compile_definitions(test_jit PRIVATE ${LLVM_DEFINITIONS})
  target_compile_definitions(test_jit PRIVATE USE_LLVM_API)
endif()

if(USE_LLVM_API)
  target_sources(weavec0 PRIVATE src/llvm_compile.c)
  target_sources(weavec0 PRIVATE src/llvm_jit_helper.c)
  target_sources(weavec0 PRIVATE src/llvm_compile_helper.c)
  # Add C++ JIT support if C++ compiler is available
  if(CMAKE_CXX_COMPILER)
    target_sources(weavec0 PRIVATE src/llvm_jit.cpp)
    set_target_properties(weavec0 PROPERTIES LINKER_LANGUAGE CXX)
  endif()
  target_include_directories(weavec0 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_include_directories(weavec0 PRIVATE ${LLVM_INCLUDE_DIRS})
  target_link_libraries(weavec0 ${LLVM_LIBS})
  target_compile_definitions(weavec0 PRIVATE ${LLVM_DEFINITIONS})
  
  # If we built LLVM externally, ensure it's built before weavec0
  if(WEAVE_AUTO_BUILD_LLVM AND TARGET llvm_external)
    add_dependencies(weavec0 llvm_external)
  endif()
else()
  target_include_directories(weavec0 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

add_custom_target(stage1-ll
  COMMAND weavec0 -I${CMAKE_CURRENT_SOURCE_DIR}/.. -I${CMAKE_CURRENT_SOURCE_DIR}/../../stdlib
          --input ${CMAKE_CURRENT_SOURCE_DIR}/../stage1/weavec1.weave
          --output ${CMAKE_CURRENT_BINARY_DIR}/stage1_from0.ll
  DEPENDS weavec0
  COMMENT "Building stage1 LLVM IR with weavec0 (-I style include dirs)"
  VERBATIM
)

enable_testing()

find_program(CLANG_EXE clang)
if (NOT CLANG_EXE)
  message(FATAL_ERROR "clang not found (required to link and run stage0 tests)")
endif()

set(STAGE0_TESTS
  tests/test_return42.weave
  tests/test_fn_call_return42.weave
  tests/test_ccall_abs_return42.weave
  tests/test_ccall_puts_stringlit_return42.weave
  tests/test_typed_fn_return_string_return42.weave
  tests/test_typed_fn_param_string_return42.weave
  tests/test_struct_make_get_return42.weave
  tests/test_struct_set_field_return42.weave
  tests/test_addr_load_store_int_return42.weave
)

foreach(test_file IN LISTS STAGE0_TESTS)
  get_filename_component(test_name ${test_file} NAME_WE)
  add_test(
    NAME stage0_${test_name}
    COMMAND ${CMAKE_COMMAND}
      -DWEAVEC0=$<TARGET_FILE:weavec0>
      -DCLANG=${CLANG_EXE}
      -DTEST_FILE=${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
      -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/${test_name}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_test.cmake
  )
  set_tests_properties(stage0_${test_name} PROPERTIES LABELS "stage0")
endforeach()

# Embedded tests: one CTest per test name discovered by weavec0.
foreach(test_file IN LISTS STAGE0_TESTS)
  get_filename_component(test_name ${test_file} NAME_WE)
  # Discover test names via weavec0 -list-tests
  execute_process(
    COMMAND $<TARGET_FILE:weavec0> -list-tests ${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
    OUTPUT_VARIABLE EMBED_TESTS_RAW
    RESULT_VARIABLE EMBED_DISCOVERY_RC
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if (EMBED_DISCOVERY_RC EQUAL 0 AND EMBED_TESTS_RAW)
    string(REPLACE "\n" ";" EMBED_TESTS ${EMBED_TESTS_RAW})
    foreach(tname IN LISTS EMBED_TESTS)
      add_test(
        NAME stage0_embedded_${test_name}_${tname}
        COMMAND ${CMAKE_COMMAND}
          -DWEAVEC0=$<TARGET_FILE:weavec0>
          -DCLANG=${CLANG_EXE}
          -DSRC_FILE=${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
          -DTEST_NAME=${tname}
          -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/${test_name}_embedded
          -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_embedded_tests.cmake
      )
      set_tests_properties(stage0_embedded_${test_name}_${tname} PROPERTIES LABELS "stage0;embedded")
    endforeach()
  else()
    # Fallback: coarse per-file embedded test
    add_test(
      NAME stage0_embedded_${test_name}
      COMMAND ${CMAKE_COMMAND}
        -DWEAVEC0=$<TARGET_FILE:weavec0>
        -DCLANG=${CLANG_EXE}
        -DSRC_FILE=${CMAKE_CURRENT_SOURCE_DIR}/${test_file}
        -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/${test_name}_embedded
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_embedded_tests.cmake
    )
    set_tests_properties(stage0_embedded_${test_name} PROPERTIES LABELS "stage0;embedded")
  endif()
endforeach()
