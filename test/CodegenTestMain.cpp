/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include <gtest/gtest.h>

namespace
{
constexpr int CtestSkipReturnCode = 77;
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    int result = RUN_ALL_TESTS();

    // When executed under CTest, a GoogleTest "SKIP" still exits with code 0, which appears as
    // "Passed". Make skips visible by returning a dedicated exit code that CTest is configured to
    // treat as "Skipped".
    if (result == 0)
    {
        ::testing::UnitTest* ut = ::testing::UnitTest::GetInstance();

        // Only convert to "Skipped" when there were no failures.
        if (ut != nullptr && ut->skipped_test_count() > 0)
        {
            return CtestSkipReturnCode;
        }
    }

    return result;
}
