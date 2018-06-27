#include <iostream>
#include <ctime>
#include <cstdint>
#include "Operators.h"
#include "SyntaxAnalysis.h"
#include "Jit.h"

enum class ExecutionType {
    JIT, Interpreter,
};

using NumberType = int64_t;

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

    bool optimize = true, printInfo = true;
    while (true) {
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

        if (printInfo) {
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
            switch (type) {
            case ExecutionType::JIT:
            {
                result = RunByJIT<NumberType>(context, op, optimize, printInfo);
                break;
            }
            case ExecutionType::Interpreter:
            {
                Evaluator<NumberType> eval(&context);
                op->Accept(eval);
                result = eval.value;
                break;
            }
            default:
                cout << "Error: Unknown executor specified" << endl;
                result = 0;
                break;
            }
        }
        clock_t end = clock();

        cout << result << endl
            << "Elapsed: " << (double)(end - start) / CLOCKS_PER_SEC << endl
            << endl;
    }

    llvm_shutdown();
}
