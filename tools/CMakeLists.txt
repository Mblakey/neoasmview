cmake_minimum_required(VERSION 3.15)
project(asmview_tools C)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_executable(bear-cargo bear_cargo.c ${ASMVIEW_SRC_DIR}/cJSON.c)

target_compile_options(bear-cargo PRIVATE -O3)
target_include_directories(bear-cargo PRIVATE ${ASMVIEW_INCLUDE_DIRS})
