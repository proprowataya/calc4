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

#include <cstdint>
#include <vector>

#ifdef ENABLE_GMP
#include <gmpxx.h>
#endif // ENABLE_GMP

namespace
{
enum class ExecutorType
{
#ifdef ENABLE_JIT
    JIT,
#endif // ENABLE_JIT
    StackMachine,
    Interpreter,
};

enum class IntegerType
{
    Int32,
    Int64,
#ifdef ENABLE_INT128
    Int128,
#endif // ENABLE_INT128
#ifdef ENABLE_GMP
    GMP,
#endif // ENABLE_GMP
};

template<typename TTestCaseBase>
struct TestCase : public TTestCaseBase
{
    IntegerType integerType;
    ExecutorType executorType;
    bool optimize;

    TestCase(const TTestCaseBase& base, IntegerType integerType, ExecutorType executorType,
             bool optimize)
        : TTestCaseBase(base), integerType(integerType), executorType(executorType),
          optimize(optimize)
    {
    }
};

template<typename TTestCase, typename TContainer>
std::vector<TTestCase> GenerateTestCases(const TContainer& testCaseBases)
{
    std::vector<TTestCase> result;

    for (auto& base : testCaseBases)
    {
        for (auto optimize : { true, false })
        {
            for (auto executor : { ExecutorType::Interpreter, ExecutorType::StackMachine,
#ifdef ENABLE_JIT
                                   ExecutorType::JIT
#endif // ENABLE_JIT
                 })
            {
                result.emplace_back(base, IntegerType::Int32, executor, optimize);
                result.emplace_back(base, IntegerType::Int64, executor, optimize);

#ifdef ENABLE_INT128
                result.emplace_back(base, IntegerType::Int128, executor, optimize);
#endif // ENABLE_INT128

#ifdef ENABLE_GMP
#ifdef ENABLE_JIT
                if (executor != ExecutorType::JIT)
#endif // ENABLE_JIT
                {
                    result.emplace_back(base, IntegerType::GMP, executor, optimize);
                }
#endif // ENABLE_GMP
            }
        }
    }

    return result;
}

template<typename TNumber>
std::pair<TNumber, std::string> Execute(const char* source, bool optimize, ExecutorType executor)
{
    using namespace calc4;

    CompilationContext context;
    auto tokens = Lex(source, context);
    auto op = Parse(tokens, context);
    if (optimize)
    {
        op = Optimize<TNumber>(context, op);
    }

    TNumber result;
    std::string consoleOutput;
    BufferedPrinter printer(&consoleOutput);
    ExecutionState<TNumber, DefaultVariableSource<TNumber>, DefaultGlobalArraySource<TNumber>,
                   BufferedPrinter>
        state(printer);

    switch (executor)
    {
#ifdef ENABLE_JIT
    case ExecutorType::JIT:
#ifdef ENABLE_GMP
        if constexpr (std::is_same_v<TNumber, mpz_class>)
        {
            throw "JIT does not support GMP";
        }
        else
#endif // ENABLE_GMP
        {
            result = EvaluateByJIT<TNumber>(context, state, op, optimize, false);
        }
        break;
#endif // ENABLE_JIT
    case ExecutorType::StackMachine:
    {
        auto module = GenerateStackMachineModule<TNumber>(op, context);
        result = ExecuteStackMachineModule(module, state);
        break;
    }
    case ExecutorType::Interpreter:
        result = Evaluate(context, state, op);
        break;
    default:
        UNREACHABLE();
        break;
    }

    return std::make_pair(result, std::move(consoleOutput));
}
}
