#include "vm.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define AMIVM_DEVF_AGA  0x01u
#define AMIVM_DEVF_IDE  0x02u
#define AMIVM_DEVF_ZORRO 0x04u

static bool device_aga_read8(struct amivm_vm *, const struct amivm_device_state *, uint32_t, uint8_t *);
static bool device_aga_write8(struct amivm_vm *, const struct amivm_device_state *, uint32_t, uint8_t);
static bool device_ide_read8(struct amivm_vm *, const struct amivm_device_state *, uint32_t, uint8_t *);
static bool device_ide_write8(struct amivm_vm *, const struct amivm_device_state *, uint32_t, uint8_t);
static bool device_zorro_read8(struct amivm_vm *, const struct amivm_device_state *, uint32_t, uint8_t *);
static bool device_zorro_write8(struct amivm_vm *, const struct amivm_device_state *, uint32_t, uint8_t);

static const struct amivm_device_desc amivm_devices[] = {
    {"vmserial", AMIVM_VMSERIAL_BASE, AMIVM_MMIO_PAGE_SIZE, 1u, 0u, NULL, NULL},
    {"irq", AMIVM_IRQ_BASE, AMIVM_MMIO_PAGE_SIZE, 0u, 0u, NULL, NULL},
    {"trackdisk", AMIVM_TRACKDISK_BASE, AMIVM_MMIO_PAGE_SIZE, 3u, 0u, NULL, NULL},
    {"timer", AMIVM_TIMER_BASE, AMIVM_MMIO_PAGE_SIZE, 6u, 0u, NULL, NULL},
    {"aga", AMIVM_MMIO_BASE + 0x5000u, AMIVM_MMIO_PAGE_SIZE, 7u, AMIVM_DEVF_AGA,
     device_aga_read8, device_aga_write8},
    {"ide", AMIVM_MMIO_BASE + 0x6000u, AMIVM_MMIO_PAGE_SIZE, 8u, AMIVM_DEVF_IDE,
     device_ide_read8, device_ide_write8},
    {"zorro", AMIVM_MMIO_BASE + 0x7000u, AMIVM_MMIO_PAGE_SIZE, 9u, AMIVM_DEVF_ZORRO,
     device_zorro_read8, device_zorro_write8},
};

static bool in_range(uint32_t addr, uint32_t base, size_t size)
{
    uint64_t a = addr;
    uint64_t b = base;
    uint64_t e = b + size;
    return a >= b && a < e;
}

static bool amivm_device_present(const struct amivm_vm *vm,
                                      const struct amivm_device_desc *desc)
{
    size_t i;
    if (!vm || !desc) return false;
    for (i = 0; i < vm->device_count; ++i)
        if (vm->devices[i].desc == desc &&
            vm->devices[i].instantiated &&
            vm->devices[i].enabled)
            return true;
    return false;
}

static const struct amivm_device_desc *amivm_device_for_address(
    const struct amivm_vm *vm, uint32_t addr)
{
    size_t i;
    if (!vm) return NULL;
    for (i = 0; i < vm->device_count; ++i) {
        const struct amivm_device_desc *d = vm->devices[i].desc;
        if (vm->devices[i].instantiated && vm->devices[i].enabled &&
            in_range(addr, d->base, d->size))
            return d;
    }
    return NULL;
}

static bool device_aga_read8(struct amivm_vm *vm, const struct amivm_device_state *state,
                                  uint32_t offset, uint8_t *value)
{
    (void)state;
    if (!vm || !value) return false;
    switch (offset) {
    case 0u: *value = (uint8_t)(vm->aga.bplcon0 >> 8); return true;
    case 1u: *value = (uint8_t)vm->aga.bplcon0; return true;
    case 2u: *value = (uint8_t)(vm->aga.diwstrt >> 8); return true;
    case 3u: *value = (uint8_t)vm->aga.diwstrt; return true;
    case 4u: *value = (uint8_t)(vm->aga.diwstop >> 8); return true;
    case 5u: *value = (uint8_t)vm->aga.diwstop; return true;
    case 6u: *value = (uint8_t)(vm->aga.dmaconr >> 8); return true;
    case 7u: *value = (uint8_t)vm->aga.dmaconr; return true;
    case 8u: *value = (uint8_t)(vm->aga.beam_h >> 8); return true;
    case 9u: *value = (uint8_t)vm->aga.beam_h; return true;
    case 10u: *value = (uint8_t)(vm->aga.beam_v >> 8); return true;
    case 11u: *value = (uint8_t)vm->aga.beam_v; return true;
    case 12u: *value = vm->aga.display_active ? 1u : 0u; return true;
    case 13u: *value = vm->aga.vblank_irq_enable ? 1u : 0u; return true;
    default: return false;
    }
}

static bool device_aga_write8(struct amivm_vm *vm, const struct amivm_device_state *state,
                                   uint32_t offset, uint8_t value)
{
    (void)state;
    if (!vm) return false;
    switch (offset) {
    case 0u: vm->aga.bplcon0 = (uint16_t)value << 8; return true;
    case 1u: vm->aga.bplcon0 = (vm->aga.bplcon0 & 0xff00u) | value; return true;
    case 2u: vm->aga.diwstrt = (uint16_t)value << 8; return true;
    case 3u: vm->aga.diwstrt = (vm->aga.diwstrt & 0xff00u) | value; return true;
    case 4u: vm->aga.diwstop = (uint16_t)value << 8; return true;
    case 5u: vm->aga.diwstop = (vm->aga.diwstop & 0xff00u) | value; return true;
    case 6u:
        vm->aga.dmacon = value;
        vm->aga.dmaconr = (uint16_t)value;
        return true;
    case 7u:
        vm->aga.dmacon = (vm->aga.dmacon & 0xff00u) | value;
        vm->aga.dmaconr = vm->aga.dmacon;
        return true;
    case 13u:
        vm->aga.vblank_irq_enable = value != 0u;
        return true;
    default: return false;
    }
}

static bool device_ide_read8(struct amivm_vm *vm, const struct amivm_device_state *state,
                                  uint32_t offset, uint8_t *value)
{
    (void)state;
    if (!vm || !value) return false;
    switch (offset) {
    case 0u: *value = vm->ide.status; return true;
    case 1u: *value = vm->ide.command; return true;
    case 2u: *value = (uint8_t)(vm->ide.lba >> 24); return true;
    case 3u: *value = (uint8_t)(vm->ide.lba >> 16); return true;
    case 4u: *value = (uint8_t)(vm->ide.lba >> 8); return true;
    case 5u: *value = (uint8_t)vm->ide.lba; return true;
    default: return false;
    }
}

static bool device_ide_write8(struct amivm_vm *vm, const struct amivm_device_state *state,
                                   uint32_t offset, uint8_t value)
{
    (void)state;
    if (!vm) return false;
    switch (offset) {
    case 0u: vm->ide.command = value; return true;
    case 2u: vm->ide.lba = (vm->ide.lba & 0x00ffffffu) | ((uint32_t)value << 24); return true;
    case 3u: vm->ide.lba = (vm->ide.lba & 0xff00ffffu) | ((uint32_t)value << 16); return true;
    case 4u: vm->ide.lba = (vm->ide.lba & 0xffff00ffu) | ((uint32_t)value << 8); return true;
    case 5u: vm->ide.lba = (vm->ide.lba & 0xffffff00u) | value; return true;
    default: return false;
    }
}

static bool device_zorro_read8(struct amivm_vm *vm, const struct amivm_device_state *state,
                                    uint32_t offset, uint8_t *value)
{
    (void)state;
    (void)offset;
    if (!vm || !value) return false;
    *value = vm->zorro.configured;
    return true;
}

static bool device_zorro_write8(struct amivm_vm *vm, const struct amivm_device_state *state,
                                     uint32_t offset, uint8_t value)
{
    (void)state;
    (void)offset;
    if (!vm) return false;
    vm->zorro.configured = value;
    return true;
}

void amivm_m68k_request_irq(struct amivm_vm *vm, uint8_t level)
{
    if (!vm || level < 1u || level > 7u) return;
    if (!vm->irq_pending || level > vm->irq_level)
        vm->irq_level = level;
    vm->irq_pending = true;
}

static int amivm_m68k_enter_mmu_exception(struct amivm_vm *vm)
{
    uint32_t handler;
    bool ok;
    if (!vm || vm->exception_vector_base == 0u) return -1;
    if (amivm_m68k_stack_mmu_exception(vm) != 0) {
        if (vm->exception_depth != 0u) {
            vm->exception_double_fault = true;
            vm->exception_halted = true;
        }
        return -1;
    }
    handler = amivm_m68k_read_u32(vm,
                                  vm->exception_vector_base + 56u * 4u, &ok);
    if (!ok) return -1;
    vm->m68k.sr |= 0x2000u;
    vm->pending_exception_vector = 56u;
    vm->m68k.pc = handler;
    return 0;
}

int amivm_m68k_exception_enter(struct amivm_vm *vm, uint8_t vector)
{
    uint32_t vector_addr;
    uint32_t handler;
    bool ok;
    if (!vm || vector >= 256u) return -1;
    vector_addr = vm->exception_vector_base + (uint32_t)vector * 4u;
    handler = amivm_m68k_read_u32(vm, vector_addr, &ok);
    if (!ok) return -1;
    vm->m68k.pc = handler;
    vm->m68k.sr |= 0x2000u;
    vm->pending_exception_vector = vector;
    return 0;
}

int amivm_m68k_rte_mmu_exception(struct amivm_vm *vm)
{
    const struct amivm_m68k_frame_layout *layout;
    uint32_t sp;
    uint32_t pc;
    uint32_t address;
    uint32_t status;
    uint8_t hi, lo;
    if (!vm) return -1;
    layout = amivm_m68k_frame_layout(vm->exception_frame_type);
    if (!layout || !layout->has_fault_address || !layout->has_fault_status ||
        vm->exception_frame_size != (uint8_t)(layout->words * 2u))
        return -1;
    sp = vm->m68k.a[7];
    if (!amivm_m68k_read_u32(vm, sp, &pc) ||
        !amivm_read8(vm, sp + 4u, &hi) ||
        !amivm_read8(vm, sp + 5u, &lo) ||
        !amivm_m68k_read_u32(vm, sp + layout->fault_address_offset, &address))
        return -1;
    if (layout->fault_status_bytes == 2u) {
        uint8_t shi, slo;
        if (!amivm_read8(vm, sp + layout->fault_status_offset, &shi) ||
            !amivm_read8(vm, sp + layout->fault_status_offset + 1u, &slo))
            return -1;
        status = ((uint32_t)shi << 8) | slo;
    } else {
        bool status_ok = false;
        status = amivm_m68k_read_u32(vm, sp + layout->fault_status_offset, &status_ok);
        if (!status_ok) return -1;
    }
    vm->m68k.pc = pc;
    vm->m68k.sr = (uint16_t)(((uint16_t)hi << 8) | lo);
    vm->mmu_fault_address = address;
    vm->mmu_fault_status = status;
    if (vm->exception_frame_type == AMIVM_FRAME_68060_ACCESS)
        vm->exception_fslw = status;
    vm->m68k.a[7] = sp + (uint32_t)(layout->words * 2u);
    vm->exception_frame_sp = 0u;
    vm->exception_frame_size = 0u;
    vm->exception_frame_type = AMIVM_FRAME_68000_SHORT;
    vm->exception_bus_fault_in_progress = false;
    vm->exception_frame_format = 0u;
    vm->exception_frame_word_count = 0u;
    if (vm->exception_depth != 0u)
        vm->exception_depth--;
    vm->pending_exception = AMIVM_M68K_EXC_NONE;
    vm->m68k.exception = AMIVM_M68K_EXC_NONE;
    return 0;
}

const struct amivm_m68k_frame_layout *amivm_m68k_frame_layout(uint8_t descriptor)
{
    static const struct amivm_m68k_frame_layout layouts[] = {
        { 4u,  false, false, false, 0u, 0u, 0u, 0u, 0u, 0u },
        { 6u,  true,  false, false, 6u, 0u, 0u, 0u, 0u, 0u },
        { 8u,  true,  true,  true,  6u, 8u, 12u, 4u, 0u, 0u },
        { 10u, true,  false, false, 6u, 0u, 0u, 0u, 8u, 2u },
        { 16u, true, true,  false, 6u, 16u, 0u, 0u, 8u, 4u },
        { 46u, true, true,  false, 6u, 16u, 0u, 0u, 8u, 34u },
        { 8u,  true, true,  true,  6u, 8u, 12u, 4u, 0u, 0u },
        { 30u, true, true,  false, 6u, 20u, 10u, 2u, 8u, 8u }
    };
    if (descriptor > AMIVM_FRAME_68040_ACCESS) return NULL;
    return &layouts[descriptor];
}

static int amivm_m68k_frame_descriptor_for_format(uint8_t format)
{
    switch (format) {
    case 0x0u: return AMIVM_FRAME_68000_SHORT;
    case 0x2u: return AMIVM_FRAME_68020_BUS;
    case 0x2u: return AMIVM_FRAME_68040_MMU;
    case 0x9u: return AMIVM_FRAME_68030_COPROC_MID;
    case 0xAu: return AMIVM_FRAME_68030_SHORT_BUS;
    case 0xBu: return AMIVM_FRAME_68030_LONG_BUS;
    case 0x4u: return AMIVM_FRAME_68060_ACCESS;
    case 0x7u: return AMIVM_FRAME_68040_ACCESS;
    default: return -1;
    }
}

static uint8_t amivm_m68k_exception_frame_descriptor(const struct amivm_vm *vm)
{
    if (!vm) return AMIVM_FRAME_68000_SHORT;
    if (vm->cpu_profile.id == AMIVM_CPU_68030) {
        return vm->exception_bus_fault_in_progress ?
            AMIVM_FRAME_68030_LONG_BUS : AMIVM_FRAME_68030_SHORT_BUS;
    }
    if (vm->cpu_profile.id == AMIVM_CPU_68040 ||
        vm->cpu_profile.id == AMIVM_CPU_HYPER040)
        return AMIVM_FRAME_68040_ACCESS;
    if (vm->cpu_profile.id == AMIVM_CPU_68060 ||
        vm->cpu_profile.id == AMIVM_CPU_HYPER060)
        return AMIVM_FRAME_68060_ACCESS;
    return (uint8_t)vm->exception_frame_class;
}

int amivm_m68k_set_cpu_profile(struct amivm_vm *vm, uint8_t model, uint8_t submodel)
{
    if (!vm || model > 5u) return -1;
    vm->cpu_model = model;
    vm->cpu_submodel = submodel;
    {
        const struct amivm_cpu_profile *profile =
            amivm_cpu_profile_by_id((enum amivm_cpu_profile_id)model);
        if (!profile || profile->default_exception_frame_class > 2u)
            return -1;
        vm->exception_frame_class =
            (uint8_t)profile->default_exception_frame_class;
    }
    return 0;
}

int amivm_m68k_set_cpu_model(struct amivm_vm *vm, uint8_t model)
{
    if (!vm || model > 4u) return -1;
    vm->cpu_model = model;
    return 0;
}

int amivm_m68k_validate_exception_frame(struct amivm_vm *vm)
{
    const struct amivm_m68k_frame_layout *layout;
    if (!vm) return -1;
    layout = amivm_m68k_frame_layout(vm->exception_frame_format);
    if (!layout || vm->exception_frame_size == 0u ||
        vm->exception_frame_word_count == 0u ||
        vm->exception_frame_size !=
            (uint8_t)(vm->exception_frame_word_count * 2u))
        return -1;
    if (vm->exception_frame_format > 2u ||
        vm->exception_frame_word_count != layout->words ||
        vm->exception_frame_magic != 0x45584632u)
        return -1;
    return 0;
}

int amivm_m68k_set_exception_frame_type(struct amivm_vm *vm, uint8_t type)
{
    if (!vm || type > 2u) return -1;
    vm->exception_frame_type = type;
    return 0;
}

int amivm_m68k_enter_trap(struct amivm_vm *vm, uint8_t vector)
{
    if (!vm || vector < 32u || vector > 47u) return -1;
    vm->m68k.exception = AMIVM_M68K_EXC_TRAP;
    if (amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_TRAP) != 0)
        return -1;
    return amivm_m68k_enter_exception(vm, vector);
}

int amivm_m68k_enter_exception(struct amivm_vm *vm, uint8_t vector)
{
    uint32_t old_sp, old_pc, old_sr;
    if (!vm || vm->exception_entry_active) return -1;
    old_sp = vm->m68k.a[7];
    old_pc = vm->m68k.pc;
    old_sr = vm->m68k.sr;
    vm->exception_entry_active = true;
    vm->exception_entry_vector = vector;
    if (amivm_m68k_stack_mmu_exception(vm) != 0 ||
        amivm_m68k_enter_exception_handler(vm, vector) != 0) {
        vm->m68k.a[7] = old_sp;
        vm->m68k.pc = old_pc;
        vm->m68k.sr = (uint16_t)old_sr;
        vm->exception_entry_active = false;
        vm->m68k.stopped = true;
        return -1;
    }
    vm->m68k.sr |= 0x2000u;
    vm->exception_entry_active = false;
    return 0;
}

int amivm_m68k_set_exception_vector_base(struct amivm_vm *vm, uint32_t base)
{
    if (!vm || (base & 3u) != 0u) return -1;
    vm->exception_vector_base = base;
    return 0;
}

int amivm_m68k_enter_exception_handler(struct amivm_vm *vm, uint8_t vector)
{
    uint32_t entry;
    uint8_t b0, b1, b2, b3;
    if (!vm) return -1;
    entry = vm->exception_vector_base + ((uint32_t)vector * 4u);
    if (!amivm_read8(vm, entry, &b0) || !amivm_read8(vm, entry + 1u, &b1) ||
        !amivm_read8(vm, entry + 2u, &b2) || !amivm_read8(vm, entry + 3u, &b3))
        return -1;
    vm->exception_handler_pc = ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) |
                               ((uint32_t)b2 << 8) | b3;
    vm->m68k.pc = vm->exception_handler_pc;
    return 0;
}

int amivm_m68k_stack_mmu_exception(struct amivm_vm *vm)
{
    const struct amivm_m68k_frame_layout *layout;
    uint32_t sp;
    uint8_t frame_size;

    if (!vm) return -1;

    vm->exception_frame_type = amivm_m68k_exception_frame_descriptor(vm);
    layout = amivm_m68k_frame_layout(vm->exception_frame_type);
    if (!layout) return -1;
    frame_size = (uint8_t)(layout->words * 2u);

    sp = vm->m68k.a[7];
    vm->exception_stack_fault = false;
    if (sp < frame_size) {
        vm->exception_stack_fault = true;
        return -1;
    }
    sp -= frame_size;

    if (!amivm_m68k_write_u32(vm, sp, vm->mmu_exception_pc) ||
        !amivm_write8(vm, sp + 4u, (uint8_t)(vm->mmu_exception_sr >> 8)) ||
        !amivm_write8(vm, sp + 5u, (uint8_t)vm->mmu_exception_sr)) {
        vm->exception_stack_fault = true;
        return -1;
    }

    vm->m68k.a[7] = sp;
    vm->exception_frame_sp = sp;
    vm->exception_frame_format = (vm->exception_frame_type == AMIVM_FRAME_68030_SHORT_BUS) ? 0xAu :
        (vm->exception_frame_type == AMIVM_FRAME_68040_ACCESS) ? 0x7u :
        (vm->exception_frame_type == AMIVM_FRAME_68060_ACCESS) ? 0x4u :
        vm->exception_frame_class;
    vm->exception_frame_word_count = layout->words;
    vm->exception_frame_size = frame_size;
    vm->exception_frame_magic = 0x45584632u;
    vm->exception_frame_vector_offset =
        (uint16_t)(vm->exception_entry_vector * 4u);
    vm->exception_format_vector_word =
        (uint16_t)(((uint16_t)vm->exception_frame_format << 12) |
                   (vm->exception_entry_vector & 0x0fffu));
    amivm_m68k_capture_exception_internal_state(vm, layout);
    if (vm->exception_frame_type == AMIVM_FRAME_68030_SHORT_BUS)
        amivm_m68k_capture_030_short_state(vm);
    vm->exception_fault_stage =
        (vm->exception_frame_type == 2u) ? 1u : 0u;
    if (vm->exception_frame_type == AMIVM_FRAME_68060_ACCESS) {
        vm->exception_060_access_address = vm->exception_fault_address;
        vm->exception_fslw = vm->exception_fault_status;
    }

    if (amivm_m68k_write_exception_internal_state(vm, layout, sp) != 0) {
        vm->exception_stack_fault = true;
        return -1;
    }

    if (layout->has_format_vector) {
        if (!amivm_write8(vm, sp + 6u,
                          (uint8_t)(vm->exception_format_vector_word >> 8)) ||
            !amivm_write8(vm, sp + 7u,
                          (uint8_t)vm->exception_format_vector_word)) {
            vm->exception_stack_fault = true;
            return -1;
        }
    }

    if (vm->exception_frame_type == AMIVM_FRAME_68060_ACCESS)
        vm->exception_fault_address = vm->exception_060_access_address;

    if (layout->has_fault_address) {
        if (!amivm_m68k_write_u32(vm, sp + layout->fault_address_offset,
                                  vm->exception_fault_address)) {
            vm->exception_stack_fault = true;
            return -1;
        }
    }
    if (layout->has_fault_status) {
        if (layout->fault_status_bytes == 2u) {
            if (!amivm_write8(vm, sp + layout->fault_status_offset,
                              (uint8_t)(vm->exception_fault_status >> 8)) ||
                !amivm_write8(vm, sp + layout->fault_status_offset + 1u,
                              (uint8_t)vm->exception_fault_status)) {
                vm->exception_stack_fault = true;
                return -1;
            }
        } else if (!amivm_m68k_write_u32(vm, sp + layout->fault_status_offset,
                                         vm->exception_fault_status)) {
            vm->exception_stack_fault = true;
            return -1;
        }
    }

    if (amivm_m68k_decode_frame_format(vm,
                                       vm->exception_format_vector_word) != 0)
        return -1;

    if (vm->exception_frame_type == 0u ||
        vm->exception_frame_type == 1u) {
        vm->exception_fault_stage = 0u;
    }

    if (vm->exception_depth < 255u)
        vm->exception_depth++;
    return 0;
}

static void amivm_m68k_capture_exception_internal_state(struct amivm_vm *vm,
                                                               const struct amivm_m68k_frame_layout *layout)
{
    uint8_t i;
    if (!vm || !layout || layout->internal_state_words == 0u) return;
    for (i = 0u; i < layout->internal_state_words && i < 34u; ++i)
        vm->exception_internal_state[i] =
            (uint32_t)(vm->m68k.pc + (uint32_t)i * 4u);
}

static void amivm_m68k_capture_030_short_state(struct amivm_vm *vm)
{
    if (!vm) return;
    vm->exception_internal_state[0] = vm->m68k.pc;
    vm->exception_internal_state[1] = vm->m68k.sr;
    vm->exception_internal_state[2] = vm->exception_fault_address;
    vm->exception_internal_state[3] = vm->exception_fault_status;
}

static int amivm_m68k_write_exception_internal_state(struct amivm_vm *vm,
                                                                  const struct amivm_m68k_frame_layout *layout,
                                                                  uint32_t sp)
{
    uint8_t i;
    if (!vm || !layout || layout->internal_state_words == 0u) return 0;
    for (i = 0u; i < layout->internal_state_words && i < 34u; ++i) {
        if (!amivm_m68k_write_u32(vm,
                                  sp + layout->internal_state_offset + (uint32_t)i * 4u,
                                  vm->exception_internal_state[i]))
            return -1;
    }
    return 0;
}

static int amivm_m68k_decode_frame_format(struct amivm_vm *vm, uint16_t fv)
{
    uint8_t format;
    if (!vm) return -1;
    format = (uint8_t)(fv >> 12);
    {
        const int descriptor = amivm_m68k_frame_descriptor_for_format(format);
        const struct amivm_m68k_frame_layout *layout;
        if (descriptor < 0) return -1;
        layout = amivm_m68k_frame_layout((uint8_t)descriptor);
        if (!layout) return -1;
        vm->exception_frame_format = format;
        vm->exception_frame_type = (uint8_t)descriptor;
        vm->exception_frame_word_count = layout->words;
        vm->exception_frame_size = (uint8_t)(layout->words * 2u);
    }
    vm->exception_frame_vector_offset = (uint16_t)(fv & 0x0fffu);
    return 0;
}

static int amivm_m68k_validate_exception_internal_state(const struct amivm_vm *vm,
                                                                  const struct amivm_m68k_frame_layout *layout,
                                                                  uint32_t sp)
{
    uint8_t i;
    bool ok;
    uint32_t value;
    if (!vm || !layout || layout->internal_state_words == 0u) return 0;
    for (i = 0u; i < layout->internal_state_words && i < 34u; ++i) {
        value = amivm_m68k_read_u32((struct amivm_vm *)vm,
                                    sp + layout->internal_state_offset + (uint32_t)i * 4u,
                                    &ok);
        if (!ok || value != vm->exception_internal_state[i])
            return -1;
    }
    return 0;
}

static int amivm_m68k_validate_stacked_frame(struct amivm_vm *vm)
{
    const struct amivm_m68k_frame_layout *layout;
    uint32_t sp, pc, fault_addr, fault_status;
    uint8_t b0, b1;
    bool ok;
    if (!vm) return -1;
    layout = amivm_m68k_frame_layout(vm->exception_frame_type);
    if (!layout || vm->exception_frame_size !=
        (uint8_t)(layout->words * 2u))
        return -1;
    sp = vm->m68k.a[7];
    if (amivm_m68k_validate_exception_internal_state(vm, layout, sp) != 0)
        return -1;
    pc = amivm_m68k_read_u32(vm, sp, &ok);
    if (!ok) return -1;
    if (!amivm_read8(vm, sp + 4u, &b0) ||
        !amivm_read8(vm, sp + 5u, &b1))
        return -1;

    if (layout->has_format_vector) {
        uint8_t f0 = 0u, f1 = 0u;
        uint16_t fv;
        if (!amivm_read8(vm, sp + 6u, &f0) ||
            !amivm_read8(vm, sp + 7u, &f1))
            return -1;
        fv = (uint16_t)(((uint16_t)f0 << 8) | f1);
        if (amivm_m68k_decode_frame_format(vm, fv) != 0)
            return -1;
        layout = amivm_m68k_frame_layout(vm->exception_frame_format);
        if (!layout || vm->exception_frame_size !=
            (uint8_t)(layout->words * 2u))
            return -1;
    }

    if (layout->has_fault_address) {
        fault_addr = amivm_m68k_read_u32(vm, sp + layout->fault_address_offset, &ok);
        if (!ok) return -1;
        if (layout->has_fault_status) {
            if (layout->fault_status_bytes == 2u) {
                uint8_t shi = 0u, slo = 0u;
                if (!amivm_read8(vm, sp + layout->fault_status_offset, &shi) ||
                    !amivm_read8(vm, sp + layout->fault_status_offset + 1u, &slo))
                    return -1;
                fault_status = ((uint32_t)shi << 8) | slo;
            } else {
                fault_status = amivm_m68k_read_u32(vm, sp + layout->fault_status_offset, &ok);
                if (!ok) return -1;
            }
        } else {
            fault_status = vm->exception_fault_status;
        }
        if (fault_addr != vm->exception_fault_address ||
            fault_status != vm->exception_fault_status)
            return -1;
    }
    if (pc != vm->mmu_exception_pc ||
        ((uint16_t)b0 << 8 | b1) != vm->mmu_exception_sr)
        return -1;
    return 0;
}

int amivm_m68k_return_from_interrupt(struct amivm_vm *vm)
{
    const struct amivm_m68k_frame_layout *layout;
    uint8_t frame_size;
    if (!vm) return -1;
    if (vm->exception_frame_active &&
        (amivm_m68k_validate_exception_frame(vm) != 0 ||
         amivm_m68k_validate_stacked_frame(vm) != 0)) {
        vm->m68k.stopped = true;
        return -1;
    }
    if (vm->exception_frame_active) {
        layout = amivm_m68k_frame_layout(vm->exception_frame_format);
        if (!layout || vm->exception_frame_size !=
            (uint8_t)(layout->words * 2u))
            return -1;
    }
    frame_size = vm->exception_frame_size;
    if (vm->irq_in_service) {
        bool ok;
        uint32_t frame_pc = amivm_m68k_read_u32(vm, vm->m68k.a[7], &ok);
        uint8_t sr_hi = 0u, sr_lo = 0u;
        if (!ok || !amivm_read8(vm, vm->m68k.a[7] + 4u, &sr_hi) ||
            !amivm_read8(vm, vm->m68k.a[7] + 5u, &sr_lo)) {
            vm->m68k.stopped = true;
            return -1;
        }
        vm->m68k.pc = frame_pc;
        vm->m68k.sr = (uint16_t)(((uint16_t)sr_hi << 8) | sr_lo);
        vm->m68k.supervisor = (vm->m68k.sr & 0x2000u) != 0u;
        vm->irq_in_service = false;
    } else if (vm->exception_frame_active) {
        bool ok;
        uint32_t frame_pc = amivm_m68k_read_u32(vm, vm->m68k.a[7], &ok);
        uint8_t sr_hi = 0u, sr_lo = 0u;
        if (!ok || !amivm_read8(vm, vm->m68k.a[7] + 4u, &sr_hi) ||
            !amivm_read8(vm, vm->m68k.a[7] + 5u, &sr_lo)) {
            vm->m68k.stopped = true;
            return -1;
        }
        vm->m68k.pc = frame_pc;
        vm->m68k.sr = (uint16_t)(((uint16_t)sr_hi << 8) | sr_lo);
        vm->m68k.supervisor = (vm->m68k.sr & 0x2000u) != 0u;
        vm->exception_frame_active = false;
    } else {
        return -1;
    }
    if (frame_size == 0u) return -1;
    vm->m68k.a[7] += frame_size;
    vm->exception_frame_active = false;
    vm->exception_frame_sp = 0u;
    vm->exception_frame_size = 0u;
    vm->exception_frame_format = 0u;
    vm->exception_frame_word_count = 0u;
    vm->pending_exception = AMIVM_M68K_EXC_NONE;
    vm->pending_exception_vector = 0u;
    return 0;
}

int amivm_m68k_acknowledge_irq(struct amivm_vm *vm, uint8_t *vector)
{
    uint8_t v;
    if (!vm || !vector || !vm->irq_pending) return -1;
    if (vm->irq_level <= (uint8_t)((vm->m68k.sr >> 8) & 7u)) return 0;
    v = (uint8_t)(24u + vm->irq_level);
    vm->irq_saved_pc = vm->m68k.pc;
    vm->irq_saved_sr = vm->m68k.sr;
    vm->m68k.sr = (uint16_t)((vm->m68k.sr & ~0x0700u) |
                             ((uint16_t)vm->irq_level << 8) | 0x2000u);
    vm->irq_in_service = true;
    vm->irq_pending = false;
    vm->pending_exception = AMIVM_M68K_EXC_NONE;
    vm->pending_exception_vector = v;
    *vector = v;
    if (amivm_m68k_enter_exception(vm, v) != 0)
        return -1;
    return 1;
}

int amivm_m68k_service_irq(struct amivm_vm *vm)
{
    uint8_t mask;
    if (!vm || !vm->irq_pending) return 0;
    mask = (uint8_t)((vm->m68k.sr >> 8) & 7u);
    if (vm->irq_level <= mask) return 0;
    vm->m68k.sr = (uint16_t)((vm->m68k.sr & ~0x0700u) |
                             ((uint16_t)vm->irq_level << 8) | 0x2000u);
    vm->pending_exception = AMIVM_M68K_EXC_SPURIOUS_INTERRUPT;
    vm->pending_exception_vector = (uint8_t)(24u + vm->irq_level);
    vm->irq_pending = false;
    return vm->pending_exception_vector;
}

int amivm_m68k_set_supervisor(struct amivm_vm *vm, bool supervisor)
{
    if (!vm) return -1;
    vm->m68k.supervisor = supervisor;
    if (supervisor) vm->m68k.sr |= 0x2000u;
    else vm->m68k.sr &= (uint16_t)~0x2000u;
    return 0;
}

bool amivm_m68k_is_supervisor(const struct amivm_vm *vm)
{
    return vm && vm->m68k.supervisor;
}

int amivm_m68k_enter_exception(struct amivm_vm *vm,
                                enum amivm_m68k_exception exception,
                                uint32_t return_pc)
{
    if (!vm || exception == AMIVM_M68K_EXC_NONE) return -1;
    vm->exception_saved_pc = return_pc;
    vm->exception_saved_sr = vm->m68k.sr;
    vm->exception_frame_active = true;
    vm->pending_exception = exception;
    vm->pending_exception_vector = (uint8_t)exception;
    vm->m68k.supervisor = true;
    vm->m68k.sr |= 0x2000u;
    vm->m68k.exception = (uint8_t)exception;
    return (int)exception;
}

int amivm_m68k_raise_exception(struct amivm_vm *vm,
                                      enum amivm_m68k_exception exception)
{
    if (!vm || exception == AMIVM_M68K_EXC_NONE) return -1;
    if (exception == AMIVM_M68K_EXC_MMU_FAULT) {
        vm->mmu_exception_pc = vm->m68k.pc;
        vm->mmu_exception_sr = vm->m68k.sr;
        vm->exception_fault_address = vm->mmu_fault_address;
        vm->exception_fault_status = vm->mmu_fault_status;
    }
    vm->m68k.sr |= 0x2000u;
    vm->pending_exception = exception;
    vm->pending_exception_vector = (uint8_t)exception;
    vm->exception_saved_pc = vm->m68k.pc;
    vm->exception_saved_sr = vm->m68k.sr;
    vm->exception_frame_active = true;
    vm->m68k.supervisor = true;
    vm->m68k.sr |= 0x2000u;
    vm->last_instruction_cycles = 0u;
    return (int)exception;
}

int amivm_mmu_configure_two_level(struct amivm_vm *vm,
                                      uint32_t root_base,
                                      uint8_t root_bits,
                                      uint8_t leaf_bits)
{
    if (!vm || root_bits == 0u || leaf_bits == 0u ||
        root_bits + leaf_bits > 20u)
        return -1;
    vm->mmu.root_table_base = root_base;
    vm->mmu.root_index_bits = root_bits;
    vm->mmu.leaf_index_bits = leaf_bits;
    vm->mmu.page_shift = 12u;
    vm->mmu.page_table_entries = 0u;
    return 0;
}

int amivm_mmu_configure_page_table(struct amivm_vm *vm,
                                         uint32_t base, uint32_t mask,
                                         uint8_t page_shift, uint32_t entries)
{
    if (!vm || page_shift < 8u || page_shift > 20u || entries == 0u)
        return -1;
    vm->mmu.page_table_base = base;
    vm->mmu.page_table_mask = mask;
    vm->mmu.page_shift = page_shift;
    vm->mmu.page_table_entries = entries;
    return 0;
}

int amivm_mmu_tt_match(const struct amivm_mmu_state *mmu,
                              uint32_t logical, bool write)
{
    uint32_t tt;
    uint32_t mask;
    if (!mmu) return 0;
    tt = mmu->tt0;
    if ((tt & 1u) == 0u) {
        tt = mmu->tt1;
        if ((tt & 1u) == 0u) return 0;
    }
    /* 68040-style baseline: upper 8 address bits select a transparent region.
       Lower 24 bits remain unchanged. */
    mask = 0xff000000u;
    if ((logical & mask) != (tt & mask)) return 0;
    if (write && (tt & 0x00000200u) == 0u) return 0;
    return 1;
}

int amivm_mmu_translate(struct amivm_vm *vm, uint32_t logical,
                        bool write, uint32_t *physical)
{
    bool supervisor;
    if (!vm || !physical) return -1;
    supervisor = (vm->m68k.sr & 0x2000u) != 0u;
    vm->mmu.mmu_supervisor = supervisor;
    vm->mmu.last_logical = logical;
    vm->mmu.last_write = write;
    vm->mmu.last_fault = AMIVM_MMU_FAULT_NONE;
    vm->mmu_fault_address = logical;
    vm->mmu_fault_status = (write ? 2u : 0u) |
                            (vm->mmu.mmu_supervisor ? 1u : 0u);

    if (!vm->mmu.enabled || amivm_mmu_tt_match(&vm->mmu, logical, write)) {
        *physical = logical;
        vm->mmu.last_physical = logical;
        return 0;
    }

    /* M2.203: minimal page-table translation seam. A real 68040
       table walker will replace this test mapping without changing
       the memory-bus contract. */
    if (vm->mmu.root_index_bits != 0u && vm->mmu.leaf_index_bits != 0u) {
        uint32_t page_mask = 0xfffu;
        uint32_t leaf_mask = (1u << vm->mmu.leaf_index_bits) - 1u;
        uint32_t root_mask = (1u << vm->mmu.root_index_bits) - 1u;
        uint32_t root_index = (logical >> (vm->mmu.leaf_index_bits + 12u)) & root_mask;
        uint32_t leaf_index = (logical >> 12u) & leaf_mask;
        uint32_t root_pte_addr = vm->mmu.root_table_base + root_index * 4u;
        uint32_t leaf_base, pte, pte_addr;
        bool ok;
        leaf_base = amivm_m68k_read_u32(vm, root_pte_addr, &ok);
        if (!ok || (leaf_base & 1u) == 0u) {
            vm->mmu.last_fault = AMIVM_MMU_FAULT_INVALID;
            vm->mmu_fault_status |= 16u;
            return -1;
        }
        pte_addr = (leaf_base & ~page_mask) + leaf_index * 4u;
        pte = amivm_m68k_read_u32(vm, pte_addr, &ok);
        if (!ok || (pte & 1u) == 0u) {
            vm->mmu.last_fault = AMIVM_MMU_FAULT_INVALID;
            return -1;
        }
        if (!vm->mmu.mmu_supervisor && (pte & 4u) == 0u) {
            vm->mmu.last_fault = AMIVM_MMU_FAULT_SUPERVISOR;
            vm->mmu_fault_status |= 8u;
            return -1;
        }
        if (write && (pte & 2u) != 0u) {
            vm->mmu.last_fault = AMIVM_MMU_FAULT_WRITE_PROTECT;
            vm->mmu_fault_status |= 4u;
            return -1;
        }
        *physical = (pte & ~page_mask) | (logical & page_mask);
        vm->mmu.last_physical = *physical;
        return 0;
    }

    if (vm->mmu.page_table_entries != 0u) {
        uint32_t page_mask = (1u << vm->mmu.page_shift) - 1u;
        uint32_t page = (logical & ~page_mask) & vm->mmu.page_table_mask;
        uint32_t index = (logical >> vm->mmu.page_shift) % vm->mmu.page_table_entries;
        uint32_t pte_addr = vm->mmu.page_table_base + index * 4u;
        uint32_t pte;
        bool ok;
        pte = amivm_m68k_read_u32(vm, pte_addr, &ok);
        if (!ok || (pte & 1u) == 0u) {
            vm->mmu.last_fault = AMIVM_MMU_FAULT_INVALID;
            return -1;
        }
        if (write && (pte & 2u) != 0u) {
            vm->mmu.last_fault = AMIVM_MMU_FAULT_WRITE_PROTECT;
            return -1;
        }
        *physical = (pte & ~page_mask) | (logical & page_mask);
        vm->mmu.last_physical = *physical;
        (void)page;
        return 0;
    }

    if (vm->mmu.test_page_valid &&
        (logical & 0xfffff000u) == (vm->mmu.test_logical_page & 0xfffff000u)) {
        if (!vm->mmu.mmu_supervisor) {
            vm->mmu.last_fault = AMIVM_MMU_FAULT_SUPERVISOR;
            return -1;
        }
        if (write && vm->mmu.test_write_protect) {
            vm->mmu.last_fault = AMIVM_MMU_FAULT_WRITE_PROTECT;
            return -1;
        }
        *physical = (vm->mmu.test_physical_page & 0xfffff000u) |
                    (logical & 0xfffu);
        vm->mmu.last_physical = *physical;
        return 0;
    }
    vm->mmu.last_fault = AMIVM_MMU_FAULT_INVALID;
    return -1;
}

void amivm_m68k_reset(struct amivm_vm *vm, uint32_t pc, uint16_t sr)
{
    if (!vm) return;
    memset(&vm->m68k, 0, sizeof vm->m68k);
    vm->m68k.pc = pc;
    vm->m68k.sr = sr;
    vm->m68k.supervisor = (sr & 0x2000u) != 0u;
    memset(&vm->mmu, 0, sizeof vm->mmu);
}

uint32_t amivm_m68k_read_u32(struct amivm_vm *vm, uint32_t addr, bool *ok)
{
    uint8_t b0, b1, b2, b3;
    if (!ok || !amivm_read8(vm, addr, &b0) ||
        !amivm_read8(vm, addr + 1u, &b1) ||
        !amivm_read8(vm, addr + 2u, &b2) ||
        !amivm_read8(vm, addr + 3u, &b3)) {
        if (ok) *ok = false;
        return 0u;
    }
    *ok = true;
    return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) |
           ((uint32_t)b2 << 8) | b3;
}

bool amivm_m68k_write_u32(struct amivm_vm *vm, uint32_t addr, uint32_t value)
{
    return amivm_write8(vm, addr, (uint8_t)(value >> 24)) &&
           amivm_write8(vm, addr + 1u, (uint8_t)(value >> 16)) &&
           amivm_write8(vm, addr + 2u, (uint8_t)(value >> 8)) &&
           amivm_write8(vm, addr + 3u, (uint8_t)value);
}

static bool m68k_fetch_word(struct amivm_vm *vm, uint32_t addr, uint16_t *word)
{
    uint8_t hi, lo;
    if (!amivm_read8(vm, addr, &hi) || !amivm_read8(vm, addr + 1u, &lo))
        return false;
    *word = (uint16_t)(((uint16_t)hi << 8) | lo);
    return true;
}

int amivm_m68k_execute_one(struct amivm_vm *vm)
{
    uint16_t op;
    if (!vm || vm->m68k.stopped) return -1;
    vm->m68k.exception = AMIVM_M68K_EXC_NONE;
    if (!m68k_fetch_word(vm, vm->m68k.pc, &op)) {
        vm->m68k.exception = AMIVM_M68K_EXC_BUS_ERROR;
        return amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_BUS_ERROR);
    }

    if (op == 0x4e71u) {
        vm->m68k.pc += 2u;
        amivm_vm_account_instruction(vm, 4u);
        return 0;
    }

    /* MOVE.L Dn,Dn */
    if ((op & 0xf1c0u) == 0x2000u) {
        unsigned src = op & 7u;
        unsigned dst = (op >> 9) & 7u;
        uint32_t value = vm->m68k.d[src];
        vm->m68k.d[dst] = value;
        vm->m68k.pc += 2u;
        vm->m68k.sr &= (uint16_t)~0x0fu;
        if (value == 0u) vm->m68k.sr |= 0x04u;
        if (value & 0x80000000u) vm->m68k.sr |= 0x08u;
        amivm_vm_account_instruction(vm, 4u);
        return 0;
    }

    /* MOVE.L #imm,Dn */
    if ((op & 0xf1ffu) == 0x203cu) {
        unsigned dst = (op >> 9) & 7u;
        bool ok;
        uint32_t value = amivm_m68k_read_u32(vm, vm->m68k.pc + 2u, &ok);
        if (!ok) return amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_BUS_ERROR);
        vm->m68k.d[dst] = value;
        vm->m68k.pc += 6u;
        vm->m68k.sr &= (uint16_t)~0x0fu;
        if (value == 0u) vm->m68k.sr |= 0x04u;
        if (value & 0x80000000u) vm->m68k.sr |= 0x08u;
        amivm_vm_account_instruction(vm, 12u);
        return 0;
    }

    /* MOVE.L An,Dn (address register source) */
    if ((op & 0xf1f8u) == 0x2008u) {
        unsigned src = op & 7u;
        unsigned dst = (op >> 9) & 7u;
        uint32_t value = vm->m68k.a[src];
        vm->m68k.d[dst] = value;
        vm->m68k.pc += 2u;
        vm->m68k.sr &= (uint16_t)~0x0fu;
        if (value == 0u) vm->m68k.sr |= 0x04u;
        if (value & 0x80000000u) vm->m68k.sr |= 0x08u;
        amivm_vm_account_instruction(vm, 4u);
        return 0;
    }
    if (op == 0x4e72u) {
        vm->m68k.pc += 2u;
        vm->m68k.stopped = true;
        amivm_vm_account_instruction(vm, 4u);
        return 0;
    }
    if (op == 0x4e73u) {
        if (!vm->m68k.supervisor) {
            vm->m68k.exception = AMIVM_M68K_EXC_PRIVILEGE;
            return amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_PRIVILEGE);
        }
        vm->m68k.pc += 2u;
        return amivm_m68k_return_from_interrupt(vm);
    }

    /* TRAP #n */
    if ((op & 0xfff0u) == 0x4e40u) {
        unsigned n = op & 0x0fu;
        uint32_t return_pc = vm->m68k.pc + 2u;
        vm->pending_exception = (enum amivm_m68k_exception)(32u + n);
        vm->pending_exception_vector = (uint8_t)(32u + n);
        vm->m68k.pc = return_pc;
        vm->exception_saved_pc = return_pc;
        vm->exception_saved_sr = vm->m68k.sr;
        vm->exception_frame_active = true;
        vm->m68k.supervisor = true;
        vm->m68k.sr |= 0x2000u;
        vm->m68k.exception = AMIVM_M68K_EXC_NONE;
        amivm_vm_account_instruction(vm, 4u);
        return 0;
    }

    vm->m68k.exception = AMIVM_M68K_EXC_ILLEGAL;
    return amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_ILLEGAL);
}

int amivm_vm_attach_cpu_backend(struct amivm_vm *vm,
                                const struct amivm_cpu_backend *backend)
{
    if (!vm || !backend || !backend->step) return -1;
    vm->cpu_backend = *backend;
    return 0;
}

int amivm_vm_step(struct amivm_vm *vm)
{
    uint32_t cycles;
    if (!vm || !vm->cpu_backend.step) return -1;
    cycles = vm->cpu_backend.step(vm, vm->cpu_backend.state);
    if (cycles == 0u) return -1;
    amivm_vm_account_instruction(vm, cycles);
    return 0;
}

void amivm_vm_account_instruction(struct amivm_vm *vm, unsigned cycles)
{
    if (!vm) return;
    vm->last_instruction_cycles = cycles;
    amivm_vm_advance_cycles(vm, cycles);
}

void amivm_vm_advance_cycles(struct amivm_vm *vm, unsigned cycles)
{
    unsigned chipset_cycles;
    if (!vm) return;
    vm->cpu_cycles += cycles;
    vm->chipset_cycles += cycles;
    chipset_cycles = cycles;
    amivm_aga_advance_beam(vm, chipset_cycles);
}

void amivm_aga_advance_beam(struct amivm_vm *vm, unsigned cycles)
{
    unsigned i;
    if (!vm) return;
    for (i = 0; i < cycles; ++i) {
        ++vm->aga.beam_h;
        if (vm->aga.beam_h >= 227u) {
            vm->aga.beam_h = 0u;
            ++vm->aga.beam_v;
            if (vm->aga.beam_v >= 312u) {
                vm->aga.beam_v = 0u;
        }
        if (vm->aga.beam_v == 20u && vm->aga.beam_h == 0u &&
            vm->aga.vblank_irq_enable)
            amivm_m68k_request_irq(vm, 3u);
        }
        vm->aga.display_active =
            vm->aga.beam_h >= 20u && vm->aga.beam_h < 200u &&
            vm->aga.beam_v >= 20u && vm->aga.beam_v < 292u;
    }
}

static void bump_write_generation(struct amivm_vm *vm, uint32_t addr)
{
    size_t page;

    vm->memory_write_generation++;
    if (vm->memory_write_generation == 0u) vm->memory_write_generation = 1u;

    page = (size_t)(addr - AMIVM_RAM_BASE) / AMIVM_RAM_PAGE_SIZE;
    if (page < vm->ram_page_count) {
        vm->ram_page_generation[page]++;
        if (vm->ram_page_generation[page] == 0u) vm->ram_page_generation[page] = 1u;
    }
}

static const struct amivm_machine_profile machine_profiles[] = {
    { AMIVM_MACHINE_GENERIC, "generic", 0u, 0u, false, false, 0u },
    { AMIVM_MACHINE_A1200, "A1200", 3u, 2u * 1024u * 1024u, true, true, 2u * 1024u * 1024u,
      8u * 1024u * 1024u, 128u * 1024u * 1024u, true },
    { AMIVM_MACHINE_A3000, "A3000", 2u, 2u * 1024u * 1024u, false, false, 2u * 1024u * 1024u },
    { AMIVM_MACHINE_A4000, "A4000", 3u, 2u * 1024u * 1024u, true, true, 2u * 1024u * 1024u },
    { AMIVM_MACHINE_A500, "A500", 1u, 512u * 1024u, false, false, 512u * 1024u },
    { AMIVM_MACHINE_A500PLUS, "A500+", 1u, 1u * 1024u * 1024u, false, false, 1u * 1024u * 1024u },
    { AMIVM_MACHINE_A600, "A600", 1u, 1u * 1024u * 1024u, false, true, 1u * 1024u * 1024u },
    { AMIVM_MACHINE_A1000, "A1000", 1u, 256u * 1024u, false, false, 256u * 1024u },
    { AMIVM_MACHINE_A2000, "A2000", 1u, 1u * 1024u * 1024u, false, false, 1u * 1024u * 1024u }
};

static bool machine_memory_valid(enum amivm_machine_model id, size_t chip_ram)
{
    const struct amivm_machine_profile *p = amivm_machine_profile_by_id(id);
    return p != NULL && chip_ram <= p->max_chip_ram;
}

const struct amivm_machine_profile *amivm_machine_profile_by_id(
    enum amivm_machine_model id)
{
    size_t i;
    for (i = 0; i < sizeof machine_profiles / sizeof machine_profiles[0]; ++i)
        if (machine_profiles[i].id == id) return &machine_profiles[i];
    return NULL;
}

const struct amivm_machine_profile *amivm_machine_profile_by_name(
    const char *name)
{
    size_t i;
    if (!name) return NULL;
    for (i = 0; i < sizeof machine_profiles / sizeof machine_profiles[0]; ++i)
        if (strcmp(machine_profiles[i].name, name) == 0) return &machine_profiles[i];
    return NULL;
}

int amivm_config_resolve(const struct amivm_config *config,
                             struct amivm_resolved_config *resolved)
{
    const struct amivm_machine_profile *mp;
    if (!config || !resolved || !config->cpu_profile) return 1;
    mp = amivm_machine_profile_by_id(config->machine);
    if (!mp) return 1;
    if (config->chip_ram_size > mp->max_chip_ram) return 1;
    if (config->fast_ram_size > mp->max_fast_ram) return 1;
    if (config->fast_ram_size > 0u && !config->accelerator_present) return 1;
    memset(resolved, 0, sizeof *resolved);
    resolved->machine = config->machine;
    resolved->machine_profile = mp;
    resolved->hardware.machine = mp->id;
    resolved->hardware.chipset =
        mp->chipset_generation == 3u ? AMIVM_CHIPSET_AGA :
        mp->chipset_generation == 2u ? AMIVM_CHIPSET_ECS :
        mp->chipset_generation == 1u ? AMIVM_CHIPSET_OCS :
        AMIVM_CHIPSET_NONE;
    resolved->hardware.has_aga = mp->has_aga;
    resolved->hardware.has_ide = mp->has_ide;
    resolved->hardware.has_zorro = mp->has_zorro;
    resolved->cpu_profile = config->cpu_profile;
    resolved->accelerator_profile = config->accelerator_profile;
    resolved->chip_ram_size = config->chip_ram_size;
    resolved->fast_ram_size = config->fast_ram_size;
    resolved->total_ram_size = config->chip_ram_size + config->fast_ram_size;
    resolved->external_mmu = config->external_mmu;
    resolved->rom_path = config->rom_path;
    resolved->device_count = 0u;
    {
        size_t i;
        for (i = 0; i < sizeof amivm_devices / sizeof amivm_devices[0]; ++i) {
            const struct amivm_device_desc *d = &amivm_devices[i];
            unsigned f = d->required_machine_flags;
            if ((f & AMIVM_DEVF_AGA) && !resolved->hardware.has_aga) continue;
            if ((f & AMIVM_DEVF_IDE) && !resolved->hardware.has_ide) continue;
            if ((f & AMIVM_DEVF_ZORRO) && !resolved->hardware.has_zorro) continue;
            if (resolved->device_count < 16u)
                resolved->devices[resolved->device_count++] = d;
        }
    }
    return 0;
}

void amivm_config_init(struct amivm_config *config)
{
    config->machine = AMIVM_MACHINE_GENERIC;
    config->ram_size = (size_t)AMIVM_DEFAULT_RAM_MIB * 1024u * 1024u;
    config->chip_ram_size = 0u;
    config->fast_ram_size = 0u;
    config->accelerator_present = false;
    config->accelerator_profile = NULL;
    config->rom_path = NULL;
    memset(config->floppy_images, 0, sizeof config->floppy_images);
    memset(config->hard_drives, 0, sizeof config->hard_drives);
    config->cpu_profile = amivm_cpu_profile_default();
    config->external_mmu = AMIVM_MMU_NONE;
}

bool amivm_parse_size_mib(const char *text, size_t *bytes_out)
{
    char *end = NULL;
    unsigned long value;

    if (text == NULL || *text == '\0' || bytes_out == NULL) {
        return false;
    }

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0 || value > 3072) {
        return false;
    }

    *bytes_out = (size_t)value * 1024u * 1024u;
    return true;
}

static int probe_media(const char *path, enum amivm_media_type type,
                         struct amivm_media *media)
{
    FILE *fp;
    long end;
    if (!path || !media) return -1;
    fp = fopen(path, "rb");
    if (!fp) return -1;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return -1; }
    end = ftell(fp);
    fclose(fp);
    if (end < 0) return -1;
    if (type == AMIVM_MEDIA_ADF &&
        (end == 0 || (unsigned long)end > AMIVM_MAX_ADF_SIZE))
        return -1;
    media->path = malloc(strlen(path) + 1u);
    if (!media->path) return -1;
    strcpy(media->path, path);
    media->type = type;
    media->size = (size_t)end;
    media->tracks = 80u;
    media->heads = 2u;
    media->sectors_per_track = 11u;
    media->sector_size = 512u;
    media->current_track = 0u;
    media->current_head = 0u;
    media->write_protected = true;
    media->disk_changed = true;
    if (type == AMIVM_MEDIA_ADF) {
        FILE *data_fp = fopen(path, "rb");
        if (!data_fp) { free(media->path); media->path = NULL; return -1; }
        media->data = malloc(media->size);
        if (!media->data ||
            fread(media->data, 1, media->size, data_fp) != media->size) {
            fclose(data_fp);
            free(media->data); media->data = NULL;
            free(media->path); media->path = NULL;
            return -1;
        }
        fclose(data_fp);
    }
    return 0;
}

int amivm_vm_init(struct amivm_vm *vm, const struct amivm_config *config)
{
    struct amivm_resolved_config resolved;
    if (vm == NULL || config == NULL || config->ram_size == 0u) {
        return -1;
    }
    if (amivm_config_resolve(config, &resolved) != 0)
        return -1;

    memset(vm, 0, sizeof(*vm));
    if (resolved.cpu_profile == NULL) return -1;
    if (config->external_mmu != AMIVM_MMU_NONE) {
        if (!amivm_cpu_profile_attach_mmu(&vm->cpu_profile,
                                          resolved.cpu_profile,
                                          resolved.external_mmu))
            return -1;
    } else {
        vm->cpu_profile = *resolved.cpu_profile;
    }
    vm->ram = calloc(1, resolved.total_ram_size);
    if (vm->ram == NULL) {
        return -1;
    }
    vm->ram_size = resolved.total_ram_size;
    vm->machine = resolved.machine;
    vm->chip_ram_size = resolved.chip_ram_size;
    vm->fast_ram_size = resolved.fast_ram_size;
    vm->cpu_cycles = 0u;
    vm->chipset_cycles = 0u;
        vm->device_count = resolved.device_count;
    memset(&vm->aga, 0, sizeof vm->aga);
    vm->aga.beam_h = 0u;
    vm->aga.beam_v = 0u;
    vm->aga.display_active = false;
    memset(&vm->ide, 0, sizeof vm->ide);
    memset(&vm->zorro, 0, sizeof vm->zorro);
    for (size_t i = 0; i < resolved.device_count; ++i) {
        vm->devices[i].desc = resolved.devices[i];
        vm->devices[i].instantiated = true;
        vm->devices[i].enabled = true;
    }

    vm->ram_page_count = (resolved.total_ram_size + AMIVM_RAM_PAGE_SIZE - 1u) /
                         AMIVM_RAM_PAGE_SIZE;
    vm->ram_page_generation = calloc(vm->ram_page_count, sizeof(*vm->ram_page_generation));
    if (vm->ram_page_generation == NULL) {
        amivm_vm_destroy(vm);
        return -1;
    }
    vm->memory_write_generation = 1u;
    amivm_irq_reset(vm);
    for (unsigned i = 0; i < AMIVM_MAX_IRQ_LINES; ++i) vm->irq.priority[i] = (uint8_t)i;
    amivm_timer_reset(vm);
    amivm_trackdisk_reset(vm);
    for (size_t i = 0; i < AMIVM_MAX_FLOPPY_IMAGES; ++i)
        if (config->floppy_images[i] &&
            probe_media(config->floppy_images[i], AMIVM_MEDIA_ADF,
                        &vm->floppy[i]) != 0) { amivm_vm_destroy(vm); return -1; }
    for (size_t i = 0; i < AMIVM_MAX_HARD_DRIVES; ++i)
        if (config->hard_drives[i] &&
            probe_media(config->hard_drives[i], AMIVM_MEDIA_PATH,
                        &vm->hard_drive[i]) != 0) { amivm_vm_destroy(vm); return -1; }
    if (config->rom_path != NULL && amivm_vm_load_rom(vm, config->rom_path) != 0) {
        amivm_vm_destroy(vm);
        return -1;
    }
    return 0;
}

void amivm_vm_destroy(struct amivm_vm *vm)
{
    if (vm == NULL) {
        return;
    }
    for (size_t i = 0; i < AMIVM_MAX_FLOPPY_IMAGES; ++i)
        free(vm->floppy[i].data);
        free(vm->floppy[i].path);
    for (size_t i = 0; i < AMIVM_MAX_HARD_DRIVES; ++i)
        free(vm->hard_drive[i].path);
    free(vm->ram_page_generation);
    free(vm->ram);
    memset(vm, 0, sizeof(*vm));
}

int amivm_vm_load_rom(struct amivm_vm *vm, const char *path)
{
    FILE *fp;
    size_t n;

    if (vm == NULL || path == NULL) {
        return -1;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    n = fread(vm->rom, 1, sizeof(vm->rom), fp);
    if (ferror(fp) != 0) {
        fclose(fp);
        return -1;
    }
    if (fgetc(fp) != EOF) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    vm->rom_used = n;
    return 0;
}

size_t amivm_device_count_for_hardware(
    const struct amivm_hardware_profile *hardware)
{
    size_t n = 0u;
    size_t i;
    if (!hardware) return 0u;
    for (i = 0; i < sizeof amivm_devices / sizeof amivm_devices[0]; ++i) {
        unsigned f = amivm_devices[i].required_machine_flags;
        if ((f & AMIVM_DEVF_AGA) && !hardware->has_aga) continue;
        if ((f & AMIVM_DEVF_IDE) && !hardware->has_ide) continue;
        if ((f & AMIVM_DEVF_ZORRO) && !hardware->has_zorro) continue;
        ++n;
    }
    return n;
}

size_t amivm_device_count(void)
{
    return sizeof(amivm_devices) / sizeof(amivm_devices[0]);
}

const struct amivm_device_desc *amivm_device_at(size_t index)
{
    if (index >= amivm_device_count()) {
        return NULL;
    }
    return &amivm_devices[index];
}

const struct amivm_device_desc *amivm_find_device(const char *name)
{
    size_t i;

    if (name == NULL) {
        return NULL;
    }
    for (i = 0; i < amivm_device_count(); ++i) {
        if (strcmp(amivm_devices[i].name, name) == 0) {
            return &amivm_devices[i];
        }
    }
    return NULL;
}

bool amivm_read8(struct amivm_vm *vm, uint32_t addr, uint8_t *value)
{
    uint32_t physical;
    if (vm == NULL || value == NULL) {
        return false;
    }
    if (amivm_mmu_translate(vm, addr, false, &physical) != 0) {
        vm->m68k.exception = AMIVM_M68K_EXC_MMU_FAULT;
        (void)amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_MMU_FAULT);
        (void)amivm_m68k_enter_exception(vm, (uint8_t)AMIVM_M68K_EXC_MMU_FAULT);
        if (vm->exception_vector_base != 0u)
            (void)amivm_m68k_enter_mmu_exception(vm);
        if (vm->exception_vector_base != 0u) {
            (void)amivm_m68k_stack_mmu_exception(vm);
            (void)amivm_m68k_exception_enter(vm, 56u);
        }
        return false;
    }
    addr = physical;
    if (in_range(addr, AMIVM_RAM_BASE, vm->ram_size)) {
        *value = vm->ram[(size_t)(addr - AMIVM_RAM_BASE)];
        return true;
    }
    if (in_range(addr, AMIVM_ROM_BASE, AMIVM_ROM_SIZE)) {
        *value = vm->rom[(size_t)(addr - AMIVM_ROM_BASE)];
        return true;
    }
    if (amivm_device_for_address(vm, addr) &&
        addr == AMIVM_VMSERIAL_BASE) {
        *value = 0;
        return true;
    }
    if (addr == AMIVM_VMSERIAL_BASE + 4u) {
        *value = 1u;
        return true;
    }
    if (addr == AMIVM_TIMER_BASE) {
        *value = (uint8_t)(vm->timer_ticks & 0xffu);
        return true;
    }
    if (amivm_device_for_address(vm, addr) &&
        in_range(addr, AMIVM_IRQ_BASE, AMIVM_MMIO_PAGE_SIZE)) {
        uint32_t o = addr - AMIVM_IRQ_BASE;
        switch (o) {
        case 0u: *value = (uint8_t)(vm->irq.pending & 0xffu); return true;
        case 1u: *value = (uint8_t)(vm->irq.enabled & 0xffu); return true;
        case 2u: *value = (uint8_t)(vm->irq.pending & vm->irq.enabled); return true;
        default: return false;
        }
    }
    if (amivm_device_for_address(vm, addr) &&
        in_range(addr, AMIVM_TIMER_BASE, AMIVM_MMIO_PAGE_SIZE)) {
        uint32_t o = addr - AMIVM_TIMER_BASE;
        switch (o) {
        case 0u: *value = (uint8_t)vm->timer.counter; return true;
        case 1u: *value = (uint8_t)vm->timer.period; return true;
        case 2u: *value = vm->timer.enabled ? 1u : 0u; return true;
        case 3u: *value = vm->timer.irq_enable ? 1u : 0u; return true;
        case 4u: *value = vm->timer.periodic ? 1u : 0u; return true;
        default: return false;
        }
    }
    {
        const struct amivm_device_desc *d = amivm_device_for_address(vm, addr);
        if (d && d->read8) {
            struct amivm_device_state *state = NULL;
            size_t i;
            for (i = 0; i < vm->device_count; ++i)
                if (vm->devices[i].desc == d) { state = &vm->devices[i]; break; }
            if (state)
                return d->read8(vm, state, addr - d->base, value);
        }
    }
    }
    if (amivm_device_for_address(vm, addr) &&
        in_range(addr, AMIVM_TRACKDISK_BASE, AMIVM_MMIO_PAGE_SIZE)) {
        uint32_t o = addr - AMIVM_TRACKDISK_BASE;
        switch (o) {
        case 0u: *value = (uint8_t)(vm->trackdisk.dma_address >> 24); return true;
        case 1u: *value = (uint8_t)(vm->trackdisk.dma_address >> 16); return true;
        case 2u: *value = (uint8_t)(vm->trackdisk.dma_address >> 8); return true;
        case 3u: *value = (uint8_t)vm->trackdisk.dma_address; return true;
        case 4u: *value = (uint8_t)vm->trackdisk.track; return true;
        case 5u: *value = (uint8_t)vm->trackdisk.head; return true;
        case 6u: *value = (uint8_t)vm->trackdisk.sector; return true;
        case 7u: *value = (uint8_t)vm->trackdisk.command; return true;
        case 8u: *value = vm->trackdisk.status; return true;
        case 9u: *value = vm->trackdisk.error; return true;
        case 10u: *value = vm->trackdisk.irq_enable ? 1u : 0u; return true;
        case 11u: *value = vm->trackdisk.write_protected ? 1u : 0u; return true;
        default: return false;
        }
    }
    return false;
}

bool amivm_write8(struct amivm_vm *vm, uint32_t addr, uint8_t value)
{
    uint32_t physical;
    if (vm == NULL) {
        return false;
    }
    if (amivm_mmu_translate(vm, addr, true, &physical) != 0) {
        vm->m68k.exception = AMIVM_M68K_EXC_MMU_FAULT;
        (void)amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_MMU_FAULT);
        return false;
    }
    addr = physical;
    if (in_range(addr, AMIVM_RAM_BASE, vm->ram_size)) {
        vm->ram[(size_t)(addr - AMIVM_RAM_BASE)] = value;
        bump_write_generation(vm, addr);
        return true;
    }
    if (in_range(addr, AMIVM_ROM_BASE, AMIVM_ROM_SIZE)) {
        return false;
    }
    if (amivm_device_for_address(vm, addr) &&
        addr == AMIVM_VMSERIAL_BASE) {
        fputc((int)value, stdout);
        fflush(stdout);
        return true;
    }
    if (amivm_device_for_address(vm, addr) &&
        in_range(addr, AMIVM_IRQ_BASE, AMIVM_MMIO_PAGE_SIZE)) {
        uint32_t o = addr - AMIVM_IRQ_BASE;
        switch (o) {
        case 0u: return false;
        case 1u: vm->irq.enabled = (vm->irq.enabled & 0xffffff00u) | value; return true;
        case 2u: vm->irq.pending &= ~(uint32_t)value; return true;
        default: return false;
        }
    }
    if (amivm_device_for_address(vm, addr) &&
        in_range(addr, AMIVM_TIMER_BASE, AMIVM_MMIO_PAGE_SIZE)) {
        uint32_t o = addr - AMIVM_TIMER_BASE;
        switch (o) {
        case 0u: vm->timer.counter = value; return true;
        case 1u: vm->timer.period = value; return true;
        case 2u: vm->timer.enabled = value != 0u; return true;
        case 3u: vm->timer.irq_enable = value != 0u;
                  amivm_irq_enable(vm, 6u, vm->timer.irq_enable); return true;
        case 4u: vm->timer.periodic = value != 0u; return true;
        default: return false;
        }
    }
    {
        const struct amivm_device_desc *d = amivm_device_for_address(vm, addr);
        if (d && d->write8) {
            struct amivm_device_state *state = NULL;
            size_t i;
            for (i = 0; i < vm->device_count; ++i)
                if (vm->devices[i].desc == d) { state = &vm->devices[i]; break; }
            if (state)
                return d->write8(vm, state, addr - d->base, value);
        }
    }
    }
    if (amivm_device_for_address(vm, addr) &&
        in_range(addr, AMIVM_TRACKDISK_BASE, AMIVM_MMIO_PAGE_SIZE)) {
        uint32_t o = addr - AMIVM_TRACKDISK_BASE;
        switch (o) {
        case 0u: vm->trackdisk.dma_address = (vm->trackdisk.dma_address & 0x00ffffffu) | ((uint32_t)value << 24); return true;
        case 1u: vm->trackdisk.dma_address = (vm->trackdisk.dma_address & 0xff00ffffu) | ((uint32_t)value << 16); return true;
        case 2u: vm->trackdisk.dma_address = (vm->trackdisk.dma_address & 0xffff00ffu) | ((uint32_t)value << 8); return true;
        case 3u: vm->trackdisk.dma_address = (vm->trackdisk.dma_address & 0xffffff00u) | value; return true;
        case 4u: vm->trackdisk.track = value; return true;
        case 5u: vm->trackdisk.head = value; return true;
        case 6u: vm->trackdisk.sector = value; return true;
        case 7u: return amivm_trackdisk_command(vm, (enum amivm_trackdisk_command)value) == 0;
        case 10u:
            vm->trackdisk.irq_enable = value != 0u;
            amivm_irq_enable(vm, 3u, vm->trackdisk.irq_enable);
            return true;
        default: return false;
        }
    }
    return false;
}

bool amivm_ram_page_generation(const struct amivm_vm *vm, uint32_t addr,
                               uint64_t *generation)
{
    size_t page;

    if (vm == NULL || generation == NULL ||
        !in_range(addr, AMIVM_RAM_BASE, vm->ram_size)) return false;
    page = (size_t)(addr - AMIVM_RAM_BASE) / AMIVM_RAM_PAGE_SIZE;
    if (page >= vm->ram_page_count) return false;
    *generation = vm->ram_page_generation[page];
    return true;
}

int amivm_media_read(const struct amivm_media *media, size_t offset,
                     void *buffer, size_t size)
{
    if (!media || !buffer || media->type == AMIVM_MEDIA_NONE ||
        offset > media->size || size > media->size - offset)
        return -1;
    if (media->data) {
        memcpy(buffer, media->data + offset, size);
        return 0;
    }
    return -1;
}

int amivm_media_seek(struct amivm_media *media, unsigned track, unsigned head)
{
    if (!media || media->type != AMIVM_MEDIA_ADF ||
        track >= media->tracks || head >= media->heads)
        return -1;
    media->current_track = track;
    media->current_head = head;
    media->disk_changed = false;
    return 0;
}

static int media_sector_offset(const struct amivm_media *media,
                               unsigned sector, size_t *offset)
{
    if (!media || !offset || media->type != AMIVM_MEDIA_ADF ||
        sector >= media->sectors_per_track)
        return -1;
    *offset = (((size_t)media->current_track * media->heads +
                media->current_head) * media->sectors_per_track + sector) *
              media->sector_size;
    return *offset + media->sector_size <= media->size ? 0 : -1;
}

int amivm_media_read_sector(struct amivm_media *media, unsigned sector,
                            void *buffer, size_t size)
{
    size_t offset;
    if (!buffer || size != 512u ||
        media_sector_offset(media, sector, &offset) != 0)
        return -1;
    return amivm_media_read(media, offset, buffer, size);
}

int amivm_media_write_sector(struct amivm_media *media, unsigned sector,
                             const void *buffer, size_t size)
{
    size_t offset;
    if (!buffer || size != 512u || !media || media->write_protected ||
        !media->data || media_sector_offset(media, sector, &offset) != 0)
        return -1;
    memcpy(media->data + offset, buffer, size);
    return 0;
}

void amivm_timer_reset(struct amivm_vm *vm)
{
    if (!vm) return;
    memset(&vm->timer, 0, sizeof vm->timer);
}

void amivm_timer_tick(struct amivm_vm *vm, uint64_t cycles)
{
    if (!vm || !vm->timer.enabled || vm->timer.period == 0u)
        return;
    vm->timer.counter += cycles;
    if (vm->timer.counter >= vm->timer.period) {
        vm->timer.counter %= vm->timer.period;
        if (vm->timer.irq_enable)
            amivm_raise_irq(vm, 6u);
        if (!vm->timer.periodic)
            vm->timer.enabled = false;
    }
}

void amivm_trackdisk_reset(struct amivm_vm *vm)
{
    if (!vm) return;
    memset(&vm->trackdisk, 0, sizeof vm->trackdisk);
    vm->trackdisk.command = AMIVM_TRACKDISK_NOP;
    vm->trackdisk.status = 0u;
    vm->trackdisk.error = 0u;
}

static int trackdisk_dma(struct amivm_vm *vm, uint32_t addr,
                         const void *src, void *dst, size_t size, bool write)
{
    if (!vm || addr < AMIVM_RAM_BASE ||
        !in_range(addr, AMIVM_RAM_BASE, vm->ram_size) ||
        size > vm->ram_size - (size_t)(addr - AMIVM_RAM_BASE))
        return -1;
    if (write)
        memcpy(vm->ram + (addr - AMIVM_RAM_BASE), src, size);
    else
        memcpy(dst, vm->ram + (addr - AMIVM_RAM_BASE), size);
    return 0;
}

int amivm_trackdisk_command(struct amivm_vm *vm,
                            enum amivm_trackdisk_command command)
{
    uint8_t sector[512];
    struct amivm_media *media;
    if (!vm || command == AMIVM_TRACKDISK_NOP) return -1;
    media = &vm->floppy[0];
    if (media->type != AMIVM_MEDIA_ADF) {
        vm->trackdisk.error = 1u;
        return -1;
    }
    vm->trackdisk.command = command;
    vm->trackdisk.busy = true;
    vm->trackdisk.error = 0u;
    if (command == AMIVM_TRACKDISK_SEEK) {
        if (amivm_media_seek(media, vm->trackdisk.track,
                             vm->trackdisk.head) != 0)
            vm->trackdisk.error = 2u;
    } else if (command == AMIVM_TRACKDISK_READ_SECTOR) {
        if (amivm_media_seek(media, vm->trackdisk.track,
                             vm->trackdisk.head) != 0 ||
            amivm_media_read_sector(media, vm->trackdisk.sector,
                                    sector, sizeof sector) != 0 ||
            trackdisk_dma(vm, vm->trackdisk.dma_address, sector, NULL,
                          sizeof sector, true) != 0)
            vm->trackdisk.error = 3u;
    } else if (command == AMIVM_TRACKDISK_WRITE_SECTOR) {
        if (media->write_protected ||
            trackdisk_dma(vm, vm->trackdisk.dma_address, NULL, sector,
                          sizeof sector, false) != 0 ||
            amivm_media_seek(media, vm->trackdisk.track,
                             vm->trackdisk.head) != 0 ||
            amivm_media_write_sector(media, vm->trackdisk.sector,
                                     sector, sizeof sector) != 0)
            vm->trackdisk.error = 4u;
    } else {
        vm->trackdisk.error = 5u;
    }
    vm->trackdisk.busy = false;
    vm->trackdisk.status = vm->trackdisk.error ? 0x80u : 0x01u;
    if (vm->trackdisk.irq_enable)
        amivm_raise_irq(vm, 3u);
    return vm->trackdisk.error ? -1 : 0;
}

void amivm_raise_irq(struct amivm_vm *vm, unsigned line)
{
    if (vm != NULL && line < 32u) {
        vm->irq.pending |= 1u << line;
    }
}

void amivm_clear_irq(struct amivm_vm *vm, unsigned line)
{
    if (vm != NULL && line < 32u) {
        vm->irq.pending &= ~(1u << line);
    }
}

void amivm_tick(struct amivm_vm *vm, uint64_t ticks)
{
    if (vm != NULL) {
        vm->timer_ticks += ticks;
    }
}

void amivm_dump_machine(const struct amivm_vm *vm, FILE *out)
{
    size_t i;

    fprintf(out, "machine=AmiVM\n");
    fprintf(out, "cpu=%s\n", vm->cpu_profile.name);
    fprintf(out, "isa_level=%u\n", vm->cpu_profile.isa_level);
    fprintf(out, "mmu_model=%u\n", (unsigned)vm->cpu_profile.mmu_model);
    fprintf(out, "mmu=%s\n", vm->cpu_profile.has_mmu ? "yes" : "no");
    fprintf(out, "mmu_maturity=%u\n", (unsigned)vm->cpu_profile.mmu_maturity);
    fprintf(out, "fpu_model=%u\n", (unsigned)vm->cpu_profile.fpu_model);
    fprintf(out, "fpu=%s\n", vm->cpu_profile.has_fpu ? "yes" : "no");
    fprintf(out, "master_stack=%s\n", vm->cpu_profile.has_master_stack ? "yes" : "no");
    fprintf(out, "hyper=%s\n", vm->cpu_profile.hyper ? "yes" : "no");
    fprintf(out, "endianness=big\n");
    fprintf(out, "ram_base=0x%08x\n", AMIVM_RAM_BASE);
    fprintf(out, "ram_bytes=%zu\n", vm->ram_size);
    fprintf(out, "rom_base=0x%08x\n", AMIVM_ROM_BASE);
    fprintf(out, "rom_bytes=%u\n", AMIVM_ROM_SIZE);
    fprintf(out, "mmio_base=0x%08x\n", AMIVM_MMIO_BASE);
    fprintf(out, "device_count=%zu\n", amivm_device_count());
    for (i = 0; i < amivm_device_count(); ++i) {
        const struct amivm_device_desc *dev = amivm_device_at(i);
        fprintf(out, "device.%zu=%s,0x%08x,0x%08x,irq%u\n",
                i, dev->name, dev->base, dev->size, dev->irq_line);
    }
}

int amivm_selftest(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t value = 0;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0) {
        return 1;
    }

    if (!amivm_write8(&vm, AMIVM_RAM_BASE + 123u, 0x5au) ||
        !amivm_read8(&vm, AMIVM_RAM_BASE + 123u, &value) || value != 0x5au) {
        amivm_vm_destroy(&vm);
        return 2;
    }
    if (amivm_write8(&vm, AMIVM_ROM_BASE, 1u)) {
        amivm_vm_destroy(&vm);
        return 3;
    }
    if (amivm_read8(&vm, 0x01000000u, &value)) {
        amivm_vm_destroy(&vm);
        return 4;
    }

    amivm_raise_irq(&vm, 3u);
    if ((vm.irq_pending & (1u << 3u)) == 0u) {
        amivm_vm_destroy(&vm);
        return 5;
    }
    amivm_clear_irq(&vm, 3u);
    if (vm.irq_pending != 0u) {
        amivm_vm_destroy(&vm);
        return 6;
    }

    amivm_tick(&vm, 10u);
    if (vm.timer_ticks != 10u) {
        amivm_vm_destroy(&vm);
        return 7;
    }

    if (!amivm_read8(&vm, AMIVM_VMSERIAL_BASE + 4u, &value) || value != 1u) {
        amivm_vm_destroy(&vm);
        return 8;
    }

    if (amivm_device_count() != 3u || amivm_find_device("vmserial") == NULL ||
        amivm_find_device("missing") != NULL) {
        amivm_vm_destroy(&vm);
        return 9;
    }

    amivm_vm_destroy(&vm);
    return 0;
}
