cmake_minimum_required(VERSION 3.24)
project(HUJI_OS_EX1)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(HUJI_OS_EX1
        demo_itimer.c
        demo_jmp.c
        demo_singInt_handler.c
        uthreads.cpp
        uthreads.h ThreadsManager.h Thread.h Thread.cpp ThreadsManager.cpp)
