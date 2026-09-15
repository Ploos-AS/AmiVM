#include "jit_helpers.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static int read32(struct amivm_vm *vm, uint32_t addr, uint32_t *value)
{
    uint8_t b[4];
    unsigned i;
    for (i = 0u; i < 4u; ++i)
        if (!amivm_read8(vm, addr + i, &b[i])) return 0;
    *value = ((uint32_t)b[0] << 24u) | ((uint32_t)b[1] << 16u) |
             ((uint32_t)b[2] << 8u) | (uint32_t)b[3];
    return 1;
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_jit_context context;
    uint32_t value;
    uint32_t original_sp;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = 0x2000u;
    cpu.a[7] = AMIVM_RAM_BASE + 0x1000u;
    cpu.isp = cpu.a[7];
    amivm_jit_context_init(&context, &cpu, &vm);

    CHECK(amivm_jit_helper_stack_push_long(&context, 0x12345678u) == 0);
    CHECK(cpu.a[7] == AMIVM_RAM_BASE + 0x0ffcu);
    CHECK(cpu.isp == cpu.a[7]);
    CHECK(read32(&vm, cpu.a[7], &value));
    CHECK(value == 0x12345678u);

    value = 0u;
    CHECK(amivm_jit_helper_stack_pop_long(&context, &value) == 0);
    CHECK(value == 0x12345678u);
    CHECK(cpu.a[7] == AMIVM_RAM_BASE + 0x1000u);
    CHECK(cpu.isp == cpu.a[7]);

    cpu.sr = 0u;
    cpu.a[7] = AMIVM_RAM_BASE + 0x1200u;
    cpu.usp = cpu.a[7];
    CHECK(amivm_jit_helper_stack_push_long(&context, 0xaabbccddu) == 0);
    CHECK(cpu.usp == cpu.a[7]);
    CHECK(amivm_jit_helper_stack_pop_long(&context, &value) == 0);
    CHECK(value == 0xaabbccddu);
    CHECK(cpu.usp == cpu.a[7]);

    cpu.sr = 0x3000u;
    cpu.a[7] = AMIVM_RAM_BASE + 0x1400u;
    cpu.msp = cpu.a[7];
    CHECK(amivm_jit_helper_stack_push_long(&context, 0xdeadbeefu) == 0);
    CHECK(cpu.msp == cpu.a[7]);
    CHECK(amivm_jit_helper_stack_pop_long(&context, &value) == 0);
    CHECK(value == 0xdeadbeefu);
    CHECK(cpu.msp == cpu.a[7]);

    cpu.sr = 0x2000u;
    cpu.a[7] = AMIVM_RAM_BASE + config.ram_size + 4u;
    cpu.isp = cpu.a[7];
    original_sp = cpu.a[7];
    CHECK(amivm_jit_helper_stack_push_long(&context, 0x01020304u) == -4);
    CHECK(cpu.a[7] == original_sp);
    CHECK(cpu.isp == original_sp);
    CHECK(amivm_jit_helper_stack_pop_long(&context, &value) == -4);
    CHECK(cpu.a[7] == original_sp);
    CHECK(cpu.isp == original_sp);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.38 VM-aware JIT helper ABI tests: PASS");
    return 0;
}
