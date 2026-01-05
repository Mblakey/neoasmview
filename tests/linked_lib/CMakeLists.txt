cmake_minimum_required(VERSION 3.15)
project(test_linked_lib C)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_compile_options(-O3)

find_package(CURL REQUIRED) # a sensible default

add_library(test_link_static STATIC lib.c)
target_link_libraries(test_link_static CURL::libcurl)

