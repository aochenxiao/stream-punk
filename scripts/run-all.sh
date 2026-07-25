#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

echo "========================================"
echo "  StreamPunk Examples"
echo "========================================"
echo ""

EXAMPLES=(
    "01-basic-cpp|example-01-basic-cpp|C++ Serialization"
    "02-cpp-to-ts|example-02-cpp-to-ts|C++ <-> TypeScript"
    "03-dynamic-schema|example-03-dynamic-schema|Dynamic Schema"
    "04-json|example-04-json|JSON Serialization"
    "05-orm|example-05-orm|ORM SQL Generation"
    "06-shadow-delta|example-06-shadow-delta|Shadow Delta Update"
)

passed=0
failed=0
total=${#EXAMPLES[@]}
i=0

for entry in "${EXAMPLES[@]}"; do
    IFS='|' read -r name exe_name desc <<< "$entry"
    i=$((i + 1))

    # Try multiple possible locations for the executable
    exe_path=""
    for subdir in "Release" "Debug" ""; do
        if [ -n "$subdir" ]; then
            candidate="$BUILD_DIR/examples/$name/$subdir/$exe_name"
        else
            candidate="$BUILD_DIR/examples/$name/$exe_name"
        fi
        if [ -x "$candidate" ]; then
            exe_path="$candidate"
            break
        fi
    done

    printf "[%d/%d] %s ... " "$i" "$total" "$desc"

    if [ -z "$exe_path" ]; then
        echo -e "\033[33mSKIP (not built)\033[0m"
        failed=$((failed + 1))
        continue
    fi

    start_time=$(date +%s%N)
    if "$exe_path" > /dev/null 2>&1; then
        exit_code=0
    else
        exit_code=$?
    fi
    end_time=$(date +%s%N)
    elapsed_ms=$(( (end_time - start_time) / 1000000 ))
    elapsed_s=$(awk "BEGIN {printf \"%.2f\", $elapsed_ms/1000}")

    if [ "$exit_code" -eq 0 ]; then
        echo -e "\033[32mPASS (${elapsed_s}s)\033[0m"
        passed=$((passed + 1))
    else
        echo -e "\033[31mFAIL (exit code: $exit_code)\033[0m"
        failed=$((failed + 1))
    fi
done

echo ""
echo "========================================"
echo "  Result: $passed passed, $failed failed, $total total"
echo "========================================"