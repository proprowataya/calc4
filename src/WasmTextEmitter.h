/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include "Operators.h"

#include <cstdint>
#include <ostream>
#include <string>

namespace calc4
{
struct WasmTextOptions
{
    // Imports for I/O (WAT cannot directly talk to the host; JS/Node will provide these).
    std::string importModule = "env";
    std::string importGetChar = "getchar"; // (result i32), returns -1 at EOF
    std::string importPutChar = "putchar"; // (param i32)

    // Imports for Calc4 global array (@ / ->) fallback path.
    // - Used when the array index is negative or outside the fast in-memory range.
    // - Signature depends on TNumber (i32 or i64).
    std::string importMemGet = "mem_get"; // (param T) (result T)
    std::string importMemSet = "mem_set"; // (param T T)

    // Exported entrypoint function name.
    std::string mainExportName = "main";

    // Linear memory used for Calc4's global array fast path.
    // - Indices in [0, fastMemoryLimitElements) are accessed directly via linear memory.
    // - Other indices use the fallback imports above (allows negative indices and sparse/unbounded
    // behavior).
    uint32_t fastMemoryLimitElements = 131072; // default fits in 16 pages for i64 (1MiB / 8)
    uint32_t fastMemoryBaseOffsetBytes = 0;    // base offset in bytes within linear memory

    // Minimum linear memory size for the fast region.
    // The emitter may increase this value to ensure the fast region fits.
    uint32_t memoryMinPages = 16; // 1MiB by default
    bool exportMemory = true;
    std::string memoryExportName = "memory";

    // Naming prefixes (helps avoid collisions)
    std::string funcPrefix = "user_defined_operator_";
    std::string globalVarPrefix = "user_defined_var_";
};

template<typename TNumber>
void EmitWatCode(const std::shared_ptr<const Operator>& mainOp, const CompilationContext& context,
                 std::ostream& os, const WasmTextOptions& opt = {});
}
