#pragma once

#include <cstdint>
#include "llvm/Support/ManagedStatic.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "Operators.h"

template<typename TNumber>
TNumber EvaluateByJIT(const CompilationContext &context, const std::shared_ptr<Operator> &op, bool optimize, bool printInfo);
