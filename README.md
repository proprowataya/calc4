# The Calc4 Programming Language

Calc4 is a programming language in which everything in the code is an operator.

## Try It Out

To try Calc4 in your web browser, please visit [Try Calc4](https://proprowataya.github.io/calc4/). On this site, you can run Calc4 code as you type.

## Overview

The design of Calc4 is inspired by calculators. Calc4 lets you program as if you were pressing the calculator's buttons. First, look at the following examples of Calc4 programs.

* Arithmetic operations
    ```
    46 + 2
    ```
* The Fibonacci sequence
    ```
    38{fib}
    ```

The first one is very simple and is exactly the same as using a calculator. The second is worth noting. As the name suggests, `{fib}` computes the 38th Fibonacci number. This is similar to the Fibonacci button on a calculator. In other words, the code above can be thought of as the following actions on a calculator:

1. Create a new Fibonacci button on the calculator
1. Press the "3" button
1. Press the "8" button
1. Press the Fibonacci button

To achieve this style of programming, Calc4 introduces the concept of everything being an operator. For example, the Fibonacci button is represented as a unary operator. Note that this operator is not provided by default but is defined by the programmer. In fact, "3" and "8" are also operators. Further details are provided in the next section.

Despite the simple grammar, Calc4 can perform complex computations such as the Mandelbrot set shown below. The program is available [here](sample/MandelbrotSet.txt).

#### Mandelbrot Set Drawn by Calc4
![Mandelbrot set drawn by Calc4](image/MandelbrotSet.png)

## Key Features of Calc4

### Everything is an Operator

The main feature of Calc4 is that programs use only operators in infix notation. Let me explain with the following sample code.

* Calc4 Sample Code 1
    ```
    46 + 2
    ```

The `+` above is of course an operator, but `4`, `6`, and `2` are also operators. The operator `6` takes one operand and returns `(operand * 10) + 6`. Its operand is the result of the operator `4`.

In other words, the code above is equivalent to the following C code.

```c
int operator2(int operand) {
    return (operand * 10) + 2;
}

int operator4(int operand) {
    return (operand * 10) + 4;
}

int operator6(int operand) {
    return (operand * 10) + 6;
}

int zeroOperator() {
    return 0;
}

return operator6(operator4(zeroOperator())) + operator2(zeroOperator());
```
* **NOTE:** The operators `2` and `4` in the example implicitly take the constant value 0, namely the zero operator, as their operand.

Because Calc4 aims to let you program like a calculator, "46" is not a single token but two operators. As a result, every element is expressed as an operator in Calc4.

### High Expressiveness Powered by Recursive Operators

Everything in Calc4 is an operator, but this does not mean Calc4 cannot express complex programs. Calc4 lets you define custom operators. For example, you can define a custom addition operator as follows. Here, `D` is an operator that defines a new operator.

```
D[myadd|x, y|x + y] 12{myadd}23
```

Popular programming languages such as C provide loops for complex algorithms. Calc4 does not have such syntax. Instead, you can use recursion with custom operators that call themselves. The following code defines the Fibonacci operator we saw earlier. It contains a typical recursive call.

```
D[fib|n|n <= 1? n ? (n-1){fib} + (n-2){fib}] 38{fib}
```

A more complex example is [the image at the beginning of this README](#mandelbrot-set-drawn-by-calc4), the Mandelbrot set drawn by Calc4. The program is available [here](sample/MandelbrotSet.txt). It uses [tail recursion](https://en.wikipedia.org/wiki/Tail_call) instead of loops.

Another sample is available.
* [Printing prime numbers up to 100](sample/PrintPrimes.txt)

## Getting Started

If you simply want to try Calc4, the [Try Calc4](https://proprowataya.github.io/calc4/) website is the best choice. Below are the steps to build a native Calc4 environment.

### Requirements

* C++ compiler supporting C++17
* [CMake](https://cmake.org/) (>= 3.8)
* (git)

### Building Calc4

1. Install CMake
    * dnf
        ```bash
        sudo dnf install cmake -y
        ```
    * apt
        ```bash
        sudo apt update
        sudo apt install cmake -y
        ```
    * Binary
        * https://cmake.org/
1. Build and run
    * Unix-like systems
        ```bash
        git clone https://github.com/proprowataya/calc4.git
        mkdir calc4-build
        cd calc4-build
        cmake ../calc4
        cmake --build .
        ./calc4 ../calc4/sample/MandelbrotSet.txt
        ```
    * Windows
        ```powershell
        git clone https://github.com/proprowataya/calc4.git
        mkdir calc4-build
        cd calc4-build
        cmake ..\calc4
        cmake --build . --config Release
        .\Release\calc4.exe ..\calc4\sample\MandelbrotSet.txt
        ```

If no command-line argument is specified, Calc4 runs as a REPL. Enter an expression to evaluate.

```
$ ./calc4
Calc4 REPL
    Integer size: 64
    Executor: StackMachine
    Optimize: on

> 72P101P108P108P111P32P119P111P114P108P100P33P10P
Hello world!
0
Elapsed: 0.1183 ms

> D[fib|n|n <= 1? n ? (n-1){fib} + (n-2){fib}] 38{fib}
39088169
Elapsed: 1457.58 ms

>
```

### JIT Compilation (Optional)

You can enable the LLVM-based JIT compiler as follows.

1. Install [LLVM](https://llvm.org/)
    * apt
        ```bash
        sudo apt update
        sudo apt install llvm-dev -y
        ```
    * dnf
        ```bash
        sudo dnf install llvm-devel -y
        ```
    * Windows
        * You need to build LLVM from source. Please follow [the official instructions](https://llvm.org/docs/GettingStartedVS.html).
        * Make sure that `llvm-config.exe` is added to the PATH.
1. Rebuild with this option
    * Unix-like systems
        ```bash
        cmake ../calc4 -DENABLE_JIT=ON
        cmake --build .
        ./calc4
        ```
    * Windows
        ```powershell
        cmake ..\calc4 -DENABLE_JIT=ON
        cmake --build . --config Release
        .\Release\calc4.exe
        ```

## Sample Codes

### Hello World

* Calc4 Sample Code:
    ```
    72P101P108P108P111P32P119P111P114P108P100P33P10P
    ```
* Equivalent Code in C:
    ```c
    putchar('H');
    putchar('e');
    putchar('l');
    putchar('l');
    putchar('o');
    putchar(' ');
    putchar('w');
    putchar('o');
    putchar('r');
    putchar('l');
    putchar('d');
    putchar('!');
    putchar('\n');
    return 0;
    ```
* Result:
    ```
    > 72P101P108P108P111P32P119P111P114P108P100P33P10P
    Hello world!
    0
    Elapsed: 0.1312 ms
    ```
* The `P` operator prints its operand as a character to the console. The value of `P` itself is zero.

### Addition

* Calc4 Sample Code:
    ```
    12 + 23
    ```
* Equivalent Code in C:
    ```c
    return 12 + 23;
    ```
* Result:
    ```
    > 12 + 23
    35
    Elapsed: 0.0216 ms
    ```

### Addition and Multiplication

* Calc4 Sample Code:
    ```
    1 + 2 * 3
    ```
* Equivalent Code in C:
    ```c
    return (1 + 2) * 3;
    ```
    * Not `1 + (2 * 3)`.
* Result:
    ```
    > 1 + 2 * 3
    9
    Elapsed: 0.0193 ms
    ```
* This code does not evaluate to 7. The reason is explained [later](#operator-precedence).

### Defining Custom Operators

* Calc4 Sample Code:
    ```
    D[myadd|x, y|x + y] 12{myadd}23
    ```
* Equivalent Code in C:
    ```c
    int myadd(int x, int y) {
        return x + y;
    }

    return myadd(12, 23);
    ```
* Result:
    ```
    > D[myadd|x, y|x + y] 12{myadd}23
    35
    Elapsed: 0.0765 ms
    ```

### Conditional Operators

* Calc4 Sample Code:
    ```
    1 == 2 ? 10 ? 20
    ```
* Equivalent Code in C:
    ```c
    return 1 == 2 ? 10 : 20;
    ```
* Result:
    ```
    > 1 == 2 ? 10 ? 20
    20
    Elapsed: 0.0727 ms
    ```

### Operators with Many Operands

* Calc4 Sample Code:
    ```
    D[sum|a, b, c, d, e|a + b + c + d + e] 1{sum}2{sum}3{sum}4{sum}5
    ```
* Equivalent Code in C:
    ```c
    int sum(int a, int b, int c, int d, int e) {
        return a + b + c + d + e;
    }

    return sum(1, 2, 3, 4, 5);
    ```
* Result:
    ```
    > D[sum|a, b, c, d, e|a + b + c + d + e] 1{sum}2{sum}3{sum}4{sum}5
    15
    Elapsed: 0.1059 ms
    ```
* Calc4 supports operators with many operands.

### Fibonacci Sequence (naïve version)

* Calc4 Sample Code:
    ```
    D[fib|n|n <= 1? n ? (n-1){fib} + (n-2){fib}] 38{fib}
    ```
* Equivalent Code in C:
    ```c
    int fib(int n) {
        return n <= 1 ? n : fib(n - 1) + fib(n - 2);
    }

    return fib(38);
    ```
* Result (with JIT compilation):
    ```
    > D[fib|n|n <= 1? n ? (n-1){fib} + (n-2){fib}] 38{fib}
    39088169
    Elapsed: 158.858 ms
    ```
* `fib` is slow because it has exponential time complexity.

### Fibonacci Sequence (tail call version)

* Calc4 Sample Code:
    ```
    D[fib2|x, a, b|x ? ((x-1) ? ((x-1) {fib2} (a+b) {fib2}a) ? a) ? b] 38{fib2}1{fib2}0
    ```
* Equivalent Code in C:
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
* Result (with JIT compilation):
    ```
    > D[fib2|x, a, b|x ? ((x-1) ? ((x-1) {fib2} (a+b) {fib2}a) ? a) ? b] 38{fib2}1{fib2}0
    39088169
    Elapsed: 7.289 ms
    ```
* `fib2` is much faster than `fib`.

### Variables

* Calc4 Sample Code:
    ```
    (123S)
    (L)
    ```
* Equivalent Code in C:
    ```c
    int __default_var;

    /* 123S */
    __default_var = 123;

    /* L */
    return __default_var;
    ```
* Result:
    ```
    > 123S
    123
    Elapsed: 0.068 ms

    > L
    123
    Elapsed: 0.032 ms
    ```
* The operator `S` stores its operand in a variable, and `L` loads it. The previous example stores and then loads the default variable. To specify a variable name, write `S[abc]` and `L[abc]`.
* All variables are global, so their values are shared among operator calls.

### Memory Accesses

* Calc4 Sample Code:
    ```
    (123->10)
    (10@)
    ```
* Equivalent Code in C:
    ```c
    int __memory[VERY_LARGE_SIZE];

    /* 123->10 */
    __memory[10] = 123;

    /* 10@ */
    return __memory[10];
    ```
* Result:
    ```
    > 123->10
    123
    Elapsed: 0.0493 ms

    > 10@
    123
    Elapsed: 0.0389 ms
    ```
* Calc4 has a large global memory accessible from anywhere. The `->` and `@` operators access memory. The `->` operator stores the value of its left operand at the memory location given by its right operand.
* Negative indices are also allowed.
* The parentheses in the code above are required. Without them, the code is parsed as `123->1010@`. This confusing behavior is due to the handling of line breaks and may be revisited in the future.

### Input Operators

* Calc4 Sample Code:
    ```
    I
    ```
* Equivalent Code in C:
    ```c
    return getchar();
    ```
* Standard Input:
    ```
    A
    ```
* Result:
    ```
    > I
    A
    65
    Elapsed: 633.161 ms
    ```
* The `I` operator reads a character from standard input and returns it. In the example above, the user presses "A" and the program prints 65, the ASCII code for "A".
* [A sample calculator program](./sample/calculator.txt) uses this feature. The program reads an expression from standard input and prints the result.
    * Usage:
        ```bash
        $ echo "1 + 2 * 3" | calc4 calculator.txt
        7
        ```

### Tarai Function

[Tarai function](https://en.wikipedia.org/wiki/Tak_(function)) is often used for benchmarking programming languages.

* Calc4 Sample Code:
    ```
    D[tarai|x, y, z|x <= y ? y ? (((x - 1){tarai}y{tarai}z){tarai}((y - 1){tarai}z{tarai}x){tarai}((z - 1){tarai}x{tarai}y))] 18{tarai}15{tarai}5
    ```
* Equivalent Code in C:
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
* Result (with JIT compilation):
    ```
    > D[tarai|x, y, z|x <= y ? y ? (((x - 1){tarai}y{tarai}z){tarai}((y - 1){tarai}z{tarai}x){tarai}((z - 1){tarai}x{tarai}y))] 18{tarai}15{tarai}5
    18
    Elapsed: 280.528 ms
    ```
* **NOTE:** On my machine, the above C program took 225 ms to run (compiled by clang with the `-Ofast` option). Calc4's performance appears close to native C.

## Language Specification

### Operator Precedence

The rules of operator precedence in Calc4 are as follows:
* Operators with fewer operands have higher precedence.
* Operators with the same number of operands have equal precedence. They are left-associative.

This is why `1 + 2 * 3` evaluates to 9 rather than 7.

#### Conditional Operators

When using conditional operators in Calc4, you should be aware of their associativity. All operators are left-associative, and conditional operators are no exception. The following two pieces of code produce different results.

* Calc4
    * Code:
        ```
        1 == 1 ? 2 ? 3 == 4 ? 5 ? 6
        ```
    * Result:
        ```
        > 1 == 1 ? 2 ? 3 == 4 ? 5 ? 6
        5
        ```
* C
    * Code:
        ```c
        #include <stdio.h>

        int main()
        {
            printf("%d\n", 1 == 1 ? 2 : 3 == 4 ? 5 : 6);
            return 0;
        }
        ```
    * Result:
        ```
        $ ./a.out
        2
        ```

Since all operators in Calc4 are left-associative, the code can be rewritten as:
```
(1 == 1 ? 2 ? 3 == 4) ? 5 ? 6
```

In contrast, common programming languages like C have right-associative conditional operators. Therefore, the equivalent C expression is:
```c
1 == 1 ? 2 : (3 == 4 ? 5 : 6)
```

Most programmers expect C behavior. To reproduce that behavior in Calc4, you should explicitly add parentheses:

```
> 1 == 1 ? 2 ? (3 == 4 ? 5 ? 6)
2
```

There are no plans to change this because it naturally extends the calculator model and keeps the language simple.

### Supplementary Texts

You can specify a variable name for the `S` and `L` operators, like `S[abc]` and `L[abc]`. How does Calc4's grammar treat this syntax? You may have a similar question about the `D` operator.

Every operator in Calc4 can have supplementary text immediately after the operator. `[abc]` is one example. These texts are used at compile time and are NOT operands.

The following is valid Calc4 code. `[xyz]`, `[Hello]` and `[qwerty]` are supplementary texts for the `1`, `+` and `2` operators respectively. These operators simply ignore their supplementary texts. The output is of course "3".

```
1[xyz]+[Hello]2[qwerty]
```

## Tail Recursion Optimization

Calc4 lacks loops, so you rely on recursive operators. If the recursion becomes very deep, the stack may overflow. To address this, the Calc4 runtime eliminates tail recursion when possible.

You can easily observe this optimization using an infinitely recursive operator.

```
D[x||{x}] {x}
```

The operator `x` clearly never stops. If you execute this without optimization, the program will crash.

* Without optimization
    ```
    $ ./calc4 -O0
    Calc4 REPL
        Integer size: 64
        Executor: StackMachine
        Optimize: off

    > D[x||{x}] {x}
    Error: Stack overflow
    ```
    * When using the JIT compiler, a segmentation fault will occur.

With optimization enabled, control never returns and the stack does not overflow. This means the recursion was converted into a loop.

* With optimization
    ```
    $ ./calc4
    Calc4 REPL
        Integer size: 64
        Executor: StackMachine
        Optimize: on

    > D[x||{x}] {x}

    ```

It is hard to demonstrate this in Markdown, so please try it on your machine. When the `--dump` option is specified, the Calc4 REPL displays internal representations of the given code. This information is useful for understanding optimizations.

## Conclusion

As this README shows, Calc4 is far from a practical programming language. It is a kind of [esoteric programming language (esolang)](https://en.wikipedia.org/wiki/Esoteric_programming_language). Calc4 was developed out of the belief that designing an original programming language is a lot of fun.

## Copyright

Copyright (C) 2018-2025 Yuya Watari

## License

MIT License
