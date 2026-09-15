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
    src/ir_branch_ext.c
    src/ea.c
    src/jit.c
    src/jit_runtime.c
    src/jit_helpers.c
)

set_source_files_properties(src/ir.c PROPERTIES
    COMPILE_DEFINITIONS "amivm_ir_decode_words=amivm_ir_decode_words_base;amivm_ir_execute=amivm_ir_execute_base"
)

target_include_directories(amivm_core PUBLIC src)
target_compile_options(amivm_core PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>
)

add_executable(amivm src/main.c)
target_link_libraries(amivm PRIVATE amivm_core)
target_compile_options(amivm PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

set(AMIVM_TESTS
    vm cpu fpu fpu_decode exec ir ir_m219 ir_m220 ea_m221 ir_m222
    exec_m223 jit_m224 jit_m225 exec_m226 jit_m227 exec_m228 jit_m229
    jit_m230 exec_m231 exec_m233 exec_m234 jit_m235 ir_m236 ir_m237
    jit_m238 jit_m239 jit_m240
)

foreach(test_name IN LISTS AMIVM_TESTS)
    add_executable(amivm_${test_name}_tests tests/test_${test_name}.c)
    target_link_libraries(amivm_${test_name}_tests PRIVATE amivm_core)
    target_compile_options(amivm_${test_name}_tests PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)
endforeach()

add_executable(amivm_exec_bench bench/bench_exec.c)
target_link_libraries(amivm_exec_bench PRIVATE amivm_core)
target_compile_options(amivm_exec_bench PRIVATE $<$<C_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>)

enable_testing()
add_test(NAME amivm_version COMMAND amivm --version)
add_test(NAME amivm_machine_dump COMMAND amivm --dump-machine)
add_test(NAME amivm_selftest COMMAND amivm --selftest)
foreach(test_name IN LISTS AMIVM_TESTS)
    add_test(NAME amivm_${test_name} COMMAND amivm_${test_name}_tests)
endforeach()
add_test(NAME amivm_exec_benchmark_smoke COMMAND amivm_exec_bench)
