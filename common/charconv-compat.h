#pragma once
#include <string_view>

#if defined (__cpp_lib_to_chars) || __has_include(<charconv>)
#include <charconv>

#define HAVE_TO_CHARS

template <typename T>
bool parse_number(std::string_view source, T& number) {
    auto from = source.data();
    auto to = from + source.size();
    if (auto [p, ec] = std::from_chars(from, to, number); ec == std::errc()) {
        return true;
    }
    return false;
}

#else
#include <sstream>

template <typename T>
bool parse_number(std::string_view source, T& number) {
    std::stringstream stream;
    stream << source;
    if (stream >> number) {
        return true;
    }
    return false;
}

#endif
