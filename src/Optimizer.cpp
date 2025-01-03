﻿/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2025 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "Optimizer.h"
#include "Operators.h"
#include <cstdint>
#include <stack>

#ifdef ENABLE_GMP
#include <gmpxx.h>
#endif // ENABLE_GMP

namespace calc4
{
namespace
{
template<typename TNumber>
class PrecomputeVisitor : public OperatorVisitor
{
private:
    CompilationContext& context;

    std::shared_ptr<const Operator> Precompute(const std::shared_ptr<const Operator>& op)
    {
        op->Accept(*this);
        return value;
    }

    bool TryGetPrecomputedValue(const std::shared_ptr<const Operator>& op, TNumber* dest)
    {
        if (auto precomputed = std::dynamic_pointer_cast<const PrecomputedOperator>(op))
        {
            *dest = precomputed->GetValue<TNumber>();
            return true;
        }
        else
        {
            return false;
        }
    }

public:
    std::shared_ptr<const Operator> value;

    PrecomputeVisitor(CompilationContext& context) : context(context) {}

    virtual void Visit(const std::shared_ptr<const ZeroOperator>& op) override
    {
        value = PrecomputedOperator::Create(static_cast<TNumber>(0));
    };

    virtual void Visit(const std::shared_ptr<const PrecomputedOperator>& op) override
    {
        value = op;
    };

    virtual void Visit(const std::shared_ptr<const OperandOperator>& op) override
    {
        value = op;
    };

    virtual void Visit(const std::shared_ptr<const DefineOperator>& op) override
    {
        value = PrecomputedOperator::Create(static_cast<TNumber>(0));
    };

    virtual void Visit(const std::shared_ptr<const LoadVariableOperator>& op) override
    {
        value = op;
    };

    virtual void Visit(const std::shared_ptr<const LoadArrayOperator>& op) override
    {
        std::shared_ptr<const Operator> index = Precompute(op->GetIndex());
        value = LoadArrayOperator::Create(index);
    };

    virtual void Visit(const std::shared_ptr<const InputOperator>& op) override
    {
        value = op;
    };

    virtual void Visit(const std::shared_ptr<const PrintCharOperator>& op) override
    {
        std::shared_ptr<const Operator> character = Precompute(op->GetCharacter());
        value = PrintCharOperator::Create(character);
    };

    virtual void Visit(const std::shared_ptr<const ParenthesisOperator>& op) override
    {
        std::vector<std::shared_ptr<const Operator>> optimized;
        bool allPrecomputed = true;

        for (auto& op2 : op->GetOperators())
        {
            std::shared_ptr<const Operator> precomputed = Precompute(op2);
            optimized.push_back(precomputed);

            if (dynamic_cast<const PrecomputedOperator*>(precomputed.get()) == nullptr)
            {
                allPrecomputed = false;
            }
        }

        if (allPrecomputed)
        {
            value = optimized.back();
        }
        else
        {
            value = ParenthesisOperator::Create(std::move(optimized));
        }
    };

    virtual void Visit(const std::shared_ptr<const DecimalOperator>& op) override
    {
        std::shared_ptr<const Operator> precomputed = Precompute(op->GetOperand());
        TNumber precomputedValue;
        if (TryGetPrecomputedValue(precomputed, &precomputedValue))
        {
            value = PrecomputedOperator::Create(
                static_cast<TNumber>(precomputedValue * 10 + op->GetValue()));
        }
        else
        {
            value = DecimalOperator::Create(precomputed, op->GetValue());
        }
    };

    virtual void Visit(const std::shared_ptr<const StoreVariableOperator>& op) override
    {
        std::shared_ptr<const Operator> operand = Precompute(op->GetOperand());
        value = StoreVariableOperator::Create(operand, op->GetVariableName());
    }

    virtual void Visit(const std::shared_ptr<const StoreArrayOperator>& op) override
    {
        std::shared_ptr<const Operator> valueToBeStored = Precompute(op->GetValue());
        std::shared_ptr<const Operator> index = Precompute(op->GetIndex());
        value = StoreArrayOperator::Create(valueToBeStored, index);
    }

    virtual void Visit(const std::shared_ptr<const BinaryOperator>& op) override
    {
        std::shared_ptr<const Operator> left = Precompute(op->GetLeft());
        std::shared_ptr<const Operator> right = Precompute(op->GetRight());

        TNumber leftValue, rightValue;
        if (TryGetPrecomputedValue(left, &leftValue) &&
            TryGetPrecomputedValue(right, &rightValue) &&
            !((op->GetType() == BinaryType::Div || op->GetType() == BinaryType::Mod) &&
              rightValue == 0))
        {
            TNumber result;

            switch (op->GetType())
            {
            case BinaryType::Add:
                result = leftValue + rightValue;
                break;
            case BinaryType::Sub:
                result = leftValue - rightValue;
                break;
            case BinaryType::Mult:
                result = leftValue * rightValue;
                break;
            case BinaryType::Div:
                result = leftValue / rightValue;
                break;
            case BinaryType::Mod:
                result = leftValue % rightValue;
                break;
            case BinaryType::Equal:
                result = (leftValue == rightValue) ? 1 : 0;
                break;
            case BinaryType::NotEqual:
                result = (leftValue != rightValue) ? 1 : 0;
                break;
            case BinaryType::LessThan:
                result = (leftValue < rightValue) ? 1 : 0;
                break;
            case BinaryType::LessThanOrEqual:
                result = (leftValue <= rightValue) ? 1 : 0;
                break;
            case BinaryType::GreaterThanOrEqual:
                result = (leftValue >= rightValue) ? 1 : 0;
                break;
            case BinaryType::GreaterThan:
                result = (leftValue > rightValue) ? 1 : 0;
                break;
            default:
                UNREACHABLE();
                break;
            }

            value = PrecomputedOperator::Create(result);
        }
        else
        {
            value = BinaryOperator::Create(left, right, op->GetType());
        }
    };

    virtual void Visit(const std::shared_ptr<const ConditionalOperator>& op) override
    {
        std::shared_ptr<const Operator> condition = Precompute(op->GetCondition());
        std::shared_ptr<const Operator> ifTrue = Precompute(op->GetIfTrue());
        std::shared_ptr<const Operator> ifFalse = Precompute(op->GetIfFalse());

        TNumber conditionValue;
        if (TryGetPrecomputedValue(condition, &conditionValue))
        {
            value = conditionValue != 0 ? ifTrue : ifFalse;
        }
        else
        {
            value = ConditionalOperator::Create(condition, ifTrue, ifFalse);
        }
    };

    virtual void Visit(const std::shared_ptr<const UserDefinedOperator>& op) override
    {
        std::vector<std::shared_ptr<const Operator>> operands;

        for (auto& op2 : op->GetOperands())
        {
            operands.push_back(Precompute(op2));
        }

        value = UserDefinedOperator::Create(op->GetDefinition(), std::move(operands));
    };
};

class TailCallVisitor : public OperatorVisitor
{
private:
    std::shared_ptr<const Operator> Process(const std::shared_ptr<const Operator>& op,
                                            bool isTailCall)
    {
        stack.push(isTailCall);
        op->Accept(*this);
        stack.pop();
        return value;
    }

    bool IsCurrentOperatorInTail() const
    {
        return stack.top();
    }

public:
    std::shared_ptr<const Operator> value;
    std::stack<bool> stack{ { true } };

    virtual void Visit(const std::shared_ptr<const ZeroOperator>& op) override
    {
        value = op;
    };

    virtual void Visit(const std::shared_ptr<const PrecomputedOperator>& op) override
    {
        value = op;
    };

    virtual void Visit(const std::shared_ptr<const OperandOperator>& op) override
    {
        value = op;
    };

    virtual void Visit(const std::shared_ptr<const DefineOperator>& op) override
    {
        value = op;
    };

    virtual void Visit(const std::shared_ptr<const LoadVariableOperator>& op) override
    {
        value = op;
    };

    virtual void Visit(const std::shared_ptr<const InputOperator>& op) override
    {
        value = op;
    };

    virtual void Visit(const std::shared_ptr<const LoadArrayOperator>& op) override
    {
        std::shared_ptr<const Operator> index = Process(op->GetIndex(), false);
        value = LoadArrayOperator::Create(index);
    };

    virtual void Visit(const std::shared_ptr<const PrintCharOperator>& op) override
    {
        std::shared_ptr<const Operator> character = Process(op->GetCharacter(), false);
        value = PrintCharOperator::Create(character);
    };

    virtual void Visit(const std::shared_ptr<const ParenthesisOperator>& op) override
    {
        auto& operators = op->GetOperators();
        std::vector<std::shared_ptr<const Operator>> optimized;

        for (size_t i = 0; i < operators.size(); i++)
        {
            std::shared_ptr<const Operator> processed =
                Process(operators[i], i < operators.size() - 1 ? false : IsCurrentOperatorInTail());
            optimized.push_back(processed);
        }

        value = ParenthesisOperator::Create(std::move(optimized));
    };

    virtual void Visit(const std::shared_ptr<const DecimalOperator>& op) override
    {
        std::shared_ptr<const Operator> precomputed = Process(op->GetOperand(), false);
        value = DecimalOperator::Create(precomputed, op->GetValue());
    };

    virtual void Visit(const std::shared_ptr<const StoreVariableOperator>& op) override
    {
        std::shared_ptr<const Operator> operand = Process(op->GetOperand(), false);
        value = StoreVariableOperator::Create(operand, op->GetVariableName());
    }

    virtual void Visit(const std::shared_ptr<const StoreArrayOperator>& op) override
    {
        std::shared_ptr<const Operator> valueToBeStored = Process(op->GetValue(), false);
        std::shared_ptr<const Operator> index = Process(op->GetIndex(), false);
        value = StoreArrayOperator::Create(valueToBeStored, index);
    }

    virtual void Visit(const std::shared_ptr<const BinaryOperator>& op) override
    {
        std::shared_ptr<const Operator> left = Process(op->GetLeft(), false);
        std::shared_ptr<const Operator> right = Process(op->GetRight(), false);
        value = BinaryOperator::Create(left, right, op->GetType());
    };

    virtual void Visit(const std::shared_ptr<const ConditionalOperator>& op) override
    {
        std::shared_ptr<const Operator> condition = Process(op->GetCondition(), false);
        std::shared_ptr<const Operator> ifTrue =
            Process(op->GetIfTrue(), IsCurrentOperatorInTail());
        std::shared_ptr<const Operator> ifFalse =
            Process(op->GetIfFalse(), IsCurrentOperatorInTail());
        value = ConditionalOperator::Create(condition, ifTrue, ifFalse);
    };

    virtual void Visit(const std::shared_ptr<const UserDefinedOperator>& op) override
    {
        std::vector<std::shared_ptr<const Operator>> operands;

        for (auto& op2 : op->GetOperands())
        {
            operands.push_back(Process(op2, false));
        }

        value = UserDefinedOperator::Create(op->GetDefinition(), std::move(operands),
                                            IsCurrentOperatorInTail());
    };
};

template<typename TNumber>
std::shared_ptr<const Operator> OptimizePrecomputeStep(CompilationContext& context,
                                                       const std::shared_ptr<const Operator>& op)
{
    PrecomputeVisitor<TNumber> visitor(context);
    op->Accept(visitor);
    return std::move(visitor.value);
}

std::shared_ptr<const Operator> OptimizeMarkTailCallStep(const std::shared_ptr<const Operator>& op)
{
    TailCallVisitor visitor;
    op->Accept(visitor);
    return std::move(visitor.value);
}

template<typename TNumber>
std::shared_ptr<const Operator> OptimizeCore(CompilationContext& context,
                                             const std::shared_ptr<const Operator>& op)
{
    return OptimizeMarkTailCallStep(OptimizePrecomputeStep<TNumber>(context, op));
}
}

template<typename TNumber>
std::shared_ptr<const Operator> Optimize(CompilationContext& context,
                                         const std::shared_ptr<const Operator>& op)
{
    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        std::shared_ptr<const Operator> optimized =
            OptimizeCore<TNumber>(context, it->second.GetOperator());
        context.AddOperatorImplement(
            OperatorImplement(it->second.GetDefinition(), std::move(optimized)));
    }

    return OptimizeCore<TNumber>(context, op);
}

template std::shared_ptr<const Operator> Optimize<int32_t>(
    CompilationContext& context, const std::shared_ptr<const Operator>& op);
template std::shared_ptr<const Operator> Optimize<int64_t>(
    CompilationContext& context, const std::shared_ptr<const Operator>& op);

#ifdef ENABLE_INT128
template std::shared_ptr<const Operator> Optimize<__int128_t>(
    CompilationContext& context, const std::shared_ptr<const Operator>& op);
#endif // ENABLE_INT128

#ifdef ENABLE_GMP
template std::shared_ptr<const Operator> Optimize<mpz_class>(
    CompilationContext& context, const std::shared_ptr<const Operator>& op);
#endif // ENABLE_GMP
}
