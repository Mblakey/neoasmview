cmake_minimum_required(VERSION 3.15)
project(asmview C)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Add executable from source files
add_executable(asm-filter
  src/asm_filter.c
  src/asm_instance.c
  src/cJSON.c
)


# Add executable from source files
add_executable(asm-server
  src/asm_server.c
  src/asm_instance.c
  src/cJSON.c
)

target_compile_options(asm-filter PRIVATE -O3)
target_include_directories(asm-filter PRIVATE include)

target_compile_options(asm-server PRIVATE -O3)
target_include_directories(asm-server PRIVATE include)
