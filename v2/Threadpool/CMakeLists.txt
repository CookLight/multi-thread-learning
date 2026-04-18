cmake_minimum_required(VERSION 3.26)
project(Threadpool)

set(CMAKE_CXX_STANDARD 17)

add_executable(Threadpool main.cpp
        ThreadPool.h)

add_executable(AdvancedThreadpool advanced_main.cpp
        AdvancedThreadPool.h)
