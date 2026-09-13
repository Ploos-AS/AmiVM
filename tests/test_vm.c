#include "vm.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_memory_map(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t value = 0;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    assert(amivm_vm_init(&vm, &config) == 0);

    assert(amivm_write8(&vm, AMIVM_RAM_BASE, 0x12u));
    assert(amivm_write8(&vm, AMIVM_RAM_BASE + (uint32_t)config.ram_size - 1u, 0x34u));
    assert(amivm_read8(&vm, AMIVM_RAM_BASE, &value) && value == 0x12u);
    assert(amivm_read8(&vm, AMIVM_RAM_BASE + (uint32_t)config.ram_size - 1u, &value) && value == 0x34u);
    assert(!amivm_read8(&vm, AMIVM_RAM_BASE + (uint32_t)config.ram_size, &value));
    assert(!amivm_write8(&vm, AMIVM_ROM_BASE, 0x55u));

    amivm_vm_destroy(&vm);
}

static void test_devices(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const struct amivm_device_desc *dev;
    uint8_t value = 0;

    assert(amivm_device_count() == 3u);

    dev = amivm_device_at(0u);
    assert(dev != NULL);
    assert(strcmp(dev->name, "vmserial") == 0);
    assert(dev->base == AMIVM_VMSERIAL_BASE);
    assert(dev->size == AMIVM_MMIO_PAGE_SIZE);
    assert(amivm_device_at(amivm_device_count()) == NULL);

    dev = amivm_find_device("timer");
    assert(dev != NULL && dev->base == AMIVM_TIMER_BASE);
    assert(amivm_find_device("missing") == NULL);

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    assert(amivm_vm_init(&vm, &config) == 0);

    assert(amivm_read8(&vm, AMIVM_VMSERIAL_BASE + 4u, &value));
    assert(value == 1u);

    amivm_raise_irq(&vm, 5u);
    assert(amivm_read8(&vm, AMIVM_IRQ_BASE, &value));
    assert((value & (1u << 5u)) != 0u);
    amivm_clear_irq(&vm, 5u);
    assert(vm.irq_pending == 0u);

    amivm_tick(&vm, 0x42u);
    assert(amivm_read8(&vm, AMIVM_TIMER_BASE, &value));
    assert(value == 0x42u);

    amivm_vm_destroy(&vm);
}

static void test_config(void)
{
    size_t bytes = 0;

    assert(amivm_parse_size_mib("128", &bytes));
    assert(bytes == 128u * 1024u * 1024u);
    assert(!amivm_parse_size_mib("0", &bytes));
    assert(!amivm_parse_size_mib("4096", &bytes));
    assert(!amivm_parse_size_mib("abc", &bytes));
    assert(!amivm_parse_size_mib(NULL, &bytes));
    assert(!amivm_parse_size_mib("128", NULL));
}

int main(void)
{
    test_memory_map();
    test_devices();
    test_config();
    puts("AmiVM M1 VM-core tests: PASS");
    return 0;
}
