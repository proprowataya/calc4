/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "ExecutionTestCases.h"
#include "TestCommon.h"
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>

using ExecutionTestCase = TestCase<ExecutionTestCaseBase>;

namespace
{
struct ExecutionTestCaseBaseRange
{
    const ExecutionTestCaseBase* begin() const
    {
        return GetExecutionTestCaseBases();
    }

    const ExecutionTestCaseBase* end() const
    {
        return GetExecutionTestCaseBases() + GetExecutionTestCaseBaseCount();
    }
};
}

class ExecutionTest : public ::testing::TestWithParam<ExecutionTestCase>
{
};

#ifdef ENABLE_INT128
#define ASSERT_EQ_HELPER(TYPENAME, EXPECTED, ACTUAL)                                               \
    do                                                                                             \
    {                                                                                              \
        if constexpr (std::is_same_v<TYPENAME, __int128_t>)                                        \
        {                                                                                          \
            ASSERT_TRUE(static_cast<__int128_t>(EXPECTED) == (ACTUAL));                            \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            ASSERT_EQ(static_cast<TYPENAME>(EXPECTED), (ACTUAL));                                  \
        }                                                                                          \
    } while (false)
#else
#define ASSERT_EQ_HELPER(TYPENAME, EXPECTED, ACTUAL)                                               \
    ASSERT_EQ(static_cast<TYPENAME>(EXPECTED), (ACTUAL))
#endif // ENABLE_INT128

template<typename TNumber>
void OperateOneExecutionTest(const ExecutionTestCase& test)
{
    auto [result, variables, memory, consoleOutput] = Execute<TNumber>(
        test.input, test.standardInput, test.optimize, test.checkZeroDivision, test.executorType);

    const char* expectedConsoleOutput =
        test.expectedConsoleOutput != nullptr ? test.expectedConsoleOutput : "";

    // Validate execution result
    ASSERT_EQ_HELPER(TNumber, test.expected, result);

    // Validate variables
    for (auto& variable : test.expectedVariables)
    {
        ASSERT_EQ_HELPER(TNumber, variable.second, variables.Get(variable.first));
    }

    // Validate memory
    for (auto& memoryLocation : test.expectedMemory)
    {
        ASSERT_EQ_HELPER(TNumber, memoryLocation.second,
                         memory.Get(static_cast<TNumber>(memoryLocation.first)));
    }

    ASSERT_STREQ(expectedConsoleOutput, consoleOutput.c_str());
}

INSTANTIATE_TEST_SUITE_P(
    ExecutionTest, ExecutionTest,
    ::testing::ValuesIn(GenerateTestCases<ExecutionTestCase>(ExecutionTestCaseBaseRange{})));

TEST_P(ExecutionTest, ExecutionTest)
{
    auto& test = GetParam();

    switch (test.integerType)
    {
    case IntegerType::Int32:
        OperateOneExecutionTest<int32_t>(test);
        break;
    case IntegerType::Int64:
        OperateOneExecutionTest<int64_t>(test);
        break;

#ifdef ENABLE_INT128
    case IntegerType::Int128:
        OperateOneExecutionTest<__int128_t>(test);
        break;
#endif // ENABLE_INT128

#ifdef ENABLE_GMP
    case IntegerType::GMP:
        OperateOneExecutionTest<mpz_class>(test);
        break;
#endif // ENABLE_GMP

    default:
        break;
    }
}
