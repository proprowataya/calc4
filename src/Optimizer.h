﻿/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2025 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include "Operators.h"

namespace calc4
{
template<typename TNumber>
std::shared_ptr<const Operator> Optimize(CompilationContext& context,
                                         const std::shared_ptr<const Operator>& op);
}
