/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include "Common.h"
#include "ExecutionState.h"
#include "Operators.h"
#include <numeric>
#include <string>
#include <vector>

namespace calc4
{
enum class StackMachineOpcode : int8_t
{
    Push,
    Pop,
    LoadConst,
    LoadConstTable,
    LoadArg,
    StoreArg,
    LoadVariable,
    StoreVariable,
    LoadArrayElement,
    StoreArrayElement,
    Input,
    PrintChar,
    Add,
    Sub,
    Mult,
    Div,
    DivChecked,
    Mod,
    ModChecked,
    Goto,
    GotoIfTrue,
    GotoIfEqual,
    GotoIfLessThan,
    GotoIfLessThanOrEqual,
    Call,
    Return,
    Halt,
    Lavel,
};

struct StackMachineOperation
{
    using ValueType = int16_t;

    StackMachineOpcode opcode;
    ValueType value;

    StackMachineOperation() : opcode(static_cast<StackMachineOpcode>(0)), value(0) {}

    StackMachineOperation(StackMachineOpcode opcode, ValueType value = 0)
        : opcode(opcode), value(value)
    {
    }
};

struct StackMachineUserDefinedOperator
{
private:
    OperatorDefinition definition;
    std::vector<StackMachineOperation> operations;
    int maxStackSize;

public:
    StackMachineUserDefinedOperator(const OperatorDefinition& definition,
                                    const std::vector<StackMachineOperation>& operations,
                                    int maxStackSize)
        : definition(definition), operations(operations), maxStackSize(maxStackSize)
    {
    }

    const OperatorDefinition& GetDefinition() const
    {
        return definition;
    }

    const std::vector<StackMachineOperation>& GetOperations() const
    {
        return operations;
    }

    int GetMaxStackSize() const
    {
        return maxStackSize;
    }
};

template<typename TNumber>
class StackMachineModule
{
private:
    std::vector<StackMachineOperation> entryPoint;
    std::vector<TNumber> constTable;
    std::vector<StackMachineUserDefinedOperator> userDefinedOperators;
    std::vector<std::string> variables;

public:
    StackMachineModule(const std::vector<StackMachineOperation>& entryPoint,
                       const std::vector<TNumber>& constTable,
                       const std::vector<StackMachineUserDefinedOperator>& userDefinedOperators,
                       const std::vector<std::string>& variables)
        : entryPoint(entryPoint), constTable(constTable),
          userDefinedOperators(userDefinedOperators), variables(variables)
    {
    }

    std::pair<std::vector<StackMachineOperation>, std::vector<int>> FlattenOperations() const;

    const std::vector<StackMachineOperation>& GetEntryPoint() const
    {
        return entryPoint;
    }

    const std::vector<TNumber>& GetConstTable() const
    {
        return constTable;
    }

    const std::vector<StackMachineUserDefinedOperator>& GetUserDefinedOperators() const
    {
        return userDefinedOperators;
    }

    const std::vector<std::string>& GetVariables() const
    {
        return variables;
    }
};

struct StackMachineCodeGenerationOption
{
    bool checkZeroDivision = false;
};

namespace
{
inline constexpr const char* ToString(StackMachineOpcode opcode)
{
    switch (opcode)
    {
    case StackMachineOpcode::Push:
        return "Push";
    case StackMachineOpcode::Pop:
        return "Pop";
    case StackMachineOpcode::LoadConst:
        return "LoadConst";
    case StackMachineOpcode::LoadConstTable:
        return "LoadConstTable";
    case StackMachineOpcode::LoadArg:
        return "LoadArg";
    case StackMachineOpcode::StoreArg:
        return "StoreArg";
    case StackMachineOpcode::LoadVariable:
        return "LoadVariable";
    case StackMachineOpcode::StoreVariable:
        return "StoreVariable";
    case StackMachineOpcode::LoadArrayElement:
        return "LoadArrayElement";
    case StackMachineOpcode::StoreArrayElement:
        return "StoreArrayElement";
    case StackMachineOpcode::Input:
        return "Input";
    case StackMachineOpcode::PrintChar:
        return "PrintChar";
    case StackMachineOpcode::Add:
        return "Add";
    case StackMachineOpcode::Sub:
        return "Sub";
    case StackMachineOpcode::Mult:
        return "Mult";
    case StackMachineOpcode::Div:
        return "Div";
    case StackMachineOpcode::DivChecked:
        return "DivChecked";
    case StackMachineOpcode::Mod:
        return "Mod";
    case StackMachineOpcode::ModChecked:
        return "ModChecked";
    case StackMachineOpcode::Goto:
        return "Goto";
    case StackMachineOpcode::GotoIfTrue:
        return "GotoIfTrue";
    case StackMachineOpcode::GotoIfEqual:
        return "GotoIfEqual";
    case StackMachineOpcode::GotoIfLessThan:
        return "GotoIfLessThan";
    case StackMachineOpcode::GotoIfLessThanOrEqual:
        return "GotoIfLessThanOrEqual";
    case StackMachineOpcode::Call:
        return "Call";
    case StackMachineOpcode::Return:
        return "Return";
    case StackMachineOpcode::Halt:
        return "Halt";
    case StackMachineOpcode::Lavel:
        return "Lavel";
    default:
        return "<Unknown>";
    }
}
}

template<typename TNumber>
StackMachineModule<TNumber> GenerateStackMachineModule(
    const std::shared_ptr<const Operator>& op, const CompilationContext& context,
    const StackMachineCodeGenerationOption& option);

template<typename TNumber, typename TVariableSource = DefaultVariableSource<TNumber>,
         typename TGlobalArraySource = DefaultGlobalArraySource<TNumber>,
         typename TInputSource = DefaultInputSource, typename TPrinter = DefaultPrinter,
         typename TStackArray = std::vector<TNumber>, typename TPtrStackArray = std::vector<int>>
TNumber ExecuteStackMachineModule(
    const StackMachineModule<TNumber>& module,
    ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TInputSource, TPrinter>& state);
}
