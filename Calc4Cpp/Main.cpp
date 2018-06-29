#include <iostream>
#include <ctime>
#include <cstdint>
#include "Operators.h"
#include "SyntaxAnalysis.h"
#include "Jit.h"

enum class ExecutionType {
    JIT, Interpreter,
};

//using NumberType = int8_t;
//using NumberType = int16_t;
//using NumberType = int32_t;
using NumberType = int64_t;
//using NumberType = __int128_t;

// https://stackoverflow.com/questions/25114597/how-to-print-int128-in-g
std::ostream& operator<<(std::ostream& dest, __int128_t value) {
    std::ostream::sentry s(dest);

    if (s) {
        __uint128_t tmp = value < 0 ? -value : value;
        char buffer[128];
        char* d = std::end(buffer);

        do {
            --d;
            *d = "0123456789"[tmp % 10];
            tmp /= 10;
        } while (tmp != 0);
        if (value < 0) {
            --d;
            *d = '-';
        }

        int len = std::end(buffer) - d;

        if (dest.rdbuf()->sputn(d, len) != len) {
            dest.setstate(std::ios_base::badbit);
        }
    }

    return dest;
}

template<typename TNumber>
void PrintTree(const Operator<TNumber> &op, int depth) {
    for (int i = 0; i < depth * 4; i++) {
        std::cout << ' ';
    }

    std::cout << typeid(op).name() << std::endl;
    for (auto& operand : op.GetOperands()) {
        PrintTree(*operand, depth + 1);
    }
}

template<typename TNumber>
bool HasRecursiveCallInternal(const Operator<TNumber> &op, const CompilationContext<TNumber> &context, std::unordered_map<const OperatorDefinition *, int> called) {
    if (auto userDefined = dynamic_cast<const UserDefinedOperator<TNumber> *>(&op)) {
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
    } else if (auto parenthesis = dynamic_cast<const ParenthesisOperator<TNumber> *>(&op)) {
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

template<typename TNumber>
bool HasRecursiveCall(const Operator<TNumber> &op, const CompilationContext<TNumber> &context) {
    return HasRecursiveCallInternal(op, context, {});
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

int main() {
    using namespace std;
    using namespace llvm;

    constexpr ExecutionType type = ExecutionType::JIT;

    if (type == ExecutionType::JIT) {
        LLVMInitializeNativeTarget();
        LLVMInitializeNativeAsmPrinter();
        LLVMInitializeNativeAsmParser();
    }

    cout << "Calc4 REPL" << endl
        << "Executor type: " << GetExecutionTypeString(type) << endl
        << endl;

    bool optimize = true, printInfo = true, alwaysJIT = false;
    while (true) {
        try {
            string line;
            cout << "> ";
            getline(cin, line);

            if (line == "#print on") {
                printInfo = true;
                cout << endl;
                continue;
            } else if (line == "#print off") {
                printInfo = false;
                cout << endl;
                continue;
            } else if (line == "#optimize on") {
                optimize = true;
                cout << endl;
                continue;
            } else if (line == "#optimize off") {
                optimize = false;
                cout << endl;
                continue;
            }

            CompilationContext<NumberType> context;
            auto tokens = Lex(line, context);
            auto op = Parse(tokens, context);
            bool hasRecursiveCall = HasRecursiveCall(*op, context);

            if (printInfo) {
                cout << "Has recursive call: " << (hasRecursiveCall ? "True" : "False") << endl;

                cout << "Tree:" << endl
                    << "---------------------------" << endl
                    << "Module {" << endl
                    << "Main:" << endl;
                PrintTree<NumberType>(*op, 1);
                cout << endl;

                for (auto it = context.UserDefinedOperatorBegin(); it != context.UserDefinedOperatorEnd(); it++) {
                    auto& name = it->second.GetDefinition().GetName();
                    auto& op = it->second.GetOperator();

                    cout << name << ":" << endl;
                    PrintTree<NumberType>(*op, 1);
                }

                cout << "}" << endl << "---------------------------" << endl << endl;
            }

            NumberType result;
            clock_t start = clock();
            {
                if (type == ExecutionType::JIT && (alwaysJIT || HasRecursiveCall(*op, context))) {
                    result = RunByJIT<NumberType>(context, op, optimize, printInfo);
                } else {
                    Evaluator<NumberType> eval(&context);
                    op->Accept(eval);
                    result = eval.value;
                }
            }
            clock_t end = clock();

            cout << result << endl
                << "Elapsed: " << (double)(end - start) / (CLOCKS_PER_SEC / 1000.0) << " ms" << endl
                << endl;
        } catch (std::string &error) {
            cout << "Error: " << error << endl << endl;
        }
    }

    llvm_shutdown();
}
