/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2022 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "Evaluator.h"
#include "Exceptions.h"
#include "Operators.h"
#include "Optimizer.h"
#include "StackMachine.h"
#include "SyntaxAnalysis.h"
#include "Test.h"

#ifdef ENABLE_JIT
#include "Jit.h"
#endif // ENABLE_JIT

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
constexpr const char* Indent = "    ";

#ifdef ENABLE_GMP
constexpr const int InfinitePrecisionIntegerSize = std::numeric_limits<int>::max();
#endif // ENABLE_GMP

enum class ExecutorType
{
#ifdef ENABLE_JIT
    JIT,
#endif // ENABLE_JIT
    StackMachine,
    TreeTraversal,
};

enum class TreeTraversalExecutorMode
{
    Never,
    WhenNoRecursiveOperators,
    Always,
};

struct Option
{
    int integerSize = 64;

    ExecutorType executorType =
#ifdef ENABLE_JIT
        ExecutorType::JIT;
#else
        ExecutorType::StackMachine;
#endif // ENABLE_JIT

    TreeTraversalExecutorMode treeExecutorMode =
        TreeTraversalExecutorMode::WhenNoRecursiveOperators;
    bool optimize = true;
    bool dumpProgram = false;
};

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
constexpr std::string_view DumpProgram = "--dump";
constexpr std::string_view PerformTest = "--test";
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

template<typename TNumber>
void ExecuteCore(std::string_view source, const char* filePath, CompilationContext& context,
                 ExecutionState<TNumber>& state, const Option& option);

inline const char* GetIntegerSizeDescription(int size);
inline bool IsSupportedIntegerSize(int size);
void PrintHelp(int argc, char** argv);
void PrintTree(const CompilationContext& context, const std::shared_ptr<const Operator>& op);
void PrintTreeCore(const std::shared_ptr<const Operator>& op, int depth);

template<typename TNumber>
void PrintStackMachineModule(const StackMachineModule<TNumber>& module);

void PrintStackMachineOperations(const std::vector<StackMachineOperation>& operations);
bool HasRecursiveCall(const std::shared_ptr<const Operator>& op, const CompilationContext& context);
bool HasRecursiveCallInternal(const std::shared_ptr<const Operator>& op,
                              const CompilationContext& context,
                              std::unordered_map<const OperatorDefinition*, int>& called);
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

    /* ***** Perform test if specified ***** */
    if (performTest)
    {
        TestAll();
#ifdef ENABLE_JIT
        llvm_shutdown();
#endif // ENABLE_JIT
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
            else if (str == CommandLineArgs::EnableJit)
            {
#ifdef ENABLE_JIT
                option.executorType = ExecutorType::JIT;
#else
                throw std::string("Jit compilation is not supported");
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
            else if (str == CommandLineArgs::IntegerSize ||
                     str == CommandLineArgs::IntegerSizeShort)
            {
                const char* arg = GetNextArgument();
                int size;

                if (arg == CommandLineArgs::InfinitePrecisionInteger)
                {
#ifdef ENABLE_GMP
                    size = (option.integerSize = InfinitePrecisionIntegerSize);
#else
                    throw std::string("Infinite precision integer is not supported");
#endif // ENABLE_GMP
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
            else if (str == CommandLineArgs::DumpProgram)
            {
                option.dumpProgram = true;
            }
            else if (str == CommandLineArgs::PerformTest)
            {
                performTest = true;
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

#if defined(ENABLE_JIT) && defined(ENABLE_GMP)
    if (option.executorType == ExecutorType::JIT &&
        option.integerSize == InfinitePrecisionIntegerSize)
    {
        option.executorType = ExecutorType::StackMachine;
        std::cout << "Warning: Jit compilation is disabled because it does not support infinite "
                     "precision integers."
                  << std::endl;
    }
#endif // defined(ENABLE_JIT) && defined(ENABLE_GMP)

    if (option.treeExecutorMode == TreeTraversalExecutorMode::Always)
    {
        option.executorType = ExecutorType::TreeTraversal;
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
        ExecuteCore(source, path, context, state, option);
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

        ExecuteCore<TNumber>(line, nullptr, context, state, option);
    }
}

template<typename TNumber>
void ExecuteCore(std::string_view source, const char* filePath, CompilationContext& context,
                 ExecutionState<TNumber>& state, const Option& option)
{
    using namespace std;

#ifdef ENABLE_JIT
    using namespace llvm;
#endif // ENABLE_JIT

    try
    {
        auto start = chrono::high_resolution_clock::now();

        // We make a copy of the given CompilationContext so that it will not be destroyed if some
        // error occurs
        auto copyOfContext = context;
        auto tokens = Lex(source, copyOfContext);
        auto op = Parse(tokens, copyOfContext);
        if (option.optimize)
        {
            op = Optimize<TNumber>(copyOfContext, op);
        }

        // All compilation is complete, so we can update the given CompilationContext
        context = std::move(copyOfContext);

        if (option.dumpProgram)
        {
            cout << "Has recursive call: " << (HasRecursiveCall(op, context) ? "True" : "False")
                 << endl
                 << endl;
            PrintTree(context, op);
        }

        ExecutorType actualExecutionEngine = option.executorType;
        if (option.executorType != ExecutorType::TreeTraversal &&
            option.treeExecutorMode != TreeTraversalExecutorMode::Never &&
            !HasRecursiveCall(op, context))
        {
            // The given program has no heavy loops, so we use tree traversal executor.
            actualExecutionEngine = ExecutorType::TreeTraversal;
        }

        TNumber result;
        switch (actualExecutionEngine)
        {
#ifdef ENABLE_JIT
        case ExecutorType::JIT:
#ifdef ENABLE_GMP
            if constexpr (std::is_same_v<TNumber, mpz_class>)
            {
                throw Exceptions::AssertionErrorException(
                    std::nullopt, "Jit compiler does not support infinite precision integers.");
            }
            else
#endif // ENABLE_GMP
            {
                result =
                    EvaluateByJIT<TNumber>(context, state, op, option.optimize, option.dumpProgram);
            }
            break;
#endif // ENABLE_JIT
        case ExecutorType::StackMachine:
        {
            auto module = GenerateStackMachineModule<TNumber>(op, context);

            if (option.dumpProgram)
            {
                PrintStackMachineModule(module);
            }

            result = ExecuteStackMachineModule(module, state);
            break;
        }
        case ExecutorType::TreeTraversal:
            result = Evaluate<TNumber>(context, state, op);
            break;
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
    catch (Exceptions::Calc4Exception& error)
    {
        auto& position = error.GetPosition();
        if (position)
        {
            if (filePath != nullptr)
            {
                cout << filePath << ":";
            }

            cout << (position->lineNo + 1) << ":" << (position->charNo + 1) << ": ";
        }

        cout << "Error: " << error.what() << endl;

        if (position)
        {
            size_t lineStartIndex = source.substr(0, position->index).find_last_of("\r\n");
            lineStartIndex = lineStartIndex == source.npos ? 0 : (lineStartIndex + 1);

            size_t lineEndIndex = source.find_first_of("\r\n", position->index);
            lineEndIndex = lineEndIndex == source.npos ? source.length() : lineEndIndex;

            std::string_view line = source.substr(lineStartIndex, lineEndIndex - lineStartIndex);

            static constexpr int LineNoWidth = 8;
            static constexpr std::string_view Splitter = " | ";
            cout << std::right << std::setw(LineNoWidth) << (position->lineNo + 1) << Splitter
                 << line << endl;
            for (int i = 0;
                 i < LineNoWidth + static_cast<int>(Splitter.length()) + position->charNo; i++)
            {
                cout << ' ';
            }
            cout << '^' << endl;
        }

        cout << endl;
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
         << CommandLineArgs::DumpProgram << endl
         << Indent << "Dump the given program's structures such as an abstract syntax tree" << endl
         << CommandLineArgs::PerformTest << endl
         << Indent << "Perform test" << endl
         << endl
         << "During the Repl mode, the following commands are available:" << endl
         << Indent << ReplCommands::DumpOff << endl
         << Indent << ReplCommands::DumpOn << endl
         << Indent << ReplCommands::OptimizeOff << endl
         << Indent << ReplCommands::OptimizeOn << endl
         << Indent << ReplCommands::ResetContext << endl;
}

void PrintTree(const CompilationContext& context, const std::shared_ptr<const Operator>& op)
{
    using std::cout;
    using std::endl;

    cout << "/*" << endl << " * Tree" << endl << " */" << endl << "{" << endl << "Main:" << endl;
    PrintTreeCore(op, 1);

    for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++)
    {
        auto& name = it->second.GetDefinition().GetName();
        auto& op = it->second.GetOperator();

        cout << endl << "Operator \"" << name << "\":" << endl;
        PrintTreeCore(op, 1);
    }

    cout << "}" << endl << endl;
}

void PrintTreeCore(const std::shared_ptr<const Operator>& op, int depth)
{
    using namespace std;

    auto PrintIndent = [depth]() {
        for (int i = 0; i < depth; i++)
        {
            cout << Indent;
        }
    };

    PrintIndent();

    cout << op->ToString() << endl;
    for (auto& operand : op->GetOperands())
    {
        PrintTreeCore(operand, depth + 1);
    }

    if (auto parenthesis = std::dynamic_pointer_cast<const ParenthesisOperator>(op))
    {
        PrintIndent();
        cout << "Contains:" << endl;
        for (auto& op2 : parenthesis->GetOperators())
        {
            PrintTreeCore(op2, depth + 1);
        }
    }
}

template<typename TNumber>
void PrintStackMachineModule(const StackMachineModule<TNumber>& module)
{
    using std::cout;
    using std::endl;

    cout << "/*" << endl << " * Stack Machine Codes" << endl << " */" << endl << "{" << endl;

    cout << "Main:" << endl;
    PrintStackMachineOperations(module.GetEntryPoint());

    auto& userDefinedOperators = module.GetUserDefinedOperators();
    for (size_t i = 0; i < userDefinedOperators.size(); i++)
    {
        auto& userDefined = userDefinedOperators[i];
        cout << "Operator \"" << userDefined.GetDefinition().GetName() << "\""
             << " (No = " << i << ")" << endl;
        PrintStackMachineOperations(userDefined.GetOperations());
    }

    auto& constants = module.GetConstTable();
    if (!constants.empty())
    {
        cout << "Constants:";

        for (size_t i = 0; i < constants.size(); i++)
        {
            cout << (i == 0 ? " " : ", ") << "[" << i << "] = " << constants[i];
        }

        cout << endl;
    }

    cout << "}" << endl << endl;
}

void PrintStackMachineOperations(const std::vector<StackMachineOperation>& operations)
{
    static constexpr int AddressWidth = 6;
    static constexpr int OpcodeWidth = 25;

    for (size_t i = 0; i < operations.size(); i++)
    {
        std::cout << std::right << std::setw(AddressWidth) << i << ": ";
        std::cout << std::left << std::setw(OpcodeWidth) << ToString(operations[i].opcode);
        std::cout << " [Value = " << operations[i].value << "]" << std::endl;
    }
}

bool HasRecursiveCall(const std::shared_ptr<const Operator>& op, const CompilationContext& context)
{
    std::unordered_map<const OperatorDefinition*, int> called;
    return HasRecursiveCallInternal(op, context, called);
}

bool HasRecursiveCallInternal(const std::shared_ptr<const Operator>& op,
                              const CompilationContext& context,
                              std::unordered_map<const OperatorDefinition*, int>& called)
{
    if (auto userDefined = std::dynamic_pointer_cast<const UserDefinedOperator>(op))
    {
        auto& definition = userDefined->GetDefinition();
        if (++called[&definition] > 1)
        {
            // Recursive call detected
            return true;
        }

        if (HasRecursiveCallInternal(
                context.GetOperatorImplement(definition.GetName()).GetOperator(), context, called))
        {
            --called[&definition];
            return true;
        }
        else
        {
            --called[&definition];
        }
    }
    else if (auto parenthesis = std::dynamic_pointer_cast<const ParenthesisOperator>(op))
    {
        for (auto& op2 : parenthesis->GetOperators())
        {
            if (HasRecursiveCallInternal(op2, context, called))
            {
                return true;
            }
        }
    }

    for (auto& operand : op->GetOperands())
    {
        if (HasRecursiveCallInternal(operand, context, called))
        {
            return true;
        }
    }

    return false;
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
