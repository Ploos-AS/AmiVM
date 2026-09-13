#ifndef AMIVM_VM_H
#define AMIVM_VM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define AMIVM_DEFAULT_RAM_MIB 128u
#define AMIVM_RAM_BASE 0x10000000u
#define AMIVM_ROM_BASE 0x00F00000u
#define AMIVM_ROM_SIZE 0x00100000u
#define AMIVM_MMIO_BASE 0xFF000000u
#define AMIVM_VMSERIAL_BASE (AMIVM_MMIO_BASE + 0x0000u)
#define AMIVM_TIMER_BASE    (AMIVM_MMIO_BASE + 0x1000u)
#define AMIVM_IRQ_BASE      (AMIVM_MMIO_BASE + 0x2000u)
#define AMIVM_MMIO_PAGE_SIZE 0x1000u

struct amivm_config {
    size_t ram_size;
    const char *rom_path;
};

struct amivm_device_desc {
    const char *name;
    uint32_t base;
    uint32_t size;
    unsigned irq_line;
};

struct amivm_vm {
    uint8_t *ram;
    size_t ram_size;
    uint8_t rom[AMIVM_ROM_SIZE];
    size_t rom_used;
    uint32_t irq_pending;
    uint64_t timer_ticks;
};

void amivm_config_init(struct amivm_config *config);
bool amivm_parse_size_mib(const char *text, size_t *bytes_out);
int amivm_vm_init(struct amivm_vm *vm, const struct amivm_config *config);
void amivm_vm_destroy(struct amivm_vm *vm);
int amivm_vm_load_rom(struct amivm_vm *vm, const char *path);

size_t amivm_device_count(void);
const struct amivm_device_desc *amivm_device_at(size_t index);
const struct amivm_device_desc *amivm_find_device(const char *name);

bool amivm_read8(struct amivm_vm *vm, uint32_t addr, uint8_t *value);
bool amivm_write8(struct amivm_vm *vm, uint32_t addr, uint8_t value);
void amivm_raise_irq(struct amivm_vm *vm, unsigned line);
void amivm_clear_irq(struct amivm_vm *vm, unsigned line);
void amivm_tick(struct amivm_vm *vm, uint64_t ticks);
void amivm_dump_machine(const struct amivm_vm *vm, FILE *out);
int amivm_selftest(void);

#endif
