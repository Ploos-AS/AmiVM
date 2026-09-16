#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_ir_block block;
    struct amivm_jit_code code;
    struct amivm_jit_context context;
    const uint16_t bsr[] = {0x6110u};
    const uint16_t moveq[] = {0x7007u};
    uint32_t sp = AMIVM_RAM_BASE + 0x2000u;

    if (amivm_jit_host_arch() != AMIVM_JIT_ARCH_X86_64) {
        puts("AmiVM M2.43 direct context JIT tests: SKIP (non-x86-64 host)");
        return 0;
    }
#if !defined(__x86_64__) || !defined(__linux__)
    puts("AmiVM M2.43 direct context JIT tests: SKIP (execution unavailable)");
    return 0;
#endif

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = 0x2000u;
    cpu.a[7] = sp;
    cpu.isp = sp;
    amivm_jit_context_init(&context, &cpu, &vm);

    amivm_ir_block_init(&block, 0x4000u);
    CHECK(amivm_ir_decode_words(&block, bsr, 1u) == 0);
    CHECK(amivm_jit_compile(&block, &code) == AMIVM_JIT_OK);
    CHECK(code.requires_context == 1);
    CHECK(amivm_jit_execute(&code, &cpu) == AMIVM_JIT_INVALID);
    CHECK(cpu.a[7] == sp);
    CHECK(amivm_jit_execute_context(&code, &cpu, NULL) == AMIVM_JIT_INVALID);
    CHECK(cpu.a[7] == sp);
    CHECK(amivm_jit_execute_context(&code, &cpu, &context) == 1);
    CHECK(cpu.pc == 0x4012u);
    CHECK(cpu.a[7] == sp - 4u && cpu.isp == sp - 4u);

    memset(&cpu, 0, sizeof(cpu));
    amivm_ir_block_init(&block, 0x5000u);
    CHECK(amivm_ir_decode_words(&block, moveq, 1u) == 0);
    CHECK(amivm_jit_compile(&block, &code) == AMIVM_JIT_OK);
    CHECK(code.requires_context == 0);
    CHECK(amivm_jit_execute(&code, &cpu) == 1);
    CHECK(cpu.d[0] == 7u && cpu.pc == 0x5002u);

    memset(&cpu, 0, sizeof(cpu));
    CHECK(amivm_jit_execute_context(&code, &cpu, NULL) == 1);
    CHECK(cpu.d[0] == 7u && cpu.pc == 0x5002u);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.43 safe direct JIT context execution tests: PASS");
    return 0;
}
