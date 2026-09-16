#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s\n", #x); return 1; } } while (0)

static uint32_t read_be32(struct amivm_vm *vm, uint32_t address)
{
    uint8_t b0, b1, b2, b3;
    if (!amivm_read8(vm, address, &b0) || !amivm_read8(vm, address + 1u, &b1) ||
        !amivm_read8(vm, address + 2u, &b2) || !amivm_read8(vm, address + 3u, &b3)) return 0xffffffffu;
    return ((uint32_t)b0 << 24u) | ((uint32_t)b1 << 16u) | ((uint32_t)b2 << 8u) | b3;
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
    const uint16_t jsr[] = {0x4e92u}; /* JSR (A2) */
    const uint16_t jmp[] = {0x4ed3u}; /* JMP (A3) */
    uint32_t sp = AMIVM_RAM_BASE + 0x2000u;

    if (amivm_jit_host_arch() != AMIVM_JIT_ARCH_X86_64) {
        puts("AmiVM M2.46 native dynamic JSR/JMP tests: SKIP");
        return 0;
    }
#if !defined(__x86_64__) || !defined(__linux__)
    puts("AmiVM M2.46 native dynamic JSR/JMP tests: SKIP");
    return 0;
#endif
    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = 0x2000u;
    cpu.a[7] = sp;
    cpu.isp = sp;
    cpu.a[2] = 0x12345678u;

    amivm_ir_block_init(&block, 0x4000u);
    CHECK(amivm_ir_decode_words(&block, jsr, 1u) == 0);
    CHECK(run_native(&block, &cpu, &vm) == 0);
    CHECK(cpu.pc == 0x12345678u);
    CHECK(cpu.a[7] == sp - 4u && cpu.isp == sp - 4u);
    CHECK(read_be32(&vm, sp - 4u) == 0x4002u);

    cpu.a[3] = 0x89abcdefu;
    amivm_ir_block_init(&block, 0x5000u);
    CHECK(amivm_ir_decode_words(&block, jmp, 1u) == 0);
    CHECK(run_native(&block, &cpu, &vm) == 0);
    CHECK(cpu.pc == 0x89abcdefu);
    CHECK(cpu.a[7] == sp - 4u);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.46 native dynamic JSR/JMP lowering tests: PASS");
    return 0;
}
