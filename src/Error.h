#pragma once

// Error messages
namespace ErrorMessage {
    static constexpr const char *OperatorOrOperandNotDefined = "Operator or operand \"%s\" is not defined";
    static constexpr const char *DefinitionTextNotSplittedProperly = "The following definition text is not splitted by two '|'s: \"%s\"";
    static constexpr const char *SomeOperandsMissing = "Some operand(s) is missing";
    static constexpr const char *TokenExpected = "\"%s\" is expected";
    static constexpr const char *UnexpectedToken = "Unexpected token \"%c\"";
    static constexpr const char *CodeIsEmpty = "Code is empty";

    static constexpr const char *AssertionError = "Assertion error (this is a bug of compiler)";
}
