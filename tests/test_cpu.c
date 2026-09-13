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

static uint16_t get16_be(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8u) | p[1]);
}

static uint32_t get32_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24u) | ((uint32_t)p[1] << 16u) |
           ((uint32_t)p[2] << 8u) | (uint32_t)p[3];
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    const struct amivm_cpu_backend *backend = amivm_cpu_reference_backend();
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t user_sp = AMIVM_RAM_BASE + 0x1800u;
    const uint32_t initial_pc = AMIVM_ROM_BASE + 0x100u;
    const uint32_t sub_pc = AMIVM_ROM_BASE + 0x180u;
    const uint32_t bus_handler = AMIVM_ROM_BASE + 0x200u;
    const uint32_t address_handler = AMIVM_ROM_BASE + 0x220u;
    const uint32_t illegal_handler = AMIVM_ROM_BASE + 0x240u;
    const uint32_t irq2_handler = AMIVM_ROM_BASE + 0x260u;
    const uint32_t privilege_handler = AMIVM_ROM_BASE + 0x280u;
    const size_t frame_offset = 0x1000u - 8u;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], initial_pc);
    put32_be(&vm.rom[AMIVM_VECTOR_BUS_ERROR * 4u], bus_handler);
    put32_be(&vm.rom[AMIVM_VECTOR_ADDRESS_ERROR * 4u], address_handler);
    put32_be(&vm.rom[AMIVM_VECTOR_ILLEGAL_INSTRUCTION * 4u], illegal_handler);
    put32_be(&vm.rom[AMIVM_VECTOR_PRIVILEGE_VIOLATION * 4u], privilege_handler);
    put32_be(&vm.rom[(AMIVM_VECTOR_AUTOVECTOR_BASE + 2u) * 4u], irq2_handler);

    put16_be(&vm.rom[0x100], 0x7005u);
    put16_be(&vm.rom[0x102], 0x223cu);
    put32_be(&vm.rom[0x104], 0x80000000u);
    put16_be(&vm.rom[0x108], 0x4a81u);
    put16_be(&vm.rom[0x10a], 0x4281u);
    put16_be(&vm.rom[0x10c], 0x6100u);
    put16_be(&vm.rom[0x10e], (uint16_t)(sub_pc - (initial_pc + 0x10u)));
    put16_be(&vm.rom[0x110], 0x4e71u);

    put16_be(&vm.rom[0x180], 0x74ffu);
    put16_be(&vm.rom[0x182], 0x4e75u);
    put16_be(&vm.rom[0x200], 0x4e71u);
    put16_be(&vm.rom[0x220], 0x4e71u);
    put16_be(&vm.rom[0x240], 0x4e71u);
    put16_be(&vm.rom[0x260], 0x4e73u);
    put16_be(&vm.rom[0x280], 0x4e71u);
    put16_be(&vm.rom[0x2a0], 0x4e73u);

    /* M2.6 control-register program:
     * MOVE.L #$12345678,D0; MOVEC D0,TC; MOVEC TC,D1;
     * MOVE.L #$10001c00,A0 is prepared by the test, then MOVEC A0,MSP;
     * MOVEC MSP,A1; MOVEC D0,URP; MOVEC URP,D2.
     */
    put16_be(&vm.rom[0x2c0], 0x203cu);
    put32_be(&vm.rom[0x2c2], 0x12345678u);
    put16_be(&vm.rom[0x2c6], 0x4e7bu);
    put16_be(&vm.rom[0x2c8], 0x0003u);
    put16_be(&vm.rom[0x2ca], 0x4e7au);
    put16_be(&vm.rom[0x2cc], 0x1003u);
    put16_be(&vm.rom[0x2ce], 0x4e7bu);
    put16_be(&vm.rom[0x2d0], 0x8803u);
    put16_be(&vm.rom[0x2d2], 0x4e7au);
    put16_be(&vm.rom[0x2d4], 0x9803u);
    put16_be(&vm.rom[0x2d6], 0x4e7bu);
    put16_be(&vm.rom[0x2d8], 0x0806u);
    put16_be(&vm.rom[0x2da], 0x4e7au);
    put16_be(&vm.rom[0x2dc], 0x2806u);

    /* User-mode MOVEC D0,TC must raise privilege violation. */
    put16_be(&vm.rom[0x300], 0x4e7bu);
    put16_be(&vm.rom[0x302], 0x0003u);
    vm.rom_used = 0x304u;

    CHECK(backend != NULL);
    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    CHECK(cpu.a[7] == initial_sp);
    CHECK(cpu.isp == initial_sp);
    CHECK(cpu.msp == initial_sp);
    CHECK(cpu.usp == 0u);
    CHECK(cpu.pc == initial_pc);
    CHECK(cpu.vbr == AMIVM_ROM_BASE);
    CHECK(cpu.tc == 0u && cpu.urp == 0u && cpu.srp == 0u && cpu.cacr == 0u);
    CHECK(cpu.sfc == 0u && cpu.dfc == 0u);
    CHECK((cpu.sr & 0x2700u) == 0x2700u);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.d[0] == 5u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.d[1] == 0x80000000u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK((cpu.sr & 0x0008u) != 0u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.d[1] == 0u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.pc == sub_pc);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.d[2] == 0xffffffffu);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.pc == initial_pc + 0x10u);
    CHECK(cpu.a[7] == initial_sp);

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    cpu.sr = 0u;
    cpu.pc = AMIVM_ROM_BASE + 0x1a0u;
    put16_be(&vm.rom[0x1a0], 0xffffu);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 2);
    CHECK(cpu.last_fault == AMIVM_CPU_FAULT_ILLEGAL);
    CHECK(cpu.last_exception_vector == AMIVM_VECTOR_ILLEGAL_INSTRUCTION);
    CHECK(cpu.pc == illegal_handler);
    CHECK(cpu.a[7] == initial_sp - 8u);
    CHECK(get16_be(&vm.ram[frame_offset]) == 0u);
    CHECK(get32_be(&vm.ram[frame_offset + 2u]) == AMIVM_ROM_BASE + 0x1a0u);

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    cpu.pc = AMIVM_ROM_BASE + 1u;
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 2);
    CHECK(cpu.last_exception_vector == AMIVM_VECTOR_ADDRESS_ERROR);
    CHECK(cpu.pc == address_handler);

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    cpu.pc = 0x01000000u;
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 2);
    CHECK(cpu.last_exception_vector == AMIVM_VECTOR_BUS_ERROR);
    CHECK(cpu.pc == bus_handler);

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    cpu.pc = initial_pc + 0x10u;
    cpu.sr = 0x2300u;
    amivm_raise_irq(&vm, 2u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK((vm.irq_pending & (1u << 2u)) != 0u);
    cpu.pc = initial_pc + 0x10u;
    cpu.sr = 0x2000u;
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 2);
    CHECK(cpu.pc == irq2_handler);
    CHECK(cpu.a[7] == initial_sp - 8u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.pc == initial_pc + 0x10u);
    CHECK(cpu.a[7] == initial_sp);
    CHECK(cpu.isp == initial_sp);

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    cpu.sr = 0u;
    cpu.usp = user_sp;
    cpu.a[7] = user_sp;
    cpu.pc = initial_pc + 0x10u;
    amivm_raise_irq(&vm, 2u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 2);
    CHECK(cpu.pc == irq2_handler);
    CHECK(cpu.usp == user_sp);
    CHECK(cpu.a[7] == initial_sp - 8u);
    CHECK(cpu.isp == initial_sp - 8u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.pc == initial_pc + 0x10u);
    CHECK(cpu.sr == 0u);
    CHECK(cpu.a[7] == user_sp);
    CHECK(cpu.usp == user_sp);
    CHECK(cpu.isp == initial_sp);

    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    cpu.sr = 0u;
    cpu.usp = user_sp;
    cpu.a[7] = user_sp;
    cpu.pc = AMIVM_ROM_BASE + 0x2a0u;
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 2);
    CHECK(cpu.last_fault == AMIVM_CPU_FAULT_PRIVILEGE);
    CHECK(cpu.last_exception_vector == AMIVM_VECTOR_PRIVILEGE_VIOLATION);
    CHECK(cpu.pc == privilege_handler);

    /* MOVEC register model and round-trip. */
    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    cpu.a[0] = AMIVM_RAM_BASE + 0x1c00u;
    cpu.pc = AMIVM_ROM_BASE + 0x2c0u;
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.d[0] == 0x12345678u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.tc == 0x12345678u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.d[1] == 0x12345678u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.msp == AMIVM_RAM_BASE + 0x1c00u);
    CHECK(cpu.a[7] == initial_sp);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.a[1] == AMIVM_RAM_BASE + 0x1c00u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.urp == 0x12345678u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.d[2] == 0x12345678u);

    /* User-mode MOVEC is privileged. */
    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    cpu.sr = 0u;
    cpu.usp = user_sp;
    cpu.a[7] = user_sp;
    cpu.pc = AMIVM_ROM_BASE + 0x300u;
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 2);
    CHECK(cpu.last_fault == AMIVM_CPU_FAULT_PRIVILEGE);
    CHECK(cpu.last_exception_vector == AMIVM_VECTOR_PRIVILEGE_VIOLATION);
    CHECK(cpu.pc == privilege_handler);

    /* A hardware interrupt from master state must use ISP and preserve MSP. */
    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    cpu.msp = AMIVM_RAM_BASE + 0x1e00u;
    cpu.a[7] = cpu.msp;
    cpu.sr = 0x3000u;
    cpu.pc = initial_pc + 0x10u;
    amivm_raise_irq(&vm, 2u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 2);
    CHECK(cpu.pc == irq2_handler);
    CHECK(cpu.msp == AMIVM_RAM_BASE + 0x1e00u);
    CHECK(cpu.a[7] == initial_sp - 8u);
    CHECK((cpu.sr & 0x1000u) == 0u);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.sr == 0x3000u);
    CHECK(cpu.a[7] == AMIVM_RAM_BASE + 0x1e00u);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.6 CPU control-state tests: PASS");
    return 0;
}
