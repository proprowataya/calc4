#include <iostream>
#include <sstream>
#include <ctime>
#include <cstdint>
#include <limits>
#include <gmpxx.h>
#include "Operators.h"
#include "SyntaxAnalysis.h"
#include "Jit.h"
#include "Test.h"
#include "Evaluator.h"

constexpr const char *ProgramName = "Calc4 REPL";
constexpr const int InfinitePrecisionIntegerSize = std::numeric_limits<int>::max();

enum class ExecutionType {
    JIT, Interpreter,
};

struct Option {
    int integerSize = 64;
    ExecutionType executionType = ExecutionType::JIT;
    bool alwaysJit = false;
    bool optimize = true;
    bool printInfo = false;
};

namespace CommandLineArgs {
    constexpr const char *Help = "--help";
    constexpr const char *PerformTest = "--test";
    constexpr const char *EnableJit = "--jit-on";
    constexpr const char *DisableJit = "--jit-off";
    constexpr const char *AlwaysJit = "--always-jit";
    constexpr const char *AlwaysJitShort = "-a";
    constexpr const char *IntegerSize = "--size";
    constexpr const char *IntegerSizeShort = "-s";
    constexpr const char *EnableOptimization = "-O";
    constexpr const char *DisableOptimization = "-Od";
    constexpr const char *InfinitePrecisionInteger = "inf";
}

inline bool StringEquals(const char *a, const char *b);
inline bool IsSupportedIntegerSize(int size);
void PrintHelp(int argc, char **argv);
template<typename TNumber> void ReplCore(const std::string &line, const Option &option);
void PrintTree(const Operator &op, int depth);
bool HasRecursiveCall(const Operator &op, const CompilationContext &context);
bool HasRecursiveCallInternal(const Operator &op, const CompilationContext &context, std::unordered_map<const OperatorDefinition *, int> called);
const char *GetExecutionTypeString(ExecutionType type);

int main(int argc, char **argv) {
    using namespace std;
    using namespace llvm;

    Option option;
    bool performTest = false;

    /* ***** Parse command line args ***** */
    for (int i = 1; i < argc; i++) {
        char *str = argv[i];

        auto GetNextArgument = [&i, argc, argv]() {
            if (i + 1 >= argc) {
                std::ostringstream oss;
                oss << "Option \"" << argv[i] << "\" requires argument";
                throw oss.str();
            }

            return argv[++i];
        };

        try {
            if (StringEquals(str, CommandLineArgs::Help)) {
                PrintHelp(argc, argv);
                return 0;
            } else if (StringEquals(str, CommandLineArgs::PerformTest)) {
                performTest = true;
            } else if (StringEquals(str, CommandLineArgs::EnableJit)) {
                option.executionType = ExecutionType::JIT;
            } else if (StringEquals(str, CommandLineArgs::DisableJit)) {
                option.executionType = ExecutionType::Interpreter;
            } else if (StringEquals(str, CommandLineArgs::AlwaysJit) || StringEquals(str, CommandLineArgs::AlwaysJitShort)) {
                option.alwaysJit = true;
            } else if (StringEquals(str, CommandLineArgs::IntegerSize) || StringEquals(str, CommandLineArgs::IntegerSizeShort)) {
                const char *arg = GetNextArgument();
                int size;

                if (StringEquals(arg, CommandLineArgs::InfinitePrecisionInteger)) {
                    size = (option.integerSize = InfinitePrecisionIntegerSize);
                } else {
                    size = (option.integerSize = atoi(arg));
                }

                if (!IsSupportedIntegerSize(size)) {
                    std::ostringstream oss;
                    oss << "Unsupported integer size " << option.integerSize;
                    throw oss.str();
                }
            } else if (StringEquals(str, CommandLineArgs::EnableOptimization)) {
                option.optimize = true;
            } else if (StringEquals(str, CommandLineArgs::DisableOptimization)) {
                option.optimize = false;
            } else {
                std::ostringstream oss;
                oss << "Unknown option \"" << str << "\"";
                throw oss.str();
            }
        } catch (std::string &error) {
            cout << "Error: " << error << endl << endl;
            PrintHelp(argc, argv);
            return EXIT_FAILURE;
        }
    }

    /* ***** Print header ***** */
    cout << ProgramName << endl;

    /* ***** Print current setting ***** */
    if (!performTest) {
        cout
            << "    Integer size: " << (option.integerSize == InfinitePrecisionIntegerSize ? "Infinite-precision" : std::to_string(option.integerSize)) << endl
            << "    Executor: " << GetExecutionTypeString(option.executionType) << endl
            << "    Always JIT: " << (option.alwaysJit ? "on" : "off") << endl
            << "    Optimize: " << (option.optimize ? "on" : "off") << endl
            << endl;
    }

    /* ***** Initialize LLVM if needed ***** */
    if (performTest || option.executionType == ExecutionType::JIT) {
        LLVMInitializeNativeTarget();
        LLVMInitializeNativeAsmPrinter();
        LLVMInitializeNativeAsmParser();
    }

    if (performTest) {
        TestAll();
        return 0;
    }

    while (true) {
        try {
            string line;
            cout << "> ";
            getline(cin, line);

            if (line == "#print on") {
                option.printInfo = true;
                cout << endl;
                continue;
            } else if (line == "#print off") {
                option.printInfo = false;
                cout << endl;
                continue;
            } else if (line == "#optimize on") {
                option.optimize = true;
                cout << endl;
                continue;
            } else if (line == "#optimize off") {
                option.optimize = false;
                cout << endl;
                continue;
            }

            switch (option.integerSize) {
            case 32:
                ReplCore<int32_t>(line, option);
                break;
            case 64:
                ReplCore<int64_t>(line, option);
                break;
            case 128:
                ReplCore<__int128_t>(line, option);
                break;
            case InfinitePrecisionIntegerSize:
                ReplCore<mpz_class>(line, option);
                break;
            default:
                UNREACHABLE();
                break;
            }
        } catch (std::string &error) {
            cout << "Error: " << error << endl << endl;
        }
    }

    llvm_shutdown();
}

inline bool StringEquals(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

inline bool IsSupportedIntegerSize(int size) {
    switch (size) {
    case 32:
    case 64:
    case 128:
    case InfinitePrecisionIntegerSize:
        return true;
    default:
        return false;
    }
}

void PrintHelp(int argc, char **argv) {
    using namespace std;
    static constexpr const char *Indent = "    ";

    cout
        << ProgramName << std::endl
        << std::endl
        << "Options:" << endl
        << CommandLineArgs::IntegerSize << "|" << CommandLineArgs::IntegerSizeShort << " <size>" << endl
        << Indent << "Specify size of integer" << endl
        << Indent << "size: 32, 64, 128, " << CommandLineArgs::InfinitePrecisionInteger << " (meaning infinite-precision or arbitrary-precision)" << endl
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

template<typename TNumber>
void ReplCore(const std::string &line, const Option &option) {
    using namespace std;
    using namespace llvm;

    CompilationContext context;

    auto tokens = Lex(line, context);
    auto op = Parse(tokens, context);
    bool hasRecursiveCall = HasRecursiveCall(*op, context);

    if (option.printInfo) {
        cout << "Has recursive call: " << (hasRecursiveCall ? "True" : "False") << endl;

        cout << "Tree:" << endl
            << "---------------------------" << endl
            << "Module {" << endl
            << "Main:" << endl;
        PrintTree(*op, 1);
        cout << endl;

        for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++) {
            auto& name = it->second.GetDefinition().GetName();
            auto& op = it->second.GetOperator();

            cout << name << ":" << endl;
            PrintTree(*op, 1);
        }

        cout << "}" << endl << "---------------------------" << endl << endl;
    }

    TNumber result;
    clock_t start = clock();
    {
        if (option.executionType == ExecutionType::JIT && (option.alwaysJit || HasRecursiveCall(*op, context))) {
            result = EvaluateByJIT<TNumber>(context, op, option.optimize, option.printInfo);
        } else {
            Evaluator<TNumber> eval(&context);
            op->Accept(eval);
            result = eval.value;
        }
    }
    clock_t end = clock();

    cout << result << endl
        << "Elapsed: " << (double)(end - start) / (CLOCKS_PER_SEC / 1000.0) << " ms" << endl
        << endl;
}

void PrintTree(const Operator &op, int depth) {
    using namespace std;

    auto PrintIndent = [depth]() {
        for (int i = 0; i < depth; i++) {
            cout << "  ";
        }
    };

    PrintIndent();

    cout << op.ToString() << endl;
    for (auto& operand : op.GetOperands()) {
        PrintTree(*operand, depth + 1);
    }

    if (auto parenthesis = dynamic_cast<const ParenthesisOperator *>(&op)) {
        PrintIndent();
        cout << "Contains:" << endl;
        for (auto& op2 : parenthesis->GetOperators()) {
            PrintTree(*op2, depth + 1);
        }
    }
}

bool HasRecursiveCall(const Operator &op, const CompilationContext &context) {
    return HasRecursiveCallInternal(op, context, {});
}

bool HasRecursiveCallInternal(const Operator &op, const CompilationContext &context, std::unordered_map<const OperatorDefinition *, int> called) {
    if (auto userDefined = dynamic_cast<const UserDefinedOperator *>(&op)) {
        auto& definition = userDefined->GetDefinition();
        if (++called[&definition] > 1) {
            // Recursive call detected
            return true;
        }

        if (HasRecursiveCallInternal(*context.GetOperatorImplement(definition.GetName()).GetOperator(), context, called)) {
            --called[&definition];
            return true;
        } else {
            --called[&definition];
        }
    } else if (auto parenthesis = dynamic_cast<const ParenthesisOperator *>(&op)) {
        for (auto& op2 : parenthesis->GetOperators()) {
            if (HasRecursiveCallInternal(*op2, context, called)) {
                return true;
            }
        }
    }

    for (auto& operand : op.GetOperands()) {
        if (HasRecursiveCallInternal(*operand, context, called)) {
            return true;
        }
    }

    return false;
}

const char *GetExecutionTypeString(ExecutionType type) {
    switch (type) {
    case ExecutionType::JIT:
        return "JIT";
    case ExecutionType::Interpreter:
        return "Interpreter";
    default:
        return "<Unknown>";
    }
}
