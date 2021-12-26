#pragma once

#include "Operators.h"

template<typename TNumber>
std::shared_ptr<const Operator> Optimize(CompilationContext& context,
                                         const std::shared_ptr<const Operator>& op);
