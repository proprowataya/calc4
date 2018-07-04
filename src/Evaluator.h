#pragma once

#include <gmpxx.h>
#include "Operators.h"

template<typename TNumber>
class Evaluator : public OperatorVisitor {
private:
    const CompilationContext *context;
    std::stack<TNumber *> arguments;

public:
    TNumber value;

    Evaluator(const CompilationContext *context)
        : context(context) {}

    virtual void Visit(const ZeroOperator &op) override {
        value = 0;
    }

    virtual void Visit(const OperandOperator &op) override {
        value = arguments.top()[op.GetIndex()];
    };

    virtual void Visit(const DefineOperator &op) override {
        value = 0;
    };

    virtual void Visit(const ParenthesisOperator &op) override {
        value = 0;

        for (auto& item : op.GetOperators()) {
            item->Accept(*this);
        }
    }

    virtual void Visit(const DecimalOperator &op) override {
        op.GetOperand()->Accept(*this);
        value = value * 10 + op.GetValue();
    }

    virtual void Visit(const BinaryOperator &op) override {
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

    virtual void Visit(const ConditionalOperator &op) override {
        op.GetCondition()->Accept(*this);

        if (value != 0) {
            op.GetIfTrue()->Accept(*this);
        } else {
            op.GetIfFalse()->Accept(*this);
        }
    };

    virtual void Visit(const UserDefinedOperator &op) override {
        size_t size = op.GetDefinition().GetNumOperands();
        TNumber *arg = std::is_same<TNumber, mpz_class>::value ? new TNumber[size] : STACK_ALLOC(TNumber, size);

        auto operands = op.GetOperands();
        for (size_t i = 0; i < size; i++) {
            operands[i]->Accept(*this);
            arg[i] = value;
        }

        arguments.push(arg);
        context->GetOperatorImplement(op.GetDefinition().GetName()).GetOperator()->Accept(*this);
        arguments.pop();

        if (std::is_same<TNumber, mpz_class>::value) {
            delete[] arg;
        }
    }
};
