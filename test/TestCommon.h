/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
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

template<typename TNumber>
struct ExecutionResult
{
    TNumber result;
    calc4::DefaultVariableSource<TNumber> variables;
    calc4::DefaultGlobalArraySource<TNumber> memory;
    std::string consoleOutput;
};

template<typename TTestCaseBase>
struct TestCase : public TTestCaseBase
{
    IntegerType integerType;
    ExecutorType executorType;
    bool optimize;
    bool checkZeroDivision;

    TestCase(const TTestCaseBase& base, IntegerType integerType, ExecutorType executorType,
             bool optimize, bool checkZeroDivision)
        : TTestCaseBase(base), integerType(integerType), executorType(executorType),
          optimize(optimize), checkZeroDivision(checkZeroDivision)
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
            for (auto checkZeroDivision : { true, false })
            {
                for (auto executor : { ExecutorType::Interpreter, ExecutorType::StackMachine,
#ifdef ENABLE_JIT
                                       ExecutorType::JIT
#endif // ENABLE_JIT
                     })
                {
                    result.emplace_back(base, IntegerType::Int32, executor, optimize,
                                        checkZeroDivision);
                    result.emplace_back(base, IntegerType::Int64, executor, optimize,
                                        checkZeroDivision);

#ifdef ENABLE_INT128
                    result.emplace_back(base, IntegerType::Int128, executor, optimize,
                                        checkZeroDivision);
#endif // ENABLE_INT128

#ifdef ENABLE_GMP
#ifdef ENABLE_JIT
                    if (executor != ExecutorType::JIT)
#endif // ENABLE_JIT
                    {
                        result.emplace_back(base, IntegerType::GMP, executor, optimize,
                                            checkZeroDivision);
                    }
#endif // ENABLE_GMP
                }
            }
        }
    }

    return result;
}

template<typename TNumber>
ExecutionResult<TNumber> Execute(const char* source, const char* standardInput, bool optimize,
                                 bool checkZeroDivision, ExecutorType executor)
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
    BufferedInputSource inputSource(standardInput);
    BufferedPrinter printer(&consoleOutput);
    ExecutionState<TNumber, DefaultVariableSource<TNumber>, DefaultGlobalArraySource<TNumber>,
                   BufferedInputSource, BufferedPrinter>
        state(inputSource, printer);

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
            result =
                EvaluateByJIT<TNumber>(context, state, op, { optimize, checkZeroDivision, false });
        }
        break;
#endif // ENABLE_JIT
    case ExecutorType::StackMachine:
    {
        auto module = GenerateStackMachineModule<TNumber>(op, context, { checkZeroDivision });
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

    return { result, state.GetVariableSource(), state.GetArraySource(), std::move(consoleOutput) };
}
}
