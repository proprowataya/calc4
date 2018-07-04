#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <stack>
#include "Common.h"

#define STACK_ALLOC(TYPE, LENGTH) reinterpret_cast<TYPE *>(alloca(sizeof(TYPE) * (LENGTH)))

/* ********** */

class Operator;
class ZeroOperator;
class OperandOperator;
class DefineOperator;
class ParenthesisOperator;
class DecimalOperator;
class BinaryOperator;
class ConditionalOperator;
class UserDefinedOperator;

/* ********** */

class OperatorVisitor {
public:
    virtual void Visit(const ZeroOperator &op) = 0;
    virtual void Visit(const OperandOperator &op) = 0;
    virtual void Visit(const DefineOperator &op) = 0;
    virtual void Visit(const ParenthesisOperator &op) = 0;
    virtual void Visit(const DecimalOperator &op) = 0;
    virtual void Visit(const BinaryOperator &op) = 0;
    virtual void Visit(const ConditionalOperator &op) = 0;
    virtual void Visit(const UserDefinedOperator &op) = 0;
};

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

class OperatorImplement {
private:
    OperatorDefinition definition;
    std::shared_ptr<Operator> op;

public:
    OperatorImplement(const OperatorDefinition &definition, const std::shared_ptr<Operator> &op)
        : definition(definition), op(op) {}

    const OperatorDefinition &GetDefinition() const {
        return definition;
    }

    const std::shared_ptr<Operator> &GetOperator() const {
        return op;
    }
};

class CompilationContext {
private:
    std::unordered_map<std::string, OperatorImplement> userDefinedOperators;

public:
    void AddOperatorImplement(const OperatorImplement &implement) {
        auto p = userDefinedOperators.insert(std::make_pair(implement.GetDefinition().GetName(), implement));
        if (!p.second) {
            p.first->second = implement;
        }
    }

    const OperatorImplement &GetOperatorImplement(const std::string &name) const {
        return userDefinedOperators.at(name);
    }

    const OperatorImplement *TryGetOperatorImplement(const std::string &name) const {
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

class Operator {
public:
    virtual void Accept(OperatorVisitor &visitor) const = 0;
    virtual std::vector<std::shared_ptr<Operator>> GetOperands() const = 0;
    virtual std::string ToString() const = 0;
    virtual ~Operator() {}
};

#define MAKE_ACCEPT virtual void Accept(OperatorVisitor &visitor) const override {\
	visitor.Visit(*this);\
}

#define MAKE_GET_OPERANDS(...) virtual std::vector<std::shared_ptr<Operator>> GetOperands() const override {\
	return std::vector<std::shared_ptr<Operator>>({ __VA_ARGS__ });\
}

class ZeroOperator : public Operator {
public:
    virtual std::string ToString() const override {
        return std::string("ZeroOperator []");
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

class OperandOperator : public Operator {
private:
    int index;

public:
    OperandOperator(int index)
        : index(index) {}

    int GetIndex() const {
        return index;
    }

    virtual std::string ToString() const override {
        snprintf(snprintfBuffer, SnprintfBufferSize, "OperandOperator [Index = %d]", index);
        return std::string(snprintfBuffer);
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

class DefineOperator : public Operator {
public:
    virtual std::string ToString() const override {
        snprintf(snprintfBuffer, SnprintfBufferSize, "DefineOperator []");
        return std::string(snprintfBuffer);
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

class ParenthesisOperator : public Operator {
private:
    std::vector<std::shared_ptr<Operator>> operators;

public:
    ParenthesisOperator(const std::vector<std::shared_ptr<Operator>> &operators)
        : operators(operators) {}

    ParenthesisOperator(std::vector<std::shared_ptr<Operator>> &&operators)
        : operators(operators) {}

    const std::vector<std::shared_ptr<Operator>> &GetOperators() const {
        return operators;
    }

    virtual std::string ToString() const override {
        snprintf(snprintfBuffer, SnprintfBufferSize, "ParenthesisOperator [%d operators]", static_cast<int>(operators.size()));
        return std::string(snprintfBuffer);
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS()
};

class DecimalOperator : public Operator {
private:
    std::shared_ptr<Operator> operand;
    int value;

public:
    DecimalOperator(const std::shared_ptr<Operator> &operand, int value)
        : operand(operand), value(value) {}

    const std::shared_ptr<Operator> &GetOperand() const {
        return operand;
    }

    int GetValue() const {
        return value;
    }

    virtual std::string ToString() const override {
        snprintf(snprintfBuffer, SnprintfBufferSize, "DecimalOperator [Value = %d]", value);
        return std::string(snprintfBuffer);
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS(operand)
};

enum class BinaryType { Add, Sub, Mult, Div, Mod, Equal, NotEqual, LessThan, LessThanOrEqual, GreaterThanOrEqual, GreaterThan };

class BinaryOperator : public Operator {
private:
    std::shared_ptr<Operator> left, right;
    BinaryType type;

public:
    BinaryOperator(const std::shared_ptr<Operator> &left, const std::shared_ptr<Operator> &right, BinaryType type)
        : left(left), right(right), type(type) {}

    BinaryType GetType() const {
        return type;
    }

    const std::shared_ptr<Operator> &GetLeft() const {
        return left;
    }

    const std::shared_ptr<Operator> &GetRight() const {
        return right;
    }

    virtual std::string ToString() const override {
        static const char *BinaryTypeTable[] = { "Add", "Sub", "Mult", "Div", "Mod", "Equal", "NotEqual", "LessThan", "LessThanOrEqual", "GreaterThanOrEqual", "GreaterThan" };
        snprintf(snprintfBuffer, SnprintfBufferSize, "BinaryOperator [Type = %s]", BinaryTypeTable[(size_t)type]);
        return std::string(snprintfBuffer);
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS(left, right)
};

class ConditionalOperator : public Operator {
private:
    std::shared_ptr<Operator> condition, ifTrue, ifFalse;

public:
    ConditionalOperator(const std::shared_ptr<Operator> &condition, const std::shared_ptr<Operator> &ifTrue, const std::shared_ptr<Operator> &ifFalse)
        : condition(condition), ifTrue(ifTrue), ifFalse(ifFalse) {}

    const std::shared_ptr<Operator> &GetCondition() const {
        return condition;
    }

    const std::shared_ptr<Operator> &GetIfTrue() const {
        return ifTrue;
    }

    const std::shared_ptr<Operator> &GetIfFalse() const {
        return ifFalse;
    }

    virtual std::string ToString() const override {
        snprintf(snprintfBuffer, SnprintfBufferSize, "ConditionalOperator []");
        return std::string(snprintfBuffer);
    }

    MAKE_ACCEPT;
    MAKE_GET_OPERANDS(condition, ifTrue, ifFalse)
};

class UserDefinedOperator : public Operator {
private:
    OperatorDefinition definition;
    std::vector<std::shared_ptr<Operator>> operands;

public:
    UserDefinedOperator(const OperatorDefinition &definition, const std::vector<std::shared_ptr<Operator>> &operands)
        : definition(definition), operands(operands) {}

    UserDefinedOperator(const OperatorDefinition &definition, std::vector<std::shared_ptr<Operator>> &&operands)
        : definition(definition), operands(operands) {}

    const OperatorDefinition &GetDefinition() const {
        return definition;
    }

    // TODO:
    std::vector<std::shared_ptr<Operator>> GetOperands() const override {
        return operands;
    }

    virtual std::string ToString() const override {
        snprintf(snprintfBuffer, SnprintfBufferSize, "UserDefinedOperator [Name = %s, NumOperands = %d]", definition.GetName().c_str(), definition.GetNumOperands());
        return std::string(snprintfBuffer);
    }

    MAKE_ACCEPT;
    //MAKE_GET_OPERANDS()
};
