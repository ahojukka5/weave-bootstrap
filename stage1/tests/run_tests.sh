#!/bin/bash
# Stage1 test runner - compiles and runs all test_*_42.weave files
# Tests are expected to return exit code 42

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
STAGE1_DIR="$(dirname "$SCRIPT_DIR")"
ROOT_DIR="$(dirname "$(dirname "$STAGE1_DIR")")"
TESTS_DIR="$SCRIPT_DIR"
BUILD_DIR="$STAGE1_DIR/build"
RUNTIME="$ROOT_DIR/src/runtime/runtime.c"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Ensure stage1 compiler exists
WEAVEC1="$BUILD_DIR/weavec1"
if [[ ! -f "$WEAVEC1" ]]; then
    echo -e "${YELLOW}[INFO] Building stage1 compiler...${NC}"
    "$STAGE1_DIR/build.sh"
fi

# Create temp directory for test artifacts
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

PASSED=0
FAILED=0
SKIPPED=0
FAILED_TESTS=""

echo "========================================"
echo "Stage1 Test Runner"
echo "========================================"
echo ""

# Run positive tests (expect exit code 42)
for test_file in "$TESTS_DIR"/test_*_42.weave; do
    if [[ ! -f "$test_file" ]]; then
        continue
    fi
    
    test_name=$(basename "$test_file" .weave)
    ll_file="$TEMP_DIR/$test_name.ll"
    exe_file="$TEMP_DIR/$test_name"
    
    printf "%-45s " "$test_name"
    
    # Compile to LLVM IR
    if ! "$WEAVEC1" --input "$test_file" --output "$ll_file" 2>/dev/null; then
        echo -e "${RED}FAIL${NC} (compile error)"
        FAILED=$((FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS $test_name"
        continue
    fi
    
    # Link with runtime
    if ! clang "$ll_file" "$RUNTIME" -o "$exe_file" 2>/dev/null; then
        echo -e "${RED}FAIL${NC} (link error)"
        FAILED=$((FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS $test_name"
        continue
    fi
    
    # Run and check exit code
    set +e
    "$exe_file" >/dev/null 2>&1
    exit_code=$?
    set -e
    
    if [[ $exit_code -eq 42 ]]; then
        echo -e "${GREEN}PASS${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}FAIL${NC} (expected 42, got $exit_code)"
        FAILED=$((FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS $test_name"
    fi
done

# Run negative tests (typecheck errors, expect non-zero exit from compiler)
for test_file in "$TESTS_DIR"/test_typecheck_*.weave; do
    if [[ ! -f "$test_file" ]]; then
        continue
    fi
    
    # Skip if it's a _42 test (those are positive tests)
    if [[ "$test_file" == *_42.weave ]]; then
        continue
    fi
    
    test_name=$(basename "$test_file" .weave)
    ll_file="$TEMP_DIR/$test_name.ll"
    
    printf "%-45s " "$test_name"
    
    # Compile - should fail typecheck
    if "$WEAVEC1" --input "$test_file" --output "$ll_file" 2>/dev/null; then
        echo -e "${RED}FAIL${NC} (should have failed typecheck)"
        FAILED=$((FAILED + 1))
        FAILED_TESTS="$FAILED_TESTS $test_name"
    else
        echo -e "${GREEN}PASS${NC} (typecheck error as expected)"
        PASSED=$((PASSED + 1))
    fi
done

echo ""
echo "========================================"
echo -e "Results: ${GREEN}$PASSED passed${NC}, ${RED}$FAILED failed${NC}, $SKIPPED skipped"
echo "========================================"

if [[ -n "$FAILED_TESTS" ]]; then
    echo ""
    echo "Failed tests:$FAILED_TESTS"
    exit 1
fi

exit 0
