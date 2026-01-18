/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#pragma once

#include "CodegenTestConfig.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace calc4
{
struct CommandResult
{
    int exitCode = -1;
    std::string output;
    std::string command;
};

inline int NormalizeSystemExitCode(int systemResult)
{
#ifdef _WIN32
    // On Windows, std::system() returns the program's exit code.
    return systemResult;
#else
    // On POSIX, std::system() returns a waitpid-style status code.
    if (systemResult == -1)
    {
        return -1;
    }
    if (WIFEXITED(systemResult))
    {
        return WEXITSTATUS(systemResult);
    }
    if (WIFSIGNALED(systemResult))
    {
        // Encode "killed by signal" as 128 + signal, similar to common shells.
        return 128 + WTERMSIG(systemResult);
    }
    return systemResult;
#endif
}

inline std::string QuoteForShell(std::string_view arg)
{
    // Basic quoting with double quotes for simple paths.
    // Windows cmd.exe needs extra handling in RunCommandRedirectToFile.
    std::string out;
    out.reserve(arg.size() + 2);
    out.push_back('"');
    for (char c : arg)
    {
        if (c == '"')
        {
            out += "\\\"";
        }
        else
        {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

inline std::string BuildCommandLine(const std::vector<std::string_view>& args)
{
    std::ostringstream oss;
    bool first = true;
    for (const auto& a : args)
    {
        if (!first)
        {
            oss << ' ';
        }
        first = false;
        oss << QuoteForShell(a);
    }
    return oss.str();
}

inline void WriteTextFile(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs)
    {
        throw std::runtime_error("Failed to open file for write: " + path.string());
    }
    ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
}

inline std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
    {
        return {};
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

inline std::filesystem::path CreateTempDirectory(std::string_view prefix)
{
    namespace fs = std::filesystem;

    // Convert to an owned string for concatenation.
    const std::string prefixString(prefix);

    // Use a configured base directory or the system default.
    fs::path base = CODEGEN_TEST_TEMP_DIR[0] != '\0' ? fs::path(CODEGEN_TEST_TEMP_DIR)
                                                     : fs::temp_directory_path();

    // Try a handful of times to avoid collisions.
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;

    for (int attempt = 0; attempt < 32; attempt++)
    {
        const uint64_t r = dist(gen);
        fs::path dir = base / (prefixString + std::to_string(r));
        std::error_code ec;
        if (fs::create_directories(dir, ec))
        {
            return dir;
        }
    }

    throw std::runtime_error("Failed to create a temporary directory.");
}

inline constexpr std::string_view GetNodeExecutable()
{
    constexpr std::string_view node = CODEGEN_TEST_NODE_EXECUTABLE;
    return !node.empty() ? node : "node";
}

inline constexpr std::string_view GetWat2WasmExecutable()
{
    constexpr std::string_view wat2wasm = CODEGEN_TEST_WAT2WASM_EXECUTABLE;
    return !wat2wasm.empty() ? wat2wasm : "wat2wasm";
}

inline constexpr std::string_view GetCxxCompiler()
{
    constexpr std::string_view cxx = CODEGEN_TEST_CXX_COMPILER;
    return !cxx.empty() ? cxx : "c++";
}

inline CommandResult RunCommandRedirectToFile(
    const std::vector<std::string_view>& args, const std::filesystem::path& stdoutPath,
    const std::optional<std::filesystem::path>& stdinPath = std::nullopt)
{
    // Build shell command:
    //   "<arg0>" "<arg1>" ... < "stdin" > "stdout" 2>&1
    std::string cmd = BuildCommandLine(args);
    if (stdinPath)
    {
        cmd += " < ";
        cmd += QuoteForShell(stdinPath->string());
    }
    cmd += " > ";
    cmd += QuoteForShell(stdoutPath.string());
    cmd += " 2>&1";

#ifdef _WIN32
    // cmd.exe can split a leading quoted command on spaces.
    // Wrap the full command in extra quotes to keep it together.
    if (!cmd.empty() && cmd.front() == '"')
    {
        cmd = "\"" + cmd + "\"";
    }
#endif

    const int systemResult = std::system(cmd.c_str());
    const int exitCode = NormalizeSystemExitCode(systemResult);

    return { exitCode, ReadTextFile(stdoutPath), cmd };
}
}
