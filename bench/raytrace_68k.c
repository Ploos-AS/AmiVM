#include "raytrace_68k.h"
#include "raytrace_68k_program.h"

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8u);
    p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24u);
    p[1] = (uint8_t)(v >> 16u);
    p[2] = (uint8_t)(v >> 8u);
    p[3] = (uint8_t)v;
}

int amivm_raytrace_68k_run(struct amivm_cpu_state *cpu,
                           struct amivm_vm *vm,
                           struct amivm_exec_engine *exec)
{
    size_t program_size;
    const uint8_t *program = amivm_raytrace_68k_program(&program_size);
    const uint32_t entry = AMIVM_ROM_BASE + 0x100u;

    if (program_size > sizeof(vm->rom) - 0x100u)
        return 2;
    for (size_t i = 0u; i < program_size; ++i)
        vm->rom[0x100u + i] = program[i];

    if (amivm_cpu_reset(cpu, vm, exec->backend) != 0)
        return 1;

    amivm_exec_init(exec, exec->backend);
    return amivm_exec_run(exec, cpu, vm, 1000000u) > 0 ? 0 : 1;
}
