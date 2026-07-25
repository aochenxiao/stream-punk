#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cxxopts.hpp>
#include <memory>
#include "generators.hpp"

namespace fs = std::filesystem;

// 计算 runtimes 目录的绝对路径
// 优先使用环境变量 SP_GEN_RUNTIMES_DIR，否则基于源码树位置计算
static std::string computeRuntimesDir(const char* argv0) {
    const char* env = std::getenv("SP_GEN_RUNTIMES_DIR");
    if (env && env[0] != '\0') return std::string(env);

    // 源码树: tools/sp-gen/main.cpp → runtimes/
    fs::path srcDir = fs::path(__FILE__).parent_path();  // tools/sp-gen/
    fs::path runtimesDir = srcDir.parent_path().parent_path() / "runtimes";
    if (fs::exists(runtimesDir)) return runtimesDir.string();

    // 安装后: bin/sp-gen → share/stream-punk/runtimes/
    if (argv0) {
        fs::path exeDir = fs::path(argv0).parent_path();
        runtimesDir = exeDir.parent_path() / "share" / "stream-punk" / "runtimes";
        if (fs::exists(runtimesDir)) return runtimesDir.string();
    }

    return "";
}

void print_usage(const cxxopts::Options& options) {
    std::cout << options.help() << std::endl;
    std::cout << "\nSupported targets:" << std::endl;
    std::cout << "  ts        - Generate TypeScript code (v2, default)" << std::endl;
    std::cout << "  ts-meta   - Generate TypeScript code from metadata" << std::endl;
    std::cout << "  py        - Generate Python code" << std::endl;
    std::cout << "  py-meta   - Generate Python code from metadata" << std::endl;
    std::cout << "  java      - Generate Java code" << std::endl;
    std::cout << "  java-meta - Generate Java code from metadata" << std::endl;
    std::cout << "  go-meta   - Generate Go code from metadata" << std::endl;
    std::cout << "  rust-meta - Generate Rust code from metadata" << std::endl;
    std::cout << "  kotlin-meta - Generate Kotlin code from metadata" << std::endl;
    std::cout << "  spoi-py     - Generate Python SPOI query/update builder" << std::endl;
    std::cout << "  spoi-ts     - Generate TypeScript SPOI query/update builder" << std::endl;
    std::cout << "  spoi-go     - Generate Go SPOI query/update builder" << std::endl;
    std::cout << "  spoi-rust   - Generate Rust SPOI query/update builder" << std::endl;
    std::cout << "  spoi-js     - Generate JavaScript SPOI query/update builder" << std::endl;
    std::cout << "  spoi-java   - Generate Java SPOI query/update builder" << std::endl;
    std::cout << "  spoi-kotlin - Generate Kotlin SPOI query/update builder" << std::endl;
    std::cout << "  spoi-py-exec  - Generate Python SPOI executor type registry" << std::endl;
    std::cout << "  spoi-ts-exec  - Generate TypeScript SPOI executor type registry" << std::endl;
    std::cout << "  spoi-js-exec  - Generate JavaScript SPOI executor type registry" << std::endl;
    std::cout << "  spoi-go-accessor     - Generate Go SPOI type-specialized accessor" << std::endl;
    std::cout << "  spoi-java-accessor   - Generate Java SPOI type-specialized accessor" << std::endl;
    std::cout << "  spoi-kotlin-accessor - Generate Kotlin SPOI type-specialized accessor" << std::endl;
    std::cout << "  spoi-py-accessor     - Generate Python SPOI type-specialized accessor" << std::endl;
    std::cout << "  spoi-ts-accessor     - Generate TypeScript SPOI type-specialized accessor" << std::endl;
    std::cout << "  spoi-js-accessor     - Generate JavaScript SPOI type-specialized accessor" << std::endl;
    std::cout << "  spoi-rust-accessor   - Generate Rust SPOI type-specialized accessor" << std::endl;
}

int main(int argc, char** argv) {
    cxxopts::Options options("stream-punk-gen", "Stream-Punk code generator for multiple languages");
    
    options.add_options()
        ("t,target", "Target language (ts, ts-meta, py, py-meta, java, java-meta, go-meta, rust-meta, kotlin-meta, spoi-py, spoi-ts, spoi-go, spoi-rust, spoi-js, spoi-java, spoi-kotlin, spoi-py-exec, spoi-ts-exec, spoi-js-exec, spoi-go-accessor, spoi-java-accessor, spoi-kotlin-accessor, spoi-py-accessor, spoi-ts-accessor, spoi-js-accessor, spoi-rust-accessor)", cxxopts::value<std::string>()->default_value("ts"))
        ("p,path", "Output file path", cxxopts::value<std::string>()->default_value(""))
        ("m,meta", "Metadata file path (default: temp/stream-punk-meta.bin)", cxxopts::value<std::string>()->default_value("temp/stream-punk-meta.bin"))
        ("I,include", "Custom type definition directory (for non-meta targets)", cxxopts::value<std::string>()->default_value(""))
        ("no-embed-runtime", "Do not embed runtime in generated file; copy runtime file next to output instead", cxxopts::value<bool>()->default_value("false"))
        ("h,help", "Print help message");
    
    try {
        auto result = options.parse(argc, argv);
        
        if (result.count("help")) {
            print_usage(options);
            return 0;
        }
        
        std::string target = result["target"].as<std::string>();
        std::string output_path = result["path"].as<std::string>();
        std::string meta_path = result["meta"].as<std::string>();
        std::string include_dir = result["include"].as<std::string>();
        bool no_embed_runtime = result["no-embed-runtime"].as<bool>();
        
        // 计算 runtimes 目录
        std::string runtimes_dir = computeRuntimesDir(argv[0]);
        if (runtimes_dir.empty() && (target == "ts" || target == "py" || target == "java")) {
            std::cerr << "Warning: Could not locate runtimes directory. "
                      << "Set SP_GEN_RUNTIMES_DIR environment variable." << std::endl;
        }
        
        // 如果未指定输出路径，则根据目标语言设置默认输出路径
        if (output_path.empty()) {
            if (target == "ts") {
                output_path = "./stream_punk_data.ts";
            } else if (target == "py") {
                output_path = "./stream_punk_data.py";
            } else if (target == "java") {
                output_path = "./StreamPunkData.java";
            } else {
                std::cerr << "Error: Unknown target language: " << target << std::endl;
                print_usage(options);
                return 1;
            }
        }
        
        // 根据目标语言调用相应的生成器
        int result_code = 0;
        if (target == "ts") {
            result_code = generate_ts_2(output_path, runtimes_dir, no_embed_runtime);
        } else if (target == "ts-meta") {
            result_code = generate_ts_meta(output_path, meta_path, no_embed_runtime);
        } else if (target == "py") {
            result_code = generate_python(output_path, runtimes_dir);
        } else if (target == "py-meta") {
            result_code = generate_python_meta(output_path, meta_path);
        } else if (target == "java") {
            result_code = generate_java(output_path, runtimes_dir);
        } else if (target == "java-meta") {
            result_code = generate_java_meta(output_path, meta_path);
        } else if (target == "go-meta") {
            result_code = generate_go_meta(output_path, meta_path);
        } else if (target == "rust-meta") {
            result_code = generate_rust_meta(output_path, meta_path);
        } else if (target == "kotlin-meta") {
            result_code = generate_kotlin_meta(output_path, meta_path);
        } else if (target == "spoi-py") {
            result_code = generate_spoi_python(output_path, meta_path);
        } else if (target == "spoi-ts") {
            result_code = generate_spoi_ts(output_path, meta_path);
        } else if (target == "spoi-go") {
            result_code = generate_spoi_go(output_path, meta_path);
        } else if (target == "spoi-rust") {
            result_code = generate_spoi_rust(output_path, meta_path);
        } else if (target == "spoi-js") {
            result_code = generate_spoi_js(output_path, meta_path);
        } else if (target == "spoi-java") {
            result_code = generate_spoi_java(output_path, meta_path);
        } else if (target == "spoi-kotlin") {
            result_code = generate_spoi_kotlin(output_path, meta_path);
        } else if (target == "spoi-py-exec") {
            result_code = generate_spoi_python_exec(output_path, meta_path);
        } else if (target == "spoi-ts-exec") {
            result_code = generate_spoi_ts_exec(output_path, meta_path);
        } else if (target == "spoi-js-exec") {
            result_code = generate_spoi_js_exec(output_path, meta_path);
        } else if (target == "spoi-go-accessor") {
            result_code = generate_spoi_go_accessor(output_path, meta_path);
        } else if (target == "spoi-java-accessor") {
            result_code = generate_spoi_java_accessor(output_path, meta_path);
        } else if (target == "spoi-kotlin-accessor") {
            result_code = generate_spoi_kotlin_accessor(output_path, meta_path);
        } else if (target == "spoi-py-accessor") {
            result_code = generate_spoi_python_accessor(output_path, meta_path);
        } else if (target == "spoi-ts-accessor") {
            result_code = generate_spoi_ts_accessor(output_path, meta_path);
        } else if (target == "spoi-js-accessor") {
            result_code = generate_spoi_js_accessor(output_path, meta_path);
        } else if (target == "spoi-rust-accessor") {
            result_code = generate_spoi_rust_accessor(output_path, meta_path);
        } else {
            std::cerr << "Error: Unsupported target language: " << target << std::endl;
            std::cerr << "Supported targets: ts, ts-meta, py, py-meta, java, java-meta, go-meta, rust-meta, kotlin-meta, spoi-py, spoi-ts, spoi-go, spoi-rust, spoi-js, spoi-java, spoi-kotlin, spoi-py-exec, spoi-ts-exec, spoi-js-exec, spoi-go-accessor, spoi-java-accessor, spoi-kotlin-accessor, spoi-py-accessor, spoi-ts-accessor, spoi-js-accessor, spoi-rust-accessor" << std::endl;
            return 1;
        }
        
        if (result_code == 0) {
            std::cout << "Successfully generated " << target << " code to " << output_path << std::endl;
        }
        
        return result_code;
        
    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing options: " << e.what() << std::endl;
        print_usage(options);
        return 1;
    }
}