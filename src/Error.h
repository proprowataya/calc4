#pragma once

#include <sstream>
#include <string>

// Error messages
namespace ErrorMessage
{
static inline std::string OperatorOrOperandNotDefined(const std::string& name)
{
    std::ostringstream oss;
    oss << "Operator or operand \"" << name << "\" is not defined";
    return oss.str();
}

static inline std::string DefinitionTextNotSplittedProperly(const std::string& text)
{
    std::ostringstream oss;
    oss << "The following definition text is not splitted by two '|'s: \"" << text << "\"";
    return oss.str();
}

static inline std::string SomeOperandsMissing()
{
    return "Some operand(s) is missing";
}

static inline std::string TokenExpected(const std::string& name)
{
    std::ostringstream oss;
    oss << "\"" << name << "\" is expected";
    return oss.str();
}

static inline std::string UnexpectedToken(char token)
{
    std::ostringstream oss;
    oss << "Unexpected token \"" << token << "\"";
    return oss.str();
}

static inline std::string CodeIsEmpty()
{
    return "Code is empty";
}

static inline std::string AssertionError()
{
    return "Assertion error (this is a bug of compiler)";
}
}
