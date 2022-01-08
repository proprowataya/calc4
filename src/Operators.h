/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2022 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include "Common.h"
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <vector>

namespace calc4
{
class Operator;
class ZeroOperator;
class PrecomputedOperator;
class OperandOperator;
class DefineOperator;
class LoadVariableOperator;
class LoadArrayOperator;
class PrintCharOperator;
class ParenthesisOperator;
class DecimalOperator;
class StoreVariableOperator;
class StoreArrayOperator;
class BinaryOperator;
class ConditionalOperator;
class UserDefinedOperator;

/* ********** */

class OperatorVisitor
{
public:
    virtual void Visit(const std::shared_ptr<const ZeroOperator>& op) = 0;
    virtual void Visit(const std::shared_ptr<const PrecomputedOperator>& op) = 0;
    virtual void Visit(const std::shared_ptr<const OperandOperator>& op) = 0;
    virtual void Visit(const std::shared_ptr<const DefineOperator>& op) = 0;
    virtual void Visit(const std::shared_ptr<const LoadVariableOperator>& op) = 0;
    virtual void Visit(const std::shared_ptr<const LoadArrayOperator>& op) = 0;
    virtual void Visit(const std::shared_ptr<const PrintCharOperator>& op) = 0;
    virtual void Visit(const std::shared_ptr<const ParenthesisOperator>& op) = 0;
    virtual void Visit(const std::shared_ptr<const DecimalOperator>& op) = 0;
    virtual void Visit(const std::shared_ptr<const StoreVariableOperator>& op) = 0;
    virtual void Visit(const std::shared_ptr<const StoreArrayOperator>& op) = 0;
    virtual void Visit(const std::shared_ptr<const BinaryOperator>& op) = 0;
    virtual void Visit(const std::shared_ptr<const ConditionalOperator>& op) = 0;
    virtual void Visit(const std::shared_ptr<const UserDefinedOperator>& op) = 0;
    virtual ~OperatorVisitor() = default;
};

/* ********** */

class OperatorDefinition
{
private:
    std::string name;
    int numOperands;

public:
    OperatorDefinition(const std::string& name, int numOperands)
        : name(name), numOperands(numOperands)
    {
    }

    const std::string& GetName() const
    {
        return name;
    }

    int GetNumOperands() const
    {
        return numOperands;
    };

    bool operator==(const OperatorDefinition& other) const
    {
        return name == other.GetName() && numOperands == other.GetNumOperands();
    }
};

class OperatorImplement
{
private:
    OperatorDefinition definition;
    std::shared_ptr<const Operator> op;

public:
    OperatorImplement(const OperatorDefinition& definition,
                      const std::shared_ptr<const Operator>& op)
        : definition(definition), op(op)
    {
    }

    const OperatorDefinition& GetDefinition() const
    {
        return definition;
    }

    const std::shared_ptr<const Operator>& GetOperator() const
    {
        return op;
    }
};

class CompilationContext
{
private:
    std::map<std::string, OperatorImplement> userDefinedOperators;

public:
    void AddOperatorImplement(const OperatorImplement& implement)
    {
        auto p = userDefinedOperators.insert(
            std::make_pair(implement.GetDefinition().GetName(), implement));
        if (!p.second)
        {
            p.first->second = implement;
        }
    }

    const OperatorImplement& GetOperatorImplement(std::string_view name) const
    {
        // TODO: unnecessary allocation of string
        return userDefinedOperators.at(std::string(name));
    }

    const OperatorImplement* TryGetOperatorImplement(std::string_view name) const
    {
        // TODO: unnecessary allocation of string
        auto it = userDefinedOperators.find(std::string(name));
        if (it != userDefinedOperators.end())
        {
            return &(it->second);
        }
        else
        {
            return nullptr;
        }
    }

    decltype(userDefinedOperators.cbegin()) UserDefinedOperatorBegin() const
    {
        return userDefinedOperators.cbegin();
    }

    decltype(userDefinedOperators.cend()) UserDefinedOperatorEnd() const
    {
        return userDefinedOperators.cend();
    }
};

/* ********** */

class Operator
{
public:
    virtual void Accept(OperatorVisitor& visitor) const = 0;
    virtual std::vector<std::shared_ptr<const Operator>> GetOperands() const = 0;
    virtual std::string ToString() const = 0;
    virtual ~Operator() = default;
};

#define MAKE_ALLOCATE_HELPER(TYPE_NAME)                                                            \
    template<typename TBase>                                                                       \
    class AllocateHelper                                                                           \
    {                                                                                              \
        friend TYPE_NAME;                                                                          \
                                                                                                   \
        struct Object : public TBase                                                               \
        {                                                                                          \
            template<typename... Args>                                                             \
            Object(Args&&... args) : TBase(std::forward<Args>(args)...)                            \
            {                                                                                      \
            }                                                                                      \
        };                                                                                         \
                                                                                                   \
        template<typename... Args>                                                                 \
        static std::shared_ptr<Object> Allocate(Args&&... args)                                    \
        {                                                                                          \
            return std::make_shared<Object>(std::forward<Args>(args)...);                          \
        }                                                                                          \
    };                                                                                             \
                                                                                                   \
    friend class AllocateHelper<TYPE_NAME>

#define MAKE_ACCEPT                                                                                \
    virtual void Accept(OperatorVisitor& visitor) const override                                   \
    {                                                                                              \
        visitor.Visit(shared_from_this());                                                         \
    }

#define MAKE_GET_OPERANDS(...)                                                                     \
    virtual std::vector<std::shared_ptr<const Operator>> GetOperands() const override              \
    {                                                                                              \
        return std::vector<std::shared_ptr<const Operator>>({ __VA_ARGS__ });                      \
    }

class ZeroOperator : public Operator, public std::enable_shared_from_this<ZeroOperator>
{
private:
    ZeroOperator() {}

    MAKE_ALLOCATE_HELPER(ZeroOperator);

public:
    static std::shared_ptr<const ZeroOperator> Create()
    {
        return AllocateHelper<ZeroOperator>::Allocate();
    }

    virtual std::string ToString() const override
    {
        return "ZeroOperator []";
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

class PrecomputedOperator : public Operator,
                            public std::enable_shared_from_this<PrecomputedOperator>
{
private:
    AnyNumber value;

    template<typename TNumber>
    PrecomputedOperator(const TNumber& value) : value(value)
    {
    }

    MAKE_ALLOCATE_HELPER(PrecomputedOperator);

public:
    template<typename TNumber>
    static std::shared_ptr<const PrecomputedOperator> Create(const TNumber& value)
    {
        return AllocateHelper<PrecomputedOperator>::Allocate(value);
    }

    template<typename TNumber>
    const TNumber& GetValue() const
    {
        return value.GetValue<TNumber>();
    }

    virtual std::string ToString() const override
    {
        std::ostringstream oss;
        oss << "PrecomputedOperator [Value = " << value.ToString() << "]";
        return oss.str();
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

class OperandOperator : public Operator, public std::enable_shared_from_this<OperandOperator>
{
private:
    int index;

    OperandOperator(int index) : index(index) {}

    MAKE_ALLOCATE_HELPER(OperandOperator);

public:
    static std::shared_ptr<const OperandOperator> Create(int index)
    {
        return AllocateHelper<OperandOperator>::Allocate(index);
    }

    int GetIndex() const
    {
        return index;
    }

    virtual std::string ToString() const override
    {
        std::ostringstream oss;
        oss << "OperandOperator [Index = " << index << "]";
        return oss.str();
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

class DefineOperator : public Operator, public std::enable_shared_from_this<DefineOperator>
{
private:
    DefineOperator() {}

    MAKE_ALLOCATE_HELPER(DefineOperator);

public:
    static std::shared_ptr<const DefineOperator> Create()
    {
        return AllocateHelper<DefineOperator>::Allocate();
    }

    virtual std::string ToString() const override
    {
        return "DefineOperator []";
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

class LoadVariableOperator : public Operator,
                             public std::enable_shared_from_this<LoadVariableOperator>
{
private:
    std::string variableName;

    LoadVariableOperator(const std::string& variableName) : variableName(variableName) {}

    MAKE_ALLOCATE_HELPER(LoadVariableOperator);

public:
    static std::shared_ptr<const LoadVariableOperator> Create(const std::string& variableName)
    {
        return AllocateHelper<LoadVariableOperator>::Allocate(variableName);
    }

    const std::string& GetVariableName() const
    {
        return variableName;
    }

    virtual std::string ToString() const override
    {
        return "LoadVariableOperator [VariableName = \"" + variableName + "\"]";
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

class LoadArrayOperator : public Operator, public std::enable_shared_from_this<LoadArrayOperator>
{
private:
    std::shared_ptr<const Operator> index;

    LoadArrayOperator(const std::shared_ptr<const Operator>& index) : index(index) {}

    MAKE_ALLOCATE_HELPER(LoadArrayOperator);

public:
    static std::shared_ptr<const LoadArrayOperator> Create(
        const std::shared_ptr<const Operator>& index)
    {
        return AllocateHelper<LoadArrayOperator>::Allocate(index);
    }

    const std::shared_ptr<const Operator>& GetIndex() const
    {
        return index;
    }

    virtual std::string ToString() const override
    {
        return "LoadArrayOperator []";
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS(index)
};

class PrintCharOperator : public Operator, public std::enable_shared_from_this<PrintCharOperator>
{
private:
    std::shared_ptr<const Operator> character;

    PrintCharOperator(const std::shared_ptr<const Operator>& character) : character(character) {}

    MAKE_ALLOCATE_HELPER(PrintCharOperator);

public:
    static std::shared_ptr<const PrintCharOperator> Create(
        const std::shared_ptr<const Operator>& character)
    {
        return AllocateHelper<PrintCharOperator>::Allocate(character);
    }

    const std::shared_ptr<const Operator>& GetCharacter() const
    {
        return character;
    }

    virtual std::string ToString() const override
    {
        return "PrintCharOperator []";
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS(character)
};

class ParenthesisOperator : public Operator,
                            public std::enable_shared_from_this<ParenthesisOperator>
{
private:
    std::vector<std::shared_ptr<const Operator>> operators;

    ParenthesisOperator(const std::vector<std::shared_ptr<const Operator>>& operators)
        : operators(operators)
    {
    }

    MAKE_ALLOCATE_HELPER(ParenthesisOperator);

public:
    static std::shared_ptr<const ParenthesisOperator> Create(
        const std::vector<std::shared_ptr<const Operator>>& operators)
    {
        return AllocateHelper<ParenthesisOperator>::Allocate(operators);
    }

    const std::vector<std::shared_ptr<const Operator>>& GetOperators() const
    {
        return operators;
    }

    virtual std::string ToString() const override
    {
        std::ostringstream oss;
        oss << "ParenthesisOperator [" << operators.size() << " operators]";
        return oss.str();
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

class DecimalOperator : public Operator, public std::enable_shared_from_this<DecimalOperator>
{
private:
    std::shared_ptr<const Operator> operand;
    int value;

    DecimalOperator(const std::shared_ptr<const Operator>& operand, int value)
        : operand(operand), value(value)
    {
    }

    MAKE_ALLOCATE_HELPER(DecimalOperator);

public:
    static std::shared_ptr<const DecimalOperator> Create(
        const std::shared_ptr<const Operator>& operand, int value)
    {
        return AllocateHelper<DecimalOperator>::Allocate(operand, value);
    }

    const std::shared_ptr<const Operator>& GetOperand() const
    {
        return operand;
    }

    int GetValue() const
    {
        return value;
    }

    virtual std::string ToString() const override
    {
        std::ostringstream oss;
        oss << "DecimalOperator [Value = " << value << "]";
        return oss.str();
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS(operand)
};

class StoreVariableOperator : public Operator,
                              public std::enable_shared_from_this<StoreVariableOperator>
{
private:
    std::shared_ptr<const Operator> operand;
    std::string variableName;

    StoreVariableOperator(const std::shared_ptr<const Operator>& operand,
                          const std::string& variableName)
        : operand(operand), variableName(variableName)
    {
    }

    MAKE_ALLOCATE_HELPER(StoreVariableOperator);

public:
    static std::shared_ptr<const StoreVariableOperator> Create(
        const std::shared_ptr<const Operator>& operand, const std::string& variableName)
    {
        return AllocateHelper<StoreVariableOperator>::Allocate(operand, variableName);
    }

    const std::shared_ptr<const Operator>& GetOperand() const
    {
        return operand;
    }

    const std::string& GetVariableName() const
    {
        return variableName;
    }

    virtual std::string ToString() const override
    {
        return "StoreVariableOperator [VariableName = " + variableName + "]";
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS(operand)
};

class StoreArrayOperator : public Operator, public std::enable_shared_from_this<StoreArrayOperator>
{
private:
    std::shared_ptr<const Operator> value, index;

    StoreArrayOperator(const std::shared_ptr<const Operator>& value,
                       const std::shared_ptr<const Operator>& index)
        : value(value), index(index)
    {
    }

    MAKE_ALLOCATE_HELPER(StoreArrayOperator);

public:
    static std::shared_ptr<const StoreArrayOperator> Create(
        const std::shared_ptr<const Operator>& value, const std::shared_ptr<const Operator>& index)
    {
        return AllocateHelper<StoreArrayOperator>::Allocate(value, index);
    }

    const std::shared_ptr<const Operator>& GetValue() const
    {
        return value;
    }

    const std::shared_ptr<const Operator>& GetIndex() const
    {
        return index;
    }

    virtual std::string ToString() const override
    {
        return "StoreArrayOperator []";
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS(value, index)
};

enum class BinaryType
{
    Add,
    Sub,
    Mult,
    Div,
    Mod,
    Equal,
    NotEqual,
    LessThan,
    LessThanOrEqual,
    GreaterThanOrEqual,
    GreaterThan
};

class BinaryOperator : public Operator, public std::enable_shared_from_this<BinaryOperator>
{
private:
    std::shared_ptr<const Operator> left, right;
    BinaryType type;

    BinaryOperator(const std::shared_ptr<const Operator>& left,
                   const std::shared_ptr<const Operator>& right, BinaryType type)
        : left(left), right(right), type(type)
    {
    }

    MAKE_ALLOCATE_HELPER(BinaryOperator);

public:
    static std::shared_ptr<const BinaryOperator> Create(
        const std::shared_ptr<const Operator>& left, const std::shared_ptr<const Operator>& right,
        BinaryType type)
    {
        return AllocateHelper<BinaryOperator>::Allocate(left, right, type);
    }

    BinaryType GetType() const
    {
        return type;
    }

    const std::shared_ptr<const Operator>& GetLeft() const
    {
        return left;
    }

    const std::shared_ptr<const Operator>& GetRight() const
    {
        return right;
    }

    virtual std::string ToString() const override
    {
        static const char* BinaryTypeTable[] = { "Add",
                                                 "Sub",
                                                 "Mult",
                                                 "Div",
                                                 "Mod",
                                                 "Equal",
                                                 "NotEqual",
                                                 "LessThan",
                                                 "LessThanOrEqual",
                                                 "GreaterThanOrEqual",
                                                 "GreaterThan" };
        std::ostringstream oss;
        oss << "BinaryOperator [Type = " << BinaryTypeTable[(size_t)type] << "]";
        return oss.str();
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS(left, right)
};

class ConditionalOperator : public Operator,
                            public std::enable_shared_from_this<ConditionalOperator>
{
private:
    std::shared_ptr<const Operator> condition, ifTrue, ifFalse;

    ConditionalOperator(const std::shared_ptr<const Operator>& condition,
                        const std::shared_ptr<const Operator>& ifTrue,
                        const std::shared_ptr<const Operator>& ifFalse)
        : condition(condition), ifTrue(ifTrue), ifFalse(ifFalse)
    {
    }

    MAKE_ALLOCATE_HELPER(ConditionalOperator);

public:
    static std::shared_ptr<const ConditionalOperator> Create(
        const std::shared_ptr<const Operator>& condition,
        const std::shared_ptr<const Operator>& ifTrue,
        const std::shared_ptr<const Operator>& ifFalse)
    {
        return AllocateHelper<ConditionalOperator>::Allocate(condition, ifTrue, ifFalse);
    }

    const std::shared_ptr<const Operator>& GetCondition() const
    {
        return condition;
    }

    const std::shared_ptr<const Operator>& GetIfTrue() const
    {
        return ifTrue;
    }

    const std::shared_ptr<const Operator>& GetIfFalse() const
    {
        return ifFalse;
    }

    virtual std::string ToString() const override
    {
        return "ConditionalOperator []";
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS(condition, ifTrue, ifFalse)
};

class UserDefinedOperator : public Operator,
                            public std::enable_shared_from_this<UserDefinedOperator>
{
private:
    OperatorDefinition definition;
    std::vector<std::shared_ptr<const Operator>> operands;
    std::optional<bool> isTailCall;

    UserDefinedOperator(const OperatorDefinition& definition,
                        const std::vector<std::shared_ptr<const Operator>>& operands,
                        std::optional<bool> isTailCall = std::nullopt)
        : definition(definition), operands(operands), isTailCall(isTailCall)
    {
    }

    MAKE_ALLOCATE_HELPER(UserDefinedOperator);

public:
    static std::shared_ptr<const UserDefinedOperator> Create(
        const OperatorDefinition& definition,
        const std::vector<std::shared_ptr<const Operator>>& operands,
        std::optional<bool> isTailCall = std::nullopt)
    {
        return AllocateHelper<UserDefinedOperator>::Allocate(definition, operands, isTailCall);
    }

    const OperatorDefinition& GetDefinition() const
    {
        return definition;
    }

    std::optional<bool> IsTailCall() const
    {
        return isTailCall;
    }

    // TODO:
    std::vector<std::shared_ptr<const Operator>> GetOperands() const override
    {
        return operands;
    }

    virtual std::string ToString() const override
    {
        std::ostringstream oss;
        oss << "UserDefinedOperator [Name = " << definition.GetName()
            << ", NumOperands = " << definition.GetNumOperands()
            << ", IsTailCall = " << (isTailCall ? (*isTailCall ? "True" : "False") : "Unknown")
            << "]";
        return oss.str();
    }

    MAKE_ACCEPT;
    // MAKE_GET_OPERANDS()
};
}
