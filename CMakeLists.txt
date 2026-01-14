cmake_minimum_required(VERSION 3.15)
project(asmview C)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(ASMVIEW_SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(ASMVIEW_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include)

add_executable(asm-server
  src/asm_server.c
  src/asm_instance.c
  src/cJSON.c
)

target_compile_options(asm-server PRIVATE -O3)
target_include_directories(asm-server PRIVATE ${ASMVIEW_INCLUDE_DIRS})

add_subdirectory(${CMAKE_SOURCE_DIR}/tools)
