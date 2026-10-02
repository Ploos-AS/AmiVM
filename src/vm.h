#ifndef AMIVM_VM_H
#define AMIVM_VM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "cpu_profile.h"

#define AMIVM_DEFAULT_RAM_MIB 128u
#define AMIVM_RAM_BASE 0x10000000u
#define AMIVM_ROM_BASE 0x00F00000u
#define AMIVM_ROM_SIZE 0x00100000u
#define AMIVM_MMIO_BASE 0xFF000000u
#define AMIVM_VMSERIAL_BASE (AMIVM_MMIO_BASE + 0x0000u)
#define AMIVM_IRQ_BASE      (AMIVM_MMIO_BASE + 0x2000u)
#define AMIVM_TRACKDISK_BASE (AMIVM_MMIO_BASE + 0x3000u)
#define AMIVM_TIMER_BASE     (AMIVM_MMIO_BASE + 0x4000u)
#define AMIVM_MMIO_PAGE_SIZE 0x1000u
#define AMIVM_RAM_PAGE_SIZE 0x1000u

#define AMIVM_MAX_FLOPPY_IMAGES 4u
#define AMIVM_MAX_HARD_DRIVES 8u
#define AMIVM_MAX_ADF_SIZE (1760u * 1024u)

#define AMIVM_MAX_IRQ_LINES 8u

struct amivm_irq_controller {
    uint32_t pending;
    uint32_t enabled;
    uint8_t priority[AMIVM_MAX_IRQ_LINES];
};

enum amivm_machine_model {
    AMIVM_MACHINE_GENERIC = 0,
    AMIVM_MACHINE_A500,
    AMIVM_MACHINE_A500PLUS,
    AMIVM_MACHINE_A600,
    AMIVM_MACHINE_A1000,
    AMIVM_MACHINE_A2000,
    AMIVM_MACHINE_A1200,
    AMIVM_MACHINE_A3000,
    AMIVM_MACHINE_A4000
};

struct amivm_machine_profile {
    enum amivm_machine_model id;
    const char *name;
    unsigned chipset_generation;
    size_t default_chip_ram;
    bool has_aga;
    bool has_ide;
    bool has_zorro;
    size_t max_chip_ram;
    size_t default_fast_ram;
    size_t max_fast_ram;
    bool accelerator_capable;
};

struct amivm_config {
    enum amivm_machine_model machine;
    size_t ram_size;
    size_t chip_ram_size;
    size_t fast_ram_size;
    bool accelerator_present;
    const struct amivm_cpu_profile *accelerator_profile;
    const char *rom_path;
    const char *floppy_images[AMIVM_MAX_FLOPPY_IMAGES];
    const char *hard_drives[AMIVM_MAX_HARD_DRIVES];
    const struct amivm_cpu_profile *cpu_profile;
    enum amivm_mmu_model external_mmu;
};

enum amivm_media_type {
    AMIVM_MEDIA_NONE = 0,
    AMIVM_MEDIA_ADF = 1,
    AMIVM_MEDIA_PATH = 2
};

struct amivm_media {
    enum amivm_media_type type;
    char *path;
    uint8_t *data;
    size_t size;
    unsigned tracks;
    unsigned heads;
    unsigned sectors_per_track;
    unsigned sector_size;
    unsigned current_track;
    unsigned current_head;
    bool write_protected;
    bool disk_changed;
};

enum amivm_trackdisk_command {
    AMIVM_TRACKDISK_NOP = 0,
    AMIVM_TRACKDISK_READ_SECTOR = 1,
    AMIVM_TRACKDISK_WRITE_SECTOR = 2,
    AMIVM_TRACKDISK_SEEK = 3
};

struct amivm_timer {
    uint64_t counter;
    uint64_t period;
    bool enabled;
    bool irq_enable;
    bool periodic;
};

struct amivm_trackdisk {
    uint32_t dma_address;
    unsigned track;
    unsigned head;
    unsigned sector;
    enum amivm_trackdisk_command command;
    uint8_t status;
    uint8_t error;
    bool busy;
    bool irq_enable;
    bool write_protected;
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
    uint64_t *ram_page_generation;
    size_t ram_page_count;
    uint8_t rom[AMIVM_ROM_SIZE];
    size_t rom_used;
    struct amivm_irq_controller irq;
    uint64_t timer_ticks;
    uint64_t memory_write_generation;
    enum amivm_machine_model machine;
    struct amivm_cpu_profile cpu_profile;
    size_t chip_ram_size;
    size_t fast_ram_size;
    struct amivm_media floppy[AMIVM_MAX_FLOPPY_IMAGES];
    struct amivm_media hard_drive[AMIVM_MAX_HARD_DRIVES];
    struct amivm_trackdisk trackdisk;
    struct amivm_timer timer;
};

const struct amivm_machine_profile *amivm_machine_profile_by_id(
    enum amivm_machine_model id);
const struct amivm_machine_profile *amivm_machine_profile_by_name(
    const char *name);

void amivm_config_init(struct amivm_config *config);
bool amivm_parse_size_mib(const char *text, size_t *bytes_out);
int amivm_vm_init(struct amivm_vm *vm, const struct amivm_config *config);
void amivm_vm_destroy(struct amivm_vm *vm);
int amivm_vm_load_rom(struct amivm_vm *vm, const char *path);
int amivm_media_read(const struct amivm_media *media, size_t offset,
                     void *buffer, size_t size);
int amivm_media_seek(struct amivm_media *media, unsigned track, unsigned head);
int amivm_media_read_sector(struct amivm_media *media, unsigned sector,
                            void *buffer, size_t size);
int amivm_media_write_sector(struct amivm_media *media, unsigned sector,
                             const void *buffer, size_t size);

size_t amivm_device_count(void);
const struct amivm_device_desc *amivm_device_at(size_t index);
const struct amivm_device_desc *amivm_find_device(const char *name);

bool amivm_read8(struct amivm_vm *vm, uint32_t addr, uint8_t *value);
bool amivm_write8(struct amivm_vm *vm, uint32_t addr, uint8_t value);
bool amivm_ram_page_generation(const struct amivm_vm *vm, uint32_t addr,
                               uint64_t *generation);
void amivm_raise_irq(struct amivm_vm *vm, unsigned line);
void amivm_clear_irq(struct amivm_vm *vm, unsigned line);
void amivm_irq_enable(struct amivm_vm *vm, unsigned line, bool enable);
bool amivm_irq_is_pending(const struct amivm_vm *vm, unsigned line);
void amivm_irq_reset(struct amivm_vm *vm);
int amivm_trackdisk_command(struct amivm_vm *vm,
                               enum amivm_trackdisk_command command);
void amivm_trackdisk_reset(struct amivm_vm *vm);
void amivm_timer_reset(struct amivm_vm *vm);
void amivm_timer_tick(struct amivm_vm *vm, uint64_t cycles);
void amivm_tick(struct amivm_vm *vm, uint64_t ticks);
void amivm_dump_machine(const struct amivm_vm *vm, FILE *out);
int amivm_selftest(void);

#endif
