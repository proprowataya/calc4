#pragma once

#include "ExecutionState.h"
#include "Operators.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Support/ManagedStatic.h"
#include <cstdint>

template<typename TNumber, typename TVariableSource, typename TGlobalArraySource, typename TPrinter>
TNumber EvaluateByJIT(const CompilationContext& context,
                      ExecutionState<TNumber, TVariableSource, TGlobalArraySource, TPrinter>& state,
                      const std::shared_ptr<const Operator>& op, bool optimize, bool dumpProgram);
