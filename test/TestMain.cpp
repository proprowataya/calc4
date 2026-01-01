/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "TestCommon.h"
#include <gtest/gtest.h>

class Environment : public ::testing::Environment
{
public:
    ~Environment() override {}

    // Override this to define how to set up the environment.
    void SetUp() override
    {
#ifdef ENABLE_JIT
        LLVMInitializeNativeTarget();
        LLVMInitializeNativeAsmPrinter();
        LLVMInitializeNativeAsmParser();
#endif // ENABLE_JIT
    }

    // Override this to define how to tear down the environment.
    void TearDown() override
    {
#ifdef ENABLE_JIT
        llvm::llvm_shutdown();
#endif // ENABLE_JIT
    }
};

::testing::Environment* const environment = testing::AddGlobalTestEnvironment(new Environment);
