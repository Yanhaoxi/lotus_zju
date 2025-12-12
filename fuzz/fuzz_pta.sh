#!/bin/bash
# export CLANG="/path/to/your/clang"
CLANG="${CLANG:-clang}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
CSMITH="$BUILD_DIR/csmith-install/bin/csmith"
CSMITH_HOME="$BUILD_DIR/csmith-install/include"

# Get SDK path only on macOS
SDK_PATH=""
if [[ "$OSTYPE" == "darwin"* ]]; then
    SDK_PATH="$(xcrun --show-sdk-path 2>/dev/null || true)"
fi

while true; do
    C_FILE="$SCRIPT_DIR/test_$$.c"
    BC_FILE="$SCRIPT_DIR/test_$$.bc"

    # Generate random C program
    echo "=== Generating C file: $C_FILE ==="
    CSMITH_CMD="$CSMITH --pointers --structs --unions --arrays --volatile-pointers --const-pointers --jumps --embedded-assigns"
    CSMITH_CMD="$CSMITH_CMD --max-pointer-depth $((RANDOM % 4 + 2))"
    CSMITH_CMD="$CSMITH_CMD --max-struct-fields $((RANDOM % 15 + 5))"
    CSMITH_CMD="$CSMITH_CMD --max-union-fields $((RANDOM % 8 + 3))"
    CSMITH_CMD="$CSMITH_CMD --max-expr-complexity $((RANDOM % 20 + 10))"
    CSMITH_CMD="$CSMITH_CMD --max-block-depth $((RANDOM % 5 + 3))"
    CSMITH_CMD="$CSMITH_CMD --max-block-size $((RANDOM % 5 + 3))"

    if ! timeout 5s bash -c "$CSMITH_CMD > \"$C_FILE\" 2>/dev/null"; then
        echo "✗ Failed to generate C file (timeout or error)"
        continue
    fi
    echo "✓ C file generated"
    
    # Compile to LLVM IR
    echo "=== Compiling to LLVM IR: $BC_FILE ==="
    CMD="$CLANG ${SDK_PATH:+-isysroot \"$SDK_PATH\"} -I\"$CSMITH_HOME\" -w -emit-llvm -c \"$C_FILE\" -o \"$BC_FILE\""
    echo "Command: $CMD"
    if ! eval "$CMD" 2>&1; then
        echo "Output: Compilation failed"
        rm -f "$C_FILE"
        continue
    fi
    echo "Output: Compilation successful"
    
    # Run analysis tools
    for tool in sparrow-aa aser-aa lotus-aa; do
        echo "=== Running $tool ==="
        if ! "$BUILD_DIR/bin/$tool" "$BC_FILE" 2>&1; then
            echo "CRASH: $tool crashed on $C_FILE"
            echo "Test files preserved: $C_FILE, $BC_FILE"
            exit 1
        fi
        echo "✓ $tool completed successfully"
    done
    # Cleanup if no crash
    rm -f "$C_FILE" "$BC_FILE"
done