#!/bin/bash
# Regression script for running PTA alias analysis on bitcode files
# Usage: ./run_pta_regression.sh <bitcode_dir> [output_csv] [timeout_seconds]
#
# Args:
#   bitcode_dir: Directory containing .bc files to analyze
#   output_csv: Optional CSV file path (default: pta_regression_results.csv)
#   timeout_seconds: Optional timeout in seconds (default: 600)

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 <bitcode_dir> [output_csv] [timeout_seconds]"
    exit 1
fi

BC_DIR="$1"
OUTPUT_CSV="${2:-pta_regression_results.csv}"
TIMEOUT_SECONDS="${3:-600}"

if [ ! -d "$BC_DIR" ]; then
    echo "Error: Directory '$BC_DIR' does not exist"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$(dirname "$SCRIPT_DIR")")")"
BUILD_DIR="$PROJECT_ROOT/build"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory '$BUILD_DIR' does not exist"
    echo "Please build the project first"
    exit 1
fi

# Initialize CSV file with header if it doesn't exist
if [ ! -f "$OUTPUT_CSV" ]; then
    echo "timestamp,bitcode_file,tool,args,status,time_seconds" > "$OUTPUT_CSV"
fi

# Function to run analysis with timeout and record results
run_analysis() {
    local tool="$1"
    local args="$2"
    local bc_file="$3"
    
    local timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
    local bc_filename=$(basename "$bc_file")
    
    echo "Running: $tool $args on $bc_filename"
    
    # Run with timeout and measure time
    local start_time=$(date +%s.%N)
    local status="success"
    
    if timeout ${TIMEOUT_SECONDS}s "$BUILD_DIR/bin/$tool" $args "$bc_file" >/dev/null 2>&1; then
        status="success"
    else
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            status="timeout"
        else
            status="failed"
        fi
    fi
    
    local end_time=$(date +%s.%N)
    local elapsed=$(echo "$end_time - $start_time" | bc)
    
    # If timeout occurred, elapsed time will be exactly timeout_seconds
    if [ "$status" == "timeout" ]; then
        elapsed=$TIMEOUT_SECONDS
    fi
    
    # Escape commas in args for CSV
    local args_csv=$(echo "$args" | sed 's/,/;/g')
    
    # Append to CSV file
    echo "$timestamp,$bc_filename,$tool,\"$args_csv\",$status,$elapsed" >> "$OUTPUT_CSV"
    
    echo "  Status: $status, Time: ${elapsed}s"
}

# Find all .bc files in the directory
BC_FILES=$(find "$BC_DIR" -maxdepth 1 -name "*.bc" -type f | sort)

if [ -z "$BC_FILES" ]; then
    echo "Warning: No .bc files found in '$BC_DIR'"
    exit 0
fi

echo "Found $(echo "$BC_FILES" | wc -l) bitcode file(s) in '$BC_DIR'"
echo "Output CSV: $OUTPUT_CSV"
echo ""

# Process each bitcode file
for BC_FILE in $BC_FILES; do
    echo "=== Processing: $(basename "$BC_FILE") ==="
    
    # sparrow-aa with different --andersen-k-cs values
    for k_cs in 0 1 2; do
        run_analysis "sparrow-aa" "--andersen-k-cs=$k_cs" "$BC_FILE"
    done
    
    # aser-aa with different --analysis-mode values
    for mode in ci 1-cfa 2-cfa origin; do
        run_analysis "aser-aa" "--analysis-mode=$mode" "$BC_FILE"
    done
    
    # aser-aa with different --analysis-mode values and --pta-use-bdd-pts
    for mode in ci 1-cfa 2-cfa origin; do
        run_analysis "aser-aa" "--analysis-mode=$mode --pta-use-bdd-pts" "$BC_FILE"
    done
    
    # lotus-aa (no variants)
    run_analysis "lotus-aa" "" "$BC_FILE"
    
    echo ""
done

echo "Regression complete! Results saved to: $OUTPUT_CSV"
