cmake_minimum_required(VERSION 3.24)

add_library(calc4-core STATIC Common.cpp CppEmitter.cpp Optimizer.cpp StackMachine.cpp SyntaxAnalysis.cpp Common.h CppEmitter.h Evaluator.h Exceptions.h ExecutionState.h Operators.h Optimizer.h ReplCommon.h StackMachine.h SyntaxAnalysis.h)
add_executable(calc4 Main.cpp ReplCommon.h)
set_target_properties(calc4 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_link_libraries(calc4 calc4-core)
target_compile_features(calc4-core PUBLIC cxx_std_17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

if (NOT MSVC)
    # Assume we are using GCC or clang
    target_compile_definitions(calc4-core PUBLIC ENABLE_INT128)
endif()

if (NOT DEFINED LLVM_CONFIG)
    set(LLVM_CONFIG "llvm-config" CACHE STRING "Path to llvm-config")
endif()

option(NO_USE_COMPUTED_GOTO "Disable the computed goto technique on the stack machine" OFF)
if (NO_USE_COMPUTED_GOTO)
    target_compile_definitions(calc4-core PUBLIC NO_USE_COMPUTED_GOTO)
endif()

option(ENABLE_JIT "Enable the JIT compilation with LLVM" OFF)
if (ENABLE_JIT)
    target_sources(calc4-core PRIVATE Jit.cpp Jit.h)
    target_compile_definitions(calc4-core PUBLIC ENABLE_JIT)

    execute_process(COMMAND ${LLVM_CONFIG} --cxxflags OUTPUT_VARIABLE LLVM_CXXFLAGS)
    # Remove unnecessary language version specification
    string(REGEX REPLACE "[-/]std[=:][^ ]*" "" "LLVM_CXXFLAGS" "${LLVM_CXXFLAGS}")
    string(REGEX REPLACE "/EHs-c-" "" "LLVM_CXXFLAGS" "${LLVM_CXXFLAGS}")
    string(REGEX REPLACE "/GR-" "" "LLVM_CXXFLAGS" "${LLVM_CXXFLAGS}")
    if (NOT NO_USE_COMPUTED_GOTO)
        # Computed goto is a GCC extension. We do not want compiler to warn it.
        string(REGEX REPLACE "-pedantic" "" "LLVM_CXXFLAGS" "${LLVM_CXXFLAGS}")
    endif()

    # Remove new-lines
    string(REGEX REPLACE "[\r\n]+" " " "LLVM_CXXFLAGS" "${LLVM_CXXFLAGS}")

    # Split arguments
    string(REGEX REPLACE "[ ]+" ";" "LLVM_CXXFLAGS" "${LLVM_CXXFLAGS}")
    target_compile_options(calc4-core PUBLIC ${LLVM_CXXFLAGS})

    execute_process(COMMAND ${LLVM_CONFIG} --ldflags --libs --system-libs OUTPUT_VARIABLE LLVM_LDFLAGS)
    string(REGEX REPLACE "[\r\n]+" " " "LLVM_LDFLAGS" "${LLVM_LDFLAGS}")
    string(REGEX REPLACE "[ ]+" ";" "LLVM_LDFLAGS" "${LLVM_LDFLAGS}")
    target_link_libraries(calc4-core PUBLIC ${LLVM_LDFLAGS})

    if (MSVC)
        target_compile_options(calc4-core PUBLIC "/sdl-" "/MP")
    else()
        target_compile_options(calc4-core PUBLIC "-fexceptions" "-Wno-unknown-warning-option" "-Wno-unused-command-line-argument")
    endif()
endif()

option(ENABLE_GMP "Enable infinite-precision integers" OFF)
if (ENABLE_GMP)
    if (MSVC)
        message(FATAL_ERROR "Infinite-precision integers are not supported on the Windows platform.")
    endif()
    target_compile_definitions(calc4-core PUBLIC ENABLE_GMP)
    target_link_libraries(calc4-core PUBLIC "-lgmpxx" "-lgmp")
endif()
