if(NOT DEFINED WEAVEC0)
  message(FATAL_ERROR "WEAVEC0 not set")
endif()
if(NOT DEFINED CLANG)
  message(FATAL_ERROR "CLANG not set")
endif()
if(NOT DEFINED SRC_FILE)
  message(FATAL_ERROR "SRC_FILE not set")
endif()
if(NOT DEFINED RUNTIME)
  message(FATAL_ERROR "RUNTIME not set")
endif()
if(NOT DEFINED OUT_DIR)
  message(FATAL_ERROR "OUT_DIR not set")
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")

get_filename_component(SRC_NAME "${SRC_FILE}" NAME_WE)
set(LL "${OUT_DIR}/${SRC_NAME}_embedded.ll")
set(EXE "${OUT_DIR}/${SRC_NAME}_embedded")

# Generate tests: compile with -generate-tests flag
execute_process(
  COMMAND "${WEAVEC0}" "${SRC_FILE}" -generate-tests -S -o "${LL}"
  RESULT_VARIABLE rc
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "weavec0 -generate-tests failed (rc=${rc}) on ${SRC_FILE}")
endif()

# Link with runtime
execute_process(
  COMMAND "${CLANG}" -Wno-null-character "${LL}" "${RUNTIME}" -lm -o "${EXE}"
  RESULT_VARIABLE rc
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "clang failed (rc=${rc}) for ${LL}")
endif()

# Run embedded tests: exit code 0 means all tests passed
execute_process(
  COMMAND "${EXE}"
  RESULT_VARIABLE rc
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "embedded tests failed (${rc} failures) for ${SRC_FILE}")
endif()
