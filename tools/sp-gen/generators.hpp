#pragma once
#include <string>

// 目标语言生成器的前向声明
// runtimes_dir: 运行时库文件所在目录（默认为空，从源码树自动定位）
int generate_ts_2(const std::string& output_path, const std::string& runtimes_dir = "", bool no_embed_runtime = false);
int generate_ts_meta(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin", bool no_embed_runtime = false);
int generate_python(const std::string& output_path, const std::string& runtimes_dir = "");
int generate_python_meta(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_java(const std::string& output_path, const std::string& runtimes_dir = "");
int generate_java_meta(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin", const std::string& runtimes_dir = "");
int generate_go_meta(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_rust_meta(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_kotlin_meta(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");

// SPOI 生成器（跨语言 SPOI 查询/更新 builder）
int generate_spoi_python(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_ts(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_go(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_rust(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_js(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_java(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_kotlin(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");

// SPOI Executor 注册表生成器（跨语言 SPOI 执行器支持）
int generate_spoi_python_exec(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_ts_exec(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_js_exec(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");

// SPOI Accessor 生成器（类型特化访问器，替代反射）
int generate_spoi_go_accessor(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_java_accessor(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_kotlin_accessor(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_python_accessor(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_ts_accessor(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_js_accessor(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");
int generate_spoi_rust_accessor(const std::string& output_path, const std::string& meta_path = "temp/stream-punk-meta.bin");