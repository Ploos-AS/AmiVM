#include "vm.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static int test_memory_map(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t value = 0;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    CHECK(amivm_write8(&vm, AMIVM_RAM_BASE, 0x12u));
    CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + (uint32_t)config.ram_size - 1u, 0x34u));
    CHECK(amivm_read8(&vm, AMIVM_RAM_BASE, &value) && value == 0x12u);
    CHECK(amivm_read8(&vm, AMIVM_RAM_BASE + (uint32_t)config.ram_size - 1u, &value) && value == 0x34u);
    CHECK(!amivm_read8(&vm, AMIVM_RAM_BASE + (uint32_t)config.ram_size, &value));
    CHECK(!amivm_write8(&vm, AMIVM_ROM_BASE, 0x55u));

    amivm_vm_destroy(&vm);
    return 0;
}

static int test_devices(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const struct amivm_device_desc *dev;
    uint8_t value = 0;

    CHECK(amivm_device_count() == 3u);

    dev = amivm_device_at(0u);
    CHECK(dev != NULL);
    CHECK(strcmp(dev->name, "vmserial") == 0);
    CHECK(dev->base == AMIVM_VMSERIAL_BASE);
    CHECK(dev->size == AMIVM_MMIO_PAGE_SIZE);
    CHECK(amivm_device_at(amivm_device_count()) == NULL);

    dev = amivm_find_device("timer");
    CHECK(dev != NULL && dev->base == AMIVM_TIMER_BASE);
    CHECK(amivm_find_device("missing") == NULL);

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    CHECK(amivm_read8(&vm, AMIVM_VMSERIAL_BASE + 4u, &value));
    CHECK(value == 1u);

    amivm_raise_irq(&vm, 5u);
    CHECK(amivm_read8(&vm, AMIVM_IRQ_BASE, &value));
    CHECK((value & (1u << 5u)) != 0u);
    amivm_clear_irq(&vm, 5u);
    CHECK(vm.irq_pending == 0u);

    amivm_tick(&vm, 0x42u);
    CHECK(amivm_read8(&vm, AMIVM_TIMER_BASE, &value));
    CHECK(value == 0x42u);

    amivm_vm_destroy(&vm);
    return 0;
}

static int test_config(void)
{
    size_t bytes = 0;

    CHECK(amivm_parse_size_mib("128", &bytes));
    CHECK(bytes == 128u * 1024u * 1024u);
    CHECK(!amivm_parse_size_mib("0", &bytes));
    CHECK(!amivm_parse_size_mib("4096", &bytes));
    CHECK(!amivm_parse_size_mib("abc", &bytes));
    CHECK(!amivm_parse_size_mib(NULL, &bytes));
    CHECK(!amivm_parse_size_mib("128", NULL));
    return 0;
}

int main(void)
{
    if (test_memory_map() != 0 || test_devices() != 0 || test_config() != 0) {
        return 1;
    }
    puts("AmiVM M1 VM-core tests: PASS");
    return 0;
}
