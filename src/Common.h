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

// Error messages
namespace ErrorMessage {
    static constexpr const char *OperatorOrOperandNotDefined = "Operator or operand \"%s\" is not defined";
    static constexpr const char *DefinitionTextNotSplittedProperly = "The following definition text is not splitted by two '|'s: \"%s\"";
    static constexpr const char *SomeOperandsMissing = "Some operand(s) is missing";
    static constexpr const char *TokenExpected = "\"%s\" is expected";
    static constexpr const char *UnexpectedToken = "Unexpected token \"%s\"";

    static constexpr const char *AssertionError = "Assertion error (this is a bug of compiler)";
}

std::vector<std::string> Split(const std::string &str, char c);
std::string TrimWhiteSpaces(const std::string &str);
std::ostream& operator<<(std::ostream& dest, __int128_t value);
