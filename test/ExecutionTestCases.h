/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

struct ExecutionTestCaseBase
{
    const char* input;
    const char* standardInput;
    int32_t expected;
    const char* expectedConsoleOutput = nullptr;
    std::unordered_map<std::string, int> expectedVariables;
    std::unordered_map<int, int> expectedMemory;
};

const ExecutionTestCaseBase* GetExecutionTestCaseBases();
size_t GetExecutionTestCaseBaseCount();
