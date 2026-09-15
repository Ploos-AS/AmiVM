#include "jit.h"
#include "jit_helpers.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_jit_context context;
    struct amivm_ir_block block;
    struct amivm_jit_code code;
    struct amivm_jit_runtime runtime;
    const uint16_t words[] = {0x7007u, 0x4e71u}; /* MOVEQ #7,D0; NOP */
    int rc;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    memset(&cpu, 0, sizeof(cpu));
    amivm_jit_context_init(&context, &cpu, &vm);
    CHECK(context.cpu == &cpu);
    CHECK(context.vm == &vm);

    amivm_ir_block_init(&block, 0x1000u);
    CHECK(amivm_ir_decode_words(&block, words, 2u) == 0);
    rc = amivm_jit_compile(&block, &code);

    if (amivm_jit_host_arch() == AMIVM_JIT_ARCH_X86_64) {
        CHECK(rc == AMIVM_JIT_OK);
#if defined(__x86_64__) && defined(__linux__)
        amivm_jit_runtime_init(&runtime);
        CHECK(amivm_jit_runtime_prepare(&runtime, &code) == AMIVM_JIT_OK);
        cpu.pc = 0xdeadbeefu;
        cpu.d[0] = 0u;
        CHECK(amivm_jit_runtime_execute_context(&runtime, &cpu, &context) == 1);
        CHECK(cpu.d[0] == 7u);
        CHECK(cpu.pc == 0x1004u);

        /* Legacy CPU-only entry remains valid while generated blocks do not
           require helper context. */
        cpu.pc = 0u;
        cpu.d[0] = 0u;
        CHECK(amivm_jit_runtime_execute(&runtime, &cpu) == 1);
        CHECK(cpu.d[0] == 7u);
        CHECK(cpu.pc == 0x1004u);
        amivm_jit_runtime_release(&runtime);
#endif
    } else {
        CHECK(rc == AMIVM_JIT_UNSUPPORTED);
    }

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.40 VM-aware JIT runtime ABI tests: PASS");
    return 0;
}
