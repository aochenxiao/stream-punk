#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <cxxopts.hpp>
#include <memory>
#include "generators.hpp"

void print_usage(const cxxopts::Options& options) {
    std::cout << options.help() << std::endl;
    std::cout << "\nSupported targets:" << std::endl;
    std::cout << "  ts     - Generate TypeScript code" << std::endl;
    std::cout << "  py     - Generate Python code" << std::endl;
}

int main(int argc, char** argv) {
    cxxopts::Options options("stream-punk-gen", "Stream-Punk code generator for multiple languages");
    
    options.add_options()
        ("t,target", "Target language (ts, py)", cxxopts::value<std::string>()->default_value("ts"))
        ("p,path", "Output file path", cxxopts::value<std::string>()->default_value(""))
        ("h,help", "Print help message");
    
    try {
        auto result = options.parse(argc, argv);
        
        if (result.count("help")) {
            print_usage(options);
            return 0;
        }
        
        std::string target = result["target"].as<std::string>();
        std::string output_path = result["path"].as<std::string>();
        
        // Set default output paths based on target if not specified
        if (output_path.empty()) {
            if (target == "ts") {
                output_path = "./stream_punk_data.ts";
            } else if (target == "py") {
                output_path = "./stream_punk_data.py";
            } else {
                std::cerr << "Error: Unknown target language: " << target << std::endl;
                print_usage(options);
                return 1;
            }
        }
        
        // Call appropriate generator based on target
        int result_code = 0;
        if (target == "ts") {
            result_code = generate_ts(output_path);
        } else if (target == "py") {
            result_code = generate_python(output_path);
        } else {
            std::cerr << "Error: Unsupported target language: " << target << std::endl;
            std::cerr << "Supported targets: js, ts, py" << std::endl;
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