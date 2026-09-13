cmake_minimum_required(VERSION 3.16)
project(AmiVM VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(amivm_core STATIC
    src/vm.c
    src/cpu.c
)

target_include_directories(amivm_core PUBLIC src)
target_compile_options(amivm_core PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>
)

add_executable(amivm
    src/main.c
)
target_link_libraries(amivm PRIVATE amivm_core)
target_compile_options(amivm PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>
)

add_executable(amivm_vm_tests
    tests/test_vm.c
)
target_link_libraries(amivm_vm_tests PRIVATE amivm_core)
target_compile_options(amivm_vm_tests PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>
)

add_executable(amivm_cpu_tests
    tests/test_cpu.c
)
target_link_libraries(amivm_cpu_tests PRIVATE amivm_core)
target_compile_options(amivm_cpu_tests PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>
)

enable_testing()
add_test(NAME amivm_version COMMAND amivm --version)
add_test(NAME amivm_machine_dump COMMAND amivm --dump-machine)
add_test(NAME amivm_selftest COMMAND amivm --selftest)
add_test(NAME amivm_vm_core COMMAND amivm_vm_tests)
add_test(NAME amivm_cpu_core COMMAND amivm_cpu_tests)
