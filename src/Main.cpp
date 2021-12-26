﻿#include "Evaluator.h"
#include "Jit.h"
#include "Operators.h"
#include "Optimizer.h"
#include "StackMachine.h"
#include "SyntaxAnalysis.h"
#include "Test.h"
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <gmpxx.h>
#include <iostream>
#include <limits>
#include <sstream>
#include <streambuf>
#include <string_view>

constexpr const char* ProgramName = "Calc4 REPL";
constexpr const int InfinitePrecisionIntegerSize = std::numeric_limits<int>::max();

enum class ExecutionType
{
    JIT,
    StackMachine,
    Interpreter,
};

struct Option
{
    int integerSize = 64;
    ExecutionType executionType = ExecutionType::JIT;
    bool alwaysJit = false;
    bool optimize = true;
    bool printInfo = false;
};

namespace CommandLineArgs
{
constexpr std::string_view Help = "--help";
constexpr std::string_view PerformTest = "--test";
constexpr std::string_view EnableJit = "--jit-on";
constexpr std::string_view DisableJit = "--jit-off";
constexpr std::string_view AlwaysJit = "--always-jit";
constexpr std::string_view AlwaysJitShort = "-a";
constexpr std::string_view IntegerSize = "--size";
constexpr std::string_view IntegerSizeShort = "-s";
constexpr std::string_view EnableOptimization = "-O";
constexpr std::string_view DisableOptimization = "-Od";
constexpr std::string_view InfinitePrecisionInteger = "inf";
}

std::tuple<Option, std::vector<const char*>, bool> ParseCommandLineArgs(int argc, char** argv);

template<typename TNumber>
void Run(Option& option, const std::vector<const char*>& sources);

template<typename TNumber>
void RunSources(const Option& option, const std::vector<const char*>& sources);

template<typename TNumber>
void RunAsRepl(Option& option);

template<typename TNumber>
void ExecuteCore(const std::string& source, CompilationContext& context,
                 ExecutionState<TNumber>& state, const Option& option);

inline bool IsSupportedIntegerSize(int size);
void PrintHelp(int argc, char** argv);
void PrintTree(const Operator& op, int depth);
bool HasRecursiveCall(const Operator& op, const CompilationContext& context);
bool HasRecursiveCallInternal(const Operator& op, const CompilationContext& context,
                              std::unordered_map<const OperatorDefinition*, int> called);
const char* GetExecutionTypeString(ExecutionType type);

int main(int argc, char** argv)
{
    using namespace std;
    using namespace llvm;

    /* ***** Parse command line args ***** */
    auto [option, sources, performTest] = ParseCommandLineArgs(argc, argv);

    /* ***** Initialize LLVM if needed ***** */
    if (performTest || option.executionType == ExecutionType::JIT)
    {
        LLVMInitializeNativeTarget();
        LLVMInitializeNativeAsmPrinter();
        LLVMInitializeNativeAsmParser();
    }

    /* ***** Perform test if specified ***** */
    if (performTest)
    {
        TestAll();
        llvm_shutdown();
        return 0;
    }

    switch (option.integerSize)
    {
    case 32:
        Run<int32_t>(option, sources);
        break;
    case 64:
        Run<int64_t>(option, sources);
        break;
    case 128:
        Run<__int128_t>(option, sources);
        break;
    case InfinitePrecisionIntegerSize:
        Run<mpz_class>(option, sources);
        break;
    default:
        UNREACHABLE();
        break;
    }

    llvm_shutdown();
}

std::tuple<Option, std::vector<const char*>, bool> ParseCommandLineArgs(int argc, char** argv)
{
    Option option;
    std::vector<const char*> sources;
    bool performTest = false;

    for (int i = 1; i < argc; i++)
    {
        char* str = argv[i];

        auto GetNextArgument = [&i, argc, argv]() {
            if (i + 1 >= argc)
            {
                std::ostringstream oss;
                oss << "Option \"" << argv[i] << "\" requires argument";
                throw oss.str();
            }

            return argv[++i];
        };

        try
        {
            if (str == CommandLineArgs::Help)
            {
                PrintHelp(argc, argv);
                exit(0);
            }
            else if (str == CommandLineArgs::PerformTest)
            {
                performTest = true;
            }
            else if (str == CommandLineArgs::EnableJit)
            {
                option.executionType = ExecutionType::JIT;
            }
            else if (str == CommandLineArgs::DisableJit)
            {
                option.executionType = ExecutionType::StackMachine;
            }
            else if (str == CommandLineArgs::AlwaysJit || str == CommandLineArgs::AlwaysJitShort)
            {
                option.alwaysJit = true;
            }
            else if (str == CommandLineArgs::IntegerSize ||
                     str == CommandLineArgs::IntegerSizeShort)
            {
                const char* arg = GetNextArgument();
                int size;

                if (arg == CommandLineArgs::InfinitePrecisionInteger)
                {
                    size = (option.integerSize = InfinitePrecisionIntegerSize);
                }
                else
                {
                    size = (option.integerSize = atoi(arg));
                }

                if (!IsSupportedIntegerSize(size))
                {
                    std::ostringstream oss;
                    oss << "Unsupported integer size " << option.integerSize;
                    throw oss.str();
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
            else
            {
                sources.push_back(str);
            }
        }
        catch (std::string& error)
        {
            std::cout << "Error: " << error << std::endl << std::endl;
            PrintHelp(argc, argv);
            exit(EXIT_FAILURE);
        }
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
        ExecuteCore(source, context, state, option);
    }
}

template<typename TNumber>
void RunAsRepl(Option& option)
{
    using namespace std;

    /* ***** Print header ***** */
    cout << ProgramName << endl;

    /* ***** Print current setting ***** */
    cout << "    Integer size: "
         << (option.integerSize == InfinitePrecisionIntegerSize
                 ? "Infinite-precision"
                 : std::to_string(option.integerSize))
         << endl
         << "    Executor: " << GetExecutionTypeString(option.executionType) << endl
         << "    Always JIT: " << (option.alwaysJit ? "on" : "off") << endl
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

        if (line == "#print on")
        {
            option.printInfo = true;
            cout << endl;
            continue;
        }
        else if (line == "#print off")
        {
            option.printInfo = false;
            cout << endl;
            continue;
        }
        else if (line == "#optimize on")
        {
            option.optimize = true;
            cout << endl;
            continue;
        }
        else if (line == "#optimize off")
        {
            option.optimize = false;
            cout << endl;
            continue;
        }

        ExecuteCore<TNumber>(line, context, state, option);
    }
}

template<typename TNumber>
void ExecuteCore(const std::string& source, CompilationContext& context,
                 ExecutionState<TNumber>& state, const Option& option)
{
    using namespace std;
    using namespace llvm;

    try
    {
        auto start = chrono::high_resolution_clock::now();

        auto tokens = Lex(source, context);
        auto op = Parse(tokens, context);
        if (option.optimize)
        {
            op = Optimize<TNumber>(context, op);
        }

        bool hasRecursiveCall = HasRecursiveCall(*op, context);

        if (option.printInfo)
        {
            cout << "Has recursive call: " << (hasRecursiveCall ? "True" : "False") << endl;

            cout << "Tree:" << endl
                 << "---------------------------" << endl
                 << "Module {" << endl
                 << "Main:" << endl;
            PrintTree(*op, 1);
            cout << endl;

            for (auto it = context.UserDefinedOperatorBegin();
                 it != context.UserDefinedOperatorEnd(); it++)
            {
                auto& name = it->second.GetDefinition().GetName();
                auto& op = it->second.GetOperator();

                cout << name << ":" << endl;
                PrintTree(*op, 1);
            }

            cout << "}" << endl << "---------------------------" << endl << endl;
        }

        ExecutionType actualExecutionEngine = option.executionType;
        if (actualExecutionEngine == ExecutionType::JIT && !option.alwaysJit &&
            !HasRecursiveCall(*op, context))
        {
            // There is no need to use Jit compilation for simple program.
            // So we use StackMachine execution instead.
            actualExecutionEngine = ExecutionType::StackMachine;
        }
        if constexpr (std::is_same_v<TNumber, mpz_class>)
        {
            if (actualExecutionEngine == ExecutionType::JIT)
            {
                // Jit compiler does not support GMP
                actualExecutionEngine = ExecutionType::StackMachine;
            }
        }

        TNumber result;
        switch (actualExecutionEngine)
        {
        case ExecutionType::JIT:
            if constexpr (std::is_same_v<TNumber, mpz_class>)
            {
                result = 0; // Suppress compiler warning
                UNREACHABLE();
            }
            else
            {
                result =
                    EvaluateByJIT<TNumber>(context, state, op, option.optimize, option.printInfo);
            }
            break;
        case ExecutionType::StackMachine:
        {
            auto module = GenerateStackMachineModule<TNumber>(op, context);
            result = ExecuteStackMachineModule(module, state);
            break;
        }
        case ExecutionType::Interpreter:
            result = Evaluate<TNumber>(context, state, op);
        default:
            result = 0; // Suppress compiler warning
            UNREACHABLE();
            break;
        }

        auto end = chrono::high_resolution_clock::now();

        cout << result << endl
             << "Elapsed: " << (chrono::duration<double>(end - start).count() * 1000) << " ms"
             << endl
             << endl;
    }
    catch (std::string& error)
    {
        cout << "Error: " << error << endl << endl;
    }
}

inline bool IsSupportedIntegerSize(int size)
{
    switch (size)
    {
    case 32:
    case 64:
    case 128:
    case InfinitePrecisionIntegerSize:
        return true;
    default:
        return false;
    }
}

void PrintHelp(int argc, char** argv)
{
    using namespace std;
    static constexpr const char* Indent = "    ";

    cout << ProgramName << std::endl
         << std::endl
         << "Options:" << endl
         << CommandLineArgs::IntegerSize << "|" << CommandLineArgs::IntegerSizeShort << " <size>"
         << endl
         << Indent << "Specify size of integer" << endl
         << Indent << "size: 32, 64, 128, " << CommandLineArgs::InfinitePrecisionInteger
         << " (meaning infinite-precision or arbitrary-precision)" << endl
         << CommandLineArgs::EnableJit << endl
         << Indent << "Enable JIT compilation" << endl
         << CommandLineArgs::DisableJit << endl
         << Indent << "Disable JIT compilation" << endl
         << CommandLineArgs::AlwaysJit << "|" << CommandLineArgs::AlwaysJitShort << endl
         << Indent << "Force JIT compilation" << endl
         << CommandLineArgs::EnableOptimization << endl
         << Indent << "Enable optimization" << endl
         << CommandLineArgs::DisableOptimization << endl
         << Indent << "Disable optimization" << endl
         << CommandLineArgs::PerformTest << endl
         << Indent << "Perform test" << endl;
}

void PrintTree(const Operator& op, int depth)
{
    using namespace std;

    auto PrintIndent = [depth]() {
        for (int i = 0; i < depth; i++)
        {
            cout << "  ";
        }
    };

    PrintIndent();

    cout << op.ToString() << endl;
    for (auto& operand : op.GetOperands())
    {
        PrintTree(*operand, depth + 1);
    }

    if (auto parenthesis = dynamic_cast<const ParenthesisOperator*>(&op))
    {
        PrintIndent();
        cout << "Contains:" << endl;
        for (auto& op2 : parenthesis->GetOperators())
        {
            PrintTree(*op2, depth + 1);
        }
    }
}

bool HasRecursiveCall(const Operator& op, const CompilationContext& context)
{
    return HasRecursiveCallInternal(op, context, {});
}

bool HasRecursiveCallInternal(const Operator& op, const CompilationContext& context,
                              std::unordered_map<const OperatorDefinition*, int> called)
{
    if (auto userDefined = dynamic_cast<const UserDefinedOperator*>(&op))
    {
        auto& definition = userDefined->GetDefinition();
        if (++called[&definition] > 1)
        {
            // Recursive call detected
            return true;
        }

        if (HasRecursiveCallInternal(
                *context.GetOperatorImplement(definition.GetName()).GetOperator(), context, called))
        {
            --called[&definition];
            return true;
        }
        else
        {
            --called[&definition];
        }
    }
    else if (auto parenthesis = dynamic_cast<const ParenthesisOperator*>(&op))
    {
        for (auto& op2 : parenthesis->GetOperators())
        {
            if (HasRecursiveCallInternal(*op2, context, called))
            {
                return true;
            }
        }
    }

    for (auto& operand : op.GetOperands())
    {
        if (HasRecursiveCallInternal(*operand, context, called))
        {
            return true;
        }
    }

    return false;
}

const char* GetExecutionTypeString(ExecutionType type)
{
    switch (type)
    {
    case ExecutionType::JIT:
        return "JIT";
    case ExecutionType::Interpreter:
        return "Interpreter";
    default:
        return "<Unknown>";
    }
}
