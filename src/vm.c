#include "vm.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static bool in_range(uint32_t addr, uint32_t base, size_t size)
{
    uint64_t a = addr;
    uint64_t b = base;
    uint64_t e = b + size;
    return a >= b && a < e;
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

    if (text == NULL || *text == '\0') {
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
    memset(vm, 0, sizeof(*vm));
    vm->ram = calloc(1, config->ram_size);
    if (vm->ram == NULL) {
        return -1;
    }
    vm->ram_size = config->ram_size;
    if (config->rom_path != NULL && amivm_vm_load_rom(vm, config->rom_path) != 0) {
        amivm_vm_destroy(vm);
        return -1;
    }
    return 0;
}

void amivm_vm_destroy(struct amivm_vm *vm)
{
    free(vm->ram);
    memset(vm, 0, sizeof(*vm));
}

int amivm_vm_load_rom(struct amivm_vm *vm, const char *path)
{
    FILE *fp = fopen(path, "rb");
    size_t n;

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

bool amivm_read8(struct amivm_vm *vm, uint32_t addr, uint8_t *value)
{
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
    if (in_range(addr, AMIVM_RAM_BASE, vm->ram_size)) {
        vm->ram[(size_t)(addr - AMIVM_RAM_BASE)] = value;
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

void amivm_raise_irq(struct amivm_vm *vm, unsigned line)
{
    if (line < 32u) {
        vm->irq_pending |= 1u << line;
    }
}

void amivm_clear_irq(struct amivm_vm *vm, unsigned line)
{
    if (line < 32u) {
        vm->irq_pending &= ~(1u << line);
    }
}

void amivm_tick(struct amivm_vm *vm, uint64_t ticks)
{
    vm->timer_ticks += ticks;
}

void amivm_dump_machine(const struct amivm_vm *vm, FILE *out)
{
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
    fprintf(out, "devices=vmserial,timer,irq\n");
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

    amivm_vm_destroy(&vm);
    return 0;
}
