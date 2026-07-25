#!/bin/bash
# 示例 08：SPOI 跨语言数据互查 — 一键运行脚本 (Linux/Mac)

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo "=== SPOI 跨语言数据互查 — 一键运行 ==="
echo ""

# 检查编译产物
if [ ! -f "$BUILD_DIR/spoi-cross-server" ]; then
    echo "C++ 服务器未编译，正在运行 setup.sh..."
    bash "$SCRIPT_DIR/setup.sh"
fi

if [ ! -f "$BUILD_DIR/java/Main.class" ]; then
    echo "Java 客户端未编译，正在运行 setup.sh..."
    bash "$SCRIPT_DIR/setup.sh"
fi

# 启动 C++ 服务器（后台运行）
echo "启动 C++ 服务器..."
"$BUILD_DIR/spoi-cross-server" &
SERVER_PID=$!
sleep 2

# 启动 Java 客户端
echo "启动 Java 客户端..."
echo ""
cd "$SCRIPT_DIR"
java -cp "$BUILD_DIR/java" Main

# 等待服务器进程结束
echo ""
echo "等待服务器关闭..."
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

echo "=== 运行完成 ==="