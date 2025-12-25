if(NOT DEFINED WEAVEC0)
  message(FATAL_ERROR "WEAVEC0 not set")
endif()
if(NOT DEFINED WEAVEC0)
  message(FATAL_ERROR "CLANG not set")
endif()
if(NOT DEFINED SRC_FILE)
  message(FATAL_ERROR "SRC_FILE not set")
endif()
if(NOT DEFINED OUT_DIR)
  message(FATAL_ERROR "OUT_DIR not set")
endif()
## Optional: WEAVEC_ARGS (extra args like -I paths)

file(MAKE_DIRECTORY "${OUT_DIR}")

## Optional: TEST_NAME for per-function filtering

get_filename_component(SRC_NAME "${SRC_FILE}" NAME_WE)
set(LL "${OUT_DIR}/${SRC_NAME}_embedded.ll")
set(EXE "${OUT_DIR}/${SRC_NAME}_embedded")
if(DEFINED TEST_NAME)
  set(LL "${OUT_DIR}/${SRC_NAME}_${TEST_NAME}_embedded.ll")
  set(EXE "${OUT_DIR}/${SRC_NAME}_${TEST_NAME}_embedded")
else()
  set(LL "${OUT_DIR}/${SRC_NAME}_embedded.ll")
  set(EXE "${OUT_DIR}/${SRC_NAME}_embedded")
endif()
if(DEFINED TEST_NAME)
  if(DEFINED WEAVEC_ARGS)
    execute_process(
      COMMAND "${WEAVEC0}" ${WEAVEC_ARGS} "${SRC_FILE}" -generate-tests -test "${TEST_NAME}" -S -o "${LL}"
      RESULT_VARIABLE rc
    )
  else()
    execute_process(
      COMMAND "${WEAVEC0}" "${SRC_FILE}" -generate-tests -test "${TEST_NAME}" -S -o "${LL}"
      RESULT_VARIABLE rc
    )
  endif()
else()
  if(DEFINED WEAVEC_ARGS)
    execute_process(
      COMMAND "${WEAVEC0}" ${WEAVEC_ARGS} "${SRC_FILE}" -generate-tests -S -o "${LL}"
      RESULT_VARIABLE rc
    )
  else()
    execute_process(
      COMMAND "${WEAVEC0}" "${SRC_FILE}" -generate-tests -S -o "${LL}"
      RESULT_VARIABLE rc
    )
  endif()
endif()
if(NOT rc EQUAL 0)
  if(DEFINED TEST_NAME)
    message(FATAL_ERROR "weavec0 -generate-tests -test ${TEST_NAME} failed (rc=${rc}) on ${SRC_FILE}")
  else()
    message(FATAL_ERROR "weavec0 -generate-tests failed (rc=${rc}) on ${SRC_FILE}")
  endif()
endif()

# Link (no runtime needed - arena-create uses only malloc which is in libc)
execute_process(
  COMMAND "${CLANG}" -Wno-null-character "${LL}" -lm -o "${EXE}"
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
