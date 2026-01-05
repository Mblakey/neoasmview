cmake_minimum_required(VERSION 3.15)
project(test_lib C)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_compile_options(-O3)

add_library(test_static STATIC lib.c api.c)
add_library(test_shared SHARED lib.c api.c)
