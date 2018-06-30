#pragma once

#include <string>
#include <vector>

#ifdef _MSC_VER
#define UNREACHABLE() __assume(false)
#else
#define UNREACHABLE() __builtin_unreachable()
#endif // _MSC_VER

static constexpr size_t SPRINTF_BUFFER_SIZE = 256;
static char sprintfBuffer[SPRINTF_BUFFER_SIZE];

// Error messages
namespace ErrorMessage {
    static constexpr const char *OperatorOrOperandNotDefined = "Operator or operand \"%s\" is not defined";
    static constexpr const char *DefinitionTextNotSplittedProperly = "The following definition text is not splitted by two '|'s: \"%s\"";
    static constexpr const char *SomeOperandsMissing = "Some operand(s) is missing";
    static constexpr const char *TokenExpected = "\"%s\" is expected";
    static constexpr const char *UnexpectedToken = "Unexpected token \"%s\"";

    static constexpr const char *AssertionError = "Assertion error (this is a bug of compiler)";
}

static std::vector<std::string> Split(const std::string &str, char c) {
    std::string::size_type begin = 0;
    std::vector<std::string> vec;

    while (begin < str.length()) {
        std::string::size_type end = str.find_first_of(c, begin);
        vec.push_back(str.substr(begin, end - begin));

        if (end == std::string::npos) {
            break;
        }

        begin = end + 1;
    }

    return vec;
}

static std::string TrimWhiteSpaces(const std::string &str) {
    std::string::size_type left = str.find_first_not_of(' ');

    if (left == std::string::npos) {
        return str;
    }

    std::string::size_type right = str.find_last_not_of(' ');
    return str.substr(left, right - left + 1);
}
