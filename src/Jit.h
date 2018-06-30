#pragma once

#include <cstdint>
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "Operators.h"

template<typename TNumber>
TNumber RunByJIT(const CompilationContext<TNumber> &context, const std::shared_ptr<Operator<TNumber>> &op, bool optimize, bool printInfo);
