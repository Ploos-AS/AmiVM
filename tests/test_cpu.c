#include "cpu.h"
#include "vm.h"

#include <stdio.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static void put16_be(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8u);
    p[1] = (uint8_t)value;
}

static void put32_be(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24u);
    p[1] = (uint8_t)(value >> 16u);
    p[2] = (uint8_t)(value >> 8u);
    p[3] = (uint8_t)value;
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    const struct amivm_cpu_backend *backend = amivm_cpu_reference_backend();
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t initial_pc = AMIVM_ROM_BASE + 0x100u;
    const uint32_t sub_pc = AMIVM_ROM_BASE + 0x180u;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], initial_pc);

    /* MOVEQ #5,D0; MOVE.L #$80000000,D1; TST.L D1; CLR.L D1; BSR sub; NOP */
    put16_be(&vm.rom[0x100], 0x7005u);
    put16_be(&vm.rom[0x102], 0x223cu);
    put32_be(&vm.rom[0x104], 0x80000000u);
    put16_be(&vm.rom[0x108], 0x4a81u);
    put16_be(&vm.rom[0x10a], 0x4281u);
    put16_be(&vm.rom[0x10c], 0x6100u);
    put16_be(&vm.rom[0x10e], (uint16_t)(sub_pc - (initial_pc + 0x10u)));
    put16_be(&vm.rom[0x110], 0x4e71u);

    /* sub: MOVEQ #-1,D2; RTS */
    put16_be(&vm.rom[0x180], 0x74ffu);
    put16_be(&vm.rom[0x182], 0x4e75u);
    vm.rom_used = 0x184u;

    CHECK(backend != NULL);
    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    CHECK(cpu.a[7] == initial_sp);
    CHECK(cpu.pc == initial_pc);
    CHECK((cpu.sr & 0x2000u) != 0u);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.d[0] == 5u);
    CHECK(cpu.pc == initial_pc + 2u);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.d[1] == 0x80000000u);
    CHECK(cpu.pc == initial_pc + 8u);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK((cpu.sr & 0x0008u) != 0u);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.d[1] == 0u);
    CHECK((cpu.sr & 0x0004u) != 0u);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.pc == sub_pc);
    CHECK(cpu.a[7] == initial_sp - 4u);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.d[2] == 0xffffffffu);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.pc == initial_pc + 0x10u);
    CHECK(cpu.a[7] == initial_sp);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.pc == initial_pc + 0x12u);

    cpu.pc = AMIVM_ROM_BASE + 0x1a0u;
    put16_be(&vm.rom[0x1a0], 0xffffu);
    vm.rom_used = 0x1a2u;
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == -2);
    CHECK(cpu.last_fault == AMIVM_CPU_FAULT_ILLEGAL);
    CHECK(cpu.fault_opcode == 0xffffu);

    cpu.pc = AMIVM_ROM_BASE + 1u;
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == -3);
    CHECK(cpu.last_fault == AMIVM_CPU_FAULT_ADDRESS);

    cpu.pc = 0x01000000u;
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == -4);
    CHECK(cpu.last_fault == AMIVM_CPU_FAULT_BUS);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.2 CPU tests: PASS");
    return 0;
}
