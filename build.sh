#!/bin/bash
# Bootstrap build script for Weave compiler
# Builds stage0 (C) -> stage1 (Weave compiled by stage0) -> stage2 (Weave compiled by stage1)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE0_DIR="$SCRIPT_DIR/stage0"
STAGE1_DIR="$SCRIPT_DIR/stage1"
BUILD_DIR="$SCRIPT_DIR/build"
RUNTIME="$SCRIPT_DIR/../weave/src/runtime/runtime.c"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }
step()  { echo -e "${BLUE}[STEP]${NC} $1"; }

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS] [COMMAND]

Weave Bootstrap Build System

Commands:
  all         Clean and build everything (default if no command given)
  stage0      Build stage0 only (C compiler)
  stage1      Build stage1 only (requires stage0)
  stage2      Build stage2 only (requires stage1) - self-hosting test
  test        Run stage1 tests
  clean       Clean all build directories

Options:
  -h, --help      Show this help message
  -v, --verbose   Verbose output
  -q, --quiet     Quiet mode (errors only)
  --no-clean      Don't clean before building (for incremental builds)

Examples:
  $(basename "$0")              # Clean and build all stages
  $(basename "$0") stage1       # Build stage1 only
  $(basename "$0") --no-clean   # Incremental build of all stages
  $(basename "$0") test         # Run stage1 tests

EOF
}

# Default options
VERBOSE=0
QUIET=0
NO_CLEAN=0
COMMAND="all"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -q|--quiet)
            QUIET=1
            shift
            ;;
        --no-clean)
            NO_CLEAN=1
            shift
            ;;
        all|stage0|stage1|stage2|test|clean)
            COMMAND=$1
            shift
            ;;
        *)
            error "Unknown option: $1. Use --help for usage."
            ;;
    esac
done

# Redirect output based on verbosity
if [ $QUIET -eq 1 ]; then
    exec 3>/dev/null
elif [ $VERBOSE -eq 1 ]; then
    exec 3>&1
else
    exec 3>/dev/null
fi

clean_all() {
    step "Cleaning all build directories..."
    rm -rf "$BUILD_DIR"
    info "Clean complete."
}

build_stage0() {
    step "Building Stage0 (C compiler)..."
    
    if [ ! -f "$STAGE0_DIR/CMakeLists.txt" ]; then
        error "Stage0 CMakeLists.txt not found at $STAGE0_DIR"
    fi
    
    mkdir -p "$BUILD_DIR/stage0"
    cd "$BUILD_DIR/stage0"
    
    cmake "$STAGE0_DIR" >&3 2>&1 || error "CMake configuration failed"
    
    if command -v ninja &> /dev/null; then
        ninja >&3 2>&1 || error "Ninja build failed"
    else
        make -j$(nproc) >&3 2>&1 || error "Make build failed"
    fi
    
    if [ ! -f "$BUILD_DIR/stage0/weavec0" ]; then
        error "Stage0 build failed - weavec0 not found"
    fi
    
    info "Stage0 built: $BUILD_DIR/stage0/weavec0"
}

build_stage1() {
    step "Building Stage1 (Weave compiler, compiled by Stage0)..."
    
    if [ ! -f "$BUILD_DIR/stage0/weavec0" ]; then
        error "Stage0 compiler not found. Build stage0 first."
    fi
    
    if [ ! -f "$RUNTIME" ]; then
        error "Runtime not found at $RUNTIME"
    fi
    
    mkdir -p "$BUILD_DIR/stage1"
    
    export WEAVE_RUNTIME="$RUNTIME"
    cd "$STAGE1_DIR/src"
    
    "$BUILD_DIR/stage0/weavec0" -o "$BUILD_DIR/stage1/weavec1" weavec1.weave >&3 2>&1
    
    if [ ! -f "$BUILD_DIR/stage1/weavec1" ]; then
        error "Stage1 build failed - weavec1 not found"
    fi
    
    info "Stage1 built: $BUILD_DIR/stage1/weavec1"
}

build_stage2() {
    step "Building Stage2 (self-hosting test: Stage1 compiles itself)..."
    
    if [ ! -f "$BUILD_DIR/stage1/weavec1" ]; then
        error "Stage1 compiler not found. Build stage1 first."
    fi
    
    mkdir -p "$BUILD_DIR/stage2"
    
    local LL_FILE="$BUILD_DIR/stage2/stage2.ll"
    local OUT_FILE="$BUILD_DIR/stage2/weavec2"
    
    info "Compiling stage1 source with stage1 compiler (emit LLVM)..."
    "$BUILD_DIR/stage1/weavec1" "$STAGE1_DIR/src/weavec1.weave" --emit-llvm -o "$LL_FILE" 2>&1 | tee /dev/fd/3
    
    if [ ! -f "$LL_FILE" ]; then
        error "Stage2 IR generation failed"
    fi
    
    local LL_SIZE=$(wc -c < "$LL_FILE")
    info "Generated $LL_FILE ($LL_SIZE bytes)"
    
    info "Compiling LLVM IR to executable..."
    clang -Wno-null-character -o "$OUT_FILE" "$LL_FILE" "$RUNTIME" -lm 2>&1 | tee /dev/fd/3
    
    if [ $? -ne 0 ]; then
        warn "Stage2 compilation failed. Check $LL_FILE for errors."
        # Show first few errors
        head -50 "$LL_FILE" | grep -i "error\|warning" || true
        return 1
    fi
    
    if [ ! -f "$OUT_FILE" ]; then
        error "Stage2 build failed - weavec2 not found"
    fi
    
    info "Stage2 built: $OUT_FILE"
    info "Self-hosting successful!"
}

run_tests() {
    step "Running Stage1 tests..."
    
    if [ ! -f "$BUILD_DIR/stage1/weavec1" ]; then
        error "Stage1 compiler not found. Build stage1 first."
    fi
    
    local TEST_DIR="$STAGE1_DIR/tests"
    local TEST_BUILD_DIR="$BUILD_DIR/tests"
    mkdir -p "$TEST_BUILD_DIR"
    
    local PASSED=0
    local FAILED=0
    local REJECTED=0
    
    for tf in "$TEST_DIR"/test_*.weave; do
        local name=$(basename "$tf" .weave)
        local ll_path="$TEST_BUILD_DIR/${name}.ll"
        local out_path="$TEST_BUILD_DIR/${name}"
        
        # Compile to LLVM IR using clang-style flags
        "$BUILD_DIR/stage1/weavec1" "$tf" --emit-llvm -o "$ll_path" 2>/dev/null
        
        if [ ! -f "$ll_path" ]; then
            # Check if this is an expected typecheck failure
            if [[ "$name" == *"typecheck"* ]]; then
                echo -e "${GREEN}REJECT${NC} $name (expected typecheck error)"
                ((REJECTED++))
            else
                echo -e "${RED}FAIL${NC} $name (compile error)"
                ((FAILED++))
            fi
            continue
        fi
        
        # Link with clang
        clang -Wno-null-character -o "$out_path" "$ll_path" "$RUNTIME" -lm 2>/dev/null
        if [ $? -ne 0 ]; then
            echo -e "${RED}FAIL${NC} $name (link error)"
            ((FAILED++))
            continue
        fi
        
        # Run test
        "$out_path" 2>/dev/null
        local rc=$?
        
        if [ $rc -eq 42 ]; then
            echo -e "${GREEN}PASS${NC} $name"
            ((PASSED++))
        else
            echo -e "${RED}FAIL${NC} $name (exit $rc, expected 42)"
            ((FAILED++))
        fi
    done
    
    echo ""
    info "Results: $PASSED passed, $REJECTED rejected (expected), $FAILED failed"
    
    if [ $FAILED -gt 0 ]; then
        return 1
    fi
}

# Main execution
case $COMMAND in
    clean)
        clean_all
        ;;
    stage0)
        [ $NO_CLEAN -eq 0 ] && rm -rf "$BUILD_DIR/stage0"
        build_stage0
        ;;
    stage1)
        [ $NO_CLEAN -eq 0 ] && rm -rf "$BUILD_DIR/stage1"
        build_stage1
        ;;
    stage2)
        build_stage2
        ;;
    test)
        run_tests
        ;;
    all)
        if [ $NO_CLEAN -eq 0 ]; then
            clean_all
        fi
        build_stage0
        build_stage1
        echo ""
        run_tests
        echo ""
        build_stage2
        ;;
esac

info "Done!"
