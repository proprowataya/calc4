/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2022 Yuya Watari
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
template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
TNumber EvaluateByJIT(const CompilationContext& context,
                      ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>& state,
                      const std::shared_ptr<const Operator>& op, bool optimize, bool dumpProgram);
}
