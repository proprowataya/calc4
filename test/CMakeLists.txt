cmake_minimum_required(VERSION 3.8)

# https://github.com/google/googletest/blob/main/googletest/README.md
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Now simply link against gtest or gtest_main as needed. Eg
add_executable(calc4-test TestMain.cpp ExecutionTest.cpp TestCommon.h)
target_include_directories(calc4-test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(calc4-test gtest_main calc4-core)
add_test(NAME calc4-test COMMAND calc4-test)
