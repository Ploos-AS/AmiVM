#include "guest_harness.h"
#include "rom_boot.h"
#include "vm.h"

#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

int amivm_guest_harness_validate(const struct amivm_guest_harness *h)
{
    if (!h || !h->profile || !h->rom_path || !h->disk_path)
        return 1;
    if (h->timeout_ms == 0u || h->max_instructions == 0u)
        return 1;
    if (h->qualification_mask == 0u)
        return 1;
    if (!h->os_marker || !h->filesystem_marker || !h->shell_marker)
        return 1;
    if (h->serial_capture_size == 0u || h->serial_capture_size > AMIVM_SERIAL_LOG_SIZE)
        return 1;
    return 0;
}

static bool contains_marker(const char *serial, size_t serial_size,
                         const char *marker)
{
    size_t marker_len;
    size_t i;

    if (!serial || !marker)
        return false;
    marker_len = strlen(marker);
    if (marker_len == 0u || marker_len > serial_size)
        return false;
    for (i = 0u; i + marker_len <= serial_size; ++i)
        if (memcmp(serial + i, marker, marker_len) == 0)
            return true;
    return false;
}

int amivm_guest_harness_classify_serial(const struct amivm_guest_harness *h,
                                        const char *serial,
                                        size_t serial_size,
                                        uint32_t *result_mask)
{
    if (!h || !serial || !result_mask ||
        amivm_guest_harness_validate(h) != 0)
        return 1;

    *result_mask &= ~(AMIVM_QUAL_OS_DETECTED |
                      AMIVM_QUAL_FILESYSTEM |
                      AMIVM_QUAL_SHELL);

    if (contains_marker(serial, serial_size, h->os_marker))
        *result_mask |= AMIVM_QUAL_OS_DETECTED;
    if (contains_marker(serial, serial_size, h->filesystem_marker))
        *result_mask |= AMIVM_QUAL_FILESYSTEM;
    if (contains_marker(serial, serial_size, h->shell_marker))
        *result_mask |= AMIVM_QUAL_SHELL;

    return 0;
}

bool amivm_guest_harness_qualification_complete(
    const struct amivm_guest_harness *h, uint32_t result_mask)
{
    uint32_t required;

    if (!h || amivm_guest_harness_validate(h) != 0)
        return false;
    required = h->qualification_mask;
    return (result_mask & required) == required;
}int amivm_guest_harness_capability_status(
    const struct amivm_vm *vm,
    const struct amivm_guest_harness *h,
    char *buffer, size_t buffer_size)
{
    const struct amivm_cpu_profile *p;
    const char *cpu_status;
    const char *mmu_status;
    const char *fpu_status;
    const char *backend_status;

    if (!vm || !h || !buffer || buffer_size == 0u)
        return 1;

    p = amivm_cpu_get_profile(&vm->m68k);
    if (!p)
        return 1;

    cpu_status = (p->isa_level >= 2u &&
                  p->id == AMIVM_CPU_68020) ? "PASS" : "NOT QUALIFIED";
    mmu_status = p->has_mmu ? "PASS" : "NOT REQUIRED";
    fpu_status = p->has_fpu ? "PASS" : "NOT REQUIRED";
    backend_status = vm->cpu_backend.step ? "PASS" : "NOT QUALIFIED";

    (void)snprintf(buffer, buffer_size,
                   "cpu=%s;mmu=%s;fpu=%s;backend=%s;devices=DEFERRED",
                   cpu_status, mmu_status, fpu_status, backend_status);
    return 0;
}

int amivm_guest_harness_device_status(const struct amivm_vm *vm,
                                      char *buffer, size_t buffer_size)
{
    size_t i;
    size_t used = 0u;

    if (!vm || !buffer || buffer_size == 0u)
        return 1;
    buffer[0] = '\0';

    for (i = 0u; i < vm->device_count; ++i) {
        const struct amivm_device_state *d = &vm->devices[i];
        const char *status;
        int n;

        if (!d->desc || !d->desc->name)
            continue;
        status = (d->instantiated && d->enabled) ? "PASS" : "NOT QUALIFIED";
        n = snprintf(buffer + used,
                     buffer_size > used ? buffer_size - used : 0u,
                     "%s%s=%s",
                     used ? ";" : "", d->desc->name, status);
        if (n < 0)
            return 1;
        if ((size_t)n >= buffer_size - used) {
            buffer[buffer_size - 1u] = '\0';
            return 0;
        }
        used += (size_t)n;
    }
    return 0;
}

int amivm_guest_harness_required_devices(
    const struct amivm_vm *vm,
    const char *const *names,
    size_t name_count,
    char *buffer, size_t buffer_size)
{
    size_t i;
    size_t used = 0u;
    bool all_pass = true;

    if (!vm || !names || !buffer || buffer_size == 0u)
        return 1;
    buffer[0] = '\0';

    for (i = 0u; i < name_count; ++i) {
        size_t j;
        bool found = false;
        bool pass = false;

        for (j = 0u; j < vm->device_count; ++j) {
            const struct amivm_device_state *d = &vm->devices[j];
            if (d->desc && d->desc->name &&
                strcmp(d->desc->name, names[i]) == 0) {
                found = true;
                pass = d->instantiated && d->enabled;
                break;
            }
        }

        {
            const char *status = pass ? "PASS" :
                                 found ? "FAIL" : "MISSING";
            int n = snprintf(buffer + used,
                             buffer_size > used ? buffer_size - used : 0u,
                             "%s%s=%s",
                             used ? ";" : "", names[i], status);
            if (n < 0)
                return 1;
            if ((size_t)n >= buffer_size - used) {
                buffer[buffer_size - 1u] = '\0';
                return all_pass ? 0 : 2;
            }
            used += (size_t)n;
        }
        if (!pass)
            all_pass = false;
    }
    return all_pass ? 0 : 2;
}

int amivm_guest_harness_irq_status(const struct amivm_vm *vm,
                                   char *buffer, size_t buffer_size)
{
    const struct amivm_irq_controller *irq;
    if (!vm || !buffer || buffer_size == 0u)
        return 1;

    irq = &vm->irq;
    (void)snprintf(buffer, buffer_size,
                   "controller=%s;pending=0x%08x;enabled=0x%08x;"
                   "vblank=%s;timer_ticks=%llu",
                   irq ? "PASS" : "FAIL",
                   irq ? irq->pending : 0u,
                   irq ? irq->enabled : 0u,
                   vm->aga.vblank_irq_enable ? "ENABLED" : "DISABLED",
                   (unsigned long long)vm->timer_ticks);
    return 0;
}

int amivm_guest_harness_irq_activity(const struct amivm_vm *vm,
                                     char *buffer, size_t buffer_size)
{
    bool pending = false;
    bool enabled = false;
    bool vblank = false;
    bool timer_active = false;
    unsigned i;

    if (!vm || !buffer || buffer_size == 0u)
        return 1;

    pending = vm->irq.pending != 0u;
    enabled = vm->irq.enabled != 0u;
    vblank = vm->aga.vblank_irq_enable;
    timer_active = vm->timer_ticks != 0u;

    for (i = 0u; i < AMIVM_MAX_IRQ_LINES; ++i) {
        if (vm->irq.priority[i] != 0u) {
            enabled = true;
            break;
        }
    }

    (void)snprintf(buffer, buffer_size,
                   "pending=%s;enabled=%s;vblank=%s;timer_activity=%s;"
                   "evidence=%s",
                   pending ? "yes" : "no",
                   enabled ? "yes" : "no",
                   vblank ? "yes" : "no",
                   timer_active ? "yes" : "no",
                   (pending || timer_active) ? "OBSERVED" : "NOT OBSERVED");
    return 0;
}

int amivm_guest_harness_irq_path_status(
    const struct amivm_vm *vm, char *buffer, size_t buffer_size)
{
    const struct amivm_cpu_state *cpu;
    bool source;
    bool accepted;
    bool in_service;
    bool frame;
    bool resumed;

    if (!vm || !buffer || buffer_size == 0u)
        return 1;

    cpu = &vm->m68k;
    source = vm->irq.pending != 0u || cpu->irq_pending;
    accepted = cpu->irq_level != 0u || cpu->irq_in_service;
    in_service = cpu->irq_in_service;
    frame = cpu->exception_frame_active || cpu->exception_saved_pc != 0u;
    resumed = in_service && cpu->pc != cpu->irq_saved_pc;

    (void)snprintf(buffer, buffer_size,
                   "source=%s;accepted=%s;frame=%s;in_service=%s;"
                   "resumed=%s",
                   source ? "OBSERVED" : "NOT OBSERVED",
                   accepted ? "OBSERVED" : "NOT OBSERVED",
                   frame ? "OBSERVED" : "NOT OBSERVED",
                   in_service ? "OBSERVED" : "NOT OBSERVED",
                   resumed ? "OBSERVED" : "NOT OBSERVED");
    return 0;
}

int amivm_guest_harness_storage_status(
    const struct amivm_vm *vm, char *buffer, size_t buffer_size)
{
    const struct amivm_trackdisk *td;
    const struct amivm_ide_state *ide;

    if (!vm || !buffer || buffer_size == 0u)
        return 1;

    td = &vm->trackdisk;
    ide = &vm->ide;

    (void)snprintf(buffer, buffer_size,
                   "trackdisk=%s;busy=%s;command=%u;track=%u;head=%u;"
                   "sector=%u;dma=0x%08x;status=0x%02x;error=0x%02x;"
                   "irq=%s;ide_status=0x%02x;lba=%u",
                   td->command != AMIVM_TRACKDISK_CMD_NONE ? "OBSERVED" : "IDLE",
                   td->busy ? "yes" : "no",
                   (unsigned)td->command, td->track, td->head, td->sector,
                   td->dma_address, td->status, td->error,
                   td->irq_enable ? "ENABLED" : "DISABLED",
                   ide->status, ide->lba);
    return 0;
}

int amivm_guest_harness_storage_transaction(
    const struct amivm_vm *vm, char *buffer, size_t buffer_size)
{
    const struct amivm_trackdisk *td;
    const struct amivm_ide_state *ide;
    bool attached;
    bool command;
    bool transfer;
    bool completion;
    bool error_free;

    if (!vm || !buffer || buffer_size == 0u)
        return 1;

    td = &vm->trackdisk;
    ide = &vm->ide;
    attached = vm->disk.attached;
    command = td->command != AMIVM_TRACKDISK_CMD_NONE ||
              ide->status != 0u || ide->lba != 0u;
    transfer = td->dma_address != 0u;
    completion = command && !td->busy;
    error_free = td->error == 0u;

    (void)snprintf(buffer, buffer_size,
                   "attached=%s;command=%s;transfer=%s;completion=%s;"
                   "irq=%s;error=%s",
                   attached ? "PASS" : "FAIL",
                   command ? "OBSERVED" : "NOT OBSERVED",
                   transfer ? "OBSERVED" : "NOT OBSERVED",
                   completion ? "OBSERVED" : "NOT OBSERVED",
                   td->irq_enable ? "ENABLED" : "DISABLED",
                   error_free ? "NONE" : "PRESENT");
    return 0;
}


int amivm_guest_harness_format_capabilities(const struct amivm_vm *vm,
                                            char *buffer, size_t buffer_size)
{
    const struct amivm_cpu_profile *p;
    if (!vm || !buffer || buffer_size == 0u)
        return 1;
    p = amivm_cpu_get_profile(&vm->m68k);
    if (!p)
        return 1;
    (void)snprintf(buffer, buffer_size,
                   "cpu=%s isa=%u mmu=%s mmu_maturity=%u fpu=%s "
                   "master_stack=%s hyper=%s frame_family=%u "
                   "jit=%s serial=%s",
                   p->name, p->isa_level,
                   p->has_mmu ? "yes" : "no", p->mmu_maturity,
                   p->has_fpu ? "yes" : "no",
                   p->has_master_stack ? "yes" : "no",
                   p->hyper ? "yes" : "no",
                   p->exception_frame_family,
                   amivm_cpu_backend_name(&vm->cpu_backend),
                   "yes");
    return 0;
}


int amivm_guest_harness_format_failure(const struct amivm_vm *vm,
                                       const struct amivm_guest_harness *h,
                                       uint32_t result_mask,
                                       char *buffer, size_t buffer_size)
{
    size_t serial_len;
    char serial[AMIVM_SERIAL_LOG_SIZE + 1u];

    if (!vm || !h || !buffer || buffer_size == 0u)
        return 1;

    serial_len = amivm_vm_serial_read(vm, serial, h->serial_capture_size);
    if (serial_len > AMIVM_SERIAL_LOG_SIZE)
        serial_len = AMIVM_SERIAL_LOG_SIZE;
    serial[serial_len] = '\0';

    (void)snprintf(buffer, buffer_size,
                   "profile=%s result=0x%08x pc=0x%08x sr=0x%04x "
                   "instruction=0x%08x exception=%u fault_address=0x%08x "
                   "fault_status=0x%08x serial=\"%s\"",
                   h->profile, result_mask,
                   vm->last_guest_pc, vm->last_guest_sr,
                   vm->last_guest_instruction, vm->last_guest_exception,
                   vm->last_guest_fault_address,
                   vm->last_guest_fault_status, serial);
    return 0;
}
int amivm_guest_harness_format_trace(const struct amivm_vm *vm,
                                     char *buffer, size_t buffer_size)
{
    size_t i, count;
    int used = 0;

    if (!vm || !buffer || buffer_size == 0u)
        return 1;

    buffer[0] = '\0';
    count = amivm_vm_guest_trace_count(vm);
    for (i = 0u; i < count; ++i) {
        const struct amivm_trace_entry *e =
            amivm_vm_guest_trace_at(vm, i);
        int n;
        if (!e) continue;
        n = snprintf(buffer + used, buffer_size > (size_t)used ?
                     buffer_size - (size_t)used : 0u,
                     "%s%zu: pc=%08x sr=%04x op=%04x cycles=%u exc=%u",
                     used ? "\\n" : "", i, e->pc, e->sr, e->opcode,
                     e->cycles, e->exception);
        if (n < 0) return 1;
        if ((size_t)n >= buffer_size - (size_t)used) {
            used = (int)buffer_size - 1;
            break;
        }
        used += n;
    }
    return 0;
}
const char *amivm_guest_harness_failure_class(
    const struct amivm_vm *vm,
    const struct amivm_guest_harness *h,
    uint32_t result_mask)
{
    if (!vm || !h)
        return "INVALID";
    if (amivm_guest_harness_qualification_complete(h, result_mask))
        return "PASS";

    switch (vm->m68k.last_fault) {
    case AMIVM_CPU_FAULT_ILLEGAL: return "ILLEGAL_INSTRUCTION";
    case AMIVM_CPU_FAULT_MMU: return "MMU_FAULT";
    case AMIVM_CPU_FAULT_BUS: return "BUS_ERROR";
    case AMIVM_CPU_FAULT_ADDRESS: return "ADDRESS_ERROR";
    case AMIVM_CPU_FAULT_PRIVILEGE: return "PRIVILEGE";
    default:
        break;
    }

    if ((result_mask & AMIVM_QUAL_EXECUTION) == 0u)
        return "EXECUTION";
    if ((result_mask & AMIVM_QUAL_OS_DETECTED) == 0u)
        return "BOOT_TIMEOUT";
    if ((result_mask & AMIVM_QUAL_FILESYSTEM) == 0u)
        return "FILESYSTEM";
    if ((result_mask & AMIVM_QUAL_SHELL) == 0u)
        return "SHELL";
    return "DEVICE_IO";
}


int amivm_guest_harness_run_image(struct amivm_vm *vm,
                                  const struct amivm_guest_harness *h,
                                  const uint8_t *image,
                                  size_t image_size,
                                  uint32_t *result_mask)
{
    uint64_t i;

    if (!vm || !result_mask || !image ||
        amivm_guest_harness_validate(h) != 0)
        return 1;

    if (amivm_rom_install(vm, image, image_size) != 0 ||
        amivm_rom_reset(vm) != 0)
        return 1;

    *result_mask = AMIVM_QUAL_RESET;

    for (i = 0u; i < h->max_instructions; ++i) {
        if (vm->m68k.stopped)
            break;
        if (amivm_vm_step(vm) != 0)
            return 1;
        *result_mask |= AMIVM_QUAL_EXECUTION;
    }

    return 0;
}
int amivm_guest_harness_validate_assets(
    const struct amivm_guest_harness *h, char *buffer, size_t buffer_size)
{
    FILE *rom;
    FILE *disk;

    if (!h || !buffer || buffer_size == 0u)
        return 1;

    if (!h->rom_path || h->rom_path[0] == '\0') {
        (void)snprintf(buffer, buffer_size, "ROM=MISSING;disk=MISSING");
        return 2;
    }

    rom = fopen(h->rom_path, "rb");
    if (!rom) {
        (void)snprintf(buffer, buffer_size, "ROM=MISSING;disk=MISSING");
        return 2;
    }
    fclose(rom);

    if (!h->disk_path || h->disk_path[0] == '\0') {
        (void)snprintf(buffer, buffer_size, "ROM=READY;disk=MISSING");
        return 2;
    }

    disk = fopen(h->disk_path, "rb");
    if (!disk) {
        (void)snprintf(buffer, buffer_size, "ROM=READY;disk=MISSING");
        return 2;
    }
    fclose(disk);

    (void)snprintf(buffer, buffer_size, "ROM=READY;disk=READY");
    return 0;
}


int amivm_guest_harness_run_external(struct amivm_vm *vm,
                                     const struct amivm_guest_harness *h,
                                     uint32_t *result_mask)
{
    uint64_t i;

    if (!vm || !result_mask || amivm_guest_harness_validate(h) != 0)
        return 1;

    if (amivm_vm_load_rom(vm, h->rom_path) != 0)
        return 1;

    if (h->disk_path[0] != '\0' &&
        amivm_vm_attach_hard_drive(vm, 0u, h->disk_path) != 0)
        return 1;

    if (amivm_rom_reset(vm) != 0)
        return 1;

    *result_mask = AMIVM_QUAL_RESET;
    for (i = 0u; i < h->max_instructions; ++i) {
        if (vm->m68k.stopped)
            break;
        if (amivm_vm_step(vm) != 0)
            return 1;
        *result_mask |= AMIVM_QUAL_EXECUTION;
    }

    {
        char serial[AMIVM_SERIAL_LOG_SIZE + 1u];
        size_t n = amivm_vm_serial_read(vm, serial, h->serial_capture_size);
        if (n > AMIVM_SERIAL_LOG_SIZE)
            n = AMIVM_SERIAL_LOG_SIZE;
        serial[n] = '\0';
        if (amivm_guest_harness_classify_serial(h, serial, n, result_mask) != 0)
            return 1;
    }
    return 0;
}

int amivm_guest_harness_run(struct amivm_vm *vm,
                            const struct amivm_guest_harness *h,
                            uint32_t *result_mask)
{
    if (!vm || !result_mask || amivm_guest_harness_validate(h) != 0)
        return 1;

    /*
     * External ROM/disk loading remains outside this first contract.
     * Callers with already-loaded guest assets can use run_image() to
     * qualify actual reset and CPU execution without inventing OS state.
     */
    *result_mask = AMIVM_QUAL_RESET;
    return 0;
}
