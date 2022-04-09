/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2022 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include "Evaluator.h"
#include "Exceptions.h"
#include "Operators.h"
#include "Optimizer.h"
#include "StackMachine.h"
#include "SyntaxAnalysis.h"

#ifdef ENABLE_JIT
#include "Jit.h"
#endif // ENABLE_JIT

#include <chrono>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unordered_map>

namespace calc4
{
namespace
{
/*****
 * Definitions
 *****/

inline constexpr const char* Indent = "    ";

#ifdef ENABLE_GMP
inline constexpr const int InfinitePrecisionIntegerSize = std::numeric_limits<int>::max();
#endif // ENABLE_GMP

enum class ExecutorType
{
#ifdef ENABLE_JIT
    JIT,
#endif // ENABLE_JIT
    StackMachine,
    TreeTraversal,
};

enum class TreeTraversalExecutorMode
{
    Never,
    WhenNoRecursiveOperators,
    Always,
};

struct Option
{
    int integerSize = 64;

    ExecutorType executorType =
#ifdef ENABLE_JIT
        ExecutorType::JIT;
#else
        ExecutorType::StackMachine;
#endif // ENABLE_JIT

    TreeTraversalExecutorMode treeExecutorMode =
        TreeTraversalExecutorMode::WhenNoRecursiveOperators;
    bool optimize = true;
    bool checkZeroDivision = true;
    bool dumpProgram = false;
    bool emitCpp = false;
};

/*****
 * Helper functions to print program structures
 *****/

bool HasRecursiveCallInternal(const std::shared_ptr<const Operator>& op,
                              const CompilationContext& context,
                              std::unordered_map<const OperatorDefinition*, int>& called)
{
    if (auto userDefined = std::dynamic_pointer_cast<const UserDefinedOperator>(op))
    {
        auto& definition = userDefined->GetDefinition();
        if (++called[&definition] > 1)
        {
            // Recursive call detected
            return true;
        }

        if (HasRecursiveCallInternal(
                context.GetOperatorImplement(definition.GetName()).GetOperator(), context, called))
        {
            --called[&definition];
            return true;
        }
        else
        {
            --called[&definition];
        }
    }
    else if (auto parenthesis = std::dynamic_pointer_cast<const ParenthesisOperator>(op))
    {
        for (auto& op2 : parenthesis->GetOperators())
        {
            if (HasRecursiveCallInternal(op2, context, called))
            {
                return true;
            }
        }
    }

    for (auto& operand : op->GetOperands())
    {
        if (HasRecursiveCallInternal(operand, context, called))
        {
            return true;
        }
    }

    return false;
}

bool HasRecursiveCall(const std::shared_ptr<const Operator>& op, const CompilationContext& context)
{
    std::unordered_map<const OperatorDefinition*, int> called;
    return HasRecursiveCallInternal(op, context, called);
}

void PrintTreeCore(const std::shared_ptr<const Operator>& op, int depth, std::ostream& out)
{
    using std::endl;

    auto PrintIndent = [depth, &out]() {
        for (int i = 0; i < depth; i++)
        {
            out << Indent;
        }
    };

    PrintIndent();

    out << op->ToString() << endl;
    for (auto& operand : op->GetOperands())
    {
        PrintTreeCore(operand, depth + 1, out);
    }

    if (auto parenthesis = std::dynamic_pointer_cast<const ParenthesisOperator>(op))
    {
        PrintIndent();
        out << "Contains:" << endl;
        for (auto& op2 : parenthesis->GetOperators())
        {
            PrintTreeCore(op2, depth + 1, out);
        }
    }
}

void PrintTree(const CompilationContext& context, const std::shared_ptr<const Operator>& op,
               std::ostream& out)
{
    using std::endl;

    out << "/*" << endl << " * Tree" << endl << " */" << endl << "{" << endl << "Main:" << endl;
    PrintTreeCore(op, 1, out);

    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        auto& name = it->second.GetDefinition().GetName();
        auto& op = it->second.GetOperator();

        out << endl << "Operator \"" << name << "\":" << endl;
        PrintTreeCore(op, 1, out);
    }

    out << "}" << endl << endl;
}

void PrintStackMachineOperations(const std::vector<StackMachineOperation>& operations,
                                 std::ostream& out)
{
    static constexpr int AddressWidth = 6;
    static constexpr int OpcodeWidth = 25;

    for (size_t i = 0; i < operations.size(); i++)
    {
        out << std::right << std::setw(AddressWidth) << i << ": ";
        out << std::left << std::setw(OpcodeWidth) << ToString(operations[i].opcode);
        out << " [Value = " << operations[i].value << "]" << std::endl;
    }
}

template<typename TNumber>
void PrintStackMachineModule(const StackMachineModule<TNumber>& module, std::ostream& out)
{
    using std::endl;

    out << "/*" << endl << " * Stack Machine Codes" << endl << " */" << endl << "{" << endl;

    out << "Main:" << endl;
    PrintStackMachineOperations(module.GetEntryPoint(), out);

    auto& userDefinedOperators = module.GetUserDefinedOperators();
    for (size_t i = 0; i < userDefinedOperators.size(); i++)
    {
        auto& userDefined = userDefinedOperators[i];
        out << "Operator \"" << userDefined.GetDefinition().GetName() << "\""
            << " (No = " << i << ")" << endl;
        PrintStackMachineOperations(userDefined.GetOperations(), out);
    }

    auto& constants = module.GetConstTable();
    if (!constants.empty())
    {
        out << "Constants:";

        for (size_t i = 0; i < constants.size(); i++)
        {
            out << (i == 0 ? " " : ", ") << "[" << i << "] = " << constants[i];
        }

        out << endl;
    }

    out << "}" << endl << endl;
}

/*****
 * Core part of execution
 *****/

template<typename TNumber>
std::shared_ptr<const Operator> SyntaxAnalysis(std::string_view source, const char* filePath,
                                               CompilationContext& context, const Option& option,
                                               std::ostream& out)
{
    // We make a copy of the given CompilationContext so that it will not be destroyed if some
    // error occurs
    auto copyOfContext = context;
    auto tokens = Lex(source, copyOfContext);
    auto op = Parse(tokens, copyOfContext);
    if (option.optimize)
    {
        op = Optimize<TNumber>(copyOfContext, op);
    }

    // All compilation is complete, so we can update the given CompilationContext
    context = std::move(copyOfContext);
    return op;
}

template<typename TNumber, typename TVariableSource = DefaultVariableSource<TNumber>,
         typename TGlobalArraySource = DefaultGlobalArraySource<TNumber>,
         typename TPrinter = DefaultPrinter>
TNumber ExecuteOperator(
    const std::shared_ptr<const Operator>& op, const CompilationContext& context,
    ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>& state,
    const Option& option, std::ostream& out)
{
    // Determine actual executor
    ExecutorType actualExecutor = option.executorType;
    if (option.executorType != ExecutorType::TreeTraversal &&
        option.treeExecutorMode != TreeTraversalExecutorMode::Never &&
        !HasRecursiveCall(op, context))
    {
        // The given program has no heavy loops, so we use tree traversal executor.
        actualExecutor = ExecutorType::TreeTraversal;
    }

    switch (actualExecutor)
    {
#ifdef ENABLE_JIT
    case ExecutorType::JIT:
#ifdef ENABLE_GMP
        if constexpr (std::is_same_v<TNumber, mpz_class>)
        {
            throw Exceptions::AssertionErrorException(
                std::nullopt, "Jit compiler does not support infinite precision integers.");
        }
        else
#endif // ENABLE_GMP
        {
            return EvaluateByJIT<TNumber>(
                context, state, op,
                { option.optimize, option.checkZeroDivision, option.dumpProgram });
        }
        break;
#endif // ENABLE_JIT
    case ExecutorType::StackMachine:
    {
        auto module =
            GenerateStackMachineModule<TNumber>(op, context, { option.checkZeroDivision });

        if (option.dumpProgram)
        {
            PrintStackMachineModule(module, out);
        }

        return ExecuteStackMachineModule(module, state);
    }
    case ExecutorType::TreeTraversal:
        return Evaluate<TNumber>(context, state, op);
    default:
        UNREACHABLE();
        return 0;
    }
}

template<typename TNumber, typename TVariableSource = DefaultVariableSource<TNumber>,
         typename TGlobalArraySource = DefaultGlobalArraySource<TNumber>,
         typename TPrinter = DefaultPrinter>
void ExecuteSource(std::string_view source, const char* filePath, CompilationContext& context,
                   ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>& state,
                   const Option& option, std::ostream& out)
{
    using namespace std;

#ifdef ENABLE_JIT
    using namespace llvm;
#endif // ENABLE_JIT

    try
    {
        auto start = chrono::high_resolution_clock::now();
        std::shared_ptr<const Operator> op =
            SyntaxAnalysis<TNumber>(source, filePath, context, option, out);

        if (option.dumpProgram)
        {
            out << "Has recursive call: " << (HasRecursiveCall(op, context) ? "True" : "False")
                << endl
                << endl;
            PrintTree(context, op, out);
        }

        if (option.emitCpp)
        {
            assert(filePath != nullptr);

            std::string outputFilePath = filePath;
            auto index = outputFilePath.find_last_of('.');
            if (index != std::string_view::npos)
            {
                outputFilePath.erase(index);
            }
            outputFilePath += ".cpp";

            std::ofstream ofs(outputFilePath);
            EmitCppCode<TNumber>(op, context, ofs);
        }
        else
        {
            TNumber result = ExecuteOperator(op, context, state, option, out);
            auto end = chrono::high_resolution_clock::now();

            out << result << endl
                << "Elapsed: " << (chrono::duration<double>(end - start).count() * 1000) << " ms"
                << endl;
        }
    }
    catch (Exceptions::Calc4Exception& error)
    {
        auto& position = error.GetPosition();
        if (position)
        {
            if (filePath != nullptr)
            {
                out << filePath << ":";
            }

            out << (position->lineNo + 1) << ":" << (position->charNo + 1) << ": ";
        }

        out << "Error: " << error.what() << endl;

        if (position)
        {
            size_t lineStartIndex = source.substr(0, position->index).find_last_of("\r\n");
            lineStartIndex = lineStartIndex == source.npos ? 0 : (lineStartIndex + 1);

            size_t lineEndIndex = source.find_first_of("\r\n", position->index);
            lineEndIndex = lineEndIndex == source.npos ? source.length() : lineEndIndex;

            std::string_view line = source.substr(lineStartIndex, lineEndIndex - lineStartIndex);

            static constexpr int LineNoWidth = 8;
            static constexpr std::string_view Splitter = " | ";
            out << std::right << std::setw(LineNoWidth) << (position->lineNo + 1) << Splitter
                << line << endl;
            for (int i = 0;
                 i < LineNoWidth + static_cast<int>(Splitter.length()) + position->charNo; i++)
            {
                out << ' ';
            }
            out << '^' << endl;
        }
    }
    catch (std::exception& e)
    {
        out << "Fatal error: " << e.what() << endl;
    }
    catch (...)
    {
        out << "Fatal error" << endl;
    }
}
}
}
