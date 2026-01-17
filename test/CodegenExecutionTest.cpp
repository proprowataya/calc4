/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "CodegenTestCommon.h"
#include "CppEmitter.h"
#include "ExecutionTestCases.h"
#include "TestCommon.h"
#include "WasmTextEmitter.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
struct Program
{
    const ExecutionTestCaseBase* testcase;
    bool optimize;
    std::string expectedOutput;
};

std::string GetExpectedOutput(const ExecutionTestCaseBase& base)
{
    std::string result;

    if (base.expectedConsoleOutput != nullptr)
    {
        result += base.expectedConsoleOutput;
    }
    result += std::to_string(base.expected);
    result += '\n';

    return result;
}

std::vector<Program> MakeAllPrograms()
{
    std::vector<Program> programs;
    const auto* bases = GetExecutionTestCaseBases();
    const size_t baseCount = GetExecutionTestCaseBaseCount();
    programs.reserve(baseCount * 2);

    for (size_t i = 0; i < baseCount; i++)
    {
        const auto& base = bases[i];
        std::string expectedOutput = GetExpectedOutput(base);
        programs.push_back({ &base, /* optimize */ false, expectedOutput });
        programs.push_back({ &base, /* optimize */ true, expectedOutput });
    }

    return programs;
}

template<typename TNumber>
std::string GenerateWat(const Program& p)
{
    using namespace calc4;

    CompilationContext context;
    auto tokens = Lex(p.testcase->input, context);
    auto op = Parse(tokens, context);

    if (p.optimize)
    {
        op = Optimize<TNumber>(context, op);
    }

    std::ostringstream wat;
    WasmTextOptions wopt;
    EmitWatCode<TNumber>(op, context, wat, wopt);
    return wat.str();
}

template<typename TNumber>
std::string GenerateCpp(const Program& p)
{
    using namespace calc4;

    CompilationContext context;
    auto tokens = Lex(p.testcase->input, context);
    auto op = Parse(tokens, context);

    if (p.optimize)
    {
        op = Optimize<TNumber>(context, op);
    }

    std::ostringstream cpp;
    EmitCppCode<TNumber>(op, context, cpp);
    return cpp.str();
}
}

TEST(CodegenWasmIntegrationTest, Smoke)
{
    namespace fs = std::filesystem;
    using namespace std::literals::string_literals;
    using namespace calc4;

    const fs::path scriptPath = fs::path(CODEGEN_TEST_SOURCE_DIR) / "tools" / "run_wat.mjs";

    if (!fs::exists(scriptPath))
    {
        FAIL() << "run_wat.mjs not found: " << scriptPath.string();
    }

    const std::string_view node = GetNodeExecutable();
    const std::string_view wat2wasm = GetWat2WasmExecutable();

    // Check Node availability early.
    {
        const fs::path tmp = CreateTempDirectory("calc4_codegen_node_check_");
        const fs::path out = tmp / "node_version.txt";

        CommandResult r = RunCommandRedirectToFile({ node, "--version" }, out);
        if (r.exitCode != 0)
        {
            FAIL() << "Node.js is not available (cannot run '" << node << " --version'). Output:\n"
                   << r.output;
        }
    }

    const fs::path tmp = CreateTempDirectory("calc4_codegen_wasm_");

    std::vector<Program> programs = MakeAllPrograms();

    auto RunForIntegerType = [&](const std::string& typeName, auto generateWat) {
        for (size_t i = 0; i < programs.size(); i++)
        {
            auto& p = programs[i];
            SCOPED_TRACE("WASM source: \""s + p.testcase->input + "\", Optimize: " +
                         (p.optimize ? "true" : "false") + ", IntegerType: " + typeName);

            const std::string wat = generateWat(p);
            const fs::path watPath =
                tmp / ("program_"s + typeName + "_" + std::to_string(i) + ".wat");
            const fs::path stdinPath =
                tmp / ("stdin_"s + typeName + "_" + std::to_string(i) + ".txt");
            const fs::path outPath = tmp / ("out_"s + typeName + "_" + std::to_string(i) + ".txt");

            WriteTextFile(watPath, wat);
            WriteTextFile(stdinPath, p.testcase->standardInput ? p.testcase->standardInput : "");

            CommandResult r =
                RunCommandRedirectToFile({ node, scriptPath.string(), "--wat", watPath.string(),
                                           "--stdin", stdinPath.string(), "--wat2wasm", wat2wasm },
                                         outPath);

            // Exit code 2 is a special signal: a required dependency (wat2wasm) is missing.
            if (r.exitCode == 2)
            {
                FAIL() << r.output;
            }

            ASSERT_EQ(r.exitCode, 0) << "Command failed.\nCommand: " << r.command << "\nOutput:\n"
                                     << r.output;

            ASSERT_EQ(p.expectedOutput, r.output);
        }
    };

    RunForIntegerType("i32", [](const Program& p) { return GenerateWat<int32_t>(p); });
    RunForIntegerType("i64", [](const Program& p) { return GenerateWat<int64_t>(p); });
}

TEST(CodegenCppIntegrationTest, Smoke)
{
#if defined(_MSC_VER)
    FAIL() << "C++ codegen integration test is not available on MSVC toolchains.";
#else
    namespace fs = std::filesystem;
    using namespace std::literals::string_literals;
    using namespace calc4;

    const std::string_view compiler = GetCxxCompiler();

    // Basic compiler availability check.
    {
        const fs::path tmp = CreateTempDirectory("calc4_codegen_cxx_check_");
        const fs::path out = tmp / "cxx_version.txt";

        CommandResult r = RunCommandRedirectToFile({ compiler, "--version" }, out);
        if (r.exitCode != 0)
        {
            FAIL() << "C++ compiler is not available (cannot run '" << compiler
                   << " --version'). Output:\n"
                   << r.output;
        }
    }

    const fs::path tmp = CreateTempDirectory("calc4_codegen_cpp_");

    std::vector<Program> programs = MakeAllPrograms();

    auto RunForIntegerType = [&](const std::string& typeName, auto generateCpp) {
        for (size_t i = 0; i < programs.size(); i++)
        {
            auto& p = programs[i];
            SCOPED_TRACE("C++ source: \""s + p.testcase->input + "\", Optimize: " +
                         (p.optimize ? "true" : "false") + ", IntegerType: " + typeName);

            const std::string cpp = generateCpp(p);
            const fs::path cppPath =
                tmp / ("program_" + typeName + "_" + std::to_string(i) + ".cpp");
            const fs::path stdinPath =
                tmp / ("stdin_" + typeName + "_" + std::to_string(i) + ".txt");
            const fs::path exePath =
                tmp / ("program_" + typeName + "_" + std::to_string(i) + ".exe");
            const fs::path buildOutPath =
                tmp / ("build_" + typeName + "_" + std::to_string(i) + ".txt");
            const fs::path runOutPath =
                tmp / ("run_" + typeName + "_" + std::to_string(i) + ".txt");

            WriteTextFile(cppPath, cpp);
            WriteTextFile(stdinPath, p.testcase->standardInput ? p.testcase->standardInput : "");

            // Compile generated C++.
            CommandResult build = RunCommandRedirectToFile(
                { compiler, "-std=c++17", "-O0", cppPath.string(), "-o", exePath.string() },
                buildOutPath);

            ASSERT_EQ(build.exitCode, 0)
                << "Build failed.\nCommand: " << build.command << "\nOutput:\n"
                << build.output;

            // Execute compiled program.
            CommandResult run =
                RunCommandRedirectToFile({ exePath.string() }, runOutPath, stdinPath);

            ASSERT_EQ(run.exitCode, 0)
                << "Execution failed.\nCommand: " << run.command << "\nOutput:\n"
                << run.output;

            ASSERT_EQ(p.expectedOutput, run.output);
        }
    };

    RunForIntegerType("i32", [](const Program& p) { return GenerateCpp<int32_t>(p); });
    RunForIntegerType("i64", [](const Program& p) { return GenerateCpp<int64_t>(p); });
#endif
}
