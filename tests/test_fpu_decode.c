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

static void put_fpu(uint8_t *rom, size_t offset, uint16_t ext)
{
    put16_be(&rom[offset], 0xf200u);
    put16_be(&rom[offset + 2u], ext);
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    const struct amivm_cpu_backend *backend = amivm_cpu_reference_backend();
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t initial_pc = AMIVM_ROM_BASE + 0x100u;
    const uint32_t illegal_handler = AMIVM_ROM_BASE + 0x300u;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    put32_be(&vm.rom[0], initial_sp);
    put32_be(&vm.rom[4], initial_pc);
    put32_be(&vm.rom[AMIVM_VECTOR_ILLEGAL_INSTRUCTION * 4u], illegal_handler);

    /* cpGEN register-source form: src FPm bits 12..10, dst FPn bits 9..7. */
    put_fpu(vm.rom, 0x100u, 0x0422u); /* FADD.X FP1,FP0 */
    put_fpu(vm.rom, 0x104u, 0x0823u); /* FMUL.X FP2,FP0 */
    put_fpu(vm.rom, 0x108u, 0x0428u); /* FSUB.X FP1,FP0 */
    put_fpu(vm.rom, 0x10cu, 0x0820u); /* FDIV.X FP2,FP0 */
    put_fpu(vm.rom, 0x110u, 0x0438u); /* FCMP.X FP1,FP0 */
    put_fpu(vm.rom, 0x114u, 0x0980u); /* FMOVE.X FP2,FP3 */
    put_fpu(vm.rom, 0x118u, 0x0c3au); /* FTST.X FP3 */
    put_fpu(vm.rom, 0x11cu, 0x047fu); /* unsupported opmode -> illegal */
    put16_be(&vm.rom[0x300u], 0x4e71u);
    vm.rom_used = 0x302u;

    CHECK(backend != NULL);
    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    CHECK(cpu.fpu.fpcr == 0u);
    CHECK(cpu.fpu.fpsr == 0u);
    CHECK(cpu.fpu.fpiar == 0u);
    CHECK(cpu.fpu.fp[0] == 0.0);

    cpu.fpu.fp[0] = 10.0;
    cpu.fpu.fp[1] = 2.0;
    cpu.fpu.fp[2] = 4.0;

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.fpu.fp[0] == 12.0);
    CHECK(cpu.fpu.fpiar == initial_pc);
    CHECK(cpu.pc == initial_pc + 4u);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.fpu.fp[0] == 48.0);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.fpu.fp[0] == 46.0);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.fpu.fp[0] == 11.5);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK((cpu.fpu.fpsr & (AMIVM_FPSR_CC_N | AMIVM_FPSR_CC_Z | AMIVM_FPSR_CC_NAN)) == 0u);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK(cpu.fpu.fp[3] == 4.0);
    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
    CHECK((cpu.fpu.fpsr & (AMIVM_FPSR_CC_N | AMIVM_FPSR_CC_Z | AMIVM_FPSR_CC_NAN)) == 0u);

    CHECK(amivm_cpu_step(&cpu, &vm, backend) == 2);
    CHECK(cpu.last_fault == AMIVM_CPU_FAULT_ILLEGAL);
    CHECK(cpu.last_exception_vector == AMIVM_VECTOR_ILLEGAL_INSTRUCTION);
    CHECK(cpu.pc == illegal_handler);

    /* CPU reset must reset the integrated FPU state too. */
    cpu.fpu.fp[0] = 99.0;
    cpu.fpu.fpcr = 0xffffffffu;
    cpu.fpu.fpsr = 0xffffffffu;
    cpu.fpu.fpiar = 0xffffffffu;
    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);
    CHECK(cpu.fpu.fp[0] == 0.0);
    CHECK(cpu.fpu.fpcr == 0u && cpu.fpu.fpsr == 0u && cpu.fpu.fpiar == 0u);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.9 F-line register decoder tests: PASS");
    return 0;
}
