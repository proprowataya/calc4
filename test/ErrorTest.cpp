/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2022 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "Exceptions.h"
#include "TestCommon.h"
#include <functional>
#include <gtest/gtest.h>

using namespace calc4::Exceptions;

struct ErrorTestCaseBase
{
    const char* input;
    std::function<bool(const Calc4Exception&)> exceptionValidator;
    std::function<bool(IntegerType, ExecutorType, bool /* optimize*/, bool /* checkZeroDivision */)>
        enableThisTest;
};

using ErrorTestCase = TestCase<ErrorTestCaseBase>;

template<typename TException>
std::function<bool(const Calc4Exception&)> CreateValidator()
{
    return [](const Calc4Exception& e) { return dynamic_cast<const TException*>(&e) != nullptr; };
}

std::function<bool(IntegerType, ExecutorType, bool, bool)> EnableAllConfigurations()
{
    return [](IntegerType, ExecutorType, bool, bool) { return true; };
}

ErrorTestCaseBase ErrorTestCaseBases[] = {
    // clang-format off
    { "{notdefined}", CreateValidator<OperatorOrOperandNotDefinedException>(), EnableAllConfigurations() },
    { "D[op|x, y]", CreateValidator<DefinitionTextNotSplittedProperlyException>(), EnableAllConfigurations() },
    { "1+", CreateValidator<SomeOperandsMissingException>(), EnableAllConfigurations() },
    { "(1+2", CreateValidator<TokenExpectedException>(), EnableAllConfigurations() },
    { "1+2)", CreateValidator<UnexpectedTokenException>(), EnableAllConfigurations() },
    { "", CreateValidator<CodeIsEmptyException>(), EnableAllConfigurations() },
    { "D[x||{x}] {x}", CreateValidator<StackOverflowException>(),
      [](IntegerType, ExecutorType executor, bool optimize, bool) {
          return !optimize && executor == ExecutorType::StackMachine;
      } },
#ifndef _MSC_VER
    { "1/0", CreateValidator<ZeroDivisionException>(),
      [](IntegerType, ExecutorType executor, bool, bool checkZeroDivision) {
          return checkZeroDivision;
      } },
    { "1/(10 - 10)", CreateValidator<ZeroDivisionException>(),
      [](IntegerType, ExecutorType executor, bool, bool checkZeroDivision) {
          return checkZeroDivision;
      } },
    { "1/L", CreateValidator<ZeroDivisionException>(),
      [](IntegerType, ExecutorType executor, bool, bool checkZeroDivision) {
          return checkZeroDivision;
      } },
    { "1/(123@)", CreateValidator<ZeroDivisionException>(),
      [](IntegerType, ExecutorType executor, bool, bool checkZeroDivision) {
          return checkZeroDivision;
      } },
    { "D[getzero||0] 1/{getzero}", CreateValidator<ZeroDivisionException>(),
      [](IntegerType, ExecutorType executor, bool, bool checkZeroDivision) {
          return checkZeroDivision;
      } },
    { "1%0", CreateValidator<ZeroDivisionException>(),
      [](IntegerType, ExecutorType executor, bool, bool checkZeroDivision) {
          return checkZeroDivision;
      } },
    { "1%(10 - 10)", CreateValidator<ZeroDivisionException>(),
      [](IntegerType, ExecutorType executor, bool, bool checkZeroDivision) {
          return checkZeroDivision;
      } },
    { "1%L", CreateValidator<ZeroDivisionException>(),
      [](IntegerType, ExecutorType executor, bool, bool checkZeroDivision) {
          return checkZeroDivision;
      } },
    { "1%(123@)", CreateValidator<ZeroDivisionException>(),
      [](IntegerType, ExecutorType executor, bool, bool checkZeroDivision) {
          return checkZeroDivision;
      } },
    { "D[getzero||0] 1%{getzero}", CreateValidator<ZeroDivisionException>(),
      [](IntegerType, ExecutorType executor, bool, bool checkZeroDivision) {
          return checkZeroDivision;
      } },
#endif // !_MSC_VER
    // clang-format on
};

class ErrorTest : public ::testing::TestWithParam<ErrorTestCase>
{
};

template<typename TNumber>
void OperateOneErrorTest(const ErrorTestCase& test)
{
    if (!test.enableThisTest(test.integerType, test.executorType, test.optimize,
                             test.checkZeroDivision))
    {
        return;
    }

    try
    {
        Execute<TNumber>(test.input, test.optimize, test.checkZeroDivision, test.executorType);
        FAIL();
    }
    catch (Calc4Exception& e)
    {
        ASSERT_TRUE(test.exceptionValidator(e));
    }
    catch (...)
    {
        FAIL();
    }
}

INSTANTIATE_TEST_SUITE_P(ErrorTest, ErrorTest,
                         ::testing::ValuesIn(GenerateTestCases<ErrorTestCase>(ErrorTestCaseBases)));

TEST_P(ErrorTest, ErrorTest)
{
    auto& test = GetParam();

    switch (test.integerType)
    {
    case IntegerType::Int32:
        OperateOneErrorTest<int32_t>(test);
        break;
    case IntegerType::Int64:
        OperateOneErrorTest<int64_t>(test);
        break;

#ifdef ENABLE_INT128
    case IntegerType::Int128:
        OperateOneErrorTest<__int128_t>(test);
        break;
#endif // ENABLE_INT128

#ifdef ENABLE_GMP
    case IntegerType::GMP:
        OperateOneErrorTest<mpz_class>(test);
        break;
#endif // ENABLE_GMP

    default:
        break;
    }
}
