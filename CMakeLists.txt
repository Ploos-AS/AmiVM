cmake_minimum_required(VERSION 3.16)
project(AmiVM VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(amivm_core STATIC
    src/vm.c
    src/cpu.c
    src/fpu.c
    src/exec.c
    src/ir.c
    src/ea.c
    src/jit.c
    src/jit_runtime.c
)

target_include_directories(amivm_core PUBLIC src)
target_compile_options(amivm_core PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>
)

add_executable(amivm src/main.c)
target_link_libraries(amivm PRIVATE amivm_core)
target_compile_options(amivm PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_vm_tests tests/test_vm.c)
target_link_libraries(amivm_vm_tests PRIVATE amivm_core)
target_compile_options(amivm_vm_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_cpu_tests tests/test_cpu.c)
target_link_libraries(amivm_cpu_tests PRIVATE amivm_core)
target_compile_options(amivm_cpu_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_fpu_tests tests/test_fpu.c)
target_link_libraries(amivm_fpu_tests PRIVATE amivm_core)
target_compile_options(amivm_fpu_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_fpu_decode_tests tests/test_fpu_decode.c)
target_link_libraries(amivm_fpu_decode_tests PRIVATE amivm_core)
target_compile_options(amivm_fpu_decode_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_exec_tests tests/test_exec.c)
target_link_libraries(amivm_exec_tests PRIVATE amivm_core)
target_compile_options(amivm_exec_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_ir_tests tests/test_ir.c)
target_link_libraries(amivm_ir_tests PRIVATE amivm_core)
target_compile_options(amivm_ir_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_ir_m219_tests tests/test_ir_m219.c)
target_link_libraries(amivm_ir_m219_tests PRIVATE amivm_core)
target_compile_options(amivm_ir_m219_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_ir_m220_tests tests/test_ir_m220.c)
target_link_libraries(amivm_ir_m220_tests PRIVATE amivm_core)
target_compile_options(amivm_ir_m220_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_ea_m221_tests tests/test_ea_m221.c)
target_link_libraries(amivm_ea_m221_tests PRIVATE amivm_core)
target_compile_options(amivm_ea_m221_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_ir_m222_tests tests/test_ir_m222.c)
target_link_libraries(amivm_ir_m222_tests PRIVATE amivm_core)
target_compile_options(amivm_ir_m222_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_exec_m223_tests tests/test_exec_m223.c)
target_link_libraries(amivm_exec_m223_tests PRIVATE amivm_core)
target_compile_options(amivm_exec_m223_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_jit_m224_tests tests/test_jit_m224.c)
target_link_libraries(amivm_jit_m224_tests PRIVATE amivm_core)
target_compile_options(amivm_jit_m224_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_jit_m225_tests tests/test_jit_m225.c)
target_link_libraries(amivm_jit_m225_tests PRIVATE amivm_core)
target_compile_options(amivm_jit_m225_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_exec_m226_tests tests/test_exec_m226.c)
target_link_libraries(amivm_exec_m226_tests PRIVATE amivm_core)
target_compile_options(amivm_exec_m226_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_jit_m227_tests tests/test_jit_m227.c)
target_link_libraries(amivm_jit_m227_tests PRIVATE amivm_core)
target_compile_options(amivm_jit_m227_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_exec_m228_tests tests/test_exec_m228.c)
target_link_libraries(amivm_exec_m228_tests PRIVATE amivm_core)
target_compile_options(amivm_exec_m228_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_jit_m229_tests tests/test_jit_m229.c)
target_link_libraries(amivm_jit_m229_tests PRIVATE amivm_core)
target_compile_options(amivm_jit_m229_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_jit_m230_tests tests/test_jit_m230.c)
target_link_libraries(amivm_jit_m230_tests PRIVATE amivm_core)
target_compile_options(amivm_jit_m230_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_exec_m231_tests tests/test_exec_m231.c)
target_link_libraries(amivm_exec_m231_tests PRIVATE amivm_core)
target_compile_options(amivm_exec_m231_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

add_executable(amivm_exec_bench bench/bench_exec.c)
target_link_libraries(amivm_exec_bench PRIVATE amivm_core)
target_compile_options(amivm_exec_bench PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

enable_testing()
add_test(NAME amivm_version COMMAND amivm --version)
add_test(NAME amivm_machine_dump COMMAND amivm --dump-machine)
add_test(NAME amivm_selftest COMMAND amivm --selftest)
add_test(NAME amivm_vm_core COMMAND amivm_vm_tests)
add_test(NAME amivm_cpu_core COMMAND amivm_cpu_tests)
add_test(NAME amivm_fpu_core COMMAND amivm_fpu_tests)
add_test(NAME amivm_fpu_decode COMMAND amivm_fpu_decode_tests)
add_test(NAME amivm_exec_core COMMAND amivm_exec_tests)
add_test(NAME amivm_ir_core COMMAND amivm_ir_tests)
add_test(NAME amivm_ir_m219 COMMAND amivm_ir_m219_tests)
add_test(NAME amivm_ir_m220 COMMAND amivm_ir_m220_tests)
add_test(NAME amivm_ea_m221 COMMAND amivm_ea_m221_tests)
add_test(NAME amivm_ir_m222 COMMAND amivm_ir_m222_tests)
add_test(NAME amivm_exec_m223 COMMAND amivm_exec_m223_tests)
add_test(NAME amivm_jit_m224 COMMAND amivm_jit_m224_tests)
add_test(NAME amivm_jit_m225 COMMAND amivm_jit_m225_tests)
add_test(NAME amivm_exec_m226 COMMAND amivm_exec_m226_tests)
add_test(NAME amivm_jit_m227 COMMAND amivm_jit_m227_tests)
add_test(NAME amivm_exec_m228 COMMAND amivm_exec_m228_tests)
add_test(NAME amivm_jit_m229 COMMAND amivm_jit_m229_tests)
add_test(NAME amivm_jit_m230 COMMAND amivm_jit_m230_tests)
add_test(NAME amivm_exec_m231 COMMAND amivm_exec_m231_tests)
add_test(NAME amivm_exec_benchmark_smoke COMMAND amivm_exec_bench)
