#!/usr/bin/env bash
# ===================================================================
# sp26 反射版刁钻测试运行脚本（需 GCC 16+，通过 WSL 运行）
#
# 运行方式（在 Windows 下）：
#   wsl.exe bash scripts/run_sp26_reflection_checks.sh
#
# 会逐个编译并运行 test/test_sp26_tricky_*.cpp，
# 结果非 0 或运行期存在 FAIL 即视为该组测试暴露问题。
# ===================================================================

set -u

ROOT="/mnt/e/project/stream-punk"
CXX="${CXX:-g++-16}"
FLAGS=(-std=c++26 -freflection -I "$ROOT/include" -I "$ROOT/examples")
OUT="${TMPDIR:-/tmp}/sp26_checks"
mkdir -p "$OUT"

echo "=============================================================================="
echo "  sp26 反射版刁钻测试"
echo "  compiler: $("$CXX" --version | head -n 1)"
echo "=============================================================================="
echo ""

pass=0
fail=0

run_one() {
    local name="$1"
    local src="$ROOT/test/$2"

    if ! "$CXX" "${FLAGS[@]}" "$src" -o "$OUT/$name" 2> "$OUT/$name.compile.log"; then
        fail=$((fail + 1))
        echo "  [编译失败] $name"
        grep -m1 -E 'error:' "$OUT/$name.compile.log" | sed 's/^/      /'
        return
    fi

    if "$OUT/$name" > "$OUT/$name.run.log" 2>&1; then
        pass=$((pass + 1))
        echo "  [通过] $name"
    else
        fail=$((fail + 1))
        echo "  [失败] $name"
        grep -E 'FAIL:' "$OUT/$name.run.log" | sed 's/^/      /'
    fi
}

run_one tricky_misc     test_sp26_tricky_misc.cpp
run_one tricky_nested   test_sp26_tricky_nested.cpp
run_one tricky_pointers test_sp26_tricky_pointers.cpp
run_one tricky_regress  test_sp26_tricky_regress.cpp
run_one binary_compat   test_sp26_binary_compat.cpp
run_one tricky_poly     test_sp26_tricky_poly.cpp

echo ""
echo "=============================================================================="
echo "  结果: $pass 组通过, $fail 组失败。"
echo "  tricky_pointers 已覆盖 weak_ptr 语义：与 owning Sptr 成对时（含 Wptr 在前）"
echo "  均能正确还原；独立 weak_ptr 无 Sptr 时按设计语义过期。"
echo "=============================================================================="