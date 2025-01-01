/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2025 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include "Operators.h"
#include <ostream>

namespace calc4
{
template<typename TNumber>
void EmitCppCode(const std::shared_ptr<const Operator>& op, const CompilationContext& context,
                 std::ostream& ostream);
}
