#include "Optimizer.h"
#include "Operators.h"
#include <cstdint>
#include <gmpxx.h>

namespace
{
template<typename TNumber>
class Visitor : public OperatorVisitor
{
private:
    CompilationContext& context;

    std::shared_ptr<Operator> Precompute(const std::shared_ptr<Operator>& op)
    {
        op->Accept(*this);
        return value;
    }

    bool TryGetPrecomputedValue(const std::shared_ptr<Operator>& op, TNumber* dest)
    {
        if (auto precomputed = dynamic_cast<const PrecomputedOperator*>(op.get()))
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
    std::shared_ptr<Operator> value;

    Visitor(CompilationContext& context) : context(context) {}

    virtual void Visit(const ZeroOperator& op) override
    {
        value = std::make_shared<PrecomputedOperator>(static_cast<TNumber>(0));
    };

    virtual void Visit(const PrecomputedOperator& op) override
    {
        // TODO:
        value = std::make_shared<PrecomputedOperator>(op.GetValue<TNumber>());
    };

    virtual void Visit(const OperandOperator& op) override
    {
        // TODO:
        value = std::make_shared<OperandOperator>(op.GetIndex());
    };

    virtual void Visit(const DefineOperator& op) override
    {
        value = std::make_shared<PrecomputedOperator>(static_cast<TNumber>(0));
    };

    virtual void Visit(const ParenthesisOperator& op) override
    {
        std::vector<std::shared_ptr<Operator>> optimized;
        bool allPrecomputed = true;

        for (auto& op2 : op.GetOperators())
        {
            std::shared_ptr<Operator> precomputed = Precompute(op2);
            optimized.push_back(op2);

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
            value = std::make_shared<ParenthesisOperator>(std::move(optimized));
        }
    };

    virtual void Visit(const DecimalOperator& op) override
    {
        std::shared_ptr<Operator> precomputed = Precompute(op.GetOperand());
        TNumber precomputedValue;
        if (TryGetPrecomputedValue(precomputed, &precomputedValue))
        {
            value = std::make_shared<PrecomputedOperator>(
                static_cast<TNumber>(precomputedValue * 10 + op.GetValue()));
        }
        else
        {
            value = std::make_shared<DecimalOperator>(precomputed, op.GetValue());
        }
    };

    virtual void Visit(const BinaryOperator& op) override
    {
        std::shared_ptr<Operator> left = Precompute(op.GetLeft());
        std::shared_ptr<Operator> right = Precompute(op.GetRight());

        TNumber leftValue, rightValue;
        if (TryGetPrecomputedValue(left, &leftValue) && TryGetPrecomputedValue(right, &rightValue))
        {
            TNumber result;

            switch (op.GetType())
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

            value = std::make_shared<PrecomputedOperator>(result);
        }
        else
        {
            value = std::make_shared<BinaryOperator>(left, right, op.GetType());
        }
    };

    virtual void Visit(const ConditionalOperator& op) override
    {
        std::shared_ptr<Operator> condition = Precompute(op.GetCondition());
        std::shared_ptr<Operator> ifTrue = Precompute(op.GetIfTrue());
        std::shared_ptr<Operator> ifFalse = Precompute(op.GetIfFalse());

        TNumber conditionValue;
        if (TryGetPrecomputedValue(condition, &conditionValue))
        {
            value = conditionValue != 0 ? ifTrue : ifFalse;
        }
        else
        {
            value = std::make_shared<ConditionalOperator>(condition, ifTrue, ifFalse);
        }
    };

    virtual void Visit(const UserDefinedOperator& op) override
    {
        std::vector<std::shared_ptr<Operator>> operands;

        for (auto& op2 : op.GetOperands())
        {
            operands.push_back(Precompute(op2));
        }

        value = std::make_shared<UserDefinedOperator>(op.GetDefinition(), std::move(operands));
    };
};
}

template<typename TNumber>
std::shared_ptr<Operator> Optimize(CompilationContext& context, const std::shared_ptr<Operator>& op)
{
    auto OptimizeCore = [&context](const std::shared_ptr<Operator>& op) {
        Visitor<TNumber> visitor(context);
        op->Accept(visitor);
        return visitor.value;
    };

    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        std::shared_ptr<Operator> optimized = OptimizeCore(it->second.GetOperator());
        context.AddOperatorImplement(OperatorImplement(it->second.GetDefinition(), optimized));
    }

    return OptimizeCore(op);
}

template std::shared_ptr<Operator> Optimize<int32_t>(CompilationContext& context,
                                                     const std::shared_ptr<Operator>& op);
template std::shared_ptr<Operator> Optimize<int64_t>(CompilationContext& context,
                                                     const std::shared_ptr<Operator>& op);
template std::shared_ptr<Operator> Optimize<__int128_t>(CompilationContext& context,
                                                        const std::shared_ptr<Operator>& op);
template std::shared_ptr<Operator> Optimize<mpz_class>(CompilationContext& context,
                                                       const std::shared_ptr<Operator>& op);
