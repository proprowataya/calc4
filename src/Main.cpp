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

constexpr const char* ProgramName = "Calc4 REPL";
constexpr const char* Indent = "    ";

#ifdef ENABLE_GMP
constexpr const int InfinitePrecisionIntegerSize = std::numeric_limits<int>::max();
#endif // ENABLE_GMP

enum class ExecutionType
{
#ifdef ENABLE_JIT
    JIT,
#endif // ENABLE_JIT
    StackMachine,
    Interpreter,
};

struct Option
{
    int integerSize = 64;

    ExecutionType executionType =
#ifdef ENABLE_JIT
        ExecutionType::JIT;
#else
        ExecutionType::StackMachine;
#endif // ENABLE_JIT

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
void ExecuteCore(std::string_view source, std::string_view filePath, CompilationContext& context,
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
const char* GetExecutionTypeString(ExecutionType type);

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
    if (performTest || option.executionType == ExecutionType::JIT)
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
            else if (str == CommandLineArgs::PerformTest)
            {
                performTest = true;
            }
            else if (str == CommandLineArgs::EnableJit)
            {
#ifdef ENABLE_JIT
                option.executionType = ExecutionType::JIT;
#else
                throw std::string("Jit compilation is not supported");
#endif // ENABLE_JIT
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

        ExecuteCore<TNumber>(line, "repl-input", context, state, option);
    }
}

template<typename TNumber>
void ExecuteCore(std::string_view source, std::string_view filePath, CompilationContext& context,
                 ExecutionState<TNumber>& state, const Option& option)
{
    using namespace std;

#ifdef ENABLE_JIT
    using namespace llvm;
#endif // ENABLE_JIT

    try
    {
        auto start = chrono::high_resolution_clock::now();

        auto tokens = Lex(source, context);
        auto op = Parse(tokens, context);
        if (option.optimize)
        {
            op = Optimize<TNumber>(context, op);
        }

        bool hasRecursiveCall = HasRecursiveCall(op, context);

        if (option.printInfo)
        {
            cout << "Has recursive call: " << (hasRecursiveCall ? "True" : "False") << endl << endl;
            PrintTree(context, op);
        }

        ExecutionType actualExecutionEngine = option.executionType;
#ifdef ENABLE_JIT
        if (actualExecutionEngine == ExecutionType::JIT && !option.alwaysJit &&
            !HasRecursiveCall(op, context))
        {
            // There is no need to use Jit compilation for simple program.
            // So we use StackMachine execution instead.
            actualExecutionEngine = ExecutionType::StackMachine;
        }
#ifdef ENABLE_GMP
        if constexpr (std::is_same_v<TNumber, mpz_class>)
        {
            if (actualExecutionEngine == ExecutionType::JIT)
            {
                // Jit compiler does not support GMP
                actualExecutionEngine = ExecutionType::StackMachine;
            }
        }
#endif // ENABLE_GMP
#endif // ENABLE_JIT

        TNumber result;
        switch (actualExecutionEngine)
        {
#ifdef ENABLE_JIT
        case ExecutionType::JIT:
#ifdef ENABLE_GMP
            if constexpr (std::is_same_v<TNumber, mpz_class>)
            {
                result = 0; // Suppress compiler warning
                UNREACHABLE();
            }
            else
#endif // ENABLE_GMP
            {
                result =
                    EvaluateByJIT<TNumber>(context, state, op, option.optimize, option.printInfo);
            }
            break;
#endif // ENABLE_JIT
        case ExecutionType::StackMachine:
        {
            auto module = GenerateStackMachineModule<TNumber>(op, context);

            if (option.printInfo)
            {
                PrintStackMachineModule(module);
            }

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
    catch (Exceptions::Calc4Exception& error)
    {
        cout << filePath;

        auto& position = error.GetPosition();
        if (position)
        {
            cout << ":" << (position->lineNo + 1) << ":" << (position->charNo + 1) << ":";
        }

        cout << " Error: " << error.what() << endl;

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

    cout << ProgramName << std::endl
         << std::endl
         << "Options:" << endl
         << CommandLineArgs::IntegerSize << "|" << CommandLineArgs::IntegerSizeShort << " <size>"
         << endl
         << Indent << "Specify size of integer" << endl
         << Indent << "size: 32, 64"
#ifdef ENABLE_INT128
         << ", 128"
#endif // ENABLE_INT128
#ifdef ENABLE_GMP
         << ", " << CommandLineArgs::InfinitePrecisionInteger
         << " (meaning infinite-precision or arbitrary-precision)"
#endif // ENABLE_GMP
         << endl
#ifdef ENABLE_JIT
         << CommandLineArgs::EnableJit << endl
         << Indent << "Enable JIT compilation" << endl
         << CommandLineArgs::DisableJit << endl
         << Indent << "Disable JIT compilation" << endl
         << CommandLineArgs::AlwaysJit << "|" << CommandLineArgs::AlwaysJitShort << endl
         << Indent << "Force JIT compilation" << endl
#endif // ENABLE_JIT
         << CommandLineArgs::EnableOptimization << endl
         << Indent << "Enable optimization" << endl
         << CommandLineArgs::DisableOptimization << endl
         << Indent << "Disable optimization" << endl
         << CommandLineArgs::PerformTest << endl
         << Indent << "Perform test" << endl;
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

const char* GetExecutionTypeString(ExecutionType type)
{
    switch (type)
    {
#ifdef ENABLE_JIT
    case ExecutionType::JIT:
        return "JIT";
#endif // ENABLE_JIT
    case ExecutionType::StackMachine:
        return "StackMachine";
    case ExecutionType::Interpreter:
        return "Interpreter";
    default:
        return "<Unknown>";
    }
}
