/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2025 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include "Common.h"
#include <exception>
#include <optional>
#include <sstream>
#include <string>

// Error messages
namespace calc4::Exceptions
{
class Calc4Exception : std::exception
{
protected:
    std::optional<CharPosition> position;
    std::string message;

    Calc4Exception(const std::optional<CharPosition>& position, const std::string& message)
        : position(position), message(message)
    {
    }

public:
    const std::optional<CharPosition>& GetPosition() const
    {
        return position;
    }

    virtual const char* what() const noexcept override
    {
        return message.c_str();
    }
};

class OperatorOrOperandNotDefinedException : public Calc4Exception
{
public:
    OperatorOrOperandNotDefinedException(const std::optional<CharPosition>& position,
                                         const std::string& name)
        : Calc4Exception(position, CreateMessage(name))
    {
    }

private:
    static std::string CreateMessage(const std::string& name)
    {
        std::ostringstream oss;
        oss << "Operator or operand \"" << name << "\" is not defined";
        return oss.str();
    }
};

class DefinitionTextNotSplittedProperlyException : public Calc4Exception
{
public:
    DefinitionTextNotSplittedProperlyException(const std::optional<CharPosition>& position,
                                               const std::string& text)
        : Calc4Exception(position, CreateMessage(text))
    {
    }

private:
    static std::string CreateMessage(const std::string& text)
    {
        std::ostringstream oss;
        oss << "The following definition text is not splitted by two '|'s: \"" << text << "\"";
        return oss.str();
    }
};

class SomeOperandsMissingException : public Calc4Exception
{
public:
    SomeOperandsMissingException(const std::optional<CharPosition>& position)
        : Calc4Exception(position, CreateMessage())
    {
    }

private:
    static std::string CreateMessage()
    {
        return "Some operand(s) is missing";
    }
};

class TokenExpectedException : public Calc4Exception
{
public:
    TokenExpectedException(const std::optional<CharPosition>& position, const std::string& name)
        : Calc4Exception(position, CreateMessage(name))
    {
    }

private:
    static std::string CreateMessage(const std::string& name)
    {
        std::ostringstream oss;
        oss << "\"" << name << "\" is expected";
        return oss.str();
    }
};

class UnexpectedTokenException : public Calc4Exception
{
public:
    UnexpectedTokenException(const std::optional<CharPosition>& position, char token)
        : Calc4Exception(position, CreateMessage(token))
    {
    }

private:
    static std::string CreateMessage(char token)
    {
        std::ostringstream oss;
        oss << "Unexpected token \"" << token << "\"";
        return oss.str();
    }
};

class CodeIsEmptyException : public Calc4Exception
{
public:
    CodeIsEmptyException(const std::optional<CharPosition>& position)
        : Calc4Exception(position, CreateMessage())
    {
    }

private:
    static std::string CreateMessage()
    {
        return "Code is empty";
    }
};

class StackOverflowException : public Calc4Exception
{
public:
    StackOverflowException(const std::optional<CharPosition>& position)
        : Calc4Exception(position, CreateMessage())
    {
    }

private:
    static std::string CreateMessage()
    {
        return "Stack overflow";
    }
};

class ZeroDivisionException : public Calc4Exception
{
public:
    ZeroDivisionException(const std::optional<CharPosition>& position)
        : Calc4Exception(position, CreateMessage())
    {
    }

private:
    static std::string CreateMessage()
    {
        return "Zero division";
    }
};

class AssertionErrorException : public Calc4Exception
{
public:
    AssertionErrorException(const std::optional<CharPosition>& position,
                            std::string_view message = "")
        : Calc4Exception(position, CreateMessage(message))
    {
    }

private:
    static std::string CreateMessage(std::string_view message = "")
    {
        std::string text = "Assertion error (this is a bug of compiler)";

        if (!message.empty())
        {
            text += ": ";
            text += message;
        }

        return text;
    }
};
}
