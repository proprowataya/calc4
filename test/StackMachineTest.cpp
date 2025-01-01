/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2025 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "StackMachine.h"
#include "TestCommon.h"
#include <gtest/gtest.h>
#include <string_view>

// Assert ToString(StackMachineOpcode opcode) does not return an invalid string for all opcodes
TEST(StackMachineTest, ToStringTest)
{
    using namespace calc4;

    // Get an invalid string with a dummy value
    std::string_view unknownText = ToString(static_cast<StackMachineOpcode>(-1));

    // Here, we assume that StackMachineOpcode::Lavel is the last opcode
    for (int i = 0; i <= static_cast<int>(StackMachineOpcode::Lavel); i++)
    {
        // The returned text should never be an invalid string
        ASSERT_NE(unknownText, ToString(static_cast<StackMachineOpcode>(i)));
    }
}
