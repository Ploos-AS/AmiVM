#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static int read_long(struct amivm_vm *vm, uint32_t address, uint32_t *value)
{
    uint8_t b0, b1, b2, b3;
    if (!amivm_read8(vm, address, &b0) || !amivm_read8(vm, address + 1u, &b1) ||
        !amivm_read8(vm, address + 2u, &b2) || !amivm_read8(vm, address + 3u, &b3)) return -1;
    *value = ((uint32_t)b0 << 24u) | ((uint32_t)b1 << 16u) |
             ((uint32_t)b2 << 8u) | (uint32_t)b3;
    return 0;
}

static int run_native(struct amivm_ir_block *block, struct amivm_cpu_state *cpu,
                      struct amivm_vm *vm)
{
    struct amivm_jit_code code;
    struct amivm_jit_runtime runtime;
    struct amivm_jit_context context;

    CHECK(amivm_jit_compile(block, &code) == AMIVM_JIT_OK);
    CHECK(code.requires_context == 1);
    amivm_jit_runtime_init(&runtime);
    CHECK(amivm_jit_runtime_prepare(&runtime, &code) == AMIVM_JIT_OK);
    CHECK(runtime.requires_context == 1);
    CHECK(amivm_jit_runtime_execute(&runtime, cpu) == AMIVM_JIT_INVALID);
    amivm_jit_context_init(&context, cpu, vm);
    CHECK(amivm_jit_runtime_execute_context(&runtime, cpu, &context) == 1);
    amivm_jit_runtime_release(&runtime);
    return 0;
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_ir_block block;
    const uint16_t bsr_short[] = {0x6110u};
    const uint16_t bsr_word[] = {0x6100u, 0x0010u};
    const uint16_t rts[] = {0x4e75u};
    uint32_t sp = AMIVM_RAM_BASE + 0x2000u;
    uint32_t stacked = 0u;

    if (amivm_jit_host_arch() != AMIVM_JIT_ARCH_X86_64) {
        puts("AmiVM M2.41 native BSR/RTS tests: SKIP (non-x86-64 host)");
        return 0;
    }
#if !defined(__x86_64__) || !defined(__linux__)
    puts("AmiVM M2.41 native BSR/RTS tests: SKIP (execution unavailable)");
    return 0;
#endif

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = 0x2000u;
    cpu.a[7] = sp;
    cpu.isp = sp;

    amivm_ir_block_init(&block, 0x4000u);
    CHECK(amivm_ir_decode_words(&block, bsr_short, 1u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_BSR);
    CHECK(run_native(&block, &cpu, &vm) == 0);
    CHECK(cpu.pc == 0x4012u);
    CHECK(cpu.a[7] == sp - 4u && cpu.isp == sp - 4u);
    CHECK(read_long(&vm, cpu.a[7], &stacked) == 0 && stacked == 0x4002u);

    amivm_ir_block_init(&block, 0x5000u);
    CHECK(amivm_ir_decode_words(&block, rts, 1u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_RTS);
    CHECK(run_native(&block, &cpu, &vm) == 0);
    CHECK(cpu.pc == 0x4002u);
    CHECK(cpu.a[7] == sp && cpu.isp == sp);

    cpu.a[7] = sp;
    cpu.isp = sp;
    amivm_ir_block_init(&block, 0x6000u);
    CHECK(amivm_ir_decode_words(&block, bsr_word, 2u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_BSR);
    CHECK(run_native(&block, &cpu, &vm) == 0);
    CHECK(cpu.pc == 0x6012u);
    CHECK(read_long(&vm, cpu.a[7], &stacked) == 0 && stacked == 0x6004u);

    /* Helper fault propagates through native code without mutating SP/PC. */
    cpu.pc = 0x77770000u;
    cpu.a[7] = AMIVM_RAM_BASE + (uint32_t)config.ram_size + 4u;
    cpu.isp = cpu.a[7];
    amivm_ir_block_init(&block, 0x7000u);
    CHECK(amivm_ir_decode_words(&block, rts, 1u) == 0);
    {
        struct amivm_jit_code code;
        struct amivm_jit_runtime runtime;
        struct amivm_jit_context context;
        uint32_t bad_sp = cpu.a[7];
        CHECK(amivm_jit_compile(&block, &code) == AMIVM_JIT_OK);
        amivm_jit_runtime_init(&runtime);
        CHECK(amivm_jit_runtime_prepare(&runtime, &code) == AMIVM_JIT_OK);
        amivm_jit_context_init(&context, &cpu, &vm);
        CHECK(amivm_jit_runtime_execute_context(&runtime, &cpu, &context) == -4);
        CHECK(cpu.a[7] == bad_sp && cpu.isp == bad_sp);
        CHECK(cpu.pc == 0x77770000u);
        amivm_jit_runtime_release(&runtime);
    }

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.41 native BSR/RTS lowering tests: PASS");
    return 0;
}
