/*****
 *
 * The Calc4 Programming Language
 *
 * Copyright (C) 2018-2026 Yuya Watari
 * This software is released under the MIT License, see LICENSE file for details
 *
 *****/

#include "ExecutionTestCases.h"

#include <iterator>

namespace
{
ExecutionTestCaseBase ExecutionTestCaseBases[] = {
    // clang-format off
    { "1<2", "", 1 },
    { "1<=2", "", 1 },
    { "1>=2", "", 0 },
    { "1>2", "", 0 },
    { "2<1", "", 0 },
    { "2<=1", "", 0 },
    { "2>=1", "", 1 },
    { "2>1", "", 1 },
    { "1<1", "", 0 },
    { "1<=1", "", 1 },
    { "1>=1", "", 1 },
    { "1>1", "", 0 },
    { "12345678", "", 12345678 },
    { "1+2*3-10", "", -1 },
    { "0?1?2?3?4", "", 3 },
    { "1==0?2?3", "", 3 },
    { "0==1?2?3", "", 3 },
    { "0==0?2?3", "", 2 },
    { "I==0?2?3", "A", 3 },
    { "0==I?2?3", "A", 3 },
    { "1!=0?2?3", "", 2 },
    { "0!=1?2?3", "", 2 },
    { "0!=0?2?3", "", 3 },
    { "I!=0?2?3", "A", 2 },
    { "0!=I?2?3", "A", 2 },
    { "72P101P108P108P111P10P", "", 0, "Hello\n" },
    { "1+// C++ style comment\n2", "", 3 },
    { "1+/* C style comment*/2", "", 3 },
    { "1&&2?3?4", "", 3 },
    { "1&&0?3?4", "", 4 },
    { "0&&2?3?4", "", 4 },
    { "0&&0?3?4", "", 4 },
    { "1||2?3?4", "", 3 },
    { "1||0?3?4", "", 3 },
    { "0||2?3?4", "", 3 },
    { "0||0?3?4", "", 4 },
    { "1&&(65P)", "", 0, "A" },
    { "0&&(65P)", "", 0 },
    { "1||(65P)", "", 1 },
    { "0||(65P)", "", 0, "A" },
    { "0&&(1/0)?1?2", "", 2 },
    { "1||(1/0)?1?2", "", 1 },
    { "(1&&2)+5", "", 6 },
    { "(2&&3)+5", "", 6 },
    { "(0||2)+5", "", 6 },
    { "(2||0)+5", "", 6 },
    { "0&&1&&(65P)", "", 0 },
    { "1||0||(65P)", "", 1 },
    { "1&&1&&(65P)", "", 0, "A" },
    { "0||0||(65P)", "", 0, "A" },
    { "(1&&0)||1", "", 1 },
    { "1&&(0||1)", "", 1 },
    { "(1<2)&&(2<1)", "", 0 },
    { "(1<2)||(2<1)", "", 1 },
    { "(0-1)&&1", "", 1 },
    { "(0-1)||0", "", 1 },
    { "D[true||1||2]{true}", "", 1 },
    { "D[select|a,b|a?a?b] (0{select}5) + (3{select}4)", "", 8 },
    { "D[pick|a,b,c|a?b?c] (0{pick}5{pick}9) + (1{pick}2{pick}3)", "", 11 },
    { "D[sum|n,acc|n==0?{acc}?(n-1){sum}({acc}+1)] (5{sum}0) + 7", "", 12 },
    { "D[print||72P101P108P108P111P10P] {print}", "", 0, "Hello\n" },
    { "D[add|x,y|x+y] 12{add}23", "", 35 },
    { "D[get12345||12345] {get12345}+{get12345}", "", 24690 },
    { "D[fact|x,y|x==0?y?(x-1){fact}(x*y)] 10{fact}1", "", 3628800 },
    { "D[fib|n|n<=1?n?(n-1){fib}+(n-2){fib}] 10{fib}", "", 55 },
    { "D[fibImpl|x,a,b|x ? ((x-1) ? ((x-1){fibImpl}(a+b){fibImpl}a) ? a) ? b] D[fib|x|x{fibImpl}1{fibImpl}0] 10{fib}", "", 55 },
    { "D[f|a,b,p,q,c|c < 2 ? ((a*p) + (b*q)) ? (c % 2 ? ((a*p) + (b*q) {f} (a*q) + (b*q) + (b*p) {f} (p*p) + (q*q) {f} (2*p+q)*q {f} c/2) ? (a {f} b {f} (p*p) + (q*q) {f} (2*p+q)*q {f} c/2))] D[fib|n|0{f}1{f}0{f}1{f}n] 10{fib}", "", 55 },
    { "D[tarai|x,y,z|x <= y ? y ? (((x - 1){tarai}y{tarai}z){tarai}((y - 1){tarai}z{tarai}x){tarai}((z - 1){tarai}x{tarai}y))] 10{tarai}5{tarai}5", "", 5 },
    { "1S", "", 1 },
    { "L", "", 0 },
    { "1S[var]", "", 1 },
    { "L[var]", "", 0 },
    { "D[get||L[var]] D[set|x|xS[var]] 123{set} {get} * {get}", "", 15129 },
    { "D[set|x|xS] 7{set}L", "", 7 },
    { "D[set|x|xS] 7{set}LS[var1] L[zero]3{set}LS[var2] L[var1]*L[var2]", "", 21 },
    { "(123S)L*L", "", 15129 },
    { "(123S[var])L[var]*L[var]", "", 15129 },
    { "((100+20+3)S)L*L", "", 15129, nullptr, { { "", 123 } } },
    { "((100+20+3)S[var])L[var]*L[var]", "", 15129, nullptr, { { "var", 123 } } },
    { "D[op||(123S)L*L]{op}", "", 15129 },
    { "D[op||L*L](123S){op}", "", 15129 },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] (20{fib}S)+L", "", 13530, nullptr, { { "", 6765 } } },
    { "D[get||L] D[set|x|xS] D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] (20{fib}>=1000?10?5)S {get}", "", 10, nullptr, { { "", 10 } } },
    { "D[get||L] D[set|x|xS] D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] (20{fib}>=1000?10S?5S) {get}", "", 10, nullptr, { { "", 10 } } },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] D[fib2||L{fib}] D[set|x|xS] 3{set} {fib2}", "", 2 },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] D[fib2||L{fib}] D[set|x|xS] 20{set} {fib2}", "", 6765 },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] D[fib2||L{fib}] D[set|x|xS] 3S {fib2}", "", 2 },
    { "D[fib|n|n<=1?n?((n-1){fib}+(n-2){fib})] D[fib2||L{fib}] D[set|x|xS] 20S {fib2}", "", 6765 },
    { "D[fib|n|10S(n<=1?n?((n-1){fib}+(n-2){fib}))S] 20{fib} L", "", 6765, nullptr, { { "", 6765 } } },
    { "0@", "", 0 },
    { "5->0", "", 5, nullptr, {}, { { 0, 5 } } },
    { "(10->20)L[zero]20@", "", 10, nullptr, {}, { { 20, 10 } } },
    { "((4+6)->(10+10))(20@)", "", 10, nullptr, {}, { { 20, 10 } } },
    { "(5->(0-1))((0-1)@)", "", 5, nullptr, {}, { { -1, 5 }}},
    { "(7->131072)((131072)@)", "", 7, nullptr, {}, { { 131072, 7 }}},
    { "D[func||(10->20)L[zero]20@] {func} (20@)", "", 10, nullptr, {}, { { 20, 10 } } },
    { "D[func||((4+6)->(10+10))(20@)] {func} (20@)", "", 10, nullptr, {}, { { 20, 10 } } },
    { "D[func||(10->20)L[zero]20@] D[get||20@] {func} (20@)", "", 10, nullptr, {}, { { 20, 10 } } },
    { "D[func||((4+6)->(10+10))(20@)] D[get||20@] {func} {get}", "", 10, nullptr, {}, { { 20, 10 } } },
    { "I", "A", 65 },
    { "I+I", "AB", 131 },
    { "1+2+I", "A", 68 },
    { "D[Input||I]{Input}", "A", 65 },
    { "I", "", -1 },

    // Byte I/O: non-ASCII bytes must be preserved
    { "128P255P0", "", 0, "\x80\xFF" },
    { "I", "\xFF", 255 },

    // Identifier sanitization regression tests (must be collision-free)
    { "(1S)(2S[empty])(L+L[empty])", "", 3, nullptr, { { "", 1 }, { "empty", 2 } } },
    { "(1S)(2S[default])(L+L[default])", "", 3, nullptr, { { "", 1 }, { "default", 2 } } },
    { "(1S[/])(2S[_2F])(L[/]+L[_2F])", "", 3, nullptr, { { "/", 1 }, { "_2F", 2 } } },
    { "(1S[1])(2S[_1])(L[1]+L[_1])", "", 3, nullptr, { { "1", 1 }, { "_1", 2 } } },
    { "D[/||1] D[_2F||2] ({/}+{_2F})", "", 3 },
    { "D[1||1] D[_1||2] ({1}+{_1})", "", 3 },
    { "1S[a-b]L[a-b]", "", 1, nullptr, { { "a-b", 1 } } },
    { "D[a-b||1]{a-b}", "", 1 },

    // Fast-path / fallback boundary (mix linear memory + sparse fallback)
    { "(1->131071)(2->131072)(131071@+131072@)", "", 3, nullptr, {}, { { 131071, 1 }, { 131072, 2 } } },
    { "(1->0)(2->(0-1))(0@+(0-1)@)", "", 3, nullptr, {}, { { 0, 1 }, { -1, 2 } } },
    // clang-format on
};
}

const ExecutionTestCaseBase* GetExecutionTestCaseBases()
{
    return ExecutionTestCaseBases;
}

size_t GetExecutionTestCaseBaseCount()
{
    return std::size(ExecutionTestCaseBases);
}
