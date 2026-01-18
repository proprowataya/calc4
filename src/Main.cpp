/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "ReplCommon.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <streambuf>
#include <string_view>

#ifdef ENABLE_GMP
#include <gmpxx.h>
#endif // ENABLE_GMP

using namespace calc4;

constexpr const char* ProgramName = "Calc4 REPL";

namespace CommandLineArgs
{
constexpr std::string_view Help = "--help";
constexpr std::string_view EnableJit = "--enable-jit";
constexpr std::string_view DisableJit = "--disable-jit";
constexpr std::string_view NoUseTreeTraversalEvaluator = "--no-tree";
constexpr std::string_view ForceTreeTraversalEvaluator = "--force-tree";
constexpr std::string_view IntegerSize = "--size";
constexpr std::string_view IntegerSizeShort = "-s";
constexpr std::string_view EnableOptimization = "-O1";
constexpr std::string_view DisableOptimization = "-O0";
constexpr std::string_view InfinitePrecisionInteger = "inf";
constexpr std::string_view EmitCpp = "--emit-cpp";
constexpr std::string_view EmitWat = "--emit-wat";
constexpr std::string_view DumpProgram = "--dump";
}

namespace ReplCommands
{
constexpr std::string_view DumpOn = "#dump on";
constexpr std::string_view DumpOff = "#dump off";
constexpr std::string_view OptimizeOn = "#optimize on";
constexpr std::string_view OptimizeOff = "#optimize off";
constexpr std::string_view ResetContext = "#reset";
}

std::tuple<Option, std::vector<const char*>, bool> ParseCommandLineArgs(int argc, char** argv);

template<typename TNumber>
void Run(Option& option, const std::vector<const char*>& sources);

template<typename TNumber>
void RunSources(const Option& option, const std::vector<const char*>& sources);

template<typename TNumber>
void RunAsRepl(Option& option);

inline const char* GetIntegerSizeDescription(int size);
inline bool IsSupportedIntegerSize(int size);
void PrintHelp(int argc, char** argv);
const char* GetExecutorTypeString(ExecutorType type);

int main(int argc, char** argv)
{
    using namespace std;

#ifdef ENABLE_JIT
    using namespace llvm;
#endif // ENABLE_JIT

    /* ***** Parse command line args ***** */
    auto [option, sources, performTest] = ParseCommandLineArgs(argc, argv);

#ifdef ENABLE_JIT
    /* ***** Initialize LLVM if needed ***** */
    if (performTest || option.executorType == ExecutorType::JIT)
    {
        LLVMInitializeNativeTarget();
        LLVMInitializeNativeAsmPrinter();
        LLVMInitializeNativeAsmParser();
    }
#endif // ENABLE_JIT

    switch (option.integerSize)
    {
    case 32:
        Run<int32_t>(option, sources);
        break;
    case 64:
        Run<int64_t>(option, sources);
        break;

#ifdef ENABLE_INT128
    case 128:
        Run<__int128_t>(option, sources);
        break;
#endif // ENABLE_INT128

#ifdef ENABLE_GMP
    case InfinitePrecisionIntegerSize:
        Run<mpz_class>(option, sources);
        break;
#endif // ENABLE_GMP

    default:
        UNREACHABLE();
        break;
    }

#ifdef ENABLE_JIT
    llvm_shutdown();
#endif // ENABLE_JIT
}

std::tuple<Option, std::vector<const char*>, bool> ParseCommandLineArgs(int argc, char** argv)
{
    using namespace std::string_literals;

    Option option;
    std::vector<const char*> sources;
    bool performTest = false;
    bool warningsIntroduced = false;

    auto ReportError = [argc, argv](std::string_view message) {
        std::cout << "Error: " << message << std::endl << std::endl;
        PrintHelp(argc, argv);
        exit(EXIT_FAILURE);
    };

    auto ReportWarning = [&warningsIntroduced](std::string_view message) {
        std::cout << "Warning: " << message << std::endl;
        warningsIntroduced = true;
    };

    for (int i = 1; i < argc; i++)
    {
        char* str = argv[i];

        auto GetNextArgument = [&i, argc, argv, ReportError]() {
            if (i + 1 >= argc)
            {
                ReportError("Option \""s + argv[i] + "\" requires argument");
            }

            return argv[++i];
        };

        if (str == CommandLineArgs::Help)
        {
            PrintHelp(argc, argv);
            exit(0);
        }
        else if (str == CommandLineArgs::EnableJit)
        {
#ifdef ENABLE_JIT
            option.executorType = ExecutorType::JIT;
#else
            ReportError("Jit compilation is not supported");
#endif // ENABLE_JIT
        }
        else if (str == CommandLineArgs::DisableJit)
        {
            option.executorType = ExecutorType::StackMachine;
        }
        else if (str == CommandLineArgs::NoUseTreeTraversalEvaluator)
        {
            option.treeExecutorMode = TreeTraversalExecutorMode::Never;
        }
        else if (str == CommandLineArgs::ForceTreeTraversalEvaluator)
        {
            option.treeExecutorMode = TreeTraversalExecutorMode::Always;
        }
        else if (str == CommandLineArgs::IntegerSize || str == CommandLineArgs::IntegerSizeShort)
        {
            const char* arg = GetNextArgument();
            int size;

            if (arg == CommandLineArgs::InfinitePrecisionInteger)
            {
#ifdef ENABLE_GMP
                size = (option.integerSize = InfinitePrecisionIntegerSize);
#else
                ReportError("Infinite precision integer is not supported");
                size = -1; /* Keep compiler silence */
#endif // ENABLE_GMP
            }
            else
            {
                size = (option.integerSize = atoi(arg));
            }

            if (!IsSupportedIntegerSize(size))
            {
                ReportError("Unsupported integer size \"" + std::string(arg) + '\"');
            }
        }
        else if (str == CommandLineArgs::EnableOptimization)
        {
            option.optimize = true;
        }
        else if (str == CommandLineArgs::DisableOptimization)
        {
            option.optimize = false;
        }
        else if (str == CommandLineArgs::EmitCpp)
        {
            option.emitCpp = true;
        }
        else if (str == CommandLineArgs::EmitWat)
        {
            option.emitWat = true;
        }
        else if (str == CommandLineArgs::DumpProgram)
        {
            option.dumpProgram = true;
        }
        else
        {
            sources.push_back(str);
        }
    }

    if (sources.empty() && option.emitCpp)
    {
        ReportWarning('\"' + std::string(CommandLineArgs::EmitCpp) +
                      "\" option was specified, but it will be ignored in the repl mode.");
        option.emitCpp = false;
    }

    if (sources.empty() && option.emitWat)
    {
        ReportWarning('\"' + std::string(CommandLineArgs::EmitWat) +
                      "\" option was specified, but it will be ignored in the repl mode.");
        option.emitWat = false;
    }

    if (option.emitWat && (option.integerSize != 32 && option.integerSize != 64))
    {
        ReportError(
            "WebAssembly Text Format generation is not supported for the specified integer size.");
    }

#if defined(ENABLE_INT128) || defined(ENABLE_GMP)
    if (option.emitCpp &&
        (
#ifdef ENABLE_INT128
            option.integerSize == 128
#endif // ENABLE_INT128

#if defined(ENABLE_INT128) && defined(ENABLE_GMP)
            ||
#endif // defined(ENABLE_INT128) && defined(ENABLE_GMP)

#ifdef ENABLE_GMP
            option.integerSize == InfinitePrecisionIntegerSize
#endif // ENABLE_GMP
            ))
    {
        ReportError("C++ code generation is not supported for the specified integer size.");
    }
#endif // defined(ENABLE_JIT) && defined(ENABLE_GMP)

#if defined(ENABLE_JIT) && defined(ENABLE_GMP)
    if (option.executorType == ExecutorType::JIT &&
        option.integerSize == InfinitePrecisionIntegerSize)
    {
        option.executorType = ExecutorType::StackMachine;
        ReportWarning(
            "Jit compilation is disabled because it does not support infinite precision integers.");
    }
#endif // defined(ENABLE_JIT) && defined(ENABLE_GMP)

    if (option.treeExecutorMode == TreeTraversalExecutorMode::Always)
    {
        option.executorType = ExecutorType::TreeTraversal;
    }

    if (warningsIntroduced)
    {
        std::cout << std::endl;
    }

    return std::make_tuple(option, sources, performTest);
}

template<typename TNumber>
void Run(Option& option, const std::vector<const char*>& sources)
{
    if (!sources.empty())
    {
        /* ***** If the source files are specified, we execute them ***** */
        RunSources<TNumber>(option, sources);
    }
    else
    {
        /* ***** Otherwise, this program behaves as repl ***** */
        RunAsRepl<TNumber>(option);
    }
}

template<typename TNumber>
void RunSources(const Option& option, const std::vector<const char*>& sources)
{
    for (auto path : sources)
    {
        std::ifstream ifs(path);
        if (!ifs)
        {
            std::cerr << "Error: Could not open \"" << path << "\"" << std::endl;
            exit(EXIT_FAILURE);
        }

        std::string source = { std::istreambuf_iterator<char>(ifs),
                               std::istreambuf_iterator<char>() };

        CompilationContext context;
        ExecutionState<TNumber> state;
        ExecuteSource(source, path, context, state, option, std::cout);
    }
}

template<typename TNumber>
void RunAsRepl(Option& option)
{
    using namespace std;

    /* ***** Print header ***** */
    cout << ProgramName << endl;

    /* ***** Print current setting ***** */
    cout << "    Integer size: " << GetIntegerSizeDescription(option.integerSize) << endl
         << "    Executor: " << GetExecutorTypeString(option.executorType) << endl
         << "    Optimize: " << (option.optimize ? "on" : "off") << endl
         << endl;

    CompilationContext context;
    ExecutionState<TNumber> state;

    while (true)
    {
        string line;
        cout << "> ";
        if (!getline(cin, line))
        {
            // Reached EOF
            break;
        }

        if (line == ReplCommands::DumpOn)
        {
            option.dumpProgram = true;
            cout << endl;
            continue;
        }
        else if (line == ReplCommands::DumpOff)
        {
            option.dumpProgram = false;
            cout << endl;
            continue;
        }
        else if (line == ReplCommands::OptimizeOn)
        {
            option.optimize = true;
            cout << endl;
            continue;
        }
        else if (line == ReplCommands::OptimizeOff)
        {
            option.optimize = false;
            cout << endl;
            continue;
        }
        else if (line == ReplCommands::ResetContext)
        {
            context = {};
            state = {};
            cout << endl;
            continue;
        }

        ExecuteSource<TNumber>(line, nullptr, context, state, option, std::cout);
        std::cout << std::endl;
    }
}

inline const char* GetIntegerSizeDescription(int size)
{
    switch (size)
    {
    case 32:
        return "32";
    case 64:
        return "64";

#ifdef ENABLE_INT128
    case 128:
        return "128";
#endif // ENABLE_INT128

#ifdef ENABLE_GMP
    case InfinitePrecisionIntegerSize:
        return "infinite-precision";
#endif // ENABLE_GMP

    default:
        return "<unknown>";
    }
}

inline bool IsSupportedIntegerSize(int size)
{
    switch (size)
    {
    case 32:
    case 64:

#ifdef ENABLE_INT128
    case 128:
#endif // ENABLE_INT128

#ifdef ENABLE_GMP
    case InfinitePrecisionIntegerSize:
#endif // ENABLE_GMP

        return true;
    default:
        return false;
    }
}

void PrintHelp(int argc, char** argv)
{
    using namespace std;

    cout << ProgramName << endl
         << endl
         << argv[0] << " [options] [files]" << endl
         << endl
         << "Options:" << endl
         << CommandLineArgs::IntegerSize << "|" << CommandLineArgs::IntegerSizeShort << " <size>"
         << endl
         << Indent << "Specify the size of integer" << endl
         << Indent << "size: 32, 64 (default)"
#ifdef ENABLE_INT128
         << ", 128"
#endif // ENABLE_INT128
#ifdef ENABLE_GMP
         << ", " << CommandLineArgs::InfinitePrecisionInteger
         << " (meaning infinite-precision or arbitrary-precision)"
#endif // ENABLE_GMP
         << endl
#ifdef ENABLE_JIT
         << CommandLineArgs::DisableJit << endl
         << Indent << "Disable JIT compilation" << endl
         << CommandLineArgs::EnableJit << endl
         << Indent << "Enable JIT compilation (default)" << endl
#endif // ENABLE_JIT
         << CommandLineArgs::DisableOptimization << endl
         << Indent << "Disable optimization" << endl
         << CommandLineArgs::EnableOptimization << endl
         << Indent << "Enable optimization (default)" << endl
         << CommandLineArgs::NoUseTreeTraversalEvaluator << endl
         << Indent << "Always use the JIT or stack machine executors" << endl
         << Indent
         << "(By default, the tree traversal executor will be used when the given code has no "
            "recursive operators)"
         << endl
         << CommandLineArgs::ForceTreeTraversalEvaluator << endl
         << Indent << "Always use the tree traversal executors (very slow)" << endl
         << CommandLineArgs::EmitCpp << endl
         << Indent << "Emit C++ code for source input (experimental feature)" << endl
         << CommandLineArgs::EmitWat << endl
         << Indent << "Emit WebAssembly Text Format for source input (experimental feature)" << endl
         << CommandLineArgs::DumpProgram << endl
         << Indent << "Dump the given program's structures such as an abstract syntax tree" << endl
         << endl
         << "During the Repl mode, the following commands are available:" << endl
         << Indent << ReplCommands::DumpOff << endl
         << Indent << ReplCommands::DumpOn << endl
         << Indent << ReplCommands::OptimizeOff << endl
         << Indent << ReplCommands::OptimizeOn << endl
         << Indent << ReplCommands::ResetContext << endl;
}

const char* GetExecutorTypeString(ExecutorType type)
{
    switch (type)
    {
#ifdef ENABLE_JIT
    case ExecutorType::JIT:
        return "JIT";
#endif // ENABLE_JIT
    case ExecutorType::StackMachine:
        return "StackMachine";
    case ExecutorType::TreeTraversal:
        return "TreeTraversal";
    default:
        return "<Unknown>";
    }
}
