cmake_minimum_required(VERSION 3.15)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Add executable from source files
add_executable(foobar
  foo.c
  bar.c
  foobar.c
)

target_compile_options(foobar PRIVATE -O3)
