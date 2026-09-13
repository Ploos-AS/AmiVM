#include "cpu.h"
#include "vm.h"

#include <stddef.h>

#define M68K_SR_TRACE_MASK 0xc000u
#define M68K_SR_SUPERVISOR 0x2000u
#define M68K_SR_N 0x0008u
#define M68K_SR_Z 0x0004u
#define M68K_SR_V 0x0002u
#define M68K_SR_C 0x0001u
#define M68K_OPCODE_NOP 0x4e71u
#define M68K_OPCODE_RTS 0x4e75u
#define M68K_OPCODE_JMP_ABSL 0x4ef9u
#define M68K_OPCODE_JSR_ABSL 0x4eb9u

static void set_fault(struct amivm_cpu_state *cpu, enum amivm_cpu_fault fault,
                      uint32_t address, uint16_t opcode)
{
    cpu->last_fault = fault;
    cpu->fault_address = address;
    cpu->fault_opcode = opcode;
}

static bool read16_be(struct amivm_vm *vm, uint32_t addr, uint16_t *value)
{
    uint8_t hi;
    uint8_t lo;

    if ((addr & 1u) != 0u || value == NULL || !amivm_read8(vm, addr, &hi) ||
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

static bool write16_be(struct amivm_vm *vm, uint32_t addr, uint16_t value)
{
    if ((addr & 1u) != 0u) {
        return false;
    }
    return amivm_write8(vm, addr, (uint8_t)(value >> 8u)) &&
           amivm_write8(vm, addr + 1u, (uint8_t)value);
}

static bool write32_be(struct amivm_vm *vm, uint32_t addr, uint32_t value)
{
    if ((addr & 1u) != 0u) {
        return false;
    }
    return amivm_write8(vm, addr, (uint8_t)(value >> 24u)) &&
           amivm_write8(vm, addr + 1u, (uint8_t)(value >> 16u)) &&
           amivm_write8(vm, addr + 2u, (uint8_t)(value >> 8u)) &&
           amivm_write8(vm, addr + 3u, (uint8_t)value);
}

static void set_nz32(struct amivm_cpu_state *cpu, uint32_t value)
{
    cpu->sr &= (uint16_t)~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
    if (value == 0u) {
        cpu->sr |= M68K_SR_Z;
    }
    if ((value & 0x80000000u) != 0u) {
        cpu->sr |= M68K_SR_N;
    }
}

static int enter_exception(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                           unsigned vector, uint32_t saved_pc)
{
    uint32_t handler_pc;
    uint32_t new_sp;
    uint16_t old_sr;
    uint16_t format_vector;

    if (vector > 255u || !read32_be(vm, cpu->vbr + vector * 4u, &handler_pc)) {
        cpu->stopped = true;
        return -5;
    }

    old_sr = cpu->sr;
    cpu->sr = (uint16_t)((cpu->sr | M68K_SR_SUPERVISOR) & ~M68K_SR_TRACE_MASK);

    /*
     * M2.3 uses a 68020+-style format-0 short frame:
     *   +0 SR, +2 PC, +6 format/vector-offset word.
     * More detailed 68040 access-error frames are deferred until the
     * architecture needs their additional status words.
     */
    new_sp = cpu->a[7] - 8u;
    format_vector = (uint16_t)((vector * 4u) & 0x0fffu);
    if (!write16_be(vm, new_sp, old_sr) ||
        !write32_be(vm, new_sp + 2u, saved_pc) ||
        !write16_be(vm, new_sp + 6u, format_vector)) {
        cpu->stopped = true;
        return -5;
    }

    cpu->a[7] = new_sp;
    cpu->pc = handler_pc;
    cpu->last_exception_vector = (uint8_t)vector;
    return 2;
}

static int deliver_fault(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                         uint32_t saved_pc)
{
    switch (cpu->last_fault) {
    case AMIVM_CPU_FAULT_BUS:
        return enter_exception(cpu, vm, AMIVM_VECTOR_BUS_ERROR, saved_pc);
    case AMIVM_CPU_FAULT_ADDRESS:
        return enter_exception(cpu, vm, AMIVM_VECTOR_ADDRESS_ERROR, saved_pc);
    case AMIVM_CPU_FAULT_ILLEGAL:
        return enter_exception(cpu, vm, AMIVM_VECTOR_ILLEGAL_INSTRUCTION, saved_pc);
    case AMIVM_CPU_FAULT_NONE:
    default:
        return -1;
    }
}

static int reference_reset(struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    uint32_t initial_sp;
    uint32_t initial_pc;
    size_t i;

    if (cpu == NULL || vm == NULL || vm->rom_used < 8u) {
        return -1;
    }
    if (!read32_be(vm, AMIVM_ROM_BASE, &initial_sp) ||
        !read32_be(vm, AMIVM_ROM_BASE + 4u, &initial_pc)) {
        return -1;
    }

    for (i = 0; i < 8u; ++i) {
        cpu->d[i] = 0u;
        cpu->a[i] = 0u;
    }
    cpu->a[7] = initial_sp;
    cpu->pc = initial_pc;
    cpu->vbr = AMIVM_ROM_BASE;
    cpu->sr = M68K_SR_SUPERVISOR;
    cpu->stopped = false;
    cpu->last_fault = AMIVM_CPU_FAULT_NONE;
    cpu->fault_address = 0u;
    cpu->fault_opcode = 0u;
    cpu->last_exception_vector = 0u;
    return 0;
}

static int fetch16(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                   uint32_t addr, uint16_t *value)
{
    if ((addr & 1u) != 0u) {
        set_fault(cpu, AMIVM_CPU_FAULT_ADDRESS, addr, 0u);
        return -3;
    }
    if (!read16_be(vm, addr, value)) {
        set_fault(cpu, AMIVM_CPU_FAULT_BUS, addr, 0u);
        return -4;
    }
    return 0;
}

static int fetch32(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                   uint32_t addr, uint32_t *value)
{
    if ((addr & 1u) != 0u) {
        set_fault(cpu, AMIVM_CPU_FAULT_ADDRESS, addr, 0u);
        return -3;
    }
    if (!read32_be(vm, addr, value)) {
        set_fault(cpu, AMIVM_CPU_FAULT_BUS, addr, 0u);
        return -4;
    }
    return 0;
}

static int reference_step(struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    uint16_t opcode;
    uint32_t next_pc;
    uint32_t instruction_pc;

    if (cpu == NULL || vm == NULL || cpu->stopped) {
        return -1;
    }
    cpu->last_fault = AMIVM_CPU_FAULT_NONE;
    cpu->fault_address = 0u;
    cpu->fault_opcode = 0u;
    cpu->last_exception_vector = 0u;
    instruction_pc = cpu->pc;

    if (fetch16(cpu, vm, cpu->pc, &opcode) != 0) {
        return deliver_fault(cpu, vm, instruction_pc);
    }
    next_pc = cpu->pc + 2u;

    if (opcode == M68K_OPCODE_NOP) {
        cpu->pc = next_pc;
        return 1;
    }
    if (opcode == M68K_OPCODE_RTS) {
        uint32_t target;
        if (fetch32(cpu, vm, cpu->a[7], &target) != 0) {
            return deliver_fault(cpu, vm, instruction_pc);
        }
        cpu->a[7] += 4u;
        cpu->pc = target;
        return 1;
    }
    if (opcode == M68K_OPCODE_JMP_ABSL || opcode == M68K_OPCODE_JSR_ABSL) {
        uint32_t target;
        if (fetch32(cpu, vm, next_pc, &target) != 0) {
            return deliver_fault(cpu, vm, instruction_pc);
        }
        if (opcode == M68K_OPCODE_JSR_ABSL) {
            uint32_t new_sp = cpu->a[7] - 4u;
            if (!write32_be(vm, new_sp, next_pc + 4u)) {
                set_fault(cpu, (new_sp & 1u) ? AMIVM_CPU_FAULT_ADDRESS : AMIVM_CPU_FAULT_BUS,
                          new_sp, opcode);
                return deliver_fault(cpu, vm, instruction_pc);
            }
            cpu->a[7] = new_sp;
        }
        cpu->pc = target;
        return 1;
    }
    if ((opcode & 0xf100u) == 0x7000u) {
        unsigned reg = (unsigned)((opcode >> 9u) & 7u);
        int8_t imm = (int8_t)(opcode & 0xffu);
        cpu->d[reg] = (uint32_t)(int32_t)imm;
        set_nz32(cpu, cpu->d[reg]);
        cpu->pc = next_pc;
        return 1;
    }
    if ((opcode & 0xf1ffu) == 0x203cu) {
        unsigned reg = (unsigned)((opcode >> 9u) & 7u);
        uint32_t imm;
        if (fetch32(cpu, vm, next_pc, &imm) != 0) {
            return deliver_fault(cpu, vm, instruction_pc);
        }
        cpu->d[reg] = imm;
        set_nz32(cpu, imm);
        cpu->pc = next_pc + 4u;
        return 1;
    }
    if ((opcode & 0xfff8u) == 0x4280u) {
        unsigned reg = (unsigned)(opcode & 7u);
        cpu->d[reg] = 0u;
        set_nz32(cpu, 0u);
        cpu->pc = next_pc;
        return 1;
    }
    if ((opcode & 0xfff8u) == 0x4a80u) {
        unsigned reg = (unsigned)(opcode & 7u);
        set_nz32(cpu, cpu->d[reg]);
        cpu->pc = next_pc;
        return 1;
    }
    if ((opcode & 0xff00u) == 0x6000u || (opcode & 0xff00u) == 0x6100u) {
        int32_t disp;
        uint32_t base = next_pc;
        if ((opcode & 0xffu) == 0u) {
            uint16_t ext;
            if (fetch16(cpu, vm, next_pc, &ext) != 0) {
                return deliver_fault(cpu, vm, instruction_pc);
            }
            disp = (int16_t)ext;
            base += 2u;
        } else {
            disp = (int8_t)(opcode & 0xffu);
        }
        if ((opcode & 0xff00u) == 0x6100u) {
            uint32_t new_sp = cpu->a[7] - 4u;
            if (!write32_be(vm, new_sp, base)) {
                set_fault(cpu, (new_sp & 1u) ? AMIVM_CPU_FAULT_ADDRESS : AMIVM_CPU_FAULT_BUS,
                          new_sp, opcode);
                return deliver_fault(cpu, vm, instruction_pc);
            }
            cpu->a[7] = new_sp;
        }
        cpu->pc = (uint32_t)((int64_t)base + disp);
        return 1;
    }

    set_fault(cpu, AMIVM_CPU_FAULT_ILLEGAL, cpu->pc, opcode);
    return deliver_fault(cpu, vm, instruction_pc);
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
