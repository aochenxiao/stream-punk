#pragma once
#include <string>

// Forward declarations for target-specific generators
int generate_ts(const std::string& output_path);
int generate_python(const std::string& output_path);