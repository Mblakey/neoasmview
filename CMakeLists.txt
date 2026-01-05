cmake_minimum_required(VERSION 3.15)
project(asmview C)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)



# Add executable from source files
add_executable(asm-filter
  src/asm_filter.c
  src/cJSON.c
)

target_compile_options(asm-filter PRIVATE -O3)
target_include_directories(asm-filter PRIVATE include)

# Build tests in a separate build dir with its own compile_commands.json
add_custom_target(simple_test ALL
    COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR}/tests/simple_exec -B ${CMAKE_BINARY_DIR}/simple_test
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/simple_test
    COMMENT "Building tests in separate build dir"
)
