# Stage2 build and tests
# This is included as a subdirectory from the root CMakeLists.txt
# Runtime path and weavec1 target are provided by parent/stage1

enable_testing()

# Find clang for test linking
find_program(CLANG_EXE clang)
if (NOT CLANG_EXE)
  message(FATAL_ERROR "clang not found (required for linking tests)")
endif()

# Runtime path inherited from parent or default to slim internal runtime
if(NOT DEFINED WEAVE_RUNTIME)
  set(WEAVE_RUNTIME ${CMAKE_SOURCE_DIR}/stage0/runtime/stdlib_runtime.c)
endif()

# Stage1 compiler (built by parent CMake)
set(WEAVEC1 ${CMAKE_BINARY_DIR}/stage1/weavec1)

# Build Stage2 (self-hosted: stage1 source compiled by stage1 compiler)
set(STAGE1_SRC ${CMAKE_SOURCE_DIR}/stage1/src/weavec1.weave)
set(WEAVEC2_OUT ${CMAKE_BINARY_DIR}/stage2/weavec2)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/stage2)

add_custom_command(
  OUTPUT ${WEAVEC2_OUT}
  COMMAND ${CMAKE_COMMAND} -E env WEAVE_RUNTIME=${WEAVE_RUNTIME} 
    ${WEAVEC1} ${STAGE1_SRC} -I${CMAKE_SOURCE_DIR}/stage1/src -o ${WEAVEC2_OUT}
  DEPENDS ${WEAVEC1} ${STAGE1_SRC} ${WEAVE_RUNTIME}
  COMMENT "Building stage2 compiler (weavec2) via weavec1 (self-hosting)"
  VERBATIM
)
# Do not build weavec2 by default to avoid blocking stage1 iteration
add_custom_target(weavec2_target DEPENDS ${WEAVEC2_OUT})

# Register stage2 tests
file(GLOB STAGE2_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.weave)

foreach(test_file IN LISTS STAGE2_TESTS)
  get_filename_component(test_name ${test_file} NAME_WE)
  set(out_dir ${CMAKE_BINARY_DIR}/tests/${test_name})
  file(MAKE_DIRECTORY ${out_dir})
  
  # Determine if this is a typecheck failure test
  string(FIND "${test_name}" "typecheck" TYPECHECK_POS)
  if(NOT TYPECHECK_POS EQUAL -1)
    # Expected to fail at typecheck - just run weavec1 and expect non-zero
    add_test(
      NAME stage2_${test_name}
      COMMAND bash -c "${WEAVEC1} ${test_file} --emit-llvm -o ${out_dir}/${test_name}.ll 2>&1 && exit 1 || exit 0"
    )
  else()
    # Expected to succeed - compile, link, run, check exit 42
    add_test(
      NAME stage2_${test_name}
      COMMAND bash -c "${WEAVEC1} ${test_file} --emit-llvm -o ${out_dir}/${test_name}.ll && ${CLANG_EXE} -Wno-null-character ${out_dir}/${test_name}.ll ${WEAVE_RUNTIME} -lm -o ${out_dir}/${test_name} && ${out_dir}/${test_name}; test $? -eq 42"
    )
  endif()
  
  set_tests_properties(stage2_${test_name} PROPERTIES LABELS "stage2")
endforeach()
