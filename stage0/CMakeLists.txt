cmake_minimum_required(VERSION 3.16)

project(weave_stage0 C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
# Enable extensions to get POSIX functions like realpath() on Unix-like systems.
set(CMAKE_C_EXTENSIONS ON)

add_executable(weavec0
  src/common.c
  src/lexer.c
  src/sexpr.c
  src/fs.c
  src/ir.c
  src/fn_table.c
  src/type_env.c
  src/types.c
  src/env.c
  src/value.c
  src/expr.c
  src/stmt.c
  src/program.c
  src/main.c
)

target_include_directories(weavec0 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

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
      -DRUNTIME=${CMAKE_CURRENT_SOURCE_DIR}/runtime/runtime.c
      -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/${test_name}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_test.cmake
  )
  set_tests_properties(stage0_${test_name} PROPERTIES LABELS "stage0")
endforeach()
