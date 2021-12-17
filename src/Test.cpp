#include "Test.h"
#include "Evaluator.h"
#include "Jit.h"
#include "Operators.h"
#include "Optimizer.h"
#include "SyntaxAnalysis.h"
#include <cstdint>
#include <gmpxx.h>
#include <iostream>

namespace
{
struct TestCase
{
    const char* input;
    int32_t expected;
};

struct TestResult
{
    int success = 0;
    int fail = 0;
};

constexpr TestCase TestCases[] = {
    // clang-format off
    { "1<2", 1 },
    { "1<=2", 1 },
    { "1>=2", 0 },
    { "1>2", 0 },
    { "2<1", 0 },
    { "2<=1", 0 },
    { "2>=1", 1 },
    { "2>1", 1 },
    { "1<1", 0 },
    { "1<=1", 1 },
    { "1>=1", 1 },
    { "1>1", 0 },
    { "12345678", 12345678 },
    { "1+2*3-10", -1 },
    { "0?1?2?3?4", 3 },
    { "72P101P108P108P111P10P", 0 },
    { "D[print||72P101P108P108P111P10P] {print}", 0 },
    { "D[add|x,y|x+y] 12{add}23", 35 },
    { "D[get12345||12345] {get12345}+{get12345}", 24690 },
    { "D[fact|x,y|x==0?y?(x-1){fact}(x*y)] 10{fact}1", 3628800 },

    // Fibonacci
    { "D[fib|n|n<=1?n?(n-1){fib}+(n-2){fib}] 10{fib}", 55 },
    { "D[fibImpl|x,a,b|x ? ((x-1) ? ((x-1){fibImpl}(a+b){fibImpl}a) ? a) ? b] D[fib|x|x{fibImpl}1{fibImpl}0] 10{fib}", 55 },
    { "D[f|a,b,p,q,c|c < 2 ? ((a*p) + (b*q)) ? (c % 2 ? ((a*p) + (b*q) {f} (a*q) + (b*q) + (b*p) {f} (p*p) + (q*q) {f} (2*p+q)*q {f} c/2) ? (a {f} b {f} (p*p) + (q*q) {f} (2*p+q)*q {f} c/2))] D[fib|n|0{f}1{f}0{f}1{f}n] 10{fib}", 55 },

    // Tarai
    { "D[tarai|x,y,z|x <= y ? y ? (((x - 1){tarai}y{tarai}z){tarai}((y - 1){tarai}z{tarai}x){tarai}((z - 1){tarai}x{tarai}y))] 10{tarai}5{tarai}5", 5 },

    // User defined variables
    { "1S", 1 },
    { "L", 0 },
    { "1S[var]", 1 },
    { "L[var]", 0 },
    { "D[get||L[var]] D[set|x|xS[var]] 123{set} {get} * {get}", 15129 },
    { "D[set|x|xS] 7{set}L", 7 },
    { "D[set|x|xS] 7{set}LS[var1] L[zero]3{set}LS[var2] L[var1]*L[var2]", 21 },
    { "(123S)L*L", 15129 },
    { "(123S[var])L[var]*L[var]", 15129 },
    { "((100+20+3)S)L*L", 15129 },
    { "((100+20+3)S[var])L[var]*L[var]", 15129 },
    { "D[op||(123S)L*L]{op}", 15129 },
    { "D[op||L*L](123S){op}", 15129 },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] (20{fib}S)+L", 13530 },
    { "D[get||L] D[set|x|xS] D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] (20{fib}>=1000?10?5)S {get}", 10 },
    { "D[get||L] D[set|x|xS] D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] (20{fib}>=1000?10S?5S) {get}", 10 },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] D[fib2||L{fib}] D[set|x|xS] 3{set} {fib2}", 2 },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] D[fib2||L{fib}] D[set|x|xS] 20{set} {fib2}", 6765 },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] D[fib2||L{fib}] D[set|x|xS] 3S {fib2}", 2 },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] D[fib2||L{fib}] D[set|x|xS] 20S {fib2}", 6765 },
    { "D[fib|n|10S(n<=1?n?((n-1){fib}+(n-2){fib}))S] 20{fib} L", 6765 },

    // Array accesses
    { "0@", 0 },
    { "5->0", 5 },
    { "(10->20)L[zero]20@", 10 },
    { "((4+6)->(10+10))(20@)", 10 },
    { "D[func||(10->20)L[zero]20@] {func} (20@)", 10 },
    { "D[func||((4+6)->(10+10))(20@)] {func} (20@)", 10 },
    { "D[func||(10->20)L[zero]20@] D[get||20@] {func} (20@)", 10 },
    { "D[func||((4+6)->(10+10))(20@)] D[get||20@] {func} {get}", 10 },
    // clang-format on
};

constexpr size_t NumTestCases = sizeof(TestCases) / sizeof(TestCase);

template<typename TNumber>
void TestOne(TestCase test, TestResult& testResult);
}

void TestAll()
{
    using namespace std;
    TestResult result;

    for (size_t i = 0; i < NumTestCases; i++)
    {
        auto test = TestCases[i];
        TestOne<int32_t>(test, result);
        TestOne<int64_t>(test, result);
        TestOne<__int128_t>(test, result);
        TestOne<mpz_class>(test, result);
    }

    cout << "Test result" << endl
         << "    Total: " << (result.success + result.fail) << ", Success: " << result.success
         << ", Fail : " << result.fail << endl;
}

namespace
{
template<typename TNumber>
void TestOne(TestCase test, TestResult& testResult)
{
    using namespace std;

    for (auto& optimize : { true, false })
    {
        for (auto& jit : { true, false })
        {
            try
            {
                cout << "Testing for \"" << test.input
                     << "\" (optimize = " << (optimize ? "on" : "off")
                     << ", JIT = " << (jit ? "on" : "off") << ", type = " << typeid(TNumber).name()
                     << ") ";
                CompilationContext context;
                auto tokens = Lex(test.input, context);
                auto op = Parse(tokens, context);
                if (optimize)
                {
                    op = Optimize<TNumber>(context, op);
                }

                TNumber result;
                if (jit)
                {
                    result = EvaluateByJIT<TNumber>(context, op, optimize, false);
                }
                else
                {
                    using TVariableSource = DefaultVariableSource<TNumber>;
                    using TGlobalArraySource = Calc4GlobalArraySource<TNumber>;
                    ExecutionState<TNumber, TVariableSource, TGlobalArraySource> state;
                    Evaluator<TNumber, TVariableSource, TGlobalArraySource> eval(&context, &state);
                    op->Accept(eval);
                    result = eval.value;
                }

                if (result != test.expected)
                {
                    cout << "---> [Failed] Expected: " << test.expected << ", Result: " << result
                         << endl;
                    testResult.fail++;
                }
                else
                {
                    testResult.success++;
                    cout << "---> [Success]" << endl;
                }
            }
            catch (std::string& error)
            {
                cout << "---> [Failed] Exception \"" << error << "\"" << endl;
                testResult.fail++;
            }
            catch (std::exception& exception)
            {
                cout << "---> [Failed] Exception \"" << exception.what() << "\"" << endl;
                testResult.fail++;
            }
        }
    }
}
}
