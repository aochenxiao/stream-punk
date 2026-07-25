#!/bin/bash
# 示例 08：SPOI 跨语言数据互查 — 环境准备脚本 (Linux/Mac)

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "=== SPOI 跨语言数据互查 — 环境准备 ==="
echo ""

# ===== 1. 编译 C++ 服务器 =====
echo "[1/2] 编译 C++ 服务器..."

BUILD_DIR="$SCRIPT_DIR/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target spoi-cross-server
echo "  C++ 服务器编译成功！"

# ===== 2. 编译 Java 客户端 =====
echo "[2/2] 编译 Java 客户端..."

JAVA_CLIENT_DIR="$SCRIPT_DIR/client"
JAVA_OUT_DIR="$SCRIPT_DIR/build/java"
mkdir -p "$JAVA_OUT_DIR"
cd "$JAVA_CLIENT_DIR"
javac -encoding UTF-8 -d "$JAVA_OUT_DIR" Main.java
echo "  Java 客户端编译成功！"

echo ""
echo "=== 环境准备完成！==="
echo ""
echo "运行方式："
echo "  1. 启动 C++ 服务器：  ./build/spoi-cross-server"
echo "  2. 启动 Java 客户端：  java -cp build/java Main"
echo ""
echo "或使用一键运行脚本：  ./run-all.sh"