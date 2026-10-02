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

enum amivm_chipset_generation {
    AMIVM_CHIPSET_NONE = 0,
    AMIVM_CHIPSET_OCS,
    AMIVM_CHIPSET_ECS,
    AMIVM_CHIPSET_AGA
};

struct amivm_hardware_profile {
    enum amivm_machine_model machine;
    enum amivm_chipset_generation chipset;
    bool has_aga;
    bool has_ide;
    bool has_zorro;
};

struct amivm_resolved_config {
    enum amivm_machine_model machine;
    const struct amivm_machine_profile *machine_profile;
    struct amivm_hardware_profile hardware;
    const struct amivm_cpu_profile *cpu_profile;
    const struct amivm_cpu_profile *accelerator_profile;
    size_t chip_ram_size;
    size_t fast_ram_size;
    size_t total_ram_size;
    enum amivm_mmu_model external_mmu;
    const char *rom_path;
    size_t device_count;
    const struct amivm_device_desc *devices[16];
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

typedef uint32_t (*amivm_cpu_step_fn)(struct amivm_vm *vm, void *cpu_state);

enum amivm_m68k_exception {
    AMIVM_M68K_EXC_NONE = 0,
    AMIVM_M68K_EXC_BUS_ERROR = 2,
    AMIVM_M68K_EXC_ADDRESS_ERROR = 3,
    AMIVM_M68K_EXC_ILLEGAL = 4,
    AMIVM_M68K_EXC_ZERO_DIVIDE = 5,
    AMIVM_M68K_EXC_CHK = 6,
    AMIVM_M68K_EXC_TRAPV = 7,
    AMIVM_M68K_EXC_PRIVILEGE = 8,
    AMIVM_M68K_EXC_TRACE = 9,
    AMIVM_M68K_EXC_LINE_A = 10,
    AMIVM_M68K_EXC_LINE_F = 11,
    AMIVM_M68K_EXC_SPURIOUS_INTERRUPT = 24
};

enum amivm_m68k_mmu_fault {
    AMIVM_MMU_FAULT_NONE = 0,
    AMIVM_MMU_FAULT_INVALID = 1,
    AMIVM_MMU_FAULT_WRITE_PROTECT = 2,
    AMIVM_MMU_FAULT_SUPERVISOR = 3
};

struct amivm_mmu_state {
    bool enabled;
    bool test_page_valid;
    uint32_t test_logical_page;
    uint32_t test_physical_page;
    bool test_write_protect;
    uint32_t page_table_base;
    uint32_t page_table_mask;
    uint8_t page_shift;
    uint32_t page_table_entries;
    uint32_t tc;
    uint32_t srp;
    uint32_t crp;
    uint32_t tt0;
    uint32_t tt1;
    uint32_t last_logical;
    uint32_t last_physical;
    bool last_write;
    enum amivm_m68k_mmu_fault last_fault;
};

struct amivm_m68k_registers {
    uint32_t d[8];
    uint32_t a[8];
    uint32_t pc;
    uint16_t sr;
    bool stopped;
    uint8_t exception;
    bool supervisor;
};

struct amivm_m68k_step_result {
    uint32_t cycles;
    uint8_t exception;
    bool stopped;
};

struct amivm_cpu_backend {
    const char *name;
    amivm_cpu_step_fn step;
    void *state;
};

struct amivm_vm;
typedef bool (*amivm_device_read8_fn)(struct amivm_vm *vm,
                                      const struct amivm_device_state *state,
                                      uint32_t offset, uint8_t *value);
typedef bool (*amivm_device_write8_fn)(struct amivm_vm *vm,
                                       const struct amivm_device_state *state,
                                       uint32_t offset, uint8_t value);

struct amivm_device_desc {
    const char *name;
    uint32_t base;
    uint32_t size;
    unsigned irq_line;
    unsigned required_machine_flags;
    amivm_device_read8_fn read8;
    amivm_device_write8_fn write8;
};

struct amivm_aga_state {
    uint16_t bplcon0;
    uint16_t diwstrt;
    uint16_t diwstop;
    uint16_t dmacon;
    uint16_t dmaconr;
    uint16_t beam_h;
    uint16_t beam_v;
    bool display_active;
    bool vblank_irq_enable;
};

struct amivm_ide_state {
    uint8_t status;
    uint8_t command;
    uint32_t lba;
};

struct amivm_zorro_state {
    uint8_t configured;
};

struct amivm_device_state {
    const struct amivm_device_desc *desc;
    bool instantiated;
    bool enabled;
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
    struct amivm_device_state devices[16];
    struct amivm_aga_state aga;
    struct amivm_ide_state ide;
    struct amivm_zorro_state zorro;
    size_t device_count;
    uint64_t cpu_cycles;
    uint64_t chipset_cycles;
    uint32_t last_instruction_cycles;
    struct amivm_cpu_backend cpu_backend;
    struct amivm_m68k_registers m68k;
    struct amivm_mmu_state mmu;
    enum amivm_m68k_exception pending_exception;
    uint8_t pending_exception_vector;
    uint8_t irq_level;
    bool irq_pending;
    bool irq_in_service;
    uint32_t irq_saved_pc;
    uint16_t irq_saved_sr;
    uint32_t exception_saved_pc;
    uint16_t exception_saved_sr;
    bool exception_frame_active;
};

const struct amivm_machine_profile *amivm_machine_profile_by_id(
    enum amivm_machine_model id);
const struct amivm_machine_profile *amivm_machine_profile_by_name(
    const char *name);

void amivm_aga_advance_beam(struct amivm_vm *vm, unsigned cycles);
void amivm_vm_advance_cycles(struct amivm_vm *vm, unsigned cycles);
void amivm_vm_account_instruction(struct amivm_vm *vm, unsigned cycles);
int amivm_vm_attach_cpu_backend(struct amivm_vm *vm,
                                const struct amivm_cpu_backend *backend);
int amivm_vm_step(struct amivm_vm *vm);
void amivm_m68k_reset(struct amivm_vm *vm, uint32_t pc, uint16_t sr);
void amivm_m68k_request_irq(struct amivm_vm *vm, uint8_t level);
int amivm_m68k_service_irq(struct amivm_vm *vm);
int amivm_m68k_acknowledge_irq(struct amivm_vm *vm, uint8_t *vector);
int amivm_m68k_return_from_interrupt(struct amivm_vm *vm);
int amivm_m68k_enter_exception(struct amivm_vm *vm,
                                enum amivm_m68k_exception exception,
                                uint32_t return_pc);
int amivm_m68k_set_supervisor(struct amivm_vm *vm, bool supervisor);
bool amivm_m68k_is_supervisor(const struct amivm_vm *vm);
int amivm_m68k_execute_one(struct amivm_vm *vm);
int amivm_mmu_configure_page_table(struct amivm_vm *vm,
                                         uint32_t base, uint32_t mask,
                                         uint8_t page_shift, uint32_t entries);
int amivm_mmu_tt_match(const struct amivm_mmu_state *mmu,
                              uint32_t logical, bool write);
int amivm_mmu_translate(struct amivm_vm *vm, uint32_t logical,
                        bool write, uint32_t *physical);
uint32_t amivm_m68k_read_u32(struct amivm_vm *vm, uint32_t addr, bool *ok);
bool amivm_m68k_write_u32(struct amivm_vm *vm, uint32_t addr, uint32_t value);
int amivm_m68k_raise_exception(struct amivm_vm *vm,
                                      enum amivm_m68k_exception exception);



int amivm_config_resolve(const struct amivm_config *config,
                             struct amivm_resolved_config *resolved);
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

size_t amivm_device_count_for_hardware(
    const struct amivm_hardware_profile *hardware);
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
