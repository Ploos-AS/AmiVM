#include "vm.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static const struct amivm_device_desc amivm_devices[] = {
    {"vmserial", AMIVM_VMSERIAL_BASE, AMIVM_MMIO_PAGE_SIZE, 1u},
    {"timer", AMIVM_TIMER_BASE, AMIVM_MMIO_PAGE_SIZE, 2u},
    {"irq", AMIVM_IRQ_BASE, AMIVM_MMIO_PAGE_SIZE, 0u},
};

static bool in_range(uint32_t addr, uint32_t base, size_t size)
{
    uint64_t a = addr;
    uint64_t b = base;
    uint64_t e = b + size;
    return a >= b && a < e;
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

void amivm_config_init(struct amivm_config *config)
{
    config->ram_size = (size_t)AMIVM_DEFAULT_RAM_MIB * 1024u * 1024u;
    config->rom_path = NULL;
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

int amivm_vm_init(struct amivm_vm *vm, const struct amivm_config *config)
{
    if (vm == NULL || config == NULL || config->ram_size == 0u) {
        return -1;
    }

    memset(vm, 0, sizeof(*vm));
    vm->ram = calloc(1, config->ram_size);
    if (vm->ram == NULL) {
        return -1;
    }
    vm->ram_size = config->ram_size;
    vm->ram_page_count = (config->ram_size + AMIVM_RAM_PAGE_SIZE - 1u) /
                         AMIVM_RAM_PAGE_SIZE;
    vm->ram_page_generation = calloc(vm->ram_page_count, sizeof(*vm->ram_page_generation));
    if (vm->ram_page_generation == NULL) {
        amivm_vm_destroy(vm);
        return -1;
    }
    vm->memory_write_generation = 1u;
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
    if (addr == AMIVM_VMSERIAL_BASE) {
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
    if (addr == AMIVM_IRQ_BASE) {
        *value = (uint8_t)(vm->irq_pending & 0xffu);
        return true;
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
    if (addr == AMIVM_VMSERIAL_BASE) {
        fputc((int)value, stdout);
        fflush(stdout);
        return true;
    }
    if (addr == AMIVM_IRQ_BASE + 4u) {
        vm->irq_pending &= ~(uint32_t)value;
        return true;
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

void amivm_raise_irq(struct amivm_vm *vm, unsigned line)
{
    if (vm != NULL && line < 32u) {
        vm->irq_pending |= 1u << line;
    }
}

void amivm_clear_irq(struct amivm_vm *vm, unsigned line)
{
    if (vm != NULL && line < 32u) {
        vm->irq_pending &= ~(1u << line);
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

    fprintf(out, "machine=AmiVM-Hyper/040\n");
    fprintf(out, "cpu=m68040\n");
    fprintf(out, "mmu=required\n");
    fprintf(out, "fpu=required\n");
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
