#include <iostream>
#include <ctime>
#include "Operators.h"
#include "SyntaxAnalysis.h"
#include "Jit.h"

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

int main() {
    using namespace std;
    using namespace llvm;

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    while (true) {
        string line;
        cout << "> ";
        getline(cin, line);

        CompilationContext<int> context;
        auto tokens = Lex(line, context);
        auto op = Parse(tokens, context);

        Evaluator<int> eval(&context);
        clock_t start = clock();
        op->Accept(eval);
        clock_t end = clock();
        cout << eval.value << endl
            << "Elapsed: " << (double)(end - start) / CLOCKS_PER_SEC << endl
            << endl;

        start = clock();
        int result = RunByJIT<int>(context, op);
        end = clock();
        cout << result << endl
            << "JIT Elapsed: " << (double)(end - start) / CLOCKS_PER_SEC << endl
            << endl;
    }

    llvm_shutdown();
}
