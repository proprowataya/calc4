cmake_minimum_required(VERSION 3.24)

# https://github.com/google/googletest/blob/main/googletest/README.md
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------------
# calc4-test
# ---------------------------------------------------------------------
add_executable(calc4-test
    ErrorTest.cpp
    ExecutionTest.cpp
    ExecutionTestCases.cpp
    StackMachineTest.cpp
    TestMain.cpp
    ExecutionTestCases.h
    TestCommon.h
)
target_include_directories(calc4-test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(calc4-test gtest_main calc4-core)
add_test(NAME calc4-test COMMAND calc4-test)

# GTest tries to include <cxxabi.h> whenever it detects libc++ headers, but our
# ENABLE_JIT builds add LLVM's include directory (for example,
# -I/usr/lib/llvm-20/include) ahead of the standard library paths. Such LLVM
# trees ship libc++'s cxxabi.h even though this toolchain links against
# even though the actual toolchain links against libstdc++, so GTest mixes two
# incompatible declarations of __cxa_init_primary_exception and the build fails.
# By forcing GTEST_HAS_CXXABI_H_=0 we keep GTest on the code path that does not
# touch cxxabi internals, guaranteeing consistent ABI declarations regardless
# of the header search order while still letting us link against libstdc++.
target_compile_definitions(calc4-test PRIVATE GTEST_HAS_CXXABI_H_=0)

# ---------------------------------------------------------------------
# calc4-codegen-test (integration tests for WASM / C++ code generation)
# ---------------------------------------------------------------------

# Optional dependency: Node.js (used for WASM execution)
find_program(NODE_EXECUTABLE node)
if (NOT NODE_EXECUTABLE)
    set(NODE_EXECUTABLE "")
endif()

# Optional dependency: WABT's wat2wasm (used for WAT -> WASM compilation)
find_program(WAT2WASM_EXECUTABLE wat2wasm)
if (NOT WAT2WASM_EXECUTABLE)
    set(WAT2WASM_EXECUTABLE "")
endif()

# Optional base directory for codegen test temporary files
set(CODEGEN_TEST_TEMP_DIR "" CACHE PATH "Base directory for codegen test temporary files")

configure_file(CodegenTestConfig.h.in CodegenTestConfig.h @ONLY)

add_executable(calc4-codegen-test
    CodegenExecutionTest.cpp
    CodegenTestMain.cpp
    ExecutionTestCases.cpp
    CodegenTestCommon.h
    ExecutionTestCases.h
    TestCommon.h
)

target_include_directories(calc4-codegen-test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(calc4-codegen-test gtest calc4-core)
add_test(NAME calc4-codegen-test COMMAND calc4-codegen-test)
set_tests_properties(calc4-codegen-test PROPERTIES SKIP_RETURN_CODE 77)
target_compile_definitions(calc4-codegen-test PRIVATE GTEST_HAS_CXXABI_H_=0)
