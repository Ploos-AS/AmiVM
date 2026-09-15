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

static int run_bank_case(uint16_t sr)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_jit_context context;
    uint32_t value;
    const uint32_t sp = AMIVM_RAM_BASE + 0x1800u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = sr;
    cpu.a[7] = sp;
    if ((sr & 0x2000u) == 0u) cpu.usp = sp;
    else if ((sr & 0x1000u) != 0u) cpu.msp = sp;
    else cpu.isp = sp;
    cpu.pc = 0x11111111u;
    amivm_jit_context_init(&context, &cpu, &vm);

    CHECK(amivm_jit_helper_bsr(&context, 0x00f00204u, 0x00f01000u) == 0);
    CHECK(cpu.pc == 0x00f01000u);
    CHECK(cpu.a[7] == sp - 4u);
    CHECK(read32(&vm, cpu.a[7], &value));
    CHECK(value == 0x00f00204u);
    if ((sr & 0x2000u) == 0u) CHECK(cpu.usp == cpu.a[7]);
    else if ((sr & 0x1000u) != 0u) CHECK(cpu.msp == cpu.a[7]);
    else CHECK(cpu.isp == cpu.a[7]);

    CHECK(amivm_jit_helper_rts(&context) == 0);
    CHECK(cpu.pc == 0x00f00204u);
    CHECK(cpu.a[7] == sp);
    if ((sr & 0x2000u) == 0u) CHECK(cpu.usp == sp);
    else if ((sr & 0x1000u) != 0u) CHECK(cpu.msp == sp);
    else CHECK(cpu.isp == sp);

    amivm_vm_destroy(&vm);
    return 0;
}

static int fault_case(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_jit_context context;
    uint32_t sp;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = 0x2000u;
    cpu.pc = 0x12340000u;
    cpu.a[7] = AMIVM_RAM_BASE + config.ram_size + 4u;
    cpu.isp = cpu.a[7];
    sp = cpu.a[7];
    amivm_jit_context_init(&context, &cpu, &vm);

    CHECK(amivm_jit_helper_bsr(&context, 0x12340004u, 0x56780000u) == -4);
    CHECK(cpu.pc == 0x12340000u);
    CHECK(cpu.a[7] == sp);
    CHECK(cpu.isp == sp);

    CHECK(amivm_jit_helper_rts(&context) == -4);
    CHECK(cpu.pc == 0x12340000u);
    CHECK(cpu.a[7] == sp);
    CHECK(cpu.isp == sp);

    amivm_vm_destroy(&vm);
    return 0;
}

int main(void)
{
    CHECK(run_bank_case(0x0000u) == 0);
    CHECK(run_bank_case(0x2000u) == 0);
    CHECK(run_bank_case(0x3000u) == 0);
    CHECK(fault_case() == 0);
    puts("AmiVM M2.39 JIT BSR/RTS helper tests: PASS");
    return 0;
}
