#pragma once

#if defined(__cpp_lib_filesystem) || __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif defined(__cpp_lib_experimental_filesystem) || __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error "No filesystem support"
#endif
