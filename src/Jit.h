#pragma once

#include "Operators.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Support/ManagedStatic.h"
#include <cstdint>

template<typename TNumber>
TNumber EvaluateByJIT(const CompilationContext& context, const std::shared_ptr<Operator>& op,
                      bool optimize, bool printInfo);
