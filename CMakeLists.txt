cmake_minimum_required(VERSION 3.20)
project(StreamPunk LANGUAGES CXX)

# ============================================================
# 禁止 in-source build（强制 out-of-source，避免污染源码目录）
# ============================================================
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR
        "\n========================================\n"
        "  In-source build is FORBIDDEN.\n"
        "  Please use out-of-source build:\n"
        "    mkdir build && cd build\n"
        "    cmake ..\n"
        "    cmake --build .\n"
        "========================================\n")
endif()

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 全局 include 目录：库头文件 + demo 类型
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/examples)

# 跨平台 UTF-8 编码支持：确保中文注释在所有平台正常编译
# MSVC: /utf-8 强制源文件和执行字符集为 UTF-8；/Zc:preprocessor 支持 __VA_OPT__
# GCC/Clang: Linux/macOS 默认 UTF-8，显式指定以防系统区域设置不同
if(MSVC)
    add_compile_options(/utf-8 /Zc:preprocessor)
else()
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# ============================================================
# 选项
# ============================================================
option(BUILD_TOOLS "Build sp-gen code generator" ON)
option(BUILD_EXAMPLES "Build all examples" ON)
option(BUILD_TESTS "Build all tests" OFF)

# 细粒度测试选项（默认跟随 BUILD_TESTS）
option(BUILD_TEST_CORE "Build core type tests" ${BUILD_TESTS})
option(BUILD_TEST_JSON "Build JSON tests" ${BUILD_TESTS})
option(BUILD_TEST_SCHEMA "Build schema tests" ${BUILD_TESTS})
option(BUILD_TEST_SPOI_SHADOW "Build SPOI shadow tests" ${BUILD_TESTS})
option(BUILD_TEST_CROSS_LANG "Build cross-language tests" ${BUILD_TESTS})
option(BUILD_TEST_SPOI "Build SPOI tests" ${BUILD_TESTS})
option(BUILD_TEST_ORM "Build ORM integration tests" OFF)
option(BUILD_SP26_REFLECTION "Build C++26 reflection version (requires GCC 16+)" OFF)

# ============================================================
# 第三方依赖（vcpkg manifest 模式）
# 用户只需运行: vcpkg install
# ORM 测试需要额外: vcpkg install --x-feature=orm
# 如果本地有 3/ 目录（预装依赖），自动回退使用
# ============================================================
set(LEGACY_3RDPARTY_DIR "${CMAKE_SOURCE_DIR}/3/x64-windows")

if(EXISTS "${LEGACY_3RDPARTY_DIR}/include/doctest/doctest.h")
    message(STATUS "Using legacy 3rd-party directory: ${LEGACY_3RDPARTY_DIR}")
    add_library(doctest::doctest INTERFACE IMPORTED)
    target_include_directories(doctest::doctest INTERFACE "${LEGACY_3RDPARTY_DIR}/include")
    add_library(cxxopts::cxxopts INTERFACE IMPORTED)
    target_include_directories(cxxopts::cxxopts INTERFACE "${LEGACY_3RDPARTY_DIR}/include")
else()
    find_package(doctest CONFIG REQUIRED)
    find_package(cxxopts CONFIG REQUIRED)
endif()

# ============================================================
# 宏：创建测试目标
# ============================================================
macro(add_stream_punk_test TARGET_NAME)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES COMPILE_OPTIONS)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${TARGET_NAME}
        test/test_main.cpp
        ${TEST_SOURCES}
    )
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tools/sp-gen
    )
    target_link_libraries(${TARGET_NAME} PRIVATE doctest::doctest)
    target_compile_definitions(${TARGET_NAME} PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /bigobj /FS ${TEST_COMPILE_OPTIONS})
    endif()
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME} --success -s)
endmacro()

# ============================================================
# tools/sp-gen — 统一代码生成器
# ============================================================
if(BUILD_TOOLS)
    add_executable(sp-gen
        tools/sp-gen/main.cpp
        tools/sp-gen/ts_generator_2.cpp
        tools/sp-gen/python_generator.cpp
        tools/sp-gen/java_generator.cpp
        tools/sp-gen/go_generator.cpp
        tools/sp-gen/rust_generator.cpp
        tools/sp-gen/kotlin_generator.cpp
        tools/sp-gen/spoi_python_generator.cpp
        tools/sp-gen/spoi_ts_generator.cpp
        tools/sp-gen/spoi_go_generator.cpp
        tools/sp-gen/spoi_rust_generator.cpp
        tools/sp-gen/spoi_js_generator.cpp
        tools/sp-gen/spoi_java_generator.cpp
        tools/sp-gen/spoi_kotlin_generator.cpp
        tools/sp-gen/spoi_go_accessor_generator.cpp
        tools/sp-gen/spoi_java_accessor_generator.cpp
        tools/sp-gen/spoi_kotlin_accessor_generator.cpp
        tools/sp-gen/spoi_python_accessor_generator.cpp
        tools/sp-gen/spoi_ts_accessor_generator.cpp
        tools/sp-gen/spoi_js_accessor_generator.cpp
        tools/sp-gen/spoi_rust_accessor_generator.cpp
        tools/sp-gen/spoi_python_exec_generator.cpp
        tools/sp-gen/spoi_ts_exec_generator.cpp
        tools/sp-gen/spoi_js_exec_generator.cpp
    )
    target_include_directories(sp-gen PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tools/sp-gen
    )
    target_link_libraries(sp-gen PRIVATE cxxopts::cxxopts)
    target_compile_definitions(sp-gen PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(sp-gen PRIVATE /FS)
    endif()
endif()

# ============================================================
# tests — 单元测试（按模块拆分，独立编译运行）
# ============================================================

# --- test-core: 基础类型 / 容器 / 字符串 / 指针等 ---
if(BUILD_TEST_CORE)
    add_stream_punk_test(test-core
        SOURCES
            test/test_basic_types.cpp
            test/test_containers.cpp
            test/test_strings.cpp
            test/test_enum.cpp
            test/test_optional.cpp
            test/test_chrono.cpp
            test/test_variant.cpp
            test/test_tuple.cpp
            test/test_pointers.cpp
            test/test_deep_copy.cpp
            test/test_custom_types.cpp
            test/test_custom_data_coverage.cpp
            test/test_machine_info.cpp
            test/test_template_reflection.cpp
            test/test_data_definition.cpp
    )
endif()

# --- test-json: JSON 序列化 / 流式 ---
if(BUILD_TEST_JSON)
    add_stream_punk_test(test-json
        SOURCES
            test/test_json.cpp
            test/test_json_stream.cpp
    )
endif()

# --- test-schema: Schema / 生成器 / 元数据 ---
if(BUILD_TEST_SCHEMA)
    add_stream_punk_test(test-schema
        SOURCES
            test/test_schema.cpp
            test/test_generators.cpp
            test/test_meta_reader.cpp
            test/test_orm_meta.cpp
    )
    target_compile_definitions(test-schema PRIVATE SP_ENABLE_ORM_TYPES)
endif()

# --- test-spoi-shadow: SPOI 影子模式 ---
if(BUILD_TEST_SPOI_SHADOW)
    add_stream_punk_test(test-spoi-shadow
        SOURCES
            test/test_spoi_shadow.cpp
    )
endif()

# --- gen-meta: 元数据生成器（从 Data.hpp 生成 stream-punk-meta.bin） ---
if(BUILD_TESTS OR BUILD_TEST_SPOI)
    add_executable(gen-meta
        test/cross_test/gen_meta.cpp
    )
    target_include_directories(gen-meta PRIVATE
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tools/sp-gen
    )
    target_link_libraries(gen-meta PRIVATE doctest::doctest)
    target_compile_definitions(gen-meta PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(gen-meta PRIVATE /bigobj /FS)
    endif()
endif()

# --- test-cross-lang: 跨语言运行时测试 ---
if(BUILD_TEST_CROSS_LANG)
    add_stream_punk_test(test-cross-lang
        SOURCES
            test/cross_lang_test_gen.cpp
            test/test_py_runtime.cpp
            test/test_java_runtime.cpp
            test/test_go_runtime.cpp
            test/test_rust_runtime.cpp
            test/test_kotlin_runtime.cpp
    )
endif()

# --- test-spoi: SPOI 核心测试（轻量，协议+边缘+复杂场景） ---
if(BUILD_TEST_SPOI)
    add_executable(test-spoi
        test/test_main.cpp
        test/test_spoi.cpp
        test/test_spoi_edge.cpp
        test/test_spoi_complex.cpp
    )
    target_include_directories(test-spoi PRIVATE
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tools/sp-gen
    )
    target_link_libraries(test-spoi PRIVATE doctest::doctest)
    target_compile_definitions(test-spoi PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(test-spoi PRIVATE /bigobj /FS)
    endif()
    add_test(NAME test-spoi COMMAND test-spoi --success -s)

# --- test-spoi-gen: 代码生成器测试（中等负荷） ---
    add_executable(test-spoi-gen
        test/test_main.cpp
        test/test_spoi_gen.cpp
        tools/sp-gen/spoi_python_generator.cpp
        tools/sp-gen/spoi_ts_generator.cpp
        tools/sp-gen/spoi_go_generator.cpp
        tools/sp-gen/spoi_rust_generator.cpp
        tools/sp-gen/spoi_js_generator.cpp
        tools/sp-gen/spoi_java_generator.cpp
        tools/sp-gen/spoi_kotlin_generator.cpp
        tools/sp-gen/spoi_go_accessor_generator.cpp
        tools/sp-gen/spoi_java_accessor_generator.cpp
        tools/sp-gen/spoi_kotlin_accessor_generator.cpp
        tools/sp-gen/spoi_python_accessor_generator.cpp
        tools/sp-gen/spoi_ts_accessor_generator.cpp
        tools/sp-gen/spoi_js_accessor_generator.cpp
        tools/sp-gen/spoi_rust_accessor_generator.cpp
    )
    target_include_directories(test-spoi-gen PRIVATE
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tools/sp-gen
    )
    target_link_libraries(test-spoi-gen PRIVATE doctest::doctest)
    target_compile_definitions(test-spoi-gen PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(test-spoi-gen PRIVATE /bigobj /FS)
    endif()

# --- test-spoi-builder: 构建器测试（中等负荷） ---
    add_executable(test-spoi-builder
        test/test_main.cpp
        test/test_spoi_builder.cpp
    )
    target_include_directories(test-spoi-builder PRIVATE
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tools/sp-gen
    )
    target_link_libraries(test-spoi-builder PRIVATE doctest::doctest)
    target_compile_definitions(test-spoi-builder PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(test-spoi-builder PRIVATE /bigobj /FS)
    endif()

# --- test-spoi-fuzz-test: 模糊测试套件（中负荷，已降频） ---
    add_executable(test-spoi-fuzz-test
        test/test_main.cpp
        test/test_spoi_fuzz.cpp
    )
    target_include_directories(test-spoi-fuzz-test PRIVATE
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tools/sp-gen
    )
    target_link_libraries(test-spoi-fuzz-test PRIVATE doctest::doctest)
    target_compile_definitions(test-spoi-fuzz-test PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(test-spoi-fuzz-test PRIVATE /bigobj /FS)
    endif()

# --- test-spoi-e2e: 跨语言端到端测试（独立目标，资源占用大） ---
    add_executable(test-spoi-e2e
        test/test_main.cpp
        test/test_spoi_e2e.cpp
        tools/sp-gen/spoi_python_generator.cpp
        tools/sp-gen/spoi_ts_generator.cpp
        tools/sp-gen/spoi_go_generator.cpp
        tools/sp-gen/spoi_rust_generator.cpp
        tools/sp-gen/spoi_js_generator.cpp
        tools/sp-gen/spoi_java_generator.cpp
        tools/sp-gen/spoi_kotlin_generator.cpp
        tools/sp-gen/spoi_go_accessor_generator.cpp
        tools/sp-gen/spoi_java_accessor_generator.cpp
        tools/sp-gen/spoi_kotlin_accessor_generator.cpp
        tools/sp-gen/spoi_python_accessor_generator.cpp
        tools/sp-gen/spoi_ts_accessor_generator.cpp
        tools/sp-gen/spoi_js_accessor_generator.cpp
        tools/sp-gen/spoi_rust_accessor_generator.cpp
    )
    target_include_directories(test-spoi-e2e PRIVATE
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tools/sp-gen
    )
    target_link_libraries(test-spoi-e2e PRIVATE doctest::doctest)
    target_compile_definitions(test-spoi-e2e PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(test-spoi-e2e PRIVATE /bigobj /FS)
    endif()
# --- test-spoi-fuzz: 覆盖率引导模糊器（独立可执行文件，自带 main） ---
    add_executable(test-spoi-fuzz
        test/test_spoi_fuzz_harness.cpp
    )
    target_include_directories(test-spoi-fuzz PRIVATE
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tools/sp-gen
    )
    target_link_libraries(test-spoi-fuzz PRIVATE doctest::doctest)
    target_compile_definitions(test-spoi-fuzz PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(test-spoi-fuzz PRIVATE /bigobj /FS /EHa)
    endif()

    # --- test-spoi-bench: 网络传输场景性能基准测试 ---
    add_executable(test-spoi-bench
        test/test_main.cpp
        test/test_spoi_bench.cpp
    )
    target_include_directories(test-spoi-bench PRIVATE
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tools/sp-gen
    )
    target_link_libraries(test-spoi-bench PRIVATE doctest::doctest)
    target_compile_definitions(test-spoi-bench PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(test-spoi-bench PRIVATE /bigobj /FS)
    endif()

    # --- test-spoi-fuzz-network: 网络层模糊器（独立可执行文件，自带 main） ---
    add_executable(test-spoi-fuzz-network
        test/test_spoi_fuzz_network.cpp
    )
    target_include_directories(test-spoi-fuzz-network PRIVATE
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tools/sp-gen
    )
    target_link_libraries(test-spoi-fuzz-network PRIVATE ws2_32 doctest::doctest)
    target_compile_definitions(test-spoi-fuzz-network PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(test-spoi-fuzz-network PRIVATE /bigobj /FS /EHa)
    endif()

    # --- test-spoi-fuzz-crosslang: 跨语言一致性模糊器（独立可执行文件，自带 main） ---
    add_executable(test-spoi-fuzz-crosslang
        test/test_spoi_fuzz_crosslang.cpp
    )
    target_include_directories(test-spoi-fuzz-crosslang PRIVATE
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tools/sp-gen
    )
    target_link_libraries(test-spoi-fuzz-crosslang PRIVATE doctest::doctest)
    target_compile_definitions(test-spoi-fuzz-crosslang PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(test-spoi-fuzz-crosslang PRIVATE /bigobj /FS /EHa)
    endif()
endif()

# --- test-orm: ORM 集成测试（需要 SOCI，通过 vcpkg --x-feature=orm 安装） ---
if(BUILD_TEST_ORM)
    find_package(soci CONFIG REQUIRED)
    find_package(SQLite3 REQUIRED)
    find_package(unofficial-libmysql CONFIG QUIET)
    find_package(ZLIB REQUIRED)
    find_package(OpenSSL REQUIRED)

    add_executable(test-orm
        test/test_main.cpp
        test/test_orm_integration.cpp
        test/test_orm_integration_sqlite.cpp
    )
    target_include_directories(test-orm PRIVATE
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(test-orm PRIVATE
        doctest::doctest
        soci::soci
        soci::soci_sqlite3
        SQLite::SQLite3
        ZLIB::ZLIB
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    if(unofficial-libmysql_FOUND)
        target_link_libraries(test-orm PRIVATE
            soci::soci_mysql
            unofficial::libmysql::libmysql
        )
    endif()
    target_compile_definitions(test-orm PRIVATE NOMINMAX)
    if(MSVC)
        target_compile_options(test-orm PRIVATE /bigobj /FS)
    endif()
    add_test(NAME test-orm COMMAND test-orm)
endif()

# ============================================================
# 便利目标：构建所有测试
# ============================================================
if(BUILD_TESTS OR BUILD_TEST_CORE OR BUILD_TEST_JSON OR BUILD_TEST_SCHEMA
   OR BUILD_TEST_SPOI_SHADOW OR BUILD_TEST_CROSS_LANG OR BUILD_TEST_SPOI OR BUILD_TEST_ORM)
    add_custom_target(all-tests)
    if(BUILD_TEST_CORE)
        add_dependencies(all-tests test-core)
    endif()
    if(BUILD_TEST_JSON)
        add_dependencies(all-tests test-json)
    endif()
    if(BUILD_TEST_SCHEMA)
        add_dependencies(all-tests test-schema)
    endif()
    if(BUILD_TEST_SPOI_SHADOW)
        add_dependencies(all-tests test-spoi-shadow)
    endif()
    if(BUILD_TEST_CROSS_LANG)
        add_dependencies(all-tests test-cross-lang)
    endif()
    if(BUILD_TEST_SPOI)
        add_dependencies(all-tests test-spoi)
        add_dependencies(all-tests test-spoi-fuzz)
        add_dependencies(all-tests test-spoi-fuzz-network)
        add_dependencies(all-tests test-spoi-fuzz-crosslang)
    endif()
    if(BUILD_TEST_ORM)
        add_dependencies(all-tests test-orm)
    endif()
endif()

# ============================================================
# C++26 反射版本 — 测试（仅 GCC 16+ 支持 -freflection）
# 测试目标均以 sp26- 前缀命名，与旧版 test- 前缀区分
# ============================================================
if(BUILD_SP26_REFLECTION)
    # 宏：创建 sp26 测试目标（独立可执行文件，不使用 doctest）
    macro(add_sp26_test TARGET_NAME)
        add_executable(${TARGET_NAME} ${ARGN})
        target_include_directories(${TARGET_NAME} PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/examples
        )
        target_compile_features(${TARGET_NAME} PRIVATE cxx_std_26)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${TARGET_NAME} PRIVATE -freflection)
        endif()
        add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
    endmacro()

    add_sp26_test(test-sp26-serialization
        test/test_sp26_serialization.cpp
    )
    add_sp26_test(test-sp26-containers
        test/test_sp26_containers.cpp
    )
    add_sp26_test(test-sp26-advanced
        test/test_sp26_advanced.cpp
    )
    add_sp26_test(test-sp26-custom-types
        test/test_sp26_custom_types.cpp
    )
    add_sp26_test(test-sp26-quick
        test/test_sp26_quick.cpp
    )

    # 便利目标
    add_custom_target(all-sp26-tests)
    add_dependencies(all-sp26-tests
        test-sp26-serialization
        test-sp26-containers
        test-sp26-advanced
        test-sp26-custom-types
        test-sp26-quick
    )
endif()

# ============================================================
# examples — 场景示例
# ============================================================
if(BUILD_EXAMPLES)
    add_subdirectory(examples/01-basic-cpp)
    add_subdirectory(examples/02-cpp-to-ts)
    add_subdirectory(examples/03-dynamic-schema)
    add_subdirectory(examples/04-json)
    add_subdirectory(examples/05-orm)
    add_subdirectory(examples/06-shadow-delta)
    add_subdirectory(examples/07-spoi)
    add_subdirectory(examples/08-spoi-cross-lang)
    add_subdirectory(examples/09-spoi-cross-lang-all)
    add_subdirectory(examples/10-game)
    add_subdirectory(examples/11-pod-demo)
    add_subdirectory(examples/12-gomoku/server)
    add_subdirectory(examples/14-tank-battle)
    add_subdirectory(examples/15-restaurant/server)
    add_subdirectory(examples/17-wal-persistence)
    add_subdirectory(examples/18-iot-sensor)
    add_subdirectory(examples/19-leaderboard)
    add_subdirectory(examples/20-microservice-mesh)
endif()

# C++26 反射版示例（仅当 BUILD_SP26_REFLECTION=ON 时构建）
if(BUILD_EXAMPLES AND BUILD_SP26_REFLECTION)
    add_subdirectory(examples/26-reflection)
endif()

# ============================================================
# 清理命令：cmake --build build --target clean-all
# 清空整个构建目录（比 clean 更彻底，会删除 CMake 缓存）
# ============================================================
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/CMakeFiles"
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_BINARY_DIR}/CMakeCache.txt"
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_BINARY_DIR}/cmake_install.cmake"
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_BINARY_DIR}/Makefile"
    COMMAND ${CMAKE_COMMAND} -E remove -f "${CMAKE_BINARY_DIR}/CTestTestfile.cmake"
    COMMAND ${CMAKE_COMMAND} -E echo "Build directory cleaned. Run 'cmake ..' to reconfigure."
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)