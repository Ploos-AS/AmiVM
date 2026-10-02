#include "vm.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define AMIVM_DEVF_AGA  0x01u
#define AMIVM_DEVF_IDE  0x02u
#define AMIVM_DEVF_ZORRO 0x04u

static const struct amivm_device_desc amivm_devices[] = {
    {"vmserial", AMIVM_VMSERIAL_BASE, AMIVM_MMIO_PAGE_SIZE, 1u, 0u},
    {"irq", AMIVM_IRQ_BASE, AMIVM_MMIO_PAGE_SIZE, 0u, 0u},
    {"trackdisk", AMIVM_TRACKDISK_BASE, AMIVM_MMIO_PAGE_SIZE, 3u, 0u},
    {"timer", AMIVM_TIMER_BASE, AMIVM_MMIO_PAGE_SIZE, 6u, 0u},
    {"aga", AMIVM_MMIO_BASE + 0x5000u, AMIVM_MMIO_PAGE_SIZE, 7u, AMIVM_DEVF_AGA},
    {"ide", AMIVM_MMIO_BASE + 0x6000u, AMIVM_MMIO_PAGE_SIZE, 8u, AMIVM_DEVF_IDE},
    {"zorro", AMIVM_MMIO_BASE + 0x7000u, AMIVM_MMIO_PAGE_SIZE, 9u, AMIVM_DEVF_ZORRO},
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
    vm->device_count = resolved.device_count;
    memset(&vm->aga, 0, sizeof vm->aga);
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
    if (vm == NULL || value == NULL) {
        return false;
    }
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
    if (vm == NULL) {
        return false;
    }
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
