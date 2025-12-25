# Stage1 build and tests
# This is included as a subdirectory from the root CMakeLists.txt
# Runtime path and weavec0 target are provided by parent

enable_testing()

# Find clang for test linking
find_program(CLANG_EXE clang)
if (NOT CLANG_EXE)
  message(FATAL_ERROR "clang not found (required for linking tests)")
endif()

# Runtime no longer needed - Weave defines main() directly

# Build Stage1 using Stage0 compiler
set(STAGE1_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/weavec1.weave)
file(GLOB_RECURSE STAGE1_ALL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.weave)
set(STAGE1_OUT ${CMAKE_BINARY_DIR}/stage1/weavec1)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/stage1)

add_custom_command(
  OUTPUT ${STAGE1_OUT}
  COMMAND $<TARGET_FILE:weavec0> -I${CMAKE_CURRENT_SOURCE_DIR}/src -o ${STAGE1_OUT} ${STAGE1_SRC}
  DEPENDS weavec0 ${STAGE1_SRC} ${STAGE1_ALL_SRC}
  COMMENT "Building stage1 compiler (weavec1) via weavec0"
  VERBATIM
)
add_custom_target(weavec1_target ALL DEPENDS ${STAGE1_OUT})

# Export for use by other subdirectories
set(WEAVEC1 ${STAGE1_OUT} PARENT_SCOPE)

# Register stage1 tests
file(GLOB STAGE1_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.weave)

foreach(test_file IN LISTS STAGE1_TESTS)
  get_filename_component(test_name ${test_file} NAME_WE)
  set(out_dir ${CMAKE_BINARY_DIR}/tests/${test_name})
  file(MAKE_DIRECTORY ${out_dir})
  
  # Determine if this is a typecheck failure test
  string(FIND "${test_name}" "typecheck" TYPECHECK_POS)
  if(NOT TYPECHECK_POS EQUAL -1)
    # Expected to fail at typecheck - just run weavec1 and expect non-zero
    add_test(
      NAME stage1_${test_name}
      COMMAND bash -c "${STAGE1_OUT} ${test_file} --emit-llvm -o ${out_dir}/${test_name}.ll 2>&1 && exit 1 || exit 0"
    )
  else()
    # Expected to succeed - compile, link, run, check exit 42
    add_test(
      NAME stage1_${test_name}
      COMMAND bash -c "${STAGE1_OUT} ${test_file} --emit-llvm -o ${out_dir}/${test_name}.ll && ${CLANG_EXE} -Wno-null-character ${out_dir}/${test_name}.ll -lm -o ${out_dir}/${test_name} && ${out_dir}/${test_name}; test $? -eq 42"
    )
  endif()
  
  set_tests_properties(stage1_${test_name} PROPERTIES LABELS "stage1")
endforeach()

# Embedded tests for Stage1 source: compile ONCE with all tests, run together
# This is much faster than compiling the entire stage1 source for each individual test
add_test(
  NAME stage1_embedded_all
  COMMAND ${CMAKE_COMMAND}
    -DWEAVEC0=$<TARGET_FILE:weavec0>
    -DCLANG=${CLANG_EXE}
    -DSRC_FILE=${STAGE1_SRC}
    -DWEAVEC_ARGS=-I${CMAKE_CURRENT_SOURCE_DIR}/src\;-I${CMAKE_SOURCE_DIR}/stdlib
    -DOUT_DIR=${CMAKE_BINARY_DIR}/tests/stage1_embedded
    -P ${CMAKE_SOURCE_DIR}/stage0/tests/run_embedded_tests.cmake
)
set_tests_properties(stage1_embedded_all PROPERTIES LABELS "stage1;embedded")
