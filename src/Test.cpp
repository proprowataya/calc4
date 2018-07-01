#include <iostream>
#include <cstdint>
#include "Operators.h"
#include "SyntaxAnalysis.h"
#include "Jit.h"
#include "Test.h"
#include "Evaluator.h"

namespace {
    struct TestCase {
        const char *input;
        int32_t expected;
    };

    struct TestResult {
        int success = 0;
        int fail = 0;
    };

    constexpr TestCase TestCases[] = {
        { "1<2", 1 },
        { "12345678", 12345678 },
        { "1+2*3", (1 + 2) * 3 },
        { "0?1?2?3?4", 3 },
        { "D[add|x,y|x+y] 12{add}23", 12 + 23 },
        { "D[get12345||12345] {get12345}+{get12345}", 12345 + 12345 },
        { "D[fact|x,y|x==0?y?(x-1){fact}(x*y)] 10{fact}1", 3628800 },

        // Fibonacci
        { "D[fib|n|n<=1?n?(n-1){fib}+(n-2){fib}] 10{fib}", 55 },
        { "D[fibImpl|x,a,b|x ? ((x-1) ? ((x-1){fibImpl}(a+b){fibImpl}a) ? a) ? b] D[fib|x|x{fibImpl}1{fibImpl}0] 10{fib}", 55 },
        { "D[f|a,b,p,q,c|c < 2 ? ((a*p) + (b*q)) ? (c % 2 ? ((a*p) + (b*q) {f} (a*q) + (b*q) + (b*p) {f} (p*p) + (q*q) {f} (2*p+q)*q {f} c/2) ? (a {f} b {f} (p*p) + (q*q) {f} (2*p+q)*q {f} c/2))] D[fib|n|0{f}1{f}0{f}1{f}n] 10{fib}", 55 },

        // Tarai
        { "D[tarai|x,y,z|x <= y ? y ? (((x - 1){tarai}y{tarai}z){tarai}((y - 1){tarai}z{tarai}x){tarai}((z - 1){tarai}x{tarai}y))] 10{tarai}5{tarai}5", 5 },
    };

    constexpr size_t NumTestCases = sizeof(TestCases) / sizeof(TestCase);

    template<typename TNumber> void TestOne(TestCase test, TestResult &testResult);
}

void TestAll() {
    using namespace std;
    TestResult result;

    for (size_t i = 0; i < NumTestCases; i++) {
        auto test = TestCases[i];
        TestOne<int32_t>(test, result);
        TestOne<int64_t>(test, result);
        TestOne<__int128_t>(test, result);
    }

    cout << "Test result" << endl
        << "    Total: " << (result.success + result.fail)
        << ", Success: " << result.success
        << ", Fail : " << result.fail
        << endl;
}

namespace {
    template<typename TNumber>
    void TestOne(TestCase test, TestResult &testResult) {
        using namespace std;

        for (auto& optimize : { true, false }) {
            for (auto& jit : { true, false }) {
                try {
                    CompilationContext context;
                    auto tokens = Lex(test.input, context);
                    auto op = Parse(tokens, context);

                    TNumber result;
                    if (jit) {
                        result = RunByJIT<TNumber>(context, op, optimize, false);
                    } else {
                        Evaluator<TNumber> eval(&context);
                        op->Accept(eval);
                        result = eval.value;
                    }

                    if (result != test.expected) {
                        cout
                            << "Test failed for \"" << test.input << "\" (optimize = " << (optimize ? "on" : "off") << ", JIT = " << (jit ? "on" : "off") << ")" << endl
                            << "---> Expected: " << test.expected << ", Result: " << result << endl;
                        testResult.fail++;
                    } else {
                        testResult.success++;
                    }
                } catch (std::string &error) {
                    cout
                        << "Test failed for \"" << test.input << "\" (optimize = " << (optimize ? "on" : "off") << ", JIT = " << (jit ? "on" : "off") << ")" << endl
                        << "---> Exception \"" << error << "\"" << endl;
                    testResult.fail++;
                }
            }
        }
    }
}
