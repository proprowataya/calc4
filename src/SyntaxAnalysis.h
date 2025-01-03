﻿/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2025 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include "Common.h"
#include "Operators.h"
#include <algorithm>
#include <cassert>
#include <numeric>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace calc4
{
class Token;
std::vector<std::shared_ptr<Token>> Lex(std::string_view text, CompilationContext& context);
std::shared_ptr<const Operator> Parse(const std::vector<std::shared_ptr<Token>>& tokens,
                                      CompilationContext& context);

/* ********** */

class Token
{
private:
    CharPosition position;

protected:
    Token(const CharPosition& position) : position(position) {}

public:
    const CharPosition& GetPosition() const
    {
        return position;
    }

    virtual int GetNumOperands() const = 0;
    virtual const std::string& GetSupplementaryText() const = 0;
    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const = 0;
    virtual ~Token() {}
};

#define MAKE_GET_SUPPLEMENTARY_TEXT                                                                \
    virtual const std::string& GetSupplementaryText() const override                               \
    {                                                                                              \
        return supplementaryText;                                                                  \
    }

#define MAKE_GET_NUM_OPERANDS(NUM_OPERANDS)                                                        \
    virtual int GetNumOperands() const override                                                    \
    {                                                                                              \
        return NUM_OPERANDS;                                                                       \
    }

class ArgumentToken : public Token
{
private:
    std::string name;
    int index;
    std::string supplementaryText;

public:
    ArgumentToken(const CharPosition& position, const std::string& name, int index,
                  const std::string& supplementaryText)
        : Token(position), name(name), index(index), supplementaryText(supplementaryText)
    {
    }

    const std::string GetName() const
    {
        return name;
    }

    int GetIndex() const
    {
        return index;
    }

    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const override
    {
        return OperandOperator::Create(index);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(0)
};

class DefineToken : public Token
{
private:
    std::string name;
    std::vector<std::string> arguments;
    std::vector<std::shared_ptr<Token>> tokens;
    std::string supplementaryText;

public:
    DefineToken(const CharPosition& position, const std::string& name,
                const std::vector<std::string>& arguments,
                const std::vector<std::shared_ptr<Token>>& tokens,
                const std::string& supplementaryText)
        : Token(position), name(name), arguments(arguments), tokens(tokens),
          supplementaryText(supplementaryText)
    {
    }

    const std::string GetName() const
    {
        return name;
    }

    const std::vector<std::string> GetArguments() const
    {
        return arguments;
    }

    const std::vector<std::shared_ptr<Token>> GetTokens() const
    {
        return tokens;
    }

    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const override
    {
        return DefineOperator::Create();
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(0)
};

class LoadVariableToken : public Token
{
private:
    std::string supplementaryText;

public:
    LoadVariableToken(const CharPosition& position, const std::string& supplementaryText)
        : Token(position), supplementaryText(supplementaryText)
    {
    }

    const std::string& GetVariableName() const
    {
        return supplementaryText;
    }

    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const override
    {
        return LoadVariableOperator::Create(supplementaryText);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(0)
};

class ParenthesisToken : public Token
{
private:
    std::vector<std::shared_ptr<Token>> tokens;
    std::string supplementaryText;

public:
    ParenthesisToken(const CharPosition& position,
                     const std::vector<std::shared_ptr<Token>>& tokens,
                     const std::string& supplementaryText)
        : Token(position), tokens(tokens), supplementaryText(supplementaryText)
    {
    }

    const std::vector<std::shared_ptr<Token>> GetTokens() const
    {
        return tokens;
    }

    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const override
    {
        return Parse(tokens, context);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(0)
};

class DecimalToken : public Token
{
private:
    int value;
    std::string supplementaryText;

public:
    DecimalToken(const CharPosition& position, int value, const std::string& supplementaryText)
        : Token(position), value(value), supplementaryText(supplementaryText)
    {
    }

    int GetValue() const
    {
        return value;
    }

    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const override
    {
        return DecimalOperator::Create(operands[0], value);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(1)
};

class StoreVariableToken : public Token
{
private:
    std::string supplementaryText;

public:
    StoreVariableToken(const CharPosition& position, const std::string& supplementaryText)
        : Token(position), supplementaryText(supplementaryText)
    {
    }

    const std::string& GetVariableName() const
    {
        return supplementaryText;
    }

    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const override
    {
        return StoreVariableOperator::Create(operands[0], supplementaryText);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(1)
};

class LoadArrayToken : public Token
{
private:
    std::string supplementaryText;

public:
    LoadArrayToken(const CharPosition& position, const std::string& supplementaryText)
        : Token(position), supplementaryText(supplementaryText)
    {
    }

    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const override
    {
        return LoadArrayOperator::Create(operands[0]);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(1)
};

class PrintCharToken : public Token
{
private:
    std::string supplementaryText;

public:
    PrintCharToken(const CharPosition& position, const std::string& supplementaryText)
        : Token(position), supplementaryText(supplementaryText)
    {
    }

    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const override
    {
        return PrintCharOperator::Create(operands[0]);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(1)
};

class InputToken : public Token
{
private:
    std::string supplementaryText;

public:
    InputToken(const CharPosition& position, const std::string& supplementaryText)
        : Token(position), supplementaryText(supplementaryText)
    {
    }

    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const override
    {
        return InputOperator::Create();
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(0)
};

class BinaryOperatorToken : public Token
{
private:
    BinaryType type;
    std::string supplementaryText;

public:
    BinaryOperatorToken(const CharPosition& position, BinaryType type,
                        const std::string& supplementaryText)
        : Token(position), type(type), supplementaryText(supplementaryText)
    {
    }

    BinaryType GetType() const
    {
        return type;
    }

    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const override
    {
        return BinaryOperator::Create(operands[0], operands[1], type);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(2)
};

class StoreArrayToken : public Token
{
private:
    std::string supplementaryText;

public:
    StoreArrayToken(const CharPosition& position, const std::string& supplementaryText)
        : Token(position), supplementaryText(supplementaryText)
    {
    }

    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const override
    {
        return StoreArrayOperator::Create(operands[0], operands[1]);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(2)
};

class ConditionalOperatorToken : public Token
{
private:
    std::string supplementaryText;

public:
    ConditionalOperatorToken(const CharPosition& position, const std::string& supplementaryText)
        : Token(position), supplementaryText(supplementaryText)
    {
    }

    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const override
    {
        return ConditionalOperator::Create(operands[0], operands[1], operands[2]);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(3)
};

class UserDefinedOperatorToken : public Token
{
private:
    OperatorDefinition definition;
    std::string supplementaryText;

public:
    UserDefinedOperatorToken(const CharPosition& position, const OperatorDefinition& definition,
                             const std::string& supplementaryText)
        : Token(position), definition(definition), supplementaryText(supplementaryText)
    {
    }

    const OperatorDefinition& GetDefinition() const
    {
        return definition;
    }

    virtual std::shared_ptr<const Operator> CreateOperator(
        const std::vector<std::shared_ptr<const Operator>>& operands,
        CompilationContext& context) const override
    {
        return UserDefinedOperator::Create(definition, operands);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS((definition.GetNumOperands()))
};
}
