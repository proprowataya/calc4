cmake_minimum_required(VERSION 3.8)

project(calc4 CXX)
add_executable(calc4 Common.cpp Main.cpp Optimizer.cpp StackMachine.cpp SyntaxAnalysis.cpp Test.cpp Common.h Evaluator.h Exceptions.h ExecutionState.h Operators.h Optimizer.h StackMachine.h SyntaxAnalysis.h Test.h)
target_compile_features(calc4 PUBLIC cxx_std_17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

if (NOT MSVC)
    # Assume we are using GCC or clang
    target_compile_definitions(calc4 PUBLIC ENABLE_INT128)
endif()

if (NOT DEFINED LLVM_CONFIG)
    set(LLVM_CONFIG "llvm-config" CACHE STRING "Path to llvm-config")
endif()

option(ENABLE_JIT "Enable the JIT compilation with LLVM" OFF)
if (ENABLE_JIT)
    target_sources(calc4 PRIVATE Jit.cpp Jit.h)
    target_compile_definitions(calc4 PUBLIC ENABLE_JIT)

    execute_process(COMMAND ${LLVM_CONFIG} --cxxflags OUTPUT_VARIABLE LLVM_CXXFLAGS)
    # Remove unnecessary language version specification
    string(REGEX REPLACE "[-/]std[=:][^ ]*" "" LLVM_CXXFLAGS ${LLVM_CXXFLAGS})
    # Remove new-lines
    string(REGEX REPLACE "[\r\n]+" " " LLVM_CXXFLAGS ${LLVM_CXXFLAGS})
    # Split arguments
    string(REGEX REPLACE "[ ]+" ";" LLVM_CXXFLAGS ${LLVM_CXXFLAGS})
    target_compile_options(calc4 PUBLIC ${LLVM_CXXFLAGS})

    execute_process(COMMAND ${LLVM_CONFIG} --ldflags --libs --system-libs OUTPUT_VARIABLE LLVM_LDFLAGS)
    string(REGEX REPLACE "[\r\n]+" " " LLVM_LDFLAGS ${LLVM_LDFLAGS})
    string(REGEX REPLACE "[ ]+" ";" LLVM_LDFLAGS ${LLVM_LDFLAGS})
    target_link_libraries(calc4 PUBLIC ${LLVM_LDFLAGS})

    if (MSVC)
        target_compile_options(calc4 PUBLIC "/EHs-c" "/GR" "/sdl-" "/MP")
    else()
        target_compile_options(calc4 PUBLIC "-fexceptions" "-Wno-unknown-warning-option" "-Wno-unused-command-line-argument")
    endif()
endif()

option(ENABLE_GMP "Enable infinite-precision integers" OFF)
if (ENABLE_GMP)
    if (MSVC)
        message(FATAL_ERROR "Infinite-precision integers are not supported on the Windows platform.")
    endif()
    target_compile_definitions(calc4 PUBLIC ENABLE_GMP)
    target_link_libraries(calc4 PUBLIC "-lgmpxx" "-lgmp")
endif()
