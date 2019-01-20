cmake_minimum_required(VERSION 2.8.9)
project (lc3)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SOURCE_FILES
    lc3.cpp)

add_executable(lc3 ${SOURCE_FILES})