#pragma once

#include <iostream>
#include <string>
#include <vector>

#ifdef _MSC_VER
#define UNREACHABLE() __assume(false)
#else
#define UNREACHABLE() __builtin_unreachable()
#endif // _MSC_VER

namespace {
    constexpr size_t SnprintfBufferSize = 512;
}

extern char snprintfBuffer[SnprintfBufferSize];

std::vector<std::string> Split(const std::string &str, char c);
std::string TrimWhiteSpaces(const std::string &str);
std::ostream& operator<<(std::ostream& dest, __int128_t value);
