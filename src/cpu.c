#include "cpu.h"
#include "vm.h"
#include "ea.h"

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
#define OP_PMMU_BASE 0xf000u

#define TC_ENABLE 0x80000000u
#define PAGE_MASK 0xfffff000u
#define DESC_VALID 0x00000001u
#define DESC_WP 0x00000002u
#define MMUSR_ROOT 0x00000001u
#define MMUSR_PAGE 0x00000002u
#define MMUSR_WP 0x00000004u
#define MMUSR_TABLE 0x00000008u

/* M2.88 68851-local descriptor vocabulary. Kept separate from the 040
 * foundation so PMMU semantics can evolve without coupling the backends. */
#define PMMU51_DESC_TYPE_MASK 0x00000003u
#define PMMU51_DESC_INVALID   0x00000000u
#define PMMU51_DESC_PAGE      0x00000001u
#define PMMU51_DESC_TABLE     0x00000002u
#define PMMU51_DESC_WP        0x00000004u
#define PMMU51_ADDR_MASK      0xfffff000u

static const struct amivm_cpu_profile *active_profile(const struct amivm_cpu_state *cpu)
{
    return cpu != NULL && cpu->profile != NULL ? cpu->profile : amivm_cpu_profile_default();
}

void amivm_cpu_set_profile(struct amivm_cpu_state *cpu,
                           const struct amivm_cpu_profile *profile)
{
    if (cpu != NULL) cpu->profile = profile != NULL ? profile : amivm_cpu_profile_default();
}

const struct amivm_cpu_profile *amivm_cpu_get_profile(const struct amivm_cpu_state *cpu)
{
    return active_profile(cpu);
}

bool amivm_pmmu51_read_register(const struct amivm_cpu_state *cpu,
                                enum amivm_pmmu51_register reg,
                                uint32_t *value)
{
    if (cpu == NULL || value == NULL ||
        active_profile(cpu)->mmu_model != AMIVM_MMU_68851) return false;
    switch (reg) {
    case AMIVM_PMMU51_REG_TC: *value = cpu->pmmu_tc; return true;
    case AMIVM_PMMU51_REG_CRP:\n    case AMIVM_PMMU51_REG_SRP: return false;
    case AMIVM_PMMU51_REG_PSR: *value = cpu->pmmu_psr; return true;
    default: return false;
    }
}

bool amivm_pmmu51_write_register(struct amivm_cpu_state *cpu,
                                 enum amivm_pmmu51_register reg,
                                 uint32_t value)
{
    if (cpu == NULL || active_profile(cpu)->mmu_model != AMIVM_MMU_68851)
        return false;
    switch (reg) {
    case AMIVM_PMMU51_REG_TC: cpu->pmmu_tc = value; return true;
    case AMIVM_PMMU51_REG_CRP:\n    case AMIVM_PMMU51_REG_SRP: return false;
    case AMIVM_PMMU51_REG_PSR: return false;
    default: return false;
    }
}

bool amivm_pmmu51_read_root(const struct amivm_cpu_state *cpu,
                            enum amivm_pmmu51_register reg,
                            uint64_t *value)
{
    if (cpu == NULL || value == NULL ||
        active_profile(cpu)->mmu_model != AMIVM_MMU_68851) return false;
    if (reg == AMIVM_PMMU51_REG_CRP) { *value = cpu->pmmu_crp; return true; }
    if (reg == AMIVM_PMMU51_REG_SRP) { *value = cpu->pmmu_srp; return true; }
    return false;
}

bool amivm_pmmu51_write_root(struct amivm_cpu_state *cpu,
                             enum amivm_pmmu51_register reg,
                             uint64_t value)
{
    if (cpu == NULL || active_profile(cpu)->mmu_model != AMIVM_MMU_68851)
        return false;
    if (reg == AMIVM_PMMU51_REG_CRP) { cpu->pmmu_crp = value; return true; }
    if (reg == AMIVM_PMMU51_REG_SRP) { cpu->pmmu_srp = value; return true; }
    return false;
}

bool amivm_pmmu51_decode_root(uint64_t raw, struct amivm_pmmu51_root *root)
{
    uint32_t upper;
    if (root == NULL) return false;
    upper = (uint32_t)(raw >> 32u);
    root->lower_limit = (upper & 0x80000000u) != 0u;
    root->limit = (uint16_t)((upper >> 16u) & 0x7fffu);
    root->shared_globally = (upper & 0x00000200u) != 0u;
    root->type = (enum amivm_pmmu51_root_type)(upper & 3u);
    root->table_address = (uint32_t)raw & 0xfffffff0u;
    return root->type != AMIVM_PMMU51_ROOT_INVALID;
}

bool amivm_pmmu51_decode_long_descriptor(
    uint64_t raw, struct amivm_pmmu51_long_descriptor *desc)
{
    uint32_t upper;
    if (desc == NULL) return false;
    upper = (uint32_t)(raw >> 32u);
    desc->lower_limit = (upper & 0x80000000u) != 0u;
    desc->limit = (uint16_t)((upper >> 16u) & 0x7fffu);
    desc->read_access_level = (uint8_t)((upper >> 13u) & 7u);
    desc->write_access_level = (uint8_t)((upper >> 10u) & 7u);
    desc->shared_globally = (upper & 0x00000200u) != 0u;
    desc->supervisor_only = (upper & 0x00000100u) != 0u;
    desc->type = (enum amivm_pmmu51_root_type)(upper & 3u);
    desc->table_address = (uint32_t)raw & 0xfffffff0u;
    return desc->type != AMIVM_PMMU51_ROOT_INVALID;
}

bool amivm_pmmu51_set_access_level(struct amivm_cpu_state *cpu, uint8_t level)
{
    if (cpu == NULL || level > 7u) return false;
    cpu->pmmu_access_level = level;
    return true;
}

uint8_t amivm_pmmu51_get_access_level(const struct amivm_cpu_state *cpu)
{
    return cpu == NULL ? 0u : (uint8_t)(cpu->pmmu_access_level & 7u);
}

bool amivm_pmmu51_last_shared_globally(const struct amivm_cpu_state *cpu)
{
    return cpu != NULL && cpu->pmmu_last_shared_globally;
}

void amivm_pmmu51_atc_flush(struct amivm_cpu_state *cpu)
{
    if (cpu == NULL) return;
    memset(cpu->pmmu_atc, 0, sizeof cpu->pmmu_atc);
    cpu->pmmu_atc_next = 0u;
}

static void pmmu51_atc_flush_select(struct amivm_cpu_state *cpu,
                                    uint8_t function_code, uint8_t mask,
                                    bool include_shared, bool match_page,
                                    uint32_t logical_address)
{
    uint8_t fc = (uint8_t)(function_code & 7u);
    uint8_t m = (uint8_t)(mask & 7u);
    uint32_t page = logical_address & 0xfffff000u;
    if (cpu == NULL) return;
    for (unsigned i = 0; i < AMIVM_PMMU51_ATC_ENTRIES; ++i) {
        struct amivm_pmmu51_atc_entry *e = &cpu->pmmu_atc[i];
        if (e->valid && (include_shared || !e->shared_globally) &&
            (!match_page || e->logical_page == page) &&
            ((e->function_code & m) == (fc & m)))
            e->valid = false;
    }
}

void amivm_pmmu51_atc_flush_fc(struct amivm_cpu_state *cpu,
                               uint8_t function_code, uint8_t mask)
{
    pmmu51_atc_flush_select(cpu, function_code, mask, false, false, 0u);
}

void amivm_pmmu51_atc_flush_fc_page(struct amivm_cpu_state *cpu,
                                    uint8_t function_code, uint8_t mask,
                                    uint32_t logical_address)
{
    pmmu51_atc_flush_select(cpu, function_code, mask, false, true,
                            logical_address);
}

void amivm_pmmu51_atc_flushs_fc(struct amivm_cpu_state *cpu,
                                uint8_t function_code, uint8_t mask)
{
    pmmu51_atc_flush_select(cpu, function_code, mask, true, false, 0u);
}

void amivm_pmmu51_atc_flushs_fc_page(struct amivm_cpu_state *cpu,
                                     uint8_t function_code, uint8_t mask,
                                     uint32_t logical_address)
{
    pmmu51_atc_flush_select(cpu, function_code, mask, true, true,
                            logical_address);
}

static bool pmmu51_atc_lookup(struct amivm_cpu_state *cpu, uint32_t logical,
                              bool supervisor, uint32_t *physical)
{
    uint32_t page = logical & 0xfffff000u;
    uint8_t level = amivm_pmmu51_get_access_level(cpu);
    for (unsigned i = 0; i < AMIVM_PMMU51_ATC_ENTRIES; ++i) {
        struct amivm_pmmu51_atc_entry *e = &cpu->pmmu_atc[i];
        if (e->valid && e->logical_page == page &&
            (e->shared_globally ||
             (e->supervisor == supervisor && e->access_level == level))) {
            *physical = e->physical_page | (logical & 0xfffu);
            cpu->pmmu_last_shared_globally = e->shared_globally;
            return true;
        }
    }
    return false;
}

static void pmmu51_atc_fill(struct amivm_cpu_state *cpu, uint32_t logical,
                            uint32_t physical, bool supervisor)
{
    struct amivm_pmmu51_atc_entry *e =
        &cpu->pmmu_atc[cpu->pmmu_atc_next % AMIVM_PMMU51_ATC_ENTRIES];
    e->logical_page = logical & 0xfffff000u;
    e->physical_page = physical & 0xfffff000u;
    e->access_level = amivm_pmmu51_get_access_level(cpu);
    /* Until instruction/data access context is plumbed separately, use the
     * normal 68k data-space FC convention: user=1, supervisor=5. */
    e->function_code = supervisor ? 5u : 1u;
    e->supervisor = supervisor;
    e->shared_globally = cpu->pmmu_last_shared_globally;
    e->valid = true;
    cpu->pmmu_atc_next =
        (uint8_t)((cpu->pmmu_atc_next + 1u) % AMIVM_PMMU51_ATC_ENTRIES);
}

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

static int mmu_translate_table(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                               uint32_t logical, bool write, bool supervisor,
                               uint32_t *physical)
{    uint32_t root, l1, l2, l1_addr, l2_addr;\n    bool root_long = false;
    uint32_t i1 = logical >> 22u;
    uint32_t i2 = (logical >> 12u) & 0x3ffu;

    if (cpu == NULL || vm == NULL || physical == NULL) return AMIVM_MMU_FAULT_TABLE_BUS;
    cpu->mmusr = 0u;
    if (!active_profile(cpu)->has_mmu || (cpu->tc & TC_ENABLE) == 0u) {
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

static int mmu_translate_68851(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                               uint32_t logical, bool write, bool supervisor,
                               uint32_t *physical)
{
    uint32_t root, l1, l2, l1_addr, l2_addr;
    uint32_t i1 = logical >> 22u;
    uint32_t i2 = (logical >> 12u) & 0x3ffu;
    bool effective_shared = false;

    cpu->pmmu_psr = AMIVM_PMMU51_PSR_OK;
    cpu->pmmu_last_shared_globally = false;
    cpu->mmusr = 0u;
    if (pmmu51_atc_lookup(cpu, logical, supervisor, physical))
        return AMIVM_MMU_OK;
    {
        struct amivm_pmmu51_root rp;
        uint64_t raw = supervisor ? cpu->pmmu_srp : cpu->pmmu_crp;
        if (!amivm_pmmu51_decode_root(raw, &rp)) {
            cpu->pmmu_psr = AMIVM_PMMU51_PSR_ROOT;
            return AMIVM_MMU_FAULT_ROOT;
        }
        if ((!rp.lower_limit && i1 > rp.limit) ||
            (rp.lower_limit && i1 < rp.limit)) {
            cpu->pmmu_psr = AMIVM_PMMU51_PSR_LIMIT;
            return AMIVM_MMU_FAULT_LIMIT;
        }
        effective_shared = rp.shared_globally;
        if (rp.type == AMIVM_PMMU51_ROOT_PAGE) {
            /* Root page descriptor terminates the walk. At this scaffold's
             * 4 KiB page size the root address supplies the physical page. */
            cpu->pmmu_last_shared_globally = effective_shared;
            *physical = (rp.table_address & PMMU51_ADDR_MASK) |
                        (logical & 0xfffu);
            pmmu51_atc_fill(cpu, logical, *physical, supervisor);
            return AMIVM_MMU_OK;
        }
        if (rp.type != AMIVM_PMMU51_ROOT_TABLE_SHORT &&
            rp.type != AMIVM_PMMU51_ROOT_TABLE_LONG) {
            cpu->pmmu_psr = AMIVM_PMMU51_PSR_ROOT;
            return AMIVM_MMU_FAULT_ROOT;
        }
        root = rp.table_address & PMMU51_ADDR_MASK;
        root_long = rp.type == AMIVM_PMMU51_ROOT_TABLE_LONG;
        l1_addr = root + i1 * (root_long ? 8u : 4u);
    }
    if (root == 0u) {
        cpu->pmmu_psr = AMIVM_PMMU51_PSR_ROOT;
        return AMIVM_MMU_FAULT_ROOT;
    }

    if (!phys_read32(vm, l1_addr, &l1)) {
        cpu->pmmu_psr = AMIVM_PMMU51_PSR_TABLE_BUS;
        return AMIVM_MMU_FAULT_TABLE_BUS;
    }
    if (root_long) {
        uint32_t l1_low;
        struct amivm_pmmu51_long_descriptor ld;
        uint64_t raw;
        if (!phys_read32(vm, l1_addr + 4u, &l1_low)) {
            cpu->pmmu_psr = AMIVM_PMMU51_PSR_TABLE_BUS;
            return AMIVM_MMU_FAULT_TABLE_BUS;
        }
        raw = ((uint64_t)l1 << 32u) | l1_low;
        if (!amivm_pmmu51_decode_long_descriptor(raw, &ld) ||
            ld.type != AMIVM_PMMU51_ROOT_TABLE_SHORT) {
            cpu->pmmu_psr = AMIVM_PMMU51_PSR_ROOT;
            return AMIVM_MMU_FAULT_ROOT;
        }
        if ((!ld.lower_limit && i2 > ld.limit) ||
            (ld.lower_limit && i2 < ld.limit)) {
            cpu->pmmu_psr = AMIVM_PMMU51_PSR_LIMIT;
            return AMIVM_MMU_FAULT_LIMIT;
        }
        if (ld.supervisor_only && !supervisor) {
            cpu->pmmu_psr = AMIVM_PMMU51_PSR_SUPERVISOR;
            return AMIVM_MMU_FAULT_SUPERVISOR;
        }
        if (!write &&
            amivm_pmmu51_get_access_level(cpu) < ld.read_access_level) {
            cpu->pmmu_psr = AMIVM_PMMU51_PSR_READ_ACCESS;
            return AMIVM_MMU_FAULT_READ_ACCESS;
        }
        if (write &&
            amivm_pmmu51_get_access_level(cpu) < ld.write_access_level) {
            cpu->pmmu_psr = AMIVM_PMMU51_PSR_WRITE_ACCESS;
            return AMIVM_MMU_FAULT_WRITE_ACCESS;
        }
        effective_shared = effective_shared || ld.shared_globally;
        cpu->pmmu_last_shared_globally = effective_shared;
        l2_addr = (ld.table_address & PMMU51_ADDR_MASK) + i2 * 4u;
    } else {
        if ((l1 & PMMU51_DESC_TYPE_MASK) != PMMU51_DESC_TABLE) {
            cpu->pmmu_psr = AMIVM_PMMU51_PSR_ROOT;
            return AMIVM_MMU_FAULT_ROOT;
        }
        l2_addr = (l1 & PMMU51_ADDR_MASK) + i2 * 4u;
    }
    if (!phys_read32(vm, l2_addr, &l2)) {
        cpu->pmmu_psr = AMIVM_PMMU51_PSR_TABLE_BUS;
        return AMIVM_MMU_FAULT_TABLE_BUS;
    }
    if ((l2 & PMMU51_DESC_TYPE_MASK) != PMMU51_DESC_PAGE) {
        cpu->pmmu_psr = AMIVM_PMMU51_PSR_PAGE;
        return AMIVM_MMU_FAULT_PAGE;
    }
    if (write && (l2 & PMMU51_DESC_WP) != 0u) {
        cpu->pmmu_psr = AMIVM_PMMU51_PSR_WRITE_PROTECT;
        return AMIVM_MMU_FAULT_WRITE_PROTECT;
    }

    cpu->pmmu_last_shared_globally = effective_shared;
    *physical = (l2 & PMMU51_ADDR_MASK) | (logical & 0xfffu);
    pmmu51_atc_fill(cpu, logical, *physical, supervisor);
    return AMIVM_MMU_OK;
}

static int mmu_translate_68030(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                               uint32_t logical, bool write, bool supervisor,
                               uint32_t *physical)
{
    /* M2.84 dispatch boundary. Detailed 68030 table-walk semantics follow. */
    return mmu_translate_table(cpu, vm, logical, write, supervisor, physical);
}

static int mmu_translate_68040(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                               uint32_t logical, bool write, bool supervisor,
                               uint32_t *physical)
{
    return mmu_translate_table(cpu, vm, logical, write, supervisor, physical);
}

static int mmu_translate_68060(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                               uint32_t logical, bool write, bool supervisor,
                               uint32_t *physical)
{
    /* M2.84 dispatch boundary. Detailed 68060 status semantics follow. */
    return mmu_translate_table(cpu, vm, logical, write, supervisor, physical);
}

int amivm_mmu_translate(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                        uint32_t logical, bool write, bool supervisor,
                        uint32_t *physical)
{
    const struct amivm_cpu_profile *profile;
    if (cpu == NULL || vm == NULL || physical == NULL)
        return AMIVM_MMU_FAULT_TABLE_BUS;

    profile = active_profile(cpu);
    if (!profile->has_mmu || profile->mmu_model == AMIVM_MMU_NONE) {
        cpu->mmusr = 0u;
        *physical = logical;
        return AMIVM_MMU_OK;
    }
    if (profile->mmu_model == AMIVM_MMU_68851) {
        if ((cpu->pmmu_tc & TC_ENABLE) == 0u) {
            cpu->pmmu_psr = 0u;
            cpu->mmusr = 0u;
            *physical = logical;
            return AMIVM_MMU_OK;
        }
    } else if ((cpu->tc & TC_ENABLE) == 0u) {
        cpu->mmusr = 0u;
        *physical = logical;
        return AMIVM_MMU_OK;
    }

    switch (profile->mmu_model) {
    case AMIVM_MMU_68851:
        return mmu_translate_68851(cpu, vm, logical, write, supervisor, physical);
    case AMIVM_MMU_68030:
        return mmu_translate_68030(cpu, vm, logical, write, supervisor, physical);
    case AMIVM_MMU_68040:
        return mmu_translate_68040(cpu, vm, logical, write, supervisor, physical);
    case AMIVM_MMU_68060:
        return mmu_translate_68060(cpu, vm, logical, write, supervisor, physical);
    case AMIVM_MMU_NONE:
    default:
        cpu->mmusr = 0u;
        *physical = logical;
        return AMIVM_MMU_OK;
    }
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

static bool cpu_read64(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                       uint32_t addr, bool supervisor, uint64_t *value)
{
    uint32_t hi, lo;
    if (value == NULL || !cpu_read32(cpu, vm, addr, supervisor, &hi) ||
        !cpu_read32(cpu, vm, addr + 4u, supervisor, &lo)) return false;
    *value = ((uint64_t)hi << 32u) | lo;
    return true;
}

static bool cpu_write64(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                        uint32_t addr, bool supervisor, uint64_t value)
{
    return cpu_write32(cpu, vm, addr, supervisor, (uint32_t)(value >> 32u)) &&
           cpu_write32(cpu, vm, addr + 4u, supervisor, (uint32_t)value);
}

static void set_nz32(struct amivm_cpu_state *cpu, uint32_t value)
{
    cpu->sr &= (uint16_t)~(SR_N | SR_Z | SR_V | SR_C);
    if (value == 0u) cpu->sr |= SR_Z;
    if ((value & 0x80000000u) != 0u) cpu->sr |= SR_N;
}

static bool read_cr(const struct amivm_cpu_state *cpu, uint16_t cr, uint32_t *value)
{
    const struct amivm_cpu_profile *profile = active_profile(cpu);
    if (value == NULL) return false;
    if (!profile->has_master_stack && (cr == AMIVM_CR_MSP || cr == AMIVM_CR_ISP))
        return false;
    if (!profile->has_mmu &&
        (cr == AMIVM_CR_TC || cr == AMIVM_CR_MMUSR ||
         cr == AMIVM_CR_URP || cr == AMIVM_CR_SRP)) return false;
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
    const struct amivm_cpu_profile *profile = active_profile(cpu);
    if (!profile->has_master_stack && (cr == AMIVM_CR_MSP || cr == AMIVM_CR_ISP))
        return false;
    if (!profile->has_mmu &&
        (cr == AMIVM_CR_TC || cr == AMIVM_CR_URP || cr == AMIVM_CR_SRP))
        return false;
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
    if (cpu->profile == NULL) cpu->profile = amivm_cpu_profile_default();
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

static int execute_pmmu_68851(struct amivm_cpu_state *cpu, struct amivm_vm *vm,
                                uint32_t instruction_pc, uint32_t next_pc,
                                uint16_t opcode)
{
    uint16_t ext;
    unsigned mode = (unsigned)((opcode >> 3u) & 7u);
    unsigned reg = (unsigned)(opcode & 7u);
    unsigned preg;
    bool pmmu_to_ea;
    uint32_t value;
    uint64_t root_value;
    enum amivm_pmmu51_register root_reg;

    if (!is_supervisor(cpu)) {
        set_fault(cpu, AMIVM_CPU_FAULT_PRIVILEGE, instruction_pc, opcode);
        return deliver_fault(cpu, vm, instruction_pc);
    }
    if (active_profile(cpu)->mmu_model != AMIVM_MMU_68851 ||
        fetch16(cpu, vm, next_pc, &ext) != 0) {
        set_fault(cpu, AMIVM_CPU_FAULT_ILLEGAL, instruction_pc, opcode);
        return deliver_fault(cpu, vm, instruction_pc);
    }

    /* MC68851 PFLUSHA is the exact F000/2400 form.  It invalidates the
     * complete address translation cache and has no effective address. */
    if (opcode == OP_PMMU_BASE && ext == 0x2400u) {
        amivm_pmmu51_atc_flush(cpu);
        cpu->pc = next_pc + 2u;
        return 1;
    }

    /* MC68851 PFLUSH FC,MASK without an effective address.
     * Command word: 001 100 0 MASK FC.  FC encodings are SFC (00000),
     * DFC (00001), Dn (01RRR), or immediate (1DDDD). */
    if (opcode == OP_PMMU_BASE && (ext & 0xf800u) == 0x3000u) {
        uint8_t mask = (uint8_t)((ext >> 5u) & 0x0fu);
        uint8_t fc_field = (uint8_t)(ext & 0x1fu);
        uint8_t fc;
        if (fc_field == 0u) fc = (uint8_t)(cpu->sfc & 7u);
        else if (fc_field == 1u) fc = (uint8_t)(cpu->dfc & 7u);
        else if ((fc_field & 0x18u) == 0x08u)
            fc = (uint8_t)(cpu->d[fc_field & 7u] & 0x0fu);
        else if ((fc_field & 0x10u) != 0u)
            fc = (uint8_t)(fc_field & 0x0fu);
        else goto illegal;
        amivm_pmmu51_atc_flush_fc(cpu, fc, mask);
        cpu->pc = next_pc + 2u;
        return 1;
    }

    /* M2.111: MC68851 PFLUSH FC,MASK,<ea>, initially (An) only. */
    if (opcode == OP_PMMU_BASE && (ext & 0xf800u) == 0x3800u) {
        uint8_t mask = (uint8_t)((ext >> 5u) & 0x0fu);
        uint8_t fc_field = (uint8_t)(ext & 0x1fu);
        uint8_t fc;
        if (fc_field == 0u) fc = (uint8_t)(cpu->sfc & 7u);
        else if (fc_field == 1u) fc = (uint8_t)(cpu->dfc & 7u);
        else if ((fc_field & 0x18u) == 0x08u)
            fc = (uint8_t)(cpu->d[fc_field & 7u] & 0x0fu);
        else if ((fc_field & 0x10u) != 0u)
            fc = (uint8_t)(fc_field & 0x0fu);
        else goto illegal;
        {
            uint32_t address;
            uint32_t end_pc = next_pc + 2u;
            if (mode == 2u) {
                address = cpu->a[reg];
            } else if (mode == 5u) {
                uint16_t displacement;
                if (fetch16(cpu, vm, end_pc, &displacement) != 0)
                    return deliver_fault(cpu, vm, instruction_pc);
                address = (uint32_t)((int64_t)cpu->a[reg] + (int16_t)displacement);
                end_pc += 2u;
            } else if (mode == 6u) {
                uint16_t words[5];
                struct amivm_ea_index_extension parsed;
                size_t available = 1u;
                if (fetch16(cpu, vm, end_pc, &words[0]) != 0)
                    return deliver_fault(cpu, vm, instruction_pc);
                if ((words[0] & 0x0100u) == 0u) {
                    if (amivm_ea_resolve_brief_index(cpu->a[reg], words[0],
                                                     cpu->d, cpu->a, &address) !=
                        AMIVM_EA_PARSE_OK)
                        goto illegal;
                    end_pc += 2u;
                } else {
                    uint8_t bd_code = (uint8_t)((words[0] >> 4u) & 3u);
                    uint8_t iis = (uint8_t)(words[0] & 7u);
                    size_t bd_words = bd_code == 2u ? 1u : (bd_code == 3u ? 2u : 0u);
                    size_t od_words = (iis == 2u || iis == 6u) ? 1u :
                                      ((iis == 3u || iis == 7u) ? 2u : 0u);
                    available = 1u + bd_words + od_words;
                    for (size_t wi = 1u; wi < available; ++wi)
                        if (fetch16(cpu, vm, end_pc + (uint32_t)(wi * 2u),
                                    &words[wi]) != 0)
                            return deliver_fault(cpu, vm, instruction_pc);
                    if (amivm_ea_parse_index_extension(words, available, &parsed) !=
                        AMIVM_EA_PARSE_OK)
                        goto illegal;
                    if (parsed.indirect_mode == AMIVM_EA_INDIRECT_NONE) {
                        if (amivm_ea_resolve_full_index(cpu->a[reg], &parsed,
                                                       cpu->d, cpu->a, &address) !=
                            AMIVM_EA_PARSE_OK)
                            goto illegal;
                    } else {
                        uint32_t raw;
                        int32_t index = 0;
                        uint32_t indirect;
                        int64_t base_value = parsed.base_suppress ? 0 : (int64_t)cpu->a[reg];
                        if (!parsed.index_suppress) {
                            raw = parsed.index_is_addr ? cpu->a[parsed.index_reg]
                                                       : cpu->d[parsed.index_reg];
                            index = parsed.index_long ? (int32_t)raw
                                                      : (int16_t)(raw & 0xffffu);
                        }
                        base_value += parsed.base_displacement;
                        if (parsed.indirect_mode == AMIVM_EA_INDIRECT_PREINDEXED)
                            base_value += (int64_t)index * parsed.index_scale;
                        if (!cpu_read32(cpu, vm, (uint32_t)base_value, true, &indirect))
                            return deliver_fault(cpu, vm, instruction_pc);
                        address = indirect;
                        if (parsed.indirect_mode == AMIVM_EA_INDIRECT_POSTINDEXED)
                            address = (uint32_t)((int64_t)address +
                                      (int64_t)index * parsed.index_scale);
                        address = (uint32_t)((int64_t)address +
                                  parsed.outer_displacement);
                    }
                    end_pc += (uint32_t)(parsed.words_consumed * 2u);
                }
            } else {
                goto illegal;
            }
            amivm_pmmu51_atc_flush_fc_page(cpu, fc, mask, address);
            cpu->pc = end_pc;
            return 1;
        }
    }

    if ((ext & 0xe1ffu) != 0x4000u || (mode != 0u && mode != 2u)) {
        set_fault(cpu, AMIVM_CPU_FAULT_ILLEGAL, instruction_pc, opcode);
        return deliver_fault(cpu, vm, instruction_pc);
    }

    preg = (unsigned)((ext >> 10u) & 7u);
    pmmu_to_ea = (ext & 0x0200u) != 0u;

    if (preg == 0u) {
        if (pmmu_to_ea) {
            if (!amivm_pmmu51_read_register(cpu, AMIVM_PMMU51_REG_TC, &value)) goto illegal;
            if (mode == 0u) cpu->d[reg] = value;
            else if (!cpu_write32(cpu, vm, cpu->a[reg], true, value))
                return deliver_fault(cpu, vm, instruction_pc);
        } else {
            if (mode == 0u) value = cpu->d[reg];
            else if (!cpu_read32(cpu, vm, cpu->a[reg], true, &value))
                return deliver_fault(cpu, vm, instruction_pc);
            if (!amivm_pmmu51_write_register(cpu, AMIVM_PMMU51_REG_TC, value)) goto illegal;
        }
    } else if (preg == 2u || preg == 3u) {
        if (mode != 2u) goto illegal;
        root_reg = preg == 2u ? AMIVM_PMMU51_REG_SRP : AMIVM_PMMU51_REG_CRP;
        if (pmmu_to_ea) {
            if (!amivm_pmmu51_read_root(cpu, root_reg, &root_value)) goto illegal;
            if (!cpu_write64(cpu, vm, cpu->a[reg], true, root_value))
                return deliver_fault(cpu, vm, instruction_pc);
        } else {
            if (!cpu_read64(cpu, vm, cpu->a[reg], true, &root_value))
                return deliver_fault(cpu, vm, instruction_pc);
            if (!amivm_pmmu51_write_root(cpu, root_reg, root_value)) goto illegal;
        }
    } else {
        goto illegal;
    }

    cpu->pc = next_pc + 2u;
    return 1;

illegal:
    set_fault(cpu, AMIVM_CPU_FAULT_ILLEGAL, instruction_pc, opcode);
    return deliver_fault(cpu, vm, instruction_pc);
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
    if ((opcode & 0xffc0u) == OP_PMMU_BASE)
        return execute_pmmu_68851(cpu, vm, instruction_pc, next_pc, opcode);
    if (opcode == OP_FPU_GEN) {
        if (!active_profile(cpu)->has_fpu) {
            set_fault(cpu, AMIVM_CPU_FAULT_ILLEGAL, instruction_pc, opcode);
            return deliver_fault(cpu, vm, instruction_pc);
        }
        return execute_fpu_register(cpu, vm, instruction_pc, next_pc, opcode);
    }

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
