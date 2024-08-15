/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2024 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include "ExecutionState.h"
#include "Operators.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Support/ManagedStatic.h"
#include <cstdint>

namespace calc4
{
struct JITCodeGenerationOption
{
    bool optimize = true;
    bool checkZeroDivision = false;
    bool dumpProgram = false;
};

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource,
         typename TInputSource, typename TPrinter>
TNumber EvaluateByJIT(
    const CompilationContext& context,
    ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TInputSource, TPrinter>& state,
    const std::shared_ptr<const Operator>& op, const JITCodeGenerationOption& option);
}
