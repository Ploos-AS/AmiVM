#include "cpu.h"
#include "vm.h"

#include <stddef.h>

#define SR_T 0xc000u
#define SR_S 0x2000u
#define SR_M 0x1000u
#define SR_IPL 0x0700u
#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u

#define OP_NOP 0x4e71u
#define OP_RTE 0x4e73u
#define OP_RTS 0x4e75u
#define OP_MOVEC_FROM 0x4e7au
#define OP_MOVEC_TO 0x4e7bu
#define OP_JMP_ABSL 0x4ef9u
#define OP_JSR_ABSL 0x4eb9u
#define OP_FPU_GEN 0xf200u

#define TC_ENABLE 0x80000000u
#define PAGE_MASK 0xfffff000u
#define DESC_VALID 0x00000001u
#define DESC_WP 0x00000002u
#define MMUSR_ROOT 0x00000001u
#define MMUSR_PAGE 0x00000002u
#define MMUSR_WP 0x00000004u
#define MMUSR_TABLE 0x00000008u

static int fetch16(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                   uint32_t addr, uint16_t *value);

static bool is_supervisor(const struct amivm_cpu_state *cpu)
{
    return (cpu->sr & SR_S) != 0u;
}

static bool is_master(const struct amivm_cpu_state *cpu)
{
    return is_supervisor(cpu) && (cpu->sr & SR_M) != 0u;
}

static void save_active_sp(struct amivm_cpu_state *cpu)
{
    if (!is_supervisor(cpu)) cpu->usp = cpu->a[7];
    else if (is_master(cpu)) cpu->msp = cpu->a[7];
    else cpu->isp = cpu->a[7];
}

static void select_active_sp(struct amivm_cpu_state *cpu)
{
    if (!is_supervisor(cpu)) cpu->a[7] = cpu->usp;
    else if (is_master(cpu)) cpu->a[7] = cpu->msp;
    else cpu->a[7] = cpu->isp;
}

static void set_fault(struct amivm_cpu_state *cpu, enum amivm_cpu_fault fault,
                      uint32_t address, uint16_t opcode)
{
    cpu->last_fault = fault;
    cpu->fault_address = address;
    cpu->fault_opcode = opcode;
}

static bool phys_read16(struct amivm_vm *vm, uint32_t addr, uint16_t *value)
{
    uint8_t hi, lo;
    if ((addr & 1u) != 0u || value == NULL ||
        !amivm_read8(vm, addr, &hi) || !amivm_read8(vm, addr + 1u, &lo)) return false;
    *value = (uint16_t)(((uint16_t)hi << 8u) | lo);
    return true;
}

static bool phys_read32(struct amivm_vm *vm, uint32_t addr, uint32_t *value)
{
    uint16_t hi, lo;
    if (value == NULL || !phys_read16(vm, addr, &hi) ||
        !phys_read16(vm, addr + 2u, &lo)) return false;
    *value = ((uint32_t)hi << 16u) | lo;
    return true;
}

int amivm_mmu_translate(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                        uint32_t logical, bool write, bool supervisor,
                        uint32_t *physical)
{
    uint32_t root, l1, l2, l1_addr, l2_addr;
    uint32_t i1 = logical >> 22u;
    uint32_t i2 = (logical >> 12u) & 0x3ffu;

    if (cpu == NULL || vm == NULL || physical == NULL) return AMIVM_MMU_FAULT_TABLE_BUS;
    cpu->mmusr = 0u;
    if ((cpu->tc & TC_ENABLE) == 0u) {
        *physical = logical;
        return AMIVM_MMU_OK;
    }

    root = (supervisor ? cpu->srp : cpu->urp) & PAGE_MASK;
    if (root == 0u) {
        cpu->mmusr = MMUSR_ROOT;
        return AMIVM_MMU_FAULT_ROOT;
    }

    l1_addr = root + i1 * 4u;
    if (!phys_read32(vm, l1_addr, &l1)) {
        cpu->mmusr = MMUSR_TABLE;
        return AMIVM_MMU_FAULT_TABLE_BUS;
    }
    if ((l1 & DESC_VALID) == 0u) {
        cpu->mmusr = MMUSR_ROOT;
        return AMIVM_MMU_FAULT_ROOT;
    }

    l2_addr = (l1 & PAGE_MASK) + i2 * 4u;
    if (!phys_read32(vm, l2_addr, &l2)) {
        cpu->mmusr = MMUSR_TABLE;
        return AMIVM_MMU_FAULT_TABLE_BUS;
    }
    if ((l2 & DESC_VALID) == 0u) {
        cpu->mmusr = MMUSR_PAGE;
        return AMIVM_MMU_FAULT_PAGE;
    }
    if (write && (l2 & DESC_WP) != 0u) {
        cpu->mmusr = MMUSR_WP;
        return AMIVM_MMU_FAULT_WRITE_PROTECT;
    }

    *physical = (l2 & PAGE_MASK) | (logical & 0xfffu);
    return AMIVM_MMU_OK;
}

static bool cpu_read8(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                      uint32_t addr, bool supervisor, uint8_t *value)
{
    uint32_t physical;
    if (amivm_mmu_translate(cpu, vm, addr, false, supervisor, &physical) != AMIVM_MMU_OK) {
        set_fault(cpu, AMIVM_CPU_FAULT_MMU, addr, 0u);
        return false;
    }
    if (!amivm_read8(vm, physical, value)) {
        set_fault(cpu, AMIVM_CPU_FAULT_BUS, addr, 0u);
        return false;
    }
    return true;
}

static bool cpu_write8(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                       uint32_t addr, bool supervisor, uint8_t value)
{
    uint32_t physical;
    if (amivm_mmu_translate(cpu, vm, addr, true, supervisor, &physical) != AMIVM_MMU_OK) {
        set_fault(cpu, AMIVM_CPU_FAULT_MMU, addr, 0u);
        return false;
    }
    if (!amivm_write8(vm, physical, value)) {
        set_fault(cpu, AMIVM_CPU_FAULT_BUS, addr, 0u);
        return false;
    }
    return true;
}

static bool cpu_read16(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                       uint32_t addr, bool supervisor, uint16_t *value)
{
    uint8_t hi, lo;
    if ((addr & 1u) != 0u || value == NULL ||
        !cpu_read8(cpu, vm, addr, supervisor, &hi) ||
        !cpu_read8(cpu, vm, addr + 1u, supervisor, &lo)) return false;
    *value = (uint16_t)(((uint16_t)hi << 8u) | lo);
    return true;
}

static bool cpu_read32(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                       uint32_t addr, bool supervisor, uint32_t *value)
{
    uint16_t hi, lo;
    if (value == NULL || !cpu_read16(cpu, vm, addr, supervisor, &hi) ||
        !cpu_read16(cpu, vm, addr + 2u, supervisor, &lo)) return false;
    *value = ((uint32_t)hi << 16u) | lo;
    return true;
}

static bool cpu_write16(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                        uint32_t addr, bool supervisor, uint16_t value)
{
    if ((addr & 1u) != 0u) return false;
    return cpu_write8(cpu, vm, addr, supervisor, (uint8_t)(value >> 8u)) &&
           cpu_write8(cpu, vm, addr + 1u, supervisor, (uint8_t)value);
}

static bool cpu_write32(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                        uint32_t addr, bool supervisor, uint32_t value)
{
    if ((addr & 1u) != 0u) return false;
    return cpu_write8(cpu, vm, addr, supervisor, (uint8_t)(value >> 24u)) &&
           cpu_write8(cpu, vm, addr + 1u, supervisor, (uint8_t)(value >> 16u)) &&
           cpu_write8(cpu, vm, addr + 2u, supervisor, (uint8_t)(value >> 8u)) &&
           cpu_write8(cpu, vm, addr + 3u, supervisor, (uint8_t)value);
}

static void set_nz32(struct amivm_cpu_state *cpu, uint32_t value)
{
    cpu->sr &= (uint16_t)~(SR_N | SR_Z | SR_V | SR_C);
    if (value == 0u) cpu->sr |= SR_Z;
    if ((value & 0x80000000u) != 0u) cpu->sr |= SR_N;
}

static bool read_cr(const struct amivm_cpu_state *cpu, uint16_t cr, uint32_t *value)
{
    if (value == NULL) return false;
    switch (cr) {
    case AMIVM_CR_SFC: *value = cpu->sfc; return true;
    case AMIVM_CR_DFC: *value = cpu->dfc; return true;
    case AMIVM_CR_CACR: *value = cpu->cacr; return true;
    case AMIVM_CR_TC: *value = cpu->tc; return true;
    case AMIVM_CR_VBR: *value = cpu->vbr; return true;
    case AMIVM_CR_MSP: *value = cpu->msp; return true;
    case AMIVM_CR_ISP: *value = cpu->isp; return true;
    case AMIVM_CR_MMUSR: *value = cpu->mmusr; return true;
    case AMIVM_CR_URP: *value = cpu->urp; return true;
    case AMIVM_CR_SRP: *value = cpu->srp; return true;
    default: return false;
    }
}

static bool write_cr(struct amivm_cpu_state *cpu, uint16_t cr, uint32_t value)
{
    switch (cr) {
    case AMIVM_CR_SFC: cpu->sfc = (uint8_t)(value & 7u); return true;
    case AMIVM_CR_DFC: cpu->dfc = (uint8_t)(value & 7u); return true;
    case AMIVM_CR_CACR: cpu->cacr = value; return true;
    case AMIVM_CR_TC: cpu->tc = value; return true;
    case AMIVM_CR_VBR: cpu->vbr = value; return true;
    case AMIVM_CR_MSP:
        cpu->msp = value;
        if (is_master(cpu)) cpu->a[7] = value;
        return true;
    case AMIVM_CR_ISP:
        cpu->isp = value;
        if (is_supervisor(cpu) && !is_master(cpu)) cpu->a[7] = value;
        return true;
    case AMIVM_CR_URP: cpu->urp = value; return true;
    case AMIVM_CR_SRP: cpu->srp = value; return true;
    default: return false;
    }
}

static uint32_t *general_reg(struct amivm_cpu_state *cpu, uint16_t ext)
{
    unsigned reg = (unsigned)((ext >> 12u) & 7u);
    return (ext & 0x8000u) ? &cpu->a[reg] : &cpu->d[reg];
}

static int enter_exception_internal(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                                    unsigned vector, uint32_t saved_pc, bool force_isp)
{
    uint32_t handler_pc, new_sp;
    uint16_t old_sr = cpu->sr;
    uint16_t format_vector;

    if (vector > 255u || !cpu_read32(cpu, vm, cpu->vbr + vector * 4u, true, &handler_pc)) {
        cpu->stopped = true;
        return -5;
    }

    save_active_sp(cpu);
    cpu->sr = (uint16_t)((cpu->sr | SR_S) & ~SR_T);
    if (force_isp) cpu->sr &= (uint16_t)~SR_M;
    select_active_sp(cpu);

    new_sp = cpu->a[7] - 8u;
    format_vector = (uint16_t)((vector * 4u) & 0x0fffu);
    if (!cpu_write16(cpu, vm, new_sp, true, old_sr) ||
        !cpu_write32(cpu, vm, new_sp + 2u, true, saved_pc) ||
        !cpu_write16(cpu, vm, new_sp + 6u, true, format_vector)) {
        cpu->stopped = true;
        return -5;
    }

    cpu->a[7] = new_sp;
    if (is_master(cpu)) cpu->msp = new_sp; else cpu->isp = new_sp;
    cpu->pc = handler_pc;
    cpu->last_exception_vector = (uint8_t)vector;
    return 2;
}

static int enter_exception(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                           unsigned vector, uint32_t saved_pc)
{
    return enter_exception_internal(cpu, vm, vector, saved_pc, false);
}

static int deliver_fault(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                         uint32_t saved_pc)
{
    switch (cpu->last_fault) {
    case AMIVM_CPU_FAULT_BUS:
    case AMIVM_CPU_FAULT_MMU:
        return enter_exception(cpu, vm, AMIVM_VECTOR_BUS_ERROR, saved_pc);
    case AMIVM_CPU_FAULT_ADDRESS:
        return enter_exception(cpu, vm, AMIVM_VECTOR_ADDRESS_ERROR, saved_pc);
    case AMIVM_CPU_FAULT_ILLEGAL:
        return enter_exception(cpu, vm, AMIVM_VECTOR_ILLEGAL_INSTRUCTION, saved_pc);
    case AMIVM_CPU_FAULT_PRIVILEGE:
        return enter_exception(cpu, vm, AMIVM_VECTOR_PRIVILEGE_VIOLATION, saved_pc);
    default:
        return -1;
    }
}

static unsigned highest_pending_irq(uint32_t pending)
{
    unsigned level;
    for (level = 7u; level > 0u; --level)
        if ((pending & (1u << level)) != 0u) return level;
    return 0u;
}

static int service_interrupt(struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    unsigned level = highest_pending_irq(vm->irq_pending);
    unsigned mask = (unsigned)((cpu->sr & SR_IPL) >> 8u);
    int rc;
    if (level == 0u || level <= mask) return 0;
    rc = enter_exception_internal(cpu, vm, AMIVM_VECTOR_AUTOVECTOR_BASE + level,
                                  cpu->pc, true);
    if (rc > 0) {
        cpu->sr = (uint16_t)((cpu->sr & ~SR_IPL) | (level << 8u));
        amivm_clear_irq(vm, level);
    }
    return rc;
}

static bool decode_fpu_op(uint16_t opmode, enum amivm_fpu_op *op)
{
    if (op == NULL) return false;
    switch (opmode) {
    case 0x00u: *op = AMIVM_FPU_FMOVE; return true;
    case 0x20u: *op = AMIVM_FPU_FDIV; return true;
    case 0x22u: *op = AMIVM_FPU_FADD; return true;
    case 0x23u: *op = AMIVM_FPU_FMUL; return true;
    case 0x28u: *op = AMIVM_FPU_FSUB; return true;
    case 0x38u: *op = AMIVM_FPU_FCMP; return true;
    case 0x3au: *op = AMIVM_FPU_FTEST; return true;
    default: return false;
    }
}

static int execute_fpu_register(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                                uint32_t instruction_pc, uint32_t next_pc,
                                uint16_t opcode)
{
    uint16_t ext;
    uint16_t opmode;
    unsigned src;
    unsigned dst;
    enum amivm_fpu_op op;
    int rc;

    if (fetch16(cpu, vm, next_pc, &ext) != 0)
        return deliver_fault(cpu, vm, instruction_pc);

    /* M2.9 handles the 68881/68040 cpGEN register-source form only:
     * primary word F200, extension class 000, FPm in bits 12..10,
     * FPn in bits 9..7, and opmode in bits 6..0.
     */
    if (((ext >> 13u) & 7u) != 0u) {
        set_fault(cpu, AMIVM_CPU_FAULT_ILLEGAL, instruction_pc, opcode);
        return deliver_fault(cpu, vm, instruction_pc);
    }

    src = (unsigned)((ext >> 10u) & 7u);
    dst = (unsigned)((ext >> 7u) & 7u);
    opmode = (uint16_t)(ext & 0x007fu);
    if (!decode_fpu_op(opmode, &op)) {
        set_fault(cpu, AMIVM_CPU_FAULT_ILLEGAL, instruction_pc, opcode);
        return deliver_fault(cpu, vm, instruction_pc);
    }

    rc = amivm_fpu_execute(&cpu->fpu, op, dst, src, instruction_pc);
    if (rc == AMIVM_FPU_ERR_REGISTER || rc == AMIVM_FPU_ERR_OPERATION) {
        set_fault(cpu, AMIVM_CPU_FAULT_ILLEGAL, instruction_pc, opcode);
        return deliver_fault(cpu, vm, instruction_pc);
    }

    /* Divide-by-zero is recorded in FPSR by the M2.8 core. Exception enable
     * and deferred FPU exception delivery are intentionally deferred.
     */
    cpu->pc = next_pc + 2u;
    return 1;
}

static int reference_reset(struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    uint32_t initial_sp, initial_pc;
    size_t i;
    if (cpu == NULL || vm == NULL || vm->rom_used < 8u) return -1;
    if (!phys_read32(vm, AMIVM_ROM_BASE, &initial_sp) ||
        !phys_read32(vm, AMIVM_ROM_BASE + 4u, &initial_pc)) return -1;
    for (i = 0; i < 8u; ++i) { cpu->d[i] = 0u; cpu->a[i] = 0u; }
    cpu->usp = 0u;
    cpu->isp = initial_sp;
    cpu->msp = initial_sp;
    cpu->a[7] = initial_sp;
    cpu->pc = initial_pc;
    cpu->vbr = AMIVM_ROM_BASE;
    cpu->cacr = cpu->tc = cpu->urp = cpu->srp = cpu->mmusr = 0u;
    cpu->sfc = cpu->dfc = 0u;
    cpu->sr = (uint16_t)(SR_S | SR_IPL);
    cpu->stopped = false;
    cpu->last_fault = AMIVM_CPU_FAULT_NONE;
    cpu->fault_address = 0u;
    cpu->fault_opcode = 0u;
    cpu->last_exception_vector = 0u;
    amivm_fpu_reset(&cpu->fpu);
    return 0;
}

static int fetch16(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                   uint32_t addr, uint16_t *value)
{
    if ((addr & 1u) != 0u) {
        set_fault(cpu, AMIVM_CPU_FAULT_ADDRESS, addr, 0u);
        return -3;
    }
    if (!cpu_read16(cpu, vm, addr, is_supervisor(cpu), value)) {
        if (cpu->last_fault == AMIVM_CPU_FAULT_NONE)
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
    if (!cpu_read32(cpu, vm, addr, is_supervisor(cpu), value)) {
        if (cpu->last_fault == AMIVM_CPU_FAULT_NONE)
            set_fault(cpu, AMIVM_CPU_FAULT_BUS, addr, 0u);
        return -4;
    }
    return 0;
}

static int reference_step(struct amivm_cpu_state *cpu, struct amivm_vm *vm)
{
    uint16_t opcode;
    uint32_t next_pc, instruction_pc;
    int irq_rc;

    if (cpu == NULL || vm == NULL || cpu->stopped) return -1;
    cpu->last_fault = AMIVM_CPU_FAULT_NONE;
    cpu->fault_address = 0u;
    cpu->fault_opcode = 0u;
    cpu->last_exception_vector = 0u;

    irq_rc = service_interrupt(cpu, vm);
    if (irq_rc != 0) return irq_rc;

    instruction_pc = cpu->pc;
    if (fetch16(cpu, vm, cpu->pc, &opcode) != 0)
        return deliver_fault(cpu, vm, instruction_pc);
    next_pc = cpu->pc + 2u;

    if (opcode == OP_NOP) { cpu->pc = next_pc; return 1; }
    if (opcode == OP_FPU_GEN)
        return execute_fpu_register(cpu, vm, instruction_pc, next_pc, opcode);

    if (opcode == OP_RTE) {
        uint16_t restored_sr, format_vector;
        uint32_t restored_pc, frame_end;
        if (!is_supervisor(cpu)) {
            set_fault(cpu, AMIVM_CPU_FAULT_PRIVILEGE, instruction_pc, opcode);
            return deliver_fault(cpu, vm, instruction_pc);
        }
        if (!cpu_read16(cpu, vm, cpu->a[7], true, &restored_sr) ||
            !cpu_read32(cpu, vm, cpu->a[7] + 2u, true, &restored_pc) ||
            !cpu_read16(cpu, vm, cpu->a[7] + 6u, true, &format_vector))
            return deliver_fault(cpu, vm, instruction_pc);
        if ((format_vector & 0xf000u) != 0u) { cpu->stopped = true; return -6; }
        frame_end = cpu->a[7] + 8u;
        if (is_master(cpu)) cpu->msp = frame_end; else cpu->isp = frame_end;
        cpu->sr = restored_sr;
        cpu->pc = restored_pc;
        select_active_sp(cpu);
        return 1;
    }

    if (opcode == OP_MOVEC_FROM || opcode == OP_MOVEC_TO) {
        uint16_t ext, cr;
        uint32_t *reg, value;
        if (!is_supervisor(cpu)) {
            set_fault(cpu, AMIVM_CPU_FAULT_PRIVILEGE, instruction_pc, opcode);
            return deliver_fault(cpu, vm, instruction_pc);
        }
        if (fetch16(cpu, vm, next_pc, &ext) != 0)
            return deliver_fault(cpu, vm, instruction_pc);
        cr = (uint16_t)(ext & 0x0fffu);
        reg = general_reg(cpu, ext);
        if (opcode == OP_MOVEC_FROM) {
            if (!read_cr(cpu, cr, &value)) {
                set_fault(cpu, AMIVM_CPU_FAULT_ILLEGAL, instruction_pc, opcode);
                return deliver_fault(cpu, vm, instruction_pc);
            }
            *reg = value;
        } else if (!write_cr(cpu, cr, *reg)) {
            set_fault(cpu, AMIVM_CPU_FAULT_ILLEGAL, instruction_pc, opcode);
            return deliver_fault(cpu, vm, instruction_pc);
        }
        cpu->pc = next_pc + 2u;
        return 1;
    }

    if ((opcode & 0xfff8u) == 0x4e60u || (opcode & 0xfff8u) == 0x4e68u) {
        unsigned reg = (unsigned)(opcode & 7u);
        if (!is_supervisor(cpu)) {
            set_fault(cpu, AMIVM_CPU_FAULT_PRIVILEGE, instruction_pc, opcode);
            return deliver_fault(cpu, vm, instruction_pc);
        }
        if ((opcode & 0xfff8u) == 0x4e60u) cpu->usp = cpu->a[reg];
        else cpu->a[reg] = cpu->usp;
        cpu->pc = next_pc;
        return 1;
    }

    if (opcode == OP_RTS) {
        uint32_t target;
        if (fetch32(cpu, vm, cpu->a[7], &target) != 0)
            return deliver_fault(cpu, vm, instruction_pc);
        cpu->a[7] += 4u;
        save_active_sp(cpu);
        cpu->pc = target;
        return 1;
    }

    if (opcode == OP_JMP_ABSL || opcode == OP_JSR_ABSL) {
        uint32_t target;
        if (fetch32(cpu, vm, next_pc, &target) != 0)
            return deliver_fault(cpu, vm, instruction_pc);
        if (opcode == OP_JSR_ABSL) {
            uint32_t sp = cpu->a[7] - 4u;
            if (!cpu_write32(cpu, vm, sp, is_supervisor(cpu), next_pc + 4u))
                return deliver_fault(cpu, vm, instruction_pc);
            cpu->a[7] = sp;
            save_active_sp(cpu);
        }
        cpu->pc = target;
        return 1;
    }

    if ((opcode & 0xf100u) == 0x7000u) {
        unsigned reg = (unsigned)((opcode >> 9u) & 7u);
        cpu->d[reg] = (uint32_t)(int32_t)(int8_t)(opcode & 0xffu);
        set_nz32(cpu, cpu->d[reg]);
        cpu->pc = next_pc;
        return 1;
    }

    if ((opcode & 0xf1ffu) == 0x203cu) {
        unsigned reg = (unsigned)((opcode >> 9u) & 7u);
        uint32_t imm;
        if (fetch32(cpu, vm, next_pc, &imm) != 0)
            return deliver_fault(cpu, vm, instruction_pc);
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
        set_nz32(cpu, cpu->d[opcode & 7u]);
        cpu->pc = next_pc;
        return 1;
    }

    if ((opcode & 0xff00u) == 0x6000u || (opcode & 0xff00u) == 0x6100u) {
        int32_t disp;
        uint32_t base = next_pc;
        if ((opcode & 0xffu) == 0u) {
            uint16_t ext;
            if (fetch16(cpu, vm, next_pc, &ext) != 0)
                return deliver_fault(cpu, vm, instruction_pc);
            disp = (int16_t)ext;
            base += 2u;
        } else {
            disp = (int8_t)(opcode & 0xffu);
        }
        if ((opcode & 0xff00u) == 0x6100u) {
            uint32_t sp = cpu->a[7] - 4u;
            if (!cpu_write32(cpu, vm, sp, is_supervisor(cpu), base))
                return deliver_fault(cpu, vm, instruction_pc);
            cpu->a[7] = sp;
            save_active_sp(cpu);
        }
        cpu->pc = (uint32_t)((int64_t)base + disp);
        return 1;
    }

    set_fault(cpu, AMIVM_CPU_FAULT_ILLEGAL, cpu->pc, opcode);
    return deliver_fault(cpu, vm, instruction_pc);
}

static const struct amivm_cpu_backend reference_backend = {
    "reference-68040", reference_reset, reference_step
};

const struct amivm_cpu_backend *amivm_cpu_reference_backend(void)
{
    return &reference_backend;
}

int amivm_cpu_reset(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                    const struct amivm_cpu_backend *backend)
{
    if (backend == NULL || backend->reset == NULL) return -1;
    return backend->reset(cpu, vm);
}

int amivm_cpu_step(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                   const struct amivm_cpu_backend *backend)
{
    if (backend == NULL || backend->step == NULL) return -1;
    return backend->step(cpu, vm);
}
