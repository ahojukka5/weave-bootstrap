if(NOT DEFINED WEAVEC0)
  message(FATAL_ERROR "WEAVEC0 not set")
endif()
if(NOT DEFINED CLANG)
  message(FATAL_ERROR "CLANG not set")
endif()
if(NOT DEFINED TEST_FILE)
  message(FATAL_ERROR "TEST_FILE not set")
endif()
if(NOT DEFINED OUT_DIR)
  message(FATAL_ERROR "OUT_DIR not set")
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")

get_filename_component(TEST_NAME "${TEST_FILE}" NAME_WE)
set(LL "${OUT_DIR}/${TEST_NAME}.ll")
set(EXE "${OUT_DIR}/${TEST_NAME}")

execute_process(
  COMMAND "${WEAVEC0}" "${TEST_FILE}" -S -o "${LL}"
  RESULT_VARIABLE rc
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "weavec0 failed (rc=${rc}) on ${TEST_FILE}")
endif()

# Link (no runtime needed - arena-create uses only malloc which is in libc)
execute_process(
  COMMAND "${CLANG}" -Wno-null-character "${LL}" -lm -o "${EXE}"
  RESULT_VARIABLE rc
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "clang failed (rc=${rc}) for ${LL}")
endif()

execute_process(
  COMMAND "${EXE}"
  RESULT_VARIABLE rc
)
if(NOT rc EQUAL 42)
  message(FATAL_ERROR "expected exit code 42, got ${rc} for ${TEST_FILE}")
endif()

