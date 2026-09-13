cmake_minimum_required(VERSION 3.16)
project(AmiVM VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(amivm
    src/main.c
)

target_compile_options(amivm PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>
)

enable_testing()
add_test(NAME amivm_version COMMAND amivm --version)
add_test(NAME amivm_machine_dump COMMAND amivm --dump-machine)
