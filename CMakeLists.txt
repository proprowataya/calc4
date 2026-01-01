cmake_minimum_required(VERSION 3.24)

project(calc4 CXX)
add_subdirectory(src)

option(BUILD_TEST "Build test program" OFF)
if (BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif()
