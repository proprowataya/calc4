/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "StackMachine.h"
#include "Common.h"
#include "Exceptions.h"
#include "ExecutionState.h"
#include "Operators.h"
#include <algorithm>
#include <optional>
#include <unordered_map>
#include <vector>

#ifdef ENABLE_GMP
#include <gmp.h>
#include <gmpxx.h>
#endif // ENABLE_GMP

// We use the computed goto technique to make dispatch faster.
// This technique is not available on MSVC.
#if !defined(NO_USE_COMPUTED_GOTO) && (defined(__GNUC__) || defined(__clang__))
#define USE_COMPUTED_GOTO
#endif // !defined(NO_USE_COMPUTED_GOTO) && (defined(__GNUC__) || defined(__clang__))

#ifdef USE_COMPUTED_GOTO
#define COMPUTED_GOTO_BEGIN() goto*(op->address)
#define COMPUTED_GOTO_SWITCH()
#define COMPUTED_GOTO_LABEL_OF(LABEL) COMPUTED_GOTO_LABEL_##LABEL
#define COMPUTED_GOTO_CASE(NAME) COMPUTED_GOTO_LABEL_OF(NAME) :
#define COMPUTED_GOTO_DEFAULT()
#define COMPUTED_GOTO_JUMP(DEST)                                                                   \
    do                                                                                             \
    {                                                                                              \
        op = &operations[(DEST)];                                                                  \
        goto*(op->address);                                                                        \
    } while (false)
#define COMPUTED_GOTO_NEXT_OPERATION()                                                             \
    do                                                                                             \
    {                                                                                              \
        ++op;                                                                                      \
        goto*(op->address);                                                                        \
    } while (false)

struct ComputedGotoOperation
{
    const void* address;
    calc4::StackMachineOperation::ValueType value;
};
#else
#define COMPUTED_GOTO_BEGIN()
#define COMPUTED_GOTO_SWITCH()                                                                     \
    LoopBegin:                                                                                     \
    switch (op->opcode)
#define COMPUTED_GOTO_LABEL_OF(LABEL) LABEL
#define COMPUTED_GOTO_CASE(NAME) case StackMachineOpcode::NAME:
#define COMPUTED_GOTO_DEFAULT() default:
#define COMPUTED_GOTO_JUMP(DEST)                                                                   \
    do                                                                                             \
    {                                                                                              \
        op = &operations[(DEST)];                                                                  \
        goto LoopBegin;                                                                            \
    } while (false)
#define COMPUTED_GOTO_NEXT_OPERATION()                                                             \
    do                                                                                             \
    {                                                                                              \
        ++op;                                                                                      \
        goto LoopBegin;                                                                            \
    } while (false)
#endif // USE_COMPUTED_GOTO

namespace std
{
template<>
struct hash<calc4::OperatorDefinition>
{
    size_t operator()(const calc4::OperatorDefinition& definition) const
    {
        return hash<std::string>{}(definition.GetName()) ^ hash<int>{}(definition.GetNumOperands());
    }
};
}

namespace calc4
{
#define InstantiateGenerateStackMachineModule(TNumber)                                             \
    template StackMachineModule<TNumber> GenerateStackMachineModule(                               \
        const std::shared_ptr<const Operator>& op, const CompilationContext& context,              \
        const StackMachineCodeGenerationOption& option)

InstantiateGenerateStackMachineModule(int32_t);
InstantiateGenerateStackMachineModule(int64_t);

#ifdef ENABLE_INT128
InstantiateGenerateStackMachineModule(__int128_t);
#endif // ENABLE_INT128

#ifdef ENABLE_GMP
InstantiateGenerateStackMachineModule(mpz_class);
#endif // ENABLE_GMP

/*****/

#define InstantiateExecuteStackMachineModule(TNumber, TInputSource, TPrinter)                      \
    template TNumber ExecuteStackMachineModule<TNumber, DefaultVariableSource<TNumber>,            \
                                               DefaultGlobalArraySource<TNumber>, TInputSource,    \
                                               TPrinter, std::vector<TNumber>, std::vector<int>>(  \
        const StackMachineModule<TNumber>& module,                                                 \
        ExecutionState<TNumber, DefaultVariableSource<TNumber>, DefaultGlobalArraySource<TNumber>, \
                       TInputSource, TPrinter>& state)

InstantiateExecuteStackMachineModule(int32_t, DefaultInputSource, DefaultPrinter);
InstantiateExecuteStackMachineModule(int32_t, BufferedInputSource, BufferedPrinter);
InstantiateExecuteStackMachineModule(int32_t, StreamInputSource, StreamPrinter);
InstantiateExecuteStackMachineModule(int64_t, DefaultInputSource, DefaultPrinter);
InstantiateExecuteStackMachineModule(int64_t, BufferedInputSource, BufferedPrinter);
InstantiateExecuteStackMachineModule(int64_t, StreamInputSource, StreamPrinter);

#ifdef ENABLE_INT128
InstantiateExecuteStackMachineModule(__int128_t, DefaultInputSource, DefaultPrinter);
InstantiateExecuteStackMachineModule(__int128_t, BufferedInputSource, BufferedPrinter);
InstantiateExecuteStackMachineModule(__int128_t, StreamInputSource, StreamPrinter);
#endif // ENABLE_INT128

#ifdef ENABLE_GMP
InstantiateExecuteStackMachineModule(mpz_class, DefaultInputSource, DefaultPrinter);
InstantiateExecuteStackMachineModule(mpz_class, BufferedInputSource, BufferedPrinter);
InstantiateExecuteStackMachineModule(mpz_class, StreamInputSource, StreamPrinter);
#endif // ENABLE_GMP

/*****/

template<typename TNumber>
std::pair<std::vector<StackMachineOperation>, std::vector<int>> StackMachineModule<
    TNumber>::FlattenOperations() const
{
    size_t totalNumOperations = entryPoint.size() +
        std::accumulate(userDefinedOperators.begin(), userDefinedOperators.end(),
                        static_cast<size_t>(0),
                        [](size_t sum, auto& ud) { return sum + ud.GetOperations().size(); });

    std::vector<StackMachineOperation> result(totalNumOperations);
    std::vector<int> maxStackSizes(totalNumOperations);
    std::vector<int> startAddresses(userDefinedOperators.size());

    size_t index = 0;

    for (size_t i = 0; i < entryPoint.size(); i++)
    {
        result[index++] = entryPoint[i];
    }

    for (size_t i = 0; i < userDefinedOperators.size(); i++)
    {
        int startAddress = static_cast<int>(index);
        startAddresses[i] = startAddress;

        auto& operations = userDefinedOperators[i].GetOperations();
        for (size_t j = 0; j < operations.size(); j++)
        {
            result[index] = operations[j];

            // Resolve labels
            switch (result[index].opcode)
            {
            case StackMachineOpcode::Goto:
            case StackMachineOpcode::GotoIfTrue:
            case StackMachineOpcode::GotoIfFalse:
            case StackMachineOpcode::GotoIfEqual:
            case StackMachineOpcode::GotoIfLessThan:
            case StackMachineOpcode::GotoIfLessThanOrEqual:
                result[index].value += startAddress;
                break;
            default:
                break;
            }

            index++;
        }
    }

    assert(index == result.size());

    // Resolve call operations
    for (size_t i = 0; i < result.size(); i++)
    {
        switch (result[i].opcode)
        {
        case StackMachineOpcode::Call:
        {
            int operatorNo = result[i].value;
            result[i].value = startAddresses[operatorNo];
            maxStackSizes[result[i].value] = userDefinedOperators[operatorNo].GetMaxStackSize();
            break;
        }
        default:
            break;
        }
    }

    return std::make_pair(std::move(result), std::move(maxStackSizes));
}

template<typename TNumber>
StackMachineModule<TNumber> GenerateStackMachineModule(
    const std::shared_ptr<const Operator>& op, const CompilationContext& context,
    const StackMachineCodeGenerationOption& option)
{
    static constexpr int OperatorBeginLabel = 0;

    class Generator : public OperatorVisitor
    {
    public:
        const CompilationContext& context;
        StackMachineCodeGenerationOption option;
        std::vector<TNumber>& constTable;
        std::unordered_map<OperatorDefinition, int>& operatorLabels;
        std::optional<OperatorDefinition> definition;
        std::unordered_map<std::string, int>& variableIndices;

        std::vector<StackMachineOperation> operations;
        int nextLabel = OperatorBeginLabel;
        int stackSize = 0;
        int maxStackSize = 0;

        Generator(const CompilationContext& context, const StackMachineCodeGenerationOption& option,
                  std::vector<TNumber>& constTable,
                  std::unordered_map<OperatorDefinition, int>& operatorLabels,
                  const std::optional<OperatorDefinition>& definition,
                  std::unordered_map<std::string, int>& variableIndices)
            : context(context), option(option), constTable(constTable),
              operatorLabels(operatorLabels), definition(definition),
              variableIndices(variableIndices)
        {
        }

        void Generate(const std::shared_ptr<const Operator>& op)
        {
            assert(nextLabel == OperatorBeginLabel);
            AddOperation(StackMachineOpcode::Lavel, nextLabel++);

            if (definition)
            {
                // Generate user-defined operators' codes
                op->Accept(*this);
                AddOperation(StackMachineOpcode::Return, definition.value().GetNumOperands());
            }
            else
            {
                // Generate Main code
                op->Accept(*this);
                AddOperation(StackMachineOpcode::Halt);
            }

            ResolveLabels();
        }

        void ResolveLabels()
        {
            std::vector<StackMachineOperation> newVector;
            std::unordered_map<int, int> labelMap;

            for (auto& operation : operations)
            {
                switch (operation.opcode)
                {
                case StackMachineOpcode::Lavel:
                {
                    int label = static_cast<int>(newVector.size());
                    labelMap[operation.value] = label;
                    break;
                }
                default:
                    newVector.push_back(operation);
                    break;
                }
            }

            for (size_t i = 0; i < newVector.size(); i++)
            {
                auto& operation = newVector[i];

                switch (operation.opcode)
                {
                case StackMachineOpcode::Goto:
                case StackMachineOpcode::GotoIfTrue:
                case StackMachineOpcode::GotoIfFalse:
                case StackMachineOpcode::GotoIfEqual:
                case StackMachineOpcode::GotoIfLessThan:
                case StackMachineOpcode::GotoIfLessThanOrEqual:
                    operation.value = labelMap.at(operation.value);
                    break;
                default:
                    break;
                }
            }

            operations = std::move(newVector);
        }

        virtual void Visit(const std::shared_ptr<const ZeroOperator>& op) override
        {
            AddOperation(StackMachineOpcode::LoadConst, 0);
        }

        virtual void Visit(const std::shared_ptr<const PrecomputedOperator>& op) override
        {
            auto value = op->GetValue<TNumber>();

            StackMachineOperation::ValueType casted;
#ifdef ENABLE_GMP
            if constexpr (std::is_same_v<TNumber, mpz_class>)
            {
                casted = static_cast<StackMachineOperation::ValueType>(value.get_si());
            }
            else
#endif // ENABLE_GMP
            {
                casted = static_cast<StackMachineOperation::ValueType>(value);
            }

            if (casted == value)
            {
                // Value fits StackMachineOperation::ValueType
                AddOperation(StackMachineOpcode::LoadConst, casted);
            }
            else
            {
                // We cannot emit Opcode.LoadConst,
                // because the constant value exceeds limit of 16-bit integer.
                // So we use constTable.
                int no = static_cast<int>(constTable.size());
                constTable.push_back(value);
                AddOperation(StackMachineOpcode::LoadConstTable, no);
            }
        }

        virtual void Visit(const std::shared_ptr<const OperandOperator>& op) override
        {
            assert(definition);
            AddOperation(StackMachineOpcode::LoadArg,
                         GetArgumentAddress(definition.value().GetNumOperands(), op->GetIndex()));
        };

        virtual void Visit(const std::shared_ptr<const DefineOperator>& op) override
        {
            AddOperation(StackMachineOpcode::LoadConst, 0);
        };

        virtual void Visit(const std::shared_ptr<const LoadVariableOperator>& op) override
        {
            AddOperation(StackMachineOpcode::LoadVariable,
                         GetOrCreateVariableIndex(op->GetVariableName()));
        };

        virtual void Visit(const std::shared_ptr<const InputOperator>& op) override
        {
            AddOperation(StackMachineOpcode::Input, 0);
        };

        virtual void Visit(const std::shared_ptr<const LoadArrayOperator>& op) override
        {
            op->GetIndex()->Accept(*this);
            AddOperation(StackMachineOpcode::LoadArrayElement);
        };

        virtual void Visit(const std::shared_ptr<const PrintCharOperator>& op) override
        {
            op->GetCharacter()->Accept(*this);
            AddOperation(StackMachineOpcode::PrintChar);
        };

        virtual void Visit(const std::shared_ptr<const ParenthesisOperator>& op) override
        {
            auto& operators = op->GetOperators();

            for (size_t i = 0; i < operators.size(); i++)
            {
                operators[i]->Accept(*this);
                if (i < operators.size() - 1)
                {
                    AddOperation(StackMachineOpcode::Pop);
                }
            }
        }

        virtual void Visit(const std::shared_ptr<const DecimalOperator>& op) override
        {
            op->GetOperand()->Accept(*this);
            AddOperation(StackMachineOpcode::LoadConst, 10);
            AddOperation(StackMachineOpcode::Mult);
            AddOperation(StackMachineOpcode::LoadConst, op->GetValue());
            AddOperation(StackMachineOpcode::Add);
        }

        virtual void Visit(const std::shared_ptr<const StoreVariableOperator>& op) override
        {
            op->GetOperand()->Accept(*this);
            AddOperation(StackMachineOpcode::StoreVariable,
                         GetOrCreateVariableIndex(op->GetVariableName()));
        }

        virtual void Visit(const std::shared_ptr<const StoreArrayOperator>& op) override
        {
            op->GetValue()->Accept(*this);
            op->GetIndex()->Accept(*this);
            AddOperation(StackMachineOpcode::StoreArrayElement);
        }

        virtual void Visit(const std::shared_ptr<const BinaryOperator>& op) override
        {
            switch (op->GetType())
            {
            case BinaryType::Add:
                op->GetLeft()->Accept(*this);
                op->GetRight()->Accept(*this);
                AddOperation(StackMachineOpcode::Add);
                break;
            case BinaryType::Sub:
                op->GetLeft()->Accept(*this);
                op->GetRight()->Accept(*this);
                AddOperation(StackMachineOpcode::Sub);
                break;
            case BinaryType::Mult:
                op->GetLeft()->Accept(*this);
                op->GetRight()->Accept(*this);
                AddOperation(StackMachineOpcode::Mult);
                break;
            case BinaryType::Div:
                op->GetLeft()->Accept(*this);
                op->GetRight()->Accept(*this);
                AddOperation(option.checkZeroDivision ? StackMachineOpcode::DivChecked
                                                      : StackMachineOpcode::Div);
                break;
            case BinaryType::Mod:
                op->GetLeft()->Accept(*this);
                op->GetRight()->Accept(*this);
                AddOperation(option.checkZeroDivision ? StackMachineOpcode::ModChecked
                                                      : StackMachineOpcode::Mod);
                break;

                // For comparisons and logical operations, we generate code as "condition" to reduce
                // redundancy and to keep short-circuit behavior.
            case BinaryType::Equal:
            case BinaryType::NotEqual:
            case BinaryType::LessThan:
            case BinaryType::LessThanOrEqual:
            case BinaryType::GreaterThanOrEqual:
            case BinaryType::GreaterThan:
            case BinaryType::LogicalAnd:
            case BinaryType::LogicalOr:
            {
                int ifTrueLabel = nextLabel++, endLabel = nextLabel++;

                EmitConditionGotoIfTrue(op, ifTrueLabel);
                AddOperation(StackMachineOpcode::LoadConst, 0);
                AddOperation(StackMachineOpcode::Goto, endLabel);
                AddOperation(StackMachineOpcode::Lavel, ifTrueLabel);
                AddOperation(StackMachineOpcode::LoadConst, 1);
                AddOperation(StackMachineOpcode::Lavel, endLabel);

                // StackSize increased by two due to two LoadConst operations.
                // However, only one of them will be executed.
                // We modify StackSize here.
                AddStackSize(-1);
                return;
            }
            default:
                UNREACHABLE();
                break;
            }
        }

        void EmitConditionGoto(const std::shared_ptr<const Operator>& condition, int label,
                               bool gotoIfTrue)
        {
            if (auto* parenthesis = dynamic_cast<const ParenthesisOperator*>(condition.get()))
            {
                auto& operators = parenthesis->GetOperators();

                for (size_t i = 0; i < operators.size(); i++)
                {
                    if (i < operators.size() - 1)
                    {
                        operators[i]->Accept(*this);
                        AddOperation(StackMachineOpcode::Pop);
                    }
                    else
                    {
                        EmitConditionGoto(operators[i], label, gotoIfTrue);
                    }
                }

                return;
            }

            if (auto* binary = dynamic_cast<const BinaryOperator*>(condition.get()))
            {
                switch (binary->GetType())
                {
                case BinaryType::Equal:
                    binary->GetLeft()->Accept(*this);
                    binary->GetRight()->Accept(*this);
                    if (gotoIfTrue)
                    {
                        AddOperation(StackMachineOpcode::GotoIfEqual, label);
                    }
                    else
                    {
                        int endLabel = nextLabel++;
                        AddOperation(StackMachineOpcode::GotoIfEqual, endLabel);
                        AddOperation(StackMachineOpcode::Goto, label);
                        AddOperation(StackMachineOpcode::Lavel, endLabel);
                    }
                    return;
                case BinaryType::NotEqual:
                    binary->GetLeft()->Accept(*this);
                    binary->GetRight()->Accept(*this);
                    if (gotoIfTrue)
                    {
                        int endLabel = nextLabel++;
                        AddOperation(StackMachineOpcode::GotoIfEqual, endLabel);
                        AddOperation(StackMachineOpcode::Goto, label);
                        AddOperation(StackMachineOpcode::Lavel, endLabel);
                    }
                    else
                    {
                        AddOperation(StackMachineOpcode::GotoIfEqual, label);
                    }
                    return;
                case BinaryType::LessThan:
                    binary->GetLeft()->Accept(*this);
                    binary->GetRight()->Accept(*this);
                    if (gotoIfTrue)
                    {
                        AddOperation(StackMachineOpcode::GotoIfLessThan, label);
                    }
                    else
                    {
                        int endLabel = nextLabel++;
                        AddOperation(StackMachineOpcode::GotoIfLessThan, endLabel);
                        AddOperation(StackMachineOpcode::Goto, label);
                        AddOperation(StackMachineOpcode::Lavel, endLabel);
                    }
                    return;
                case BinaryType::LessThanOrEqual:
                    binary->GetLeft()->Accept(*this);
                    binary->GetRight()->Accept(*this);
                    if (gotoIfTrue)
                    {
                        AddOperation(StackMachineOpcode::GotoIfLessThanOrEqual, label);
                    }
                    else
                    {
                        int endLabel = nextLabel++;
                        AddOperation(StackMachineOpcode::GotoIfLessThanOrEqual, endLabel);
                        AddOperation(StackMachineOpcode::Goto, label);
                        AddOperation(StackMachineOpcode::Lavel, endLabel);
                    }
                    return;
                case BinaryType::GreaterThanOrEqual:
                    binary->GetLeft()->Accept(*this);
                    binary->GetRight()->Accept(*this);
                    if (gotoIfTrue)
                    {
                        int endLabel = nextLabel++;
                        AddOperation(StackMachineOpcode::GotoIfLessThan, endLabel);
                        AddOperation(StackMachineOpcode::Goto, label);
                        AddOperation(StackMachineOpcode::Lavel, endLabel);
                    }
                    else
                    {
                        AddOperation(StackMachineOpcode::GotoIfLessThan, label);
                    }
                    return;
                case BinaryType::GreaterThan:
                    binary->GetLeft()->Accept(*this);
                    binary->GetRight()->Accept(*this);
                    if (gotoIfTrue)
                    {
                        int endLabel = nextLabel++;
                        AddOperation(StackMachineOpcode::GotoIfLessThanOrEqual, endLabel);
                        AddOperation(StackMachineOpcode::Goto, label);
                        AddOperation(StackMachineOpcode::Lavel, endLabel);
                    }
                    else
                    {
                        AddOperation(StackMachineOpcode::GotoIfLessThanOrEqual, label);
                    }
                    return;
                case BinaryType::LogicalAnd:
                    if (gotoIfTrue)
                    {
                        int ifFalseLabel = nextLabel++;
                        EmitConditionGoto(binary->GetLeft(), ifFalseLabel, false);
                        EmitConditionGoto(binary->GetRight(), label, true);
                        AddOperation(StackMachineOpcode::Lavel, ifFalseLabel);
                    }
                    else
                    {
                        EmitConditionGoto(binary->GetLeft(), label, false);
                        EmitConditionGoto(binary->GetRight(), label, false);
                    }
                    return;
                case BinaryType::LogicalOr:
                    if (gotoIfTrue)
                    {
                        EmitConditionGoto(binary->GetLeft(), label, true);
                        EmitConditionGoto(binary->GetRight(), label, true);
                    }
                    else
                    {
                        int endLabel = nextLabel++;
                        EmitConditionGoto(binary->GetLeft(), endLabel, true);
                        EmitConditionGoto(binary->GetRight(), label, false);
                        AddOperation(StackMachineOpcode::Lavel, endLabel);
                    }
                    return;
                default:
                    break;
                }
            }

            condition->Accept(*this);
            AddOperation(gotoIfTrue ? StackMachineOpcode::GotoIfTrue
                                    : StackMachineOpcode::GotoIfFalse,
                         label);
        }

        void EmitConditionGotoIfTrue(const std::shared_ptr<const Operator>& condition,
                                     int ifTrueLabel)
        {
            EmitConditionGoto(condition, ifTrueLabel, true);
        }

        void EmitConditionGotoIfFalse(const std::shared_ptr<const Operator>& condition,
                                      int ifFalseLabel)
        {
            EmitConditionGoto(condition, ifFalseLabel, false);
        }

        virtual void Visit(const std::shared_ptr<const ConditionalOperator>& op) override
        {
            int ifTrueLabel = nextLabel++, endLabel = nextLabel++;
            EmitConditionGotoIfTrue(op->GetCondition(), ifTrueLabel);

            int savedStackSize = stackSize;
            op->GetIfFalse()->Accept(*this);

            auto lastOperation = std::find_if(operations.rbegin(), operations.rend(), [](auto op) {
                return op.opcode != StackMachineOpcode::Lavel;
            });
            if (lastOperation != operations.rend() &&
                lastOperation->opcode != StackMachineOpcode::Goto)
            {
                // "Last opcode is Goto" means elimination of "Call" (tail-call)
                AddOperation(StackMachineOpcode::Goto, endLabel);
            }

            AddOperation(StackMachineOpcode::Lavel, ifTrueLabel);
            stackSize = savedStackSize;
            op->GetIfTrue()->Accept(*this);
            AddOperation(StackMachineOpcode::Lavel, endLabel);
        };

        virtual void Visit(const std::shared_ptr<const UserDefinedOperator>& op) override
        {
            auto operands = op->GetOperands();
            for (size_t i = 0; i < operands.size(); i++)
            {
                operands[i]->Accept(*this);
            }

            if (IsReplaceableWithJump(op))
            {
                for (int i = static_cast<int>(operands.size()) - 1; i >= 0; i--)
                {
                    AddOperation(StackMachineOpcode::StoreArg,
                                 GetArgumentAddress(definition.value().GetNumOperands(), i));
                }

                AddOperation(StackMachineOpcode::Goto, OperatorBeginLabel);

                // If we eliminate tail-call, there are no values left in the stack.
                // We treat as if there are one returning value in the stack.
                AddStackSize(1);
            }
            else
            {
                AddOperation(StackMachineOpcode::Call, operatorLabels[op->GetDefinition()]);
            }
        }

        void AddStackSize(int value)
        {
            int newStackSize = stackSize + value;
            if (newStackSize < 0)
            {
                throw Exceptions::AssertionErrorException(
                    std::nullopt, "Stack size is negative: " + std::to_string(newStackSize));
            }

            maxStackSize = std::max(stackSize, newStackSize);
            stackSize = newStackSize;
        }

        void AddOperation(StackMachineOpcode opcode, StackMachineOperation::ValueType value = 0)
        {
            operations.emplace_back(opcode, value);

            switch (opcode)
            {
            case StackMachineOpcode::Push:
            case StackMachineOpcode::LoadConst:
            case StackMachineOpcode::LoadConstTable:
            case StackMachineOpcode::LoadArg:
            case StackMachineOpcode::LoadVariable:
            case StackMachineOpcode::Input:
                AddStackSize(1);
                break;
            case StackMachineOpcode::Pop:
            case StackMachineOpcode::StoreArg:
            case StackMachineOpcode::StoreArrayElement:
            case StackMachineOpcode::Add:
            case StackMachineOpcode::Sub:
            case StackMachineOpcode::Mult:
            case StackMachineOpcode::Div:
            case StackMachineOpcode::DivChecked:
            case StackMachineOpcode::Mod:
            case StackMachineOpcode::ModChecked:
            case StackMachineOpcode::GotoIfTrue:
            case StackMachineOpcode::GotoIfFalse:
            case StackMachineOpcode::Return:
            case StackMachineOpcode::Halt:
                AddStackSize(-1);
                break;
            case StackMachineOpcode::StoreVariable:
            case StackMachineOpcode::LoadArrayElement:
            case StackMachineOpcode::PrintChar:
            case StackMachineOpcode::Goto:
                // Stacksize will not change
                break;
            case StackMachineOpcode::GotoIfEqual:
            case StackMachineOpcode::GotoIfLessThan:
            case StackMachineOpcode::GotoIfLessThanOrEqual:
                AddStackSize(-2);
                break;
            case StackMachineOpcode::Call:
            {
                // Find the corresponding OperatorDefinition
                auto it = std::find_if(operatorLabels.begin(), operatorLabels.end(),
                                       [value](auto& pair) { return pair.second == value; });
                assert(it != operatorLabels.end());

                int numOperands = it->first.GetNumOperands();
                AddStackSize(-(numOperands - 1));
                break;
            }
            case StackMachineOpcode::Lavel:
                // Do nothing
                break;
            default:
                UNREACHABLE();
                break;
            }
        }

        int GetOrCreateVariableIndex(const std::string& variableName)
        {
            auto it = variableIndices.find(variableName);
            if (it != variableIndices.end())
            {
                return it->second;
            }
            else
            {
                int index = static_cast<int>(variableIndices.size());
                variableIndices[variableName] = index;
                return index;
            }
        }

        bool IsReplaceableWithJump(const std::shared_ptr<const UserDefinedOperator>& op) const
        {
            return definition == op->GetDefinition() && op->IsTailCall().value_or(false);
        }

        static int GetArgumentAddress(int numOperands, int index)
        {
            return numOperands - index;
        }
    };

    std::vector<TNumber> constTable;
    std::vector<StackMachineUserDefinedOperator> userDefinedOperators;
    std::unordered_map<OperatorDefinition, int> operatorLabels;
    std::unordered_map<std::string, int> variableIndices;

    int index = 0;
    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        auto& implement = it->second;
        operatorLabels[implement.GetDefinition()] = index++;
    }

    // Generate user-defined operators' codes
    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        auto& implement = it->second;
        Generator generator(context, option, constTable, operatorLabels, implement.GetDefinition(),
                            variableIndices);
        generator.Generate(implement.GetOperator());

        if (generator.stackSize != 0)
        {
            throw Exceptions::AssertionErrorException(
                std::nullopt, "Stacksize is not zero: " + std::to_string(generator.stackSize));
        }

        userDefinedOperators.emplace_back(implement.GetDefinition(),
                                          std::move(generator.operations), generator.maxStackSize);
    }

    // Generate Main code
    std::vector<StackMachineOperation> entryPoint;
    {
        Generator generator(context, option, constTable, operatorLabels, std::nullopt,
                            variableIndices);
        generator.Generate(op);
        entryPoint = std::move(generator.operations);
    }

    std::vector<std::string> variables(variableIndices.size());
    for (auto& pair : variableIndices)
    {
        variables[pair.second] = pair.first;
    }

    return StackMachineModule<TNumber>(entryPoint, constTable, userDefinedOperators, variables);
}

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter, typename TStackArray, typename TPtrStackArray>
TNumber ExecuteStackMachineModule(
    const StackMachineModule<TNumber>& module,
    ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TInputSource, TPrinter>& state)
{
    static constexpr size_t StackSize = 1 << 20;
    static constexpr size_t PtrStackSize = 1 << 20;

    // Get variable's values from ExecutionState
    std::vector<TNumber> variables(module.GetVariables().size());
    for (size_t i = 0; i < variables.size(); i++)
    {
        variables[i] = state.GetVariableSource().Get(module.GetVariables()[i]);
    }

    // Start execution
    auto [operationsOriginal, maxStackSizes] = module.FlattenOperations();
    TStackArray stack(StackSize);
    TPtrStackArray ptrStack(PtrStackSize);
    TNumber* top = &*stack.begin();
    TNumber* bottom = top;
    int* ptrTop = &*ptrStack.begin();
    auto& array = state.GetArraySource();

#ifdef USE_COMPUTED_GOTO
    // This dispatch table must be in the same order as the StackMachineOpcode definition
    static const void* DispatchTable[] = {
        &&COMPUTED_GOTO_LABEL_OF(Push),
        &&COMPUTED_GOTO_LABEL_OF(Pop),
        &&COMPUTED_GOTO_LABEL_OF(LoadConst),
        &&COMPUTED_GOTO_LABEL_OF(LoadConstTable),
        &&COMPUTED_GOTO_LABEL_OF(LoadArg),
        &&COMPUTED_GOTO_LABEL_OF(StoreArg),
        &&COMPUTED_GOTO_LABEL_OF(LoadVariable),
        &&COMPUTED_GOTO_LABEL_OF(StoreVariable),
        &&COMPUTED_GOTO_LABEL_OF(LoadArrayElement),
        &&COMPUTED_GOTO_LABEL_OF(StoreArrayElement),
        &&COMPUTED_GOTO_LABEL_OF(Input),
        &&COMPUTED_GOTO_LABEL_OF(PrintChar),
        &&COMPUTED_GOTO_LABEL_OF(Add),
        &&COMPUTED_GOTO_LABEL_OF(Sub),
        &&COMPUTED_GOTO_LABEL_OF(Mult),
        &&COMPUTED_GOTO_LABEL_OF(Div),
        &&COMPUTED_GOTO_LABEL_OF(DivChecked),
        &&COMPUTED_GOTO_LABEL_OF(Mod),
        &&COMPUTED_GOTO_LABEL_OF(ModChecked),
        &&COMPUTED_GOTO_LABEL_OF(Goto),
        &&COMPUTED_GOTO_LABEL_OF(GotoIfTrue),
        &&COMPUTED_GOTO_LABEL_OF(GotoIfFalse),
        &&COMPUTED_GOTO_LABEL_OF(GotoIfEqual),
        &&COMPUTED_GOTO_LABEL_OF(GotoIfLessThan),
        &&COMPUTED_GOTO_LABEL_OF(GotoIfLessThanOrEqual),
        &&COMPUTED_GOTO_LABEL_OF(Call),
        &&COMPUTED_GOTO_LABEL_OF(Return),
        &&COMPUTED_GOTO_LABEL_OF(Halt),
        &&COMPUTED_GOTO_LABEL_OF(Lavel),
    };

    // Compute the target addresses of goto
    ComputedGotoOperation operations[operationsOriginal.size()];
    for (size_t i = 0; i < operationsOriginal.size(); i++)
    {
        operations[i].address = DispatchTable[static_cast<size_t>(operationsOriginal[i].opcode)];
        operations[i].value = operationsOriginal[i].value;
    }
    ComputedGotoOperation* op = operations;
#else
    StackMachineOperation* op = &*operationsOriginal.begin();
    StackMachineOperation* operations = &*operationsOriginal.begin();
#endif // USE_COMPUTED_GOTO

    COMPUTED_GOTO_BEGIN();

    COMPUTED_GOTO_SWITCH()
    {
        COMPUTED_GOTO_CASE(Push)
        {
            *top = 0;
            top++;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(Pop)
        {
            top--;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(LoadConst)
        {
            *top = static_cast<TNumber>(op->value);
            top++;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(LoadConstTable)
        {
            *top = module.GetConstTable()[op->value];
            top++;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(LoadArg)
        {
            *top = bottom[-op->value];
            top++;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(StoreArg)
        {
            top--;
            bottom[-op->value] = *top;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(LoadVariable)
        {
            *top = variables[op->value];
            top++;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(StoreVariable)
        {
            variables[op->value] = top[-1];
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(LoadArrayElement)
        {
            top[-1] = array.Get(top[-1]);
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(StoreArrayElement)
        {
            top--;
            array.Set(*top, top[-1]);
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(Input)
        {
            *top = static_cast<TNumber>(state.GetChar());
            top++;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(PrintChar)
        {
#ifdef ENABLE_GMP
            if constexpr (std::is_same_v<TNumber, mpz_class>)
            {
                state.PrintChar(top[-1].get_si());
            }
            else
#endif // ENABLE_GMP
            {
                state.PrintChar(static_cast<char>(top[-1]));
            }
            top[-1] = 0;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(Add)
        {
            top--;
            top[-1] = top[-1] + *top;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(Sub)
        {
            top--;
            top[-1] = top[-1] - *top;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(Mult)
        {
            top--;
            top[-1] = top[-1] * *top;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(Div)
        {
            top--;
            top[-1] = top[-1] / *top;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(DivChecked)
        {
            // This block is required in order to ensure that the 'divisor' variable will be
            // properly destructed before jumping by COMPUTED_GOTO_JUMP macro.
            {
                top--;

                TNumber divisor = *top;
                if (divisor == 0)
                {
                    throw Exceptions::ZeroDivisionException(std::nullopt);
                }

                top[-1] = top[-1] / divisor;
            }

            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(Mod)
        {
            top--;
            top[-1] = top[-1] % *top;
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(ModChecked)
        {
            // This block is required in order to ensure that the 'divisor' variable will be
            // properly destructed before jumping by COMPUTED_GOTO_JUMP macro.
            {
                top--;

                TNumber divisor = *top;
                if (divisor == 0)
                {
                    throw Exceptions::ZeroDivisionException(std::nullopt);
                }

                top[-1] = top[-1] % divisor;
            }

            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(Goto)
        {
            COMPUTED_GOTO_JUMP(op->value);
        }

        COMPUTED_GOTO_CASE(GotoIfTrue)
        {
            top--;
            if (*top != 0)
            {
                COMPUTED_GOTO_JUMP(op->value);
            }
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(GotoIfFalse)
        {
            top--;
            if (*top == 0)
            {
                COMPUTED_GOTO_JUMP(op->value);
            }
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(GotoIfEqual)
        {
            top -= 2;
            if (*top == top[1])
            {
                COMPUTED_GOTO_JUMP(op->value);
            }
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(GotoIfLessThan)
        {
            top -= 2;
            if (*top < top[1])
            {
                COMPUTED_GOTO_JUMP(op->value);
            }
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(GotoIfLessThanOrEqual)
        {
            top -= 2;
            if (*top <= top[1])
            {
                COMPUTED_GOTO_JUMP(op->value);
            }
            COMPUTED_GOTO_NEXT_OPERATION();
        }

        COMPUTED_GOTO_CASE(Call)
        {
            // Check stack overflow
            if (top + maxStackSizes[op->value] >= &*stack.begin() + stack.size())
            {
                throw Exceptions::StackOverflowException(std::nullopt);
            }

            if (ptrTop + 2 >= &*ptrStack.begin() + ptrStack.size())
            {
                throw Exceptions::StackOverflowException(std::nullopt);
            }

            // Push current program counter
            *ptrTop = static_cast<int>(std::distance(operations, op));
            ptrTop++;

            // Push current stack bottom
            *ptrTop = static_cast<int>(std::distance(&*stack.begin(), bottom));
            ptrTop++;

            // Create new stack frame
            bottom = top;

            // Branch
            COMPUTED_GOTO_JUMP(op->value);
        }

        COMPUTED_GOTO_CASE(Return)
        {
            // This block is required in order to ensure that the 'valueToBeReturned' variable will
            // be properly destructed before jumping by COMPUTED_GOTO_JUMP macro.
            {
                // Store returning value
                TNumber valueToBeReturned = top[-1];

                // Restore previous stack top while removing arguments from stack
                // (We ensure space of returning value)
                top = bottom - op->value + 1;

                // Store returning value on stack
                top[-1] = valueToBeReturned;

                // Pop previous stack bottom
                ptrTop--;
                bottom = &*stack.begin() + *ptrTop;

                // Pop previous program counter
                ptrTop--;
            }

            COMPUTED_GOTO_JUMP(*ptrTop + 1);
        }

        COMPUTED_GOTO_CASE(Halt)
        {
            // Store variable's values to ExecutionState
            for (size_t i = 0; i < variables.size(); i++)
            {
                state.GetVariableSource().Set(module.GetVariables()[i], variables[i]);
            }

            return top[-1];
        }

        COMPUTED_GOTO_CASE(Lavel)
        COMPUTED_GOTO_DEFAULT()
        {
            UNREACHABLE();
            COMPUTED_GOTO_NEXT_OPERATION();
        }
    }
}
}
