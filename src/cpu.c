#include "cpu.h"
#include "vm.h"

#include <stddef.h>

#define M68K_SR_SUPERVISOR 0x2000u
#define M68K_OPCODE_NOP 0x4e71u

static bool read16_be(struct amivm_vm *vm, uint32_t addr, uint16_t *value)
{
    uint8_t hi;
    uint8_t lo;

    if (value == NULL || !amivm_read8(vm, addr, &hi) ||
        !amivm_read8(vm, addr + 1u, &lo)) {
        return false;
    }
    *value = (uint16_t)(((uint16_t)hi << 8u) | (uint16_t)lo);
    return true;
}

static bool read32_be(struct amivm_vm *vm, uint32_t addr, uint32_t *value)
{
    uint16_t hi;
    uint16_t lo;

    if (value == NULL || !read16_be(vm, addr, &hi) ||
        !read16_be(vm, addr + 2u, &lo)) {
        return false;
    }
    *value = ((uint32_t)hi << 16u) | (uint32_t)lo;
    return true;
}

static int reference_reset(struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    uint32_t initial_sp;
    uint32_t initial_pc;

    if (cpu == NULL || vm == NULL || vm->rom_used < 8u) {
        return -1;
    }

    if (!read32_be(vm, AMIVM_ROM_BASE, &initial_sp) ||
        !read32_be(vm, AMIVM_ROM_BASE + 4u, &initial_pc)) {
        return -1;
    }

    for (size_t i = 0; i < 8u; ++i) {
        cpu->d[i] = 0u;
        cpu->a[i] = 0u;
    }
    cpu->a[7] = initial_sp;
    cpu->pc = initial_pc;
    cpu->sr = M68K_SR_SUPERVISOR;
    cpu->stopped = false;
    return 0;
}

static int reference_step(struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    uint16_t opcode;

    if (cpu == NULL || vm == NULL || cpu->stopped) {
        return -1;
    }
    if (!read16_be(vm, cpu->pc, &opcode)) {
        return -1;
    }

    switch (opcode) {
    case M68K_OPCODE_NOP:
        cpu->pc += 2u;
        return 1;
    default:
        return -2;
    }
}

static const struct amivm_cpu_backend reference_backend = {
    "reference-68040",
    reference_reset,
    reference_step,
};

const struct amivm_cpu_backend *amivm_cpu_reference_backend(void)
{
    return &reference_backend;
}

int amivm_cpu_reset(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                    const struct amivm_cpu_backend *backend)
{
    if (backend == NULL || backend->reset == NULL) {
        return -1;
    }
    return backend->reset(cpu, vm);
}

int amivm_cpu_step(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                   const struct amivm_cpu_backend *backend)
{
    if (backend == NULL || backend->step == NULL) {
        return -1;
    }
    return backend->step(cpu, vm);
}
