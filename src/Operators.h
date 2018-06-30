#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <stack>
#include "Common.h"

#define STACK_ALLOC(TYPE, LENGTH) reinterpret_cast<TYPE *>(alloca(sizeof(TYPE) * (LENGTH)))

template<typename TNumber>
class Operator;

template<typename TNumber>
class OperatorVisitor;

/* ********** */

class OperatorDefinition {
private:
    std::string name;
    int numOperands;

public:
    OperatorDefinition(const std::string &name, int numOperands)
        : name(name), numOperands(numOperands) {}

    const std::string &GetName() const {
        return name;
    }

    int GetNumOperands() const {
        return numOperands;
    }
};

template<typename TNumber>
class OperatorImplement {
private:
    OperatorDefinition definition;
    std::shared_ptr<Operator<TNumber>> op;

public:
    OperatorImplement(const OperatorDefinition &definition, const std::shared_ptr<Operator<TNumber>> &op)
        : definition(definition), op(op) {}

    const OperatorDefinition &GetDefinition() const {
        return definition;
    }

    const std::shared_ptr<Operator<TNumber>> &GetOperator() const {
        return op;
    }
};

template<typename TNumber>
class CompilationContext {
private:
    std::unordered_map<std::string, OperatorImplement<TNumber>> userDefinedOperators;

public:
    void AddOperatorImplement(const OperatorImplement<TNumber> &implement) {
        auto p = userDefinedOperators.insert(std::make_pair(implement.GetDefinition().GetName(), implement));
        if (!p.second) {
            p.first->second = implement;
        }
    }

    const OperatorImplement<TNumber> &GetOperatorImplement(const std::string &name) const {
        return userDefinedOperators.at(name);
    }

    const OperatorImplement<TNumber> *TryGetOperatorImplement(const std::string &name) const {
        auto it = userDefinedOperators.find(name);
        if (it != userDefinedOperators.end()) {
            return &(it->second);
        } else {
            return nullptr;
        }
    }

    decltype(userDefinedOperators.cbegin()) UserDefinedOperatorBegin() const {
        return userDefinedOperators.cbegin();
    }

    decltype(userDefinedOperators.cend()) UserDefinedOperatorEnd() const {
        return userDefinedOperators.cend();
    }
};

/* ********** */

template<typename TNumber>
class Operator {
public:
    virtual void Accept(OperatorVisitor<TNumber> &visitor) const = 0;
    virtual std::vector<std::shared_ptr<Operator<TNumber>>> GetOperands() const = 0;
    virtual std::string ToString() const = 0;
    virtual ~Operator() {}
};

#define MAKE_ACCEPT virtual void Accept(OperatorVisitor<TNumber> &visitor) const override {\
	visitor.Visit(*this);\
}

#define MAKE_GET_OPERANDS(...) virtual std::vector<std::shared_ptr<Operator<TNumber>>> GetOperands() const override {\
	return std::vector<std::shared_ptr<Operator<TNumber>>>({ __VA_ARGS__ });\
}

template<typename TNumber>
class ZeroOperator : public Operator<TNumber> {
public:
    virtual std::string ToString() const override {
        return std::string("ZeroOperator []");
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

template<typename TNumber>
class PreComputedOperator : public Operator<TNumber> {
private:
    TNumber value;

public:
    PreComputedOperator(TNumber value)
        : value(value) {}

    TNumber GetValue() const {
        return value;
    }

    virtual std::string ToString() const override {
        sprintf(sprintfBuffer, "PreComputedOperator [Value = %s]", std::to_string(value));
        return std::string(sprintfBuffer);
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

template<typename TNumber>
class ArgumentOperator : public Operator<TNumber> {
private:
    int index;

public:
    ArgumentOperator(int index)
        : index(index) {}

    int GetIndex() const {
        return index;
    }

    virtual std::string ToString() const override {
        sprintf(sprintfBuffer, "ArgumentOperator [Index = %d]", index);
        return std::string(sprintfBuffer);
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

template<typename TNumber>
class DefineOperator : public Operator<TNumber> {
public:
    virtual std::string ToString() const override {
        sprintf(sprintfBuffer, "DefineOperator []");
        return std::string(sprintfBuffer);
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

template<typename TNumber>
class ParenthesisOperator : public Operator<TNumber> {
private:
    std::vector<std::shared_ptr<Operator<TNumber>>> operators;

public:
    ParenthesisOperator(const std::vector<std::shared_ptr<Operator<TNumber>>> &operators)
        : operators(operators) {}

    ParenthesisOperator(std::vector<std::shared_ptr<Operator<TNumber>>> &&operators)
        : operators(operators) {}

    const std::vector<std::shared_ptr<Operator<TNumber>>> &GetOperators() const {
        return operators;
    }

    virtual std::string ToString() const override {
        sprintf(sprintfBuffer, "ParenthesisOperator [%d operators]", static_cast<int>(operators.size()));
        return std::string(sprintfBuffer);
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

template<typename TNumber>
class DecimalOperator : public Operator<TNumber> {
private:
    std::shared_ptr<Operator<TNumber>> operand;
    int value;

public:
    DecimalOperator(const std::shared_ptr<Operator<TNumber>> &operand, int value)
        : operand(operand), value(value) {}

    const std::shared_ptr<Operator<TNumber>> &GetOperand() const {
        return operand;
    }

    int GetValue() const {
        return value;
    }

    virtual std::string ToString() const override {
        sprintf(sprintfBuffer, "DecimalOperator [Value = %d]", value);
        return std::string(sprintfBuffer);
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS(operand)
};

enum class BinaryType { Add, Sub, Mult, Div, Mod, Equal, NotEqual, LessThan, LessThanOrEqual, GreaterThanOrEqual, GreaterThan };

template<typename TNumber>
class BinaryOperator : public Operator<TNumber> {
private:
    std::shared_ptr<Operator<TNumber>> left, right;
    BinaryType type;

public:
    BinaryOperator(const std::shared_ptr<Operator<TNumber>> &left, const std::shared_ptr<Operator<TNumber>> &right, BinaryType type)
        : left(left), right(right), type(type) {}

    BinaryType GetType() const {
        return type;
    }

    const std::shared_ptr<Operator<TNumber>> &GetLeft() const {
        return left;
    }

    const std::shared_ptr<Operator<TNumber>> &GetRight() const {
        return right;
    }

    virtual std::string ToString() const override {
        static const char *BinaryTypeTable[] = { "Add", "Sub", "Mult", "Div", "Mod", "Equal", "NotEqual", "LessThan", "LessThanOrEqual", "GreaterThanOrEqual", "GreaterThan" };
        sprintf(sprintfBuffer, "BinaryOperator [Type = %s]", BinaryTypeTable[(size_t)type]);
        return std::string(sprintfBuffer);
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS(left, right)
};

template<typename TNumber>
class ConditionalOperator : public Operator<TNumber> {
private:
    std::shared_ptr<Operator<TNumber>> condition, ifTrue, ifFalse;

public:
    ConditionalOperator(const std::shared_ptr<Operator<TNumber>> &condition, const std::shared_ptr<Operator<TNumber>> &ifTrue, const std::shared_ptr<Operator<TNumber>> &ifFalse)
        : condition(condition), ifTrue(ifTrue), ifFalse(ifFalse) {}

    const std::shared_ptr<Operator<TNumber>> &GetCondition() const {
        return condition;
    }

    const std::shared_ptr<Operator<TNumber>> &GetIfTrue() const {
        return ifTrue;
    }

    const std::shared_ptr<Operator<TNumber>> &GetIfFalse() const {
        return ifFalse;
    }

    virtual std::string ToString() const override {
        sprintf(sprintfBuffer, "ConditionalOperator []");
        return std::string(sprintfBuffer);
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS(condition, ifTrue, ifFalse)
};

template<typename TNumber>
class UserDefinedOperator : public Operator<TNumber> {
private:
    OperatorDefinition definition;
    std::vector<std::shared_ptr<Operator<TNumber>>> operands;

public:
    UserDefinedOperator(const OperatorDefinition &definition, const std::vector<std::shared_ptr<Operator<TNumber>>> &operands)
        : definition(definition), operands(operands) {}

    UserDefinedOperator(const OperatorDefinition &definition, std::vector<std::shared_ptr<Operator<TNumber>>> &&operands)
        : definition(definition), operands(operands) {}

    const OperatorDefinition &GetDefinition() const {
        return definition;
    }

    // TODO:
    std::vector<std::shared_ptr<Operator<TNumber>>> GetOperands() const override {
        return operands;
    }

    virtual std::string ToString() const override {
        sprintf(sprintfBuffer, "UserDefinedOperator [Name = %s, NumOperands = %d]", definition.GetName().c_str(), definition.GetNumOperands());
        return std::string(sprintfBuffer);
    }

    MAKE_ACCEPT;
    //MAKE_GET_OPERANDS()
};

/* ********** */

template<typename TNumber>
class OperatorVisitor {
public:
    virtual void Visit(const ZeroOperator<TNumber> &op) = 0;
    virtual void Visit(const PreComputedOperator<TNumber> &op) = 0;
    virtual void Visit(const ArgumentOperator<TNumber> &op) = 0;
    virtual void Visit(const DefineOperator<TNumber> &op) = 0;
    virtual void Visit(const ParenthesisOperator<TNumber> &op) = 0;
    virtual void Visit(const DecimalOperator<TNumber> &op) = 0;
    virtual void Visit(const BinaryOperator<TNumber> &op) = 0;
    virtual void Visit(const ConditionalOperator<TNumber> &op) = 0;
    virtual void Visit(const UserDefinedOperator<TNumber> &op) = 0;
};

template<typename TNumber>
class Evaluator : public OperatorVisitor<TNumber> {
private:
    const CompilationContext<TNumber> *context;
    std::stack<TNumber *> arguments;

public:
    TNumber value;

    Evaluator(const CompilationContext<TNumber> *context)
        : context(context) {}

    virtual void Visit(const ZeroOperator<TNumber> &op) override {
        value = 0;
    }

    virtual void Visit(const PreComputedOperator<TNumber> &op) override {
        value = op.GetValue();
    };

    virtual void Visit(const ArgumentOperator<TNumber> &op) override {
        value = arguments.top()[op.GetIndex()];
    };

    virtual void Visit(const DefineOperator<TNumber> &op) override {
        value = 0;
    };

    virtual void Visit(const ParenthesisOperator<TNumber> &op) override {
        value = 0;

        for (auto& item : op.GetOperators()) {
            item->Accept(*this);
        }
    }

    virtual void Visit(const DecimalOperator<TNumber> &op) override {
        op.GetOperand()->Accept(*this);
        value = value * 10 + op.GetValue();
    }

    virtual void Visit(const BinaryOperator<TNumber> &op) override {
        op.GetLeft()->Accept(*this);
        auto left = value;
        op.GetRight()->Accept(*this);
        auto right = value;

        switch (op.GetType()) {
        case BinaryType::Add:
            value = left + right;
            break;
        case BinaryType::Sub:
            value = left - right;
            break;
        case BinaryType::Mult:
            value = left * right;
            break;
        case BinaryType::Div:
            value = left / right;
            break;
        case BinaryType::Mod:
            value = left % right;
            break;
        case BinaryType::Equal:
            value = left == right ? 1 : 0;
            break;
        case BinaryType::NotEqual:
            value = left != right ? 1 : 0;
            break;
        case BinaryType::LessThan:
            value = left < right ? 1 : 0;
            break;
        case BinaryType::LessThanOrEqual:
            value = left <= right ? 1 : 0;
            break;
        case BinaryType::GreaterThanOrEqual:
            value = left >= right ? 1 : 0;
            break;
        case BinaryType::GreaterThan:
            value = left > right ? 1 : 0;
            break;
        default:
            UNREACHABLE();
            break;
        }
    }

    virtual void Visit(const ConditionalOperator<TNumber> &op) override {
        op.GetCondition()->Accept(*this);

        if (value != 0) {
            op.GetIfTrue()->Accept(*this);
        } else {
            op.GetIfFalse()->Accept(*this);
        }
    };

    virtual void Visit(const UserDefinedOperator<TNumber> &op) override {
        TNumber *arg = STACK_ALLOC(TNumber, op.GetDefinition().GetNumOperands());

        auto operands = op.GetOperands();
        for (size_t i = 0; i < operands.size(); i++) {
            operands[i]->Accept(*this);
            arg[i] = value;
        }

        arguments.push(arg);
        context->GetOperatorImplement(op.GetDefinition().GetName()).GetOperator()->Accept(*this);
        arguments.pop();
    }
};
