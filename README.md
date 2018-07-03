# Operator Only Language "Calc4"

Calc4 is a programming language where everything in its code is an operator.

First of all, see [the sample codes](https://github.com/proprowataya/calc4#sample-codes) below.

## Requirements

* [clang](https://clang.llvm.org/) (>= 3.8)
* [LLVM](https://llvm.org/) (>= 3.8)
* [GMP](https://gmplib.org/)
* zlib
* (make)
* (git)

## Getting Started

### Install Requirements

Steps to install requirements on Ubuntu:
```
sudo apt update
sudo apt install clang llvm llvm-dev libgmp-dev zlib1g-dev make git
```

### Build

```
git clone https://github.com/proprowataya/calc4.git
cd calc4/src
make
```

### Run

Simply type ``calc4`` to run Calc4. Calc4 works as REPL. Please input what you want to evaluate.
```
$ ./calc4
Calc4 REPL
    Integer size: 64
    Executor: JIT
    Always JIT: off
    Optimize: on

> 1+2
3
Elapsed: 0.007 ms

> D[fib|n|n <= 1? n ? (n-1){fib} + (n-2){fib}] 38{fib}
39088169
Elapsed: 254.93 ms

>
```

## Sample Codes

### Addition

* Calc4 Sample Code
    ```
    12 + 23
    ```
* Equivalent to the following C code
    ```c
    return 12 + 23;
    ```
* Result
    ```
    > 12 + 23
    35
    Elapsed: 0.005 ms
    ```

### Define your own operator

* Calc4 Sample Code
    ```
    D[myadd|x, y|x + y] 12{myadd}23
    ```
* Equivalent to
    ```c
    int myadd(int x, int y) {
        return x + y;
    }

    return myadd(12, 23);
    ```
* Result
    ```
    > D[myadd|x, y|x + y] 12{myadd}23
    35
    Elapsed: 0.005 ms
    ```

### Addition and Multiplication

* Calc4 Sample Code
    ```
    1 + 2 * 3
    ```
* Equivalent to
    ```c
    return (1 + 2) * 3;
    ```
    * not `1 + (2 * 3)` ! 
* Result
    ```
    > 1 + 2 * 3
    9
    Elapsed: 0.006 ms
    ```
* Why isn't this code evaluated to 7? I will explain its reason in the following section.

### Fibonacci Sequence (naïve version)

* Calc4 Sample Code
    ```
    D[fib|n|n <= 1? n ? (n-1){fib} + (n-2){fib}] 38{fib}
    ```
* Equivalent to
    ```c
    int fib(int n) {
        return n <= 1 ? n : fib(n - 1) + fib(n - 2);
    }

    return fib(38);
    ```
* Result
    ```
    > D[fib|n|n <= 1? n ? (n-1){fib} + (n-2){fib}] 38{fib}
    39088169
    Elapsed: 254.93 ms
    ```
* `fib` is slow because its order is exponential.

### Fibonacci Sequence (tail call version)

* Calc4 Sample Code
    ```
    D[fib2|x, a, b|x ? ((x-1) ? ((x-1) {fib2} (a+b) {fib2}a) ? a) ? b] 38{fib2}1{fib2}0
    ```
* Equivalent to
    ```c
    int fib2(int x, int a, int b) {
        if (x == 0) {
            return b;
        } else if (x == 1) {
            return a;
        } else {
            return fib2(x - 1, a + b, a);
        }
    }

    return fib2(38, 1, 0);
    ```
* Result
    ```
    > D[fib2|x, a, b|x ? ((x-1) ? ((x-1) {fib2} (a+b) {fib2}a) ? a) ? b] 38{fib2}1{fib2}0
    39088169
    Elapsed: 7.085 ms
    ```
* `fib2` is much faster than `fib`.

### Tarai Function

[Tarai function](https://en.wikipedia.org/wiki/Tak_(function)) is used to perform programming language benchmarks.

* Calc4 Sample Code
    ```
    D[tarai|x, y, z|x <= y ? y ? (((x - 1){tarai}y{tarai}z){tarai}((y - 1){tarai}z{tarai}x){tarai}((z - 1){tarai}x{tarai}y))] 18{tarai}15{tarai}5
    ```
* Equivalent to
    ```c
    int tarai(int x, int y, int z) {
        if (x <= y) {
            return y;
        } else {
            return tarai(tarai(x - 1, y, z), tarai(y - 1, z, x), tarai(z - 1, x, y));
        }
    }

    return tarai(18, 15, 5);
    ```
* Result
    ```
    > D[tarai|x, y, z|x <= y ? y ? (((x - 1){tarai}y{tarai}z){tarai}((y - 1){tarai}z{tarai}x){tarai}((z - 1){tarai}x{tarai}y))] 18{tarai}15{tarai}5
    18
    Elapsed: 422.455 ms
    ```
* **NOTE:** The above C program took 329 ms to execute on my machine (compiled by clang with `-Ofast` option). Calc4's performance seems to be closer to native C.

## What is "Operator Only" Language?

The biggest feature of Calc4 is that the program consists only of operators. But what does that mean?

Let's take a look by using the next simple code.

* Code 1
    ```
    46
    ```

This code is actually composed by two operators, "4" and "6". There is no token such as "46". The operator "6" takes one operand and returns ``(operand * 10) + 6``. Its operand is the result of the operator "4", which is similar.

In other words, we can rewrite code 1 by the following C code.
```c
int operator4(int operand) {
    return (operand * 10) + 4;
}

int operator6(int operand) {
    return (operand * 10) + 6;
}

int zeroOperator() {
    return 0;
}

return operator6(operator4(zeroOperator()));
```
* NOTE: The operator "4" in code 1 implicitly takes a constant value 0, namely zero operator, as its operand.

An AST (Abstract Syntax Tree) clarifies what I explained.
* AST of code 1
    ```
    DecimalOperator [Value = 6]
      DecimalOperator [Value = 4]
        ZeroOperator []
    ```
    * ``DecimalOperator [Value = 4]`` and ``DecimalOperator [Value = 6]`` stand for operator "4" and "6", respectively.
    * To print AST, type ``#print on`` on the REPL.

In this way, Calc4 represents every element as an operator.

### Operator Precedence

The rules of operator precedence in Calc4 are as follows:
* Operators with fewer operands have higher precedence.
* Operators with the same number of operands have equal precedence. They are left-associative.

This is the reason why ``1 + 2 * 3`` is evaluated to 9, not 7.

## Why is it named "Calc4"? 

The answer is that "because Calc4 has been developed using calculator as a motif".

In the above section, I defined the operator that computes Fibonacci sequence. We can use it like 
```
38{fib}
```

This can be interpreted as follows:
1. Create a new Fibonacci button on your calculator
1. Press the "3" button
1. Press the "8" button
1. Press the Fibonacci button

Defining operators corresponds to creating new calculator buttons!

So what is 4 in Calc4? [Hanc marginis exiguitas non caperet](https://en.wikipedia.org/wiki/Fermat%27s_Last_Theorem) :)

## Performance

Calc4 has high performance, since its code is converted to machine code by the LLVM JIT compiler.

Calc4 execution environment first generates LLVM IR, then LLVM compiles it. To show LLVM IR, type "``#print on``" on REPL.
```
> #print on

> D[fib|n|n <= 1? n ? (n-1){fib} + (n-2){fib}] 38{fib}
    ...
LLVM IR (After optimized):
---------------------------
; ModuleID = 'calc4-jit-module'
source_filename = "calc4-jit-module"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: nounwind readnone
define i64 @fib(i64) local_unnamed_addr #0 {
entry:
  %1 = icmp slt i64 %0, 2
  br i1 %1, label %8, label %2

; <label>:2:                                      ; preds = %entry
  %3 = add i64 %0, -1
  %4 = tail call i64 @fib(i64 %3)
  %5 = add i64 %0, -2
  %6 = tail call i64 @fib(i64 %5)
  %7 = add i64 %6, %4
  ret i64 %7

; <label>:8:                                      ; preds = %entry
  ret i64 %0
}

; Function Attrs: nounwind readnone
define i64 @__Main__() local_unnamed_addr #0 {
entry:
  %0 = tail call i64 @fib(i64 38)
  ret i64 %0
}

attributes #0 = { nounwind readnone }
---------------------------
```

Note that Calc4 executor does NOT always perform JIT compilation. By default, it is operated only when an operator has recursive call. You can force JIT compilation by running Calc4 with ``--always-jit`` option.
