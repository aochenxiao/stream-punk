#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
CLEAN=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean) CLEAN=true; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

if $CLEAN; then
    echo -e "\033[33mCleaning build directory...\033[0m"
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        echo -e "  \033[32mBuild directory cleaned.\033[0m"
    fi
    echo ""
fi

echo "========================================"
echo "  StreamPunk Setup"
echo "========================================"
echo ""

# [1/5] Check CMake
echo -e "\033[33m[1/5] Checking CMake...\033[0m"
if ! command -v cmake &>/dev/null; then
    echo -e "  \033[31m[ERROR] CMake not found\033[0m"
    exit 1
fi
echo -e "  \033[32mCMake found\033[0m"

# [2/5] Check C++ compiler
echo -e "\033[33m[2/5] Checking C++ compiler...\033[0m"
CXX="${CXX:-}"
if [ -z "$CXX" ]; then
    if command -v g++ &>/dev/null; then
        CXX=g++
    elif command -v clang++ &>/dev/null; then
        CXX=clang++
    else
        echo -e "  \033[31m[ERROR] No C++ compiler found (g++ or clang++)\033[0m"
        exit 1
    fi
fi
echo -e "  \033[32mCompiler found: $CXX\033[0m"

# [3/5] Install dependencies
echo -e "\033[33m[3/5] Installing dependencies...\033[0m"
LEGACY_3RD="$ROOT_DIR/3/x64-windows/include/doctest/doctest.h"
CMAKE_EXTRA_ARGS=()
if [ -f "$LEGACY_3RD" ]; then
    echo -e "  \033[32mUsing legacy 3/ directory\033[0m"
else
    # 3/ 目录不存在，使用 vcpkg
    VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
    VCPKG_EXE="$VCPKG_ROOT/vcpkg"
    VCPKG_TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
    if [ ! -f "$VCPKG_EXE" ]; then
        echo -e "  \033[31m[ERROR] vcpkg not found.\033[0m"
        echo -e "  \033[33mPlease install vcpkg:\033[0m"
        echo -e "  \033[33m  git clone https://github.com/Microsoft/vcpkg.git\033[0m"
        echo -e "  \033[33m  ./vcpkg/bootstrap-vcpkg.sh\033[0m"
        echo -e "  \033[33mOr set VCPKG_ROOT environment variable.\033[0m"
        exit 1
    fi
    echo -e "  \033[32mvcpkg found: $VCPKG_EXE\033[0m"
    echo -e "  \033[33mRunning vcpkg install (this may take a while on first run)...\033[0m"
    cd "$ROOT_DIR"
    "$VCPKG_EXE" install --triplet x64-linux
    echo -e "  \033[32mDependencies installed\033[0m"
    CMAKE_EXTRA_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=$VCPKG_TOOLCHAIN")
fi

# [4/5] Build sp-gen
echo -e "\033[33m[4/5] Building sp-gen...\033[0m"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake "$ROOT_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX" "${CMAKE_EXTRA_ARGS[@]}"
cmake --build . --target sp-gen -j"$(nproc)"
cd "$ROOT_DIR"
echo -e "  \033[32msp-gen built\033[0m"

# [5/5] Build examples
echo -e "\033[33m[5/5] Building examples...\033[0m"
cd "$BUILD_DIR"
TARGETS=(
    "example-01-basic-cpp"
    "example-02-cpp-to-ts"
    "example-03-dynamic-schema"
    "example-04-json"
    "example-05-orm"
    "example-06-shadow-delta"
)
for t in "${TARGETS[@]}"; do
    cmake --build . --target "$t" -j"$(nproc)"
done
cd "$ROOT_DIR"
echo -e "  \033[32mAll examples built\033[0m"

echo ""
echo -e "  \033[36mSetup complete! Run ./scripts/run-all.sh to test\033[0m"