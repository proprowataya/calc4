#pragma once

#include "Common.h"
#include "Operators.h"
#include <algorithm>
#include <cassert>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

/* ********** */

class Token;
std::vector<std::shared_ptr<Token>> Lex(const std::string& text, CompilationContext& context);
std::shared_ptr<Operator> Parse(const std::vector<std::shared_ptr<Token>>& tokens,
                                CompilationContext& context);

/* ********** */

class Token
{
public:
    virtual int GetNumOperands() const = 0;
    virtual const std::string& GetSupplementaryText() const = 0;
    virtual std::shared_ptr<Operator> CreateOperator(
        const std::vector<std::shared_ptr<Operator>>& operands,
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
    ArgumentToken(const std::string& name, int index, const std::string& supplementaryText)
        : name(name), index(index), supplementaryText(supplementaryText)
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

    virtual std::shared_ptr<Operator> CreateOperator(
        const std::vector<std::shared_ptr<Operator>>& operands,
        CompilationContext& context) const override
    {
        return std::make_shared<OperandOperator>(index);
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
    DefineToken(const std::string& name, const std::vector<std::string>& arguments,
                const std::vector<std::shared_ptr<Token>>& tokens,
                const std::string& supplementaryText)
        : name(name), arguments(arguments), tokens(tokens), supplementaryText(supplementaryText)
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

    virtual std::shared_ptr<Operator> CreateOperator(
        const std::vector<std::shared_ptr<Operator>>& operands,
        CompilationContext& context) const override
    {
        return std::make_shared<DefineOperator>();
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
    ParenthesisToken(const std::vector<std::shared_ptr<Token>>& tokens,
                     const std::string& supplementaryText)
        : tokens(tokens), supplementaryText(supplementaryText)
    {
    }

    const std::vector<std::shared_ptr<Token>> GetTokens() const
    {
        return tokens;
    }

    virtual std::shared_ptr<Operator> CreateOperator(
        const std::vector<std::shared_ptr<Operator>>& operands,
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
    DecimalToken(int value, const std::string& supplementaryText)
        : value(value), supplementaryText(supplementaryText)
    {
    }

    int GetValue() const
    {
        return value;
    }

    virtual std::shared_ptr<Operator> CreateOperator(
        const std::vector<std::shared_ptr<Operator>>& operands,
        CompilationContext& context) const override
    {
        return std::make_shared<DecimalOperator>(operands[0], value);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(1)
};

class BinaryOperatorToken : public Token
{
private:
    BinaryType type;
    std::string supplementaryText;

public:
    BinaryOperatorToken(BinaryType type, const std::string& supplementaryText)
        : type(type), supplementaryText(supplementaryText)
    {
    }

    BinaryType GetType() const
    {
        return type;
    }

    virtual std::shared_ptr<Operator> CreateOperator(
        const std::vector<std::shared_ptr<Operator>>& operands,
        CompilationContext& context) const override
    {
        return std::make_shared<BinaryOperator>(operands[0], operands[1], type);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS(2)
};

class ConditionalOperatorToken : public Token
{
private:
    std::string supplementaryText;

public:
    ConditionalOperatorToken(const std::string& supplementaryText)
        : supplementaryText(supplementaryText)
    {
    }

    virtual std::shared_ptr<Operator> CreateOperator(
        const std::vector<std::shared_ptr<Operator>>& operands,
        CompilationContext& context) const override
    {
        return std::make_shared<ConditionalOperator>(operands[0], operands[1], operands[2]);
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
    UserDefinedOperatorToken(const OperatorDefinition& definition,
                             const std::string& supplementaryText)
        : definition(definition), supplementaryText(supplementaryText)
    {
    }

    const OperatorDefinition& GetDefinition() const
    {
        return definition;
    }

    virtual std::shared_ptr<Operator> CreateOperator(
        const std::vector<std::shared_ptr<Operator>>& operands,
        CompilationContext& context) const override
    {
        return std::make_shared<UserDefinedOperator>(definition, operands);
    }

    MAKE_GET_SUPPLEMENTARY_TEXT;
    MAKE_GET_NUM_OPERANDS((definition.GetNumOperands()))
};
