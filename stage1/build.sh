#!/bin/bash
# Build script for Stage1 Weave compiler
# Usage: ./build.sh [--clean] [--test]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGE0_DIR="$SCRIPT_DIR/../stage0"
BUILD_DIR="$SCRIPT_DIR/../build/stage1"
STAGE0_BUILD_DIR="$SCRIPT_DIR/../build/stage0"
SRC_DIR="$SCRIPT_DIR/src"
RUNTIME="$SCRIPT_DIR/../../weave/src/runtime/runtime.c"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# Handle --clean flag
if [ "$1" = "--clean" ]; then
    info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    info "Clean complete."
    exit 0
fi

# Check prerequisites
if [ ! -f "$STAGE0_BUILD_DIR/weavec0" ]; then
    error "Stage0 compiler not found at $STAGE0_BUILD_DIR/weavec0. Build stage0 first (run ./build.sh stage0 from root)"
fi

if [ ! -f "$RUNTIME" ]; then
    error "Runtime not found at $RUNTIME"
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Export runtime path for weavec0
export WEAVE_RUNTIME="$RUNTIME"

# Step 1: Compile stage1 source directly to executable using stage0
info "Compiling stage1 to executable..."
cd "$SRC_DIR"
"$STAGE0_BUILD_DIR/weavec0" -o "$BUILD_DIR/weavec1" weavec1.weave 2>/dev/null

if [ ! -f "$BUILD_DIR/weavec1" ]; then
    error "Failed to compile executable"
fi

info "Build successful!"
info "Stage1 compiler: $BUILD_DIR/weavec1"

# Handle --test flag
if [ "$1" = "--test" ] || [ "$2" = "--test" ]; then
    info "Running tests..."
    TEST_DIR="$SCRIPT_DIR/../../test-programs/basic"
    
    # Test simple_return.weave (should return 42)
    info "Testing simple_return.weave..."
    "$BUILD_DIR/weavec1" "$TEST_DIR/simple_return.weave" --emit-llvm -o "$BUILD_DIR/test_simple_return.ll"
    clang -Wno-null-character -o "$BUILD_DIR/test_simple_return" "$BUILD_DIR/test_simple_return.ll" "$RUNTIME" -lm 2>/dev/null
    RESULT=$("$BUILD_DIR/test_simple_return"; echo $?)
    if [ "$RESULT" = "42" ]; then
        info "  simple_return: PASS (exit code 42)"
    else
        error "  simple_return: FAIL (expected 42, got $RESULT)"
    fi
    
    info "Tests complete!"
fi

echo ""
echo "Usage: $BUILD_DIR/weavec1 <input.weave> [-o out] [-S|-emit-llvm|-c]"
