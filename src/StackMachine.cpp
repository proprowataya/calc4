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

/*****/

#define InstantiateGenerateStackMachineModule(TNumber)                                             \
    template StackMachineModule<TNumber> GenerateStackMachineModule(                               \
        const std::shared_ptr<const Operator>& op, const CompilationContext& context)

InstantiateGenerateStackMachineModule(int32_t);
InstantiateGenerateStackMachineModule(int64_t);

#ifdef ENABLE_INT128
InstantiateGenerateStackMachineModule(__int128_t);
#endif // ENABLE_INT128

#ifdef ENABLE_GMP
InstantiateGenerateStackMachineModule(mpz_class);
#endif // ENABLE_GMP

/*****/

#define InstantiateExecuteStackMachineModule(TNumber, TPrinter)                                    \
    template TNumber ExecuteStackMachineModule<TNumber, DefaultVariableSource<TNumber>,            \
                                               DefaultGlobalArraySource<TNumber>, TPrinter,        \
                                               std::vector<TNumber>, std::vector<int>>(            \
        const StackMachineModule<TNumber>& module,                                                 \
        ExecutionState<TNumber, DefaultVariableSource<TNumber>, DefaultGlobalArraySource<TNumber>, \
                       TPrinter>& state)

InstantiateExecuteStackMachineModule(int32_t, DefaultPrinter);
InstantiateExecuteStackMachineModule(int32_t, BufferedPrinter);
InstantiateExecuteStackMachineModule(int64_t, DefaultPrinter);
InstantiateExecuteStackMachineModule(int64_t, BufferedPrinter);

#ifdef ENABLE_INT128
InstantiateExecuteStackMachineModule(__int128_t, DefaultPrinter);
InstantiateExecuteStackMachineModule(__int128_t, BufferedPrinter);
#endif // ENABLE_INT128

#ifdef ENABLE_GMP
InstantiateExecuteStackMachineModule(mpz_class, DefaultPrinter);
InstantiateExecuteStackMachineModule(mpz_class, BufferedPrinter);
#endif // ENABLE_GMP

/*****/

namespace std
{
template<>
struct hash<OperatorDefinition>
{
    size_t operator()(const OperatorDefinition& definition) const
    {
        return hash<std::string>{}(definition.GetName()) ^ hash<int>{}(definition.GetNumOperands());
    }
};
}

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
            result[i].value = startAddresses[operatorNo] - 1;
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
StackMachineModule<TNumber> GenerateStackMachineModule(const std::shared_ptr<const Operator>& op,
                                                       const CompilationContext& context)
{
    static constexpr int OperatorBeginLabel = 0;

    class Generator : public OperatorVisitor
    {
    public:
        const CompilationContext& context;
        std::vector<TNumber>& constTable;
        std::unordered_map<OperatorDefinition, int>& operatorLabels;
        std::optional<OperatorDefinition> definition;
        std::unordered_map<std::string, int>& variableIndices;

        std::vector<StackMachineOperation> operations;
        int nextLabel = OperatorBeginLabel;
        int stackSize = 0;
        int maxStackSize = 0;

        Generator(const CompilationContext& context, std::vector<TNumber>& constTable,
                  std::unordered_map<OperatorDefinition, int>& operatorLabels,
                  const std::optional<OperatorDefinition>& definition,
                  std::unordered_map<std::string, int>& variableIndices)
            : context(context), constTable(constTable), operatorLabels(operatorLabels),
              definition(definition), variableIndices(variableIndices)
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
                case StackMachineOpcode::GotoIfEqual:
                case StackMachineOpcode::GotoIfLessThan:
                case StackMachineOpcode::GotoIfLessThanOrEqual:
                    // The destination address in LowLevelOperation should be just before it,
                    // because program counter will be incremented after execution of this
                    // operation.
                    operation.value = labelMap.at(operation.value) - 1;
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
            auto EmitComparisonBranch = [this](StackMachineOpcode opcode, bool reverse) {
                int ifFalse = nextLabel++, end = nextLabel++;

                AddOperation(opcode, ifFalse);
                AddOperation(StackMachineOpcode::LoadConst, reverse ? 1 : 0);
                AddOperation(StackMachineOpcode::Goto, end);
                AddOperation(StackMachineOpcode::Lavel, ifFalse);
                AddOperation(StackMachineOpcode::LoadConst, reverse ? 0 : 1);
                AddOperation(StackMachineOpcode::Lavel, end);

                // StackSize increased by two due to two LoadConst operations.
                // However, only one of them will be executed.
                // We modify StackSize here.
                AddStackSize(-1);
            };

            op->GetLeft()->Accept(*this);
            op->GetRight()->Accept(*this);

            switch (op->GetType())
            {
            case BinaryType::Add:
                AddOperation(StackMachineOpcode::Add);
                break;
            case BinaryType::Sub:
                AddOperation(StackMachineOpcode::Sub);
                break;
            case BinaryType::Mult:
                AddOperation(StackMachineOpcode::Mult);
                break;
            case BinaryType::Div:
                AddOperation(StackMachineOpcode::Div);
                break;
            case BinaryType::Mod:
                AddOperation(StackMachineOpcode::Mod);
                break;
            case BinaryType::Equal:
                EmitComparisonBranch(StackMachineOpcode::GotoIfEqual, false);
                break;
            case BinaryType::NotEqual:
                EmitComparisonBranch(StackMachineOpcode::GotoIfEqual, true);
                break;
            case BinaryType::LessThan:
                EmitComparisonBranch(StackMachineOpcode::GotoIfLessThan, false);
                break;
            case BinaryType::LessThanOrEqual:
                EmitComparisonBranch(StackMachineOpcode::GotoIfLessThanOrEqual, false);
                break;
            case BinaryType::GreaterThanOrEqual:
                EmitComparisonBranch(StackMachineOpcode::GotoIfLessThan, true);
                break;
            case BinaryType::GreaterThan:
                EmitComparisonBranch(StackMachineOpcode::GotoIfLessThanOrEqual, true);
                break;
            default:
                UNREACHABLE();
                break;
            }
        }

        virtual void Visit(const std::shared_ptr<const ConditionalOperator>& op) override
        {
            int ifTrueLabel = nextLabel++, endLabel = nextLabel++;

            // Helper function to emit code for branch
            // - Call this after generating evaluation code of condition operator
            auto Emit = [this, ifTrueLabel,
                         endLabel](StackMachineOpcode opcode,
                                   const std::shared_ptr<const Operator>& ifTrue,
                                   const std::shared_ptr<const Operator>& ifFalse) {
                AddOperation(opcode, ifTrueLabel);

                int savedStackSize = stackSize;
                ifFalse->Accept(*this);

                auto lastOperation =
                    std::find_if(operations.rbegin(), operations.rend(),
                                 [](auto op) { return op.opcode != StackMachineOpcode::Lavel; });
                if (lastOperation != operations.rend() &&
                    lastOperation->opcode != StackMachineOpcode::Goto)
                {
                    // "Last opcode is Goto" means elimination of "Call" (tail-call)
                    AddOperation(StackMachineOpcode::Goto, endLabel);
                }

                AddOperation(StackMachineOpcode::Lavel, ifTrueLabel);
                stackSize = savedStackSize;
                ifTrue->Accept(*this);
                AddOperation(StackMachineOpcode::Lavel, endLabel);
            };

            if (auto* binary = dynamic_cast<const BinaryOperator*>(op->GetCondition().get()))
            {
                // Special optimiztion for comparisons
                switch (binary->GetType())
                {
                case BinaryType::Equal:
                    binary->GetLeft()->Accept(*this);
                    binary->GetRight()->Accept(*this);
                    Emit(StackMachineOpcode::GotoIfEqual, op->GetIfTrue(), op->GetIfFalse());
                    return;
                case BinaryType::NotEqual:
                    // "a != b ? c ? d" is equivalent to "a == b ? d ? c"
                    binary->GetLeft()->Accept(*this);
                    binary->GetRight()->Accept(*this);
                    Emit(StackMachineOpcode::GotoIfEqual, op->GetIfFalse(), op->GetIfTrue());
                    return;
                case BinaryType::LessThan:
                    binary->GetLeft()->Accept(*this);
                    binary->GetRight()->Accept(*this);
                    Emit(StackMachineOpcode::GotoIfLessThan, op->GetIfTrue(), op->GetIfFalse());
                    return;
                case BinaryType::LessThanOrEqual:
                    binary->GetLeft()->Accept(*this);
                    binary->GetRight()->Accept(*this);
                    Emit(StackMachineOpcode::GotoIfLessThanOrEqual, op->GetIfTrue(),
                         op->GetIfFalse());
                    return;
                case BinaryType::GreaterThanOrEqual:
                    // "a >= b ? c ? d" is equivalent to "a < b ? d ? c"
                    binary->GetLeft()->Accept(*this);
                    binary->GetRight()->Accept(*this);
                    Emit(StackMachineOpcode::GotoIfLessThan, op->GetIfFalse(), op->GetIfTrue());
                    return;
                case BinaryType::GreaterThan:
                    // "a > b ? c ? d" is equivalent to "a <= b ? d ? c"
                    binary->GetLeft()->Accept(*this);
                    binary->GetRight()->Accept(*this);
                    Emit(StackMachineOpcode::GotoIfLessThanOrEqual, op->GetIfFalse(),
                         op->GetIfTrue());
                    return;
                default:
                    break;
                }
            }

            op->GetCondition()->Accept(*this);
            Emit(StackMachineOpcode::GotoIfTrue, op->GetIfTrue(), op->GetIfFalse());
        };

        virtual void Visit(const std::shared_ptr<const UserDefinedOperator>& op) override
        {
            auto operands = op->GetOperands();
            for (size_t i = 0; i < operands.size(); i++)
            {
                operands[i]->Accept(*this);
            }

            AddOperation(StackMachineOpcode::Call, operatorLabels[op->GetDefinition()]);
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
            case StackMachineOpcode::Mod:
            case StackMachineOpcode::GotoIfTrue:
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
        Generator generator(context, constTable, operatorLabels, implement.GetDefinition(),
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
        Generator generator(context, constTable, operatorLabels, std::nullopt, variableIndices);
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

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter,
         typename TStackArray, typename TPtrStackArray>
TNumber ExecuteStackMachineModule(
    const StackMachineModule<TNumber>& module,
    ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>& state)
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
    auto [operations, maxStackSizes] = module.FlattenOperations();
    TStackArray stack(StackSize);
    TPtrStackArray ptrStack(PtrStackSize);
    TNumber* top = &*stack.begin();
    TNumber* bottom = top;
    int* ptrTop = &*ptrStack.begin();
    auto& array = state.GetArraySource();

    for (StackMachineOperation* op = &*operations.begin();; op++)
    {
        switch (op->opcode)
        {
        case StackMachineOpcode::Push:
            *top = 0;
            top++;
            break;
        case StackMachineOpcode::Pop:
            top--;
            break;
        case StackMachineOpcode::LoadConst:
            *top = static_cast<TNumber>(op->value);
            top++;
            break;
        case StackMachineOpcode::LoadConstTable:
            *top = module.GetConstTable()[op->value];
            top++;
            break;
        case StackMachineOpcode::LoadArg:
            *top = bottom[-op->value];
            top++;
            break;
        case StackMachineOpcode::StoreArg:
            top--;
            bottom[-op->value] = *top;
            break;
        case StackMachineOpcode::LoadVariable:
            *top = variables[op->value];
            top++;
            break;
        case StackMachineOpcode::StoreVariable:
            variables[op->value] = top[-1];
            break;
        case StackMachineOpcode::LoadArrayElement:
            top[-1] = array.Get(top[-1]);
            break;
        case StackMachineOpcode::StoreArrayElement:
            top--;
            array.Set(*top, top[-1]);
            break;
        case StackMachineOpcode::Input:
            throw "Not implemented";
        case StackMachineOpcode::PrintChar:
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
            break;
        case StackMachineOpcode::Add:
            top--;
            top[-1] = top[-1] + *top;
            break;
        case StackMachineOpcode::Sub:
            top--;
            top[-1] = top[-1] - *top;
            break;
        case StackMachineOpcode::Mult:
            top--;
            top[-1] = top[-1] * *top;
            break;
        case StackMachineOpcode::Div:
            top--;
            top[-1] = top[-1] / *top;
            break;
        case StackMachineOpcode::Mod:
            top--;
            top[-1] = top[-1] % *top;
            break;
        case StackMachineOpcode::Goto:
            op = &operations[op->value];
            break;
        case StackMachineOpcode::GotoIfTrue:
            top--;
            if (*top != 0)
            {
                op = &operations[op->value];
            }
            break;
        case StackMachineOpcode::GotoIfEqual:
            top -= 2;
            if (*top == top[1])
            {
                op = &operations[op->value];
            }
            break;
        case StackMachineOpcode::GotoIfLessThan:
            top -= 2;
            if (*top < top[1])
            {
                op = &operations[op->value];
            }
            break;
        case StackMachineOpcode::GotoIfLessThanOrEqual:
            top -= 2;
            if (*top <= top[1])
            {
                op = &operations[op->value];
            }
            break;
        case StackMachineOpcode::Call:
            // Check stack overflow
            if (top + maxStackSizes[op->value] >= &*stack.end())
            {
                throw Exceptions::StackOverflowException(std::nullopt);
            }

            if (ptrTop + 2 >= &*ptrStack.end())
            {
                throw Exceptions::StackOverflowException(std::nullopt);
            }

            // Push current program counter
            *ptrTop = static_cast<int>(std::distance(&*operations.begin(), op));
            ptrTop++;

            // Push current stack bottom
            *ptrTop = static_cast<int>(std::distance(&*stack.begin(), bottom));
            ptrTop++;

            // Create new stack frame
            bottom = top;

            // Branch
            op = &operations[op->value];
            break;
        case StackMachineOpcode::Return:
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
            op = &*operations.begin() + *ptrTop;
            break;
        }
        case StackMachineOpcode::Halt:
        {
            // Store variable's values to ExecutionState
            for (size_t i = 0; i < variables.size(); i++)
            {
                state.GetVariableSource().Set(module.GetVariables()[i], variables[i]);
            }

            return top[-1];
        }
        case StackMachineOpcode::Lavel:
        default:
            UNREACHABLE();
            break;
        }
    }
}
