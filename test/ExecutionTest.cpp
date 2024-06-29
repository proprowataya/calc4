/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2024 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "TestCommon.h"
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>

struct ExecutionTestCaseBase
{
    const char* input;
    int32_t expected;
    const char* expectedConsoleOutput;
    std::unordered_map<std::string, int> expectedVariables;
    std::unordered_map<int, int> expectedMemory;
};

using ExecutionTestCase = TestCase<ExecutionTestCaseBase>;

ExecutionTestCaseBase ExecutionTestCaseBases[] = {
    // clang-format off
    { "1<2", 1 },
    { "1<=2", 1 },
    { "1>=2", 0 },
    { "1>2", 0 },
    { "2<1", 0 },
    { "2<=1", 0 },
    { "2>=1", 1 },
    { "2>1", 1 },
    { "1<1", 0 },
    { "1<=1", 1 },
    { "1>=1", 1 },
    { "1>1", 0 },
    { "12345678", 12345678 },
    { "1+2*3-10", -1 },
    { "0?1?2?3?4", 3 },
    { "72P101P108P108P111P10P", 0, "Hello\n" },
    { "1+// C++ style comment\n2", 3 },
    { "1+/* C style comment*/2", 3 },
    { "D[print||72P101P108P108P111P10P] {print}", 0, "Hello\n" },
    { "D[add|x,y|x+y] 12{add}23", 35 },
    { "D[get12345||12345] {get12345}+{get12345}", 24690 },
    { "D[fact|x,y|x==0?y?(x-1){fact}(x*y)] 10{fact}1", 3628800 },
    { "D[fib|n|n<=1?n?(n-1){fib}+(n-2){fib}] 10{fib}", 55 },
    { "D[fibImpl|x,a,b|x ? ((x-1) ? ((x-1){fibImpl}(a+b){fibImpl}a) ? a) ? b] D[fib|x|x{fibImpl}1{fibImpl}0] 10{fib}", 55 },
    { "D[f|a,b,p,q,c|c < 2 ? ((a*p) + (b*q)) ? (c % 2 ? ((a*p) + (b*q) {f} (a*q) + (b*q) + (b*p) {f} (p*p) + (q*q) {f} (2*p+q)*q {f} c/2) ? (a {f} b {f} (p*p) + (q*q) {f} (2*p+q)*q {f} c/2))] D[fib|n|0{f}1{f}0{f}1{f}n] 10{fib}", 55 },
    { "D[tarai|x,y,z|x <= y ? y ? (((x - 1){tarai}y{tarai}z){tarai}((y - 1){tarai}z{tarai}x){tarai}((z - 1){tarai}x{tarai}y))] 10{tarai}5{tarai}5", 5 },
    { "1S", 1 },
    { "L", 0 },
    { "1S[var]", 1 },
    { "L[var]", 0 },
    { "D[get||L[var]] D[set|x|xS[var]] 123{set} {get} * {get}", 15129 },
    { "D[set|x|xS] 7{set}L", 7 },
    { "D[set|x|xS] 7{set}LS[var1] L[zero]3{set}LS[var2] L[var1]*L[var2]", 21 },
    { "(123S)L*L", 15129 },
    { "(123S[var])L[var]*L[var]", 15129 },
    { "((100+20+3)S)L*L", 15129, nullptr, { { "", 123 } } },
    { "((100+20+3)S[var])L[var]*L[var]", 15129, nullptr, { { "var", 123 } } },
    { "D[op||(123S)L*L]{op}", 15129 },
    { "D[op||L*L](123S){op}", 15129 },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] (20{fib}S)+L", 13530, nullptr, { { "", 6765 } } },
    { "D[get||L] D[set|x|xS] D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] (20{fib}>=1000?10?5)S {get}", 10, nullptr, { { "", 10 } } },
    { "D[get||L] D[set|x|xS] D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] (20{fib}>=1000?10S?5S) {get}", 10, nullptr, { { "", 10 } } },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] D[fib2||L{fib}] D[set|x|xS] 3{set} {fib2}", 2 },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] D[fib2||L{fib}] D[set|x|xS] 20{set} {fib2}", 6765 },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] D[fib2||L{fib}] D[set|x|xS] 3S {fib2}", 2 },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] D[fib2||L{fib}] D[set|x|xS] 20S {fib2}", 6765 },
    { "D[fib|n|10S(n<=1?n?((n-1){fib}+(n-2){fib}))S] 20{fib} L", 6765, nullptr, { { "", 6765 } } },
    { "0@", 0 },
    { "5->0", 5, nullptr, {}, { { 0, 5 } } },
    { "(10->20)L[zero]20@", 10, nullptr, {}, { { 20, 10 } } },
    { "((4+6)->(10+10))(20@)", 10, nullptr, {}, { { 20, 10 } } },
    { "D[func||(10->20)L[zero]20@] {func} (20@)", 10, nullptr, {}, { { 20, 10 } } },
    { "D[func||((4+6)->(10+10))(20@)] {func} (20@)", 10, nullptr, {}, { { 20, 10 } } },
    { "D[func||(10->20)L[zero]20@] D[get||20@] {func} (20@)", 10, nullptr, {}, { { 20, 10 } } },
    { "D[func||((4+6)->(10+10))(20@)] D[get||20@] {func} {get}", 10, nullptr, {}, { { 20, 10 } } },
    // clang-format on
};

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
    auto [result, variables, memory, consoleOutput] =
        Execute<TNumber>(test.input, test.optimize, test.checkZeroDivision, test.executorType);

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
    ::testing::ValuesIn(GenerateTestCases<ExecutionTestCase>(ExecutionTestCaseBases)));

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
