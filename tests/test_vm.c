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

static int test_machine_dump_profile(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    FILE *fp;
    char dump[2048];
    size_t n;

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68020");
    config.external_mmu = AMIVM_MMU_68851;
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    fp = tmpfile();
    CHECK(fp != NULL);
    amivm_dump_machine(&vm, fp);
    CHECK(fflush(fp) == 0);
    CHECK(fseek(fp, 0L, SEEK_SET) == 0);
    n = fread(dump, 1u, sizeof(dump) - 1u, fp);
    dump[n] = '\0';
    fclose(fp);

    CHECK(strstr(dump, "cpu=68020\n") != NULL);
    CHECK(strstr(dump, "isa_level=20\n") != NULL);
    CHECK(strstr(dump, "mmu=yes\n") != NULL);
    CHECK(strstr(dump, "fpu=no\n") != NULL);
    CHECK(strstr(dump, "hyper=no\n") != NULL);

    amivm_vm_destroy(&vm);
    return 0;
}

static int test_exception_frame_lifecycle(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x00100000u;
    static const struct {
        const char *name;
        uint8_t frame;
    } cases[] = {
        { "68020", AMIVM_FRAME_68020_BUS },
        { "68030", AMIVM_FRAME_68030_SHORT_BUS },
        { "68040", AMIVM_FRAME_68040_ACCESS },
        { "hyper040", AMIVM_FRAME_68040_ACCESS },
        { "68060", AMIVM_FRAME_68060_ACCESS },
        { "hyper060", AMIVM_FRAME_68060_ACCESS }
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        uint32_t pc = 0x00123456u + (uint32_t)(i * 0x100u);
        uint16_t sr = (uint16_t)(0x2700u + i);

        amivm_config_init(&config);
        config.ram_size = 2u * 1024u * 1024u;
        config.cpu_profile = amivm_cpu_profile_by_name(cases[i].name);
        CHECK(config.cpu_profile != NULL);
        CHECK(amivm_vm_init(&vm, &config) == 0);

        vm.m68k.a[7] = initial_sp;
        vm.m68k.pc = pc;
        vm.m68k.sr = sr;
        vm.mmu_exception_pc = pc;
        vm.mmu_exception_sr = sr;
        vm.exception_entry_vector = 56u;

        CHECK(amivm_m68k_set_exception_frame_type(&vm, cases[i].frame) == 0);
        CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
        CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
        CHECK(vm.exception_frame_type == cases[i].frame);
        CHECK(amivm_m68k_rte_mmu_exception(&vm) == 0);
        CHECK(vm.m68k.a[7] == initial_sp);
        CHECK(vm.m68k.pc == pc);
        CHECK(vm.m68k.sr == sr);

        amivm_vm_destroy(&vm);
    }

    return 0;
}


static int test_nested_exception_frames(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x00100000u;
    static const struct {
        const char *name;
        uint8_t frame;
    } cases[] = {
        { "68020", AMIVM_FRAME_68020_BUS },
        { "68030", AMIVM_FRAME_68030_SHORT_BUS },
        { "68040", AMIVM_FRAME_68040_ACCESS },
        { "hyper040", AMIVM_FRAME_68040_ACCESS },
        { "68060", AMIVM_FRAME_68060_ACCESS },
        { "hyper060", AMIVM_FRAME_68060_ACCESS }
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        const uint32_t first_pc = 0x00110000u + (uint32_t)(i * 0x1000u);
        const uint32_t second_pc = first_pc + 0x100u;
        const uint16_t first_sr = (uint16_t)(0x2500u + i);
        const uint16_t second_sr = (uint16_t)(0x2700u + i);

        amivm_config_init(&config);
        config.ram_size = 2u * 1024u * 1024u;
        config.cpu_profile = amivm_cpu_profile_by_name(cases[i].name);
        CHECK(config.cpu_profile != NULL);
        CHECK(amivm_vm_init(&vm, &config) == 0);

        vm.m68k.a[7] = initial_sp;
        vm.m68k.pc = first_pc;
        vm.m68k.sr = first_sr;
        vm.mmu_exception_pc = first_pc;
        vm.mmu_exception_sr = first_sr;
        vm.exception_entry_vector = 56u;

        CHECK(amivm_m68k_set_exception_frame_type(&vm, cases[i].frame) == 0);
        CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
        CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
        CHECK(vm.exception_depth == 1u);

        vm.m68k.pc = second_pc;
        vm.m68k.sr = second_sr;
        vm.mmu_exception_pc = second_pc;
        vm.mmu_exception_sr = second_sr;
        vm.exception_entry_vector = 57u;

        CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
        CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
        CHECK(vm.exception_depth == 2u);

        CHECK(amivm_m68k_rte_mmu_exception(&vm) == 0);
        CHECK(vm.exception_depth == 1u);
        CHECK(vm.exception_frame_type == cases[i].frame);
        CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);

        CHECK(amivm_m68k_rte_mmu_exception(&vm) == 0);
        CHECK(vm.exception_depth == 0u);
        CHECK(vm.m68k.a[7] == initial_sp);
        CHECK(vm.m68k.pc == first_pc);
        CHECK(vm.m68k.sr == first_sr);

        amivm_vm_destroy(&vm);
    }

    return 0;
}

static int test_nested_exception_negative_cases(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x00100000u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = initial_sp;
    vm.m68k.pc = 0x00111111u;
    vm.m68k.sr = 0x2500u;
    vm.mmu_exception_pc = vm.m68k.pc;
    vm.mmu_exception_sr = vm.m68k.sr;
    vm.exception_entry_vector = 56u;
    CHECK(amivm_m68k_set_exception_frame_type(&vm, AMIVM_FRAME_68040_ACCESS) == 0);
    CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
    CHECK(vm.exception_depth == 1u);

    vm.m68k.pc = 0x00222222u;
    vm.m68k.sr = 0x2700u;
    vm.mmu_exception_pc = vm.m68k.pc;
    vm.mmu_exception_sr = vm.m68k.sr;
    vm.exception_entry_vector = 57u;
    CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
    CHECK(vm.exception_depth == 2u);

    vm.exception_context_stack[0].frame_magic ^= 1u;
    CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
    CHECK(vm.exception_stack_fault);

    vm.exception_context_stack[0].frame_magic ^= 1u;
    vm.exception_context_stack[0].frame_format = 0xAu;
    CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
    CHECK(vm.exception_stack_fault);

    vm.exception_context_stack[0].frame_format = 0x7u;
    vm.exception_context_stack[0].frame_type = AMIVM_FRAME_68020_BUS;
    CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
    CHECK(vm.exception_stack_fault);

    vm.exception_context_stack[0].frame_type = AMIVM_FRAME_68040_ACCESS;
    vm.exception_context_stack[0].frame_sp = vm.ram_size - 1u;
    CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
    CHECK(vm.exception_stack_fault);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_exception_depth_boundary(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x00180000u;
    const uint8_t max_depth = 8u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = initial_sp;
    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x2700u;
    vm.mmu_exception_pc = vm.m68k.pc;
    vm.mmu_exception_sr = vm.m68k.sr;
    vm.exception_entry_vector = 56u;

    for (uint8_t depth = 0u; depth < max_depth; ++depth) {
        vm.mmu_exception_pc += 0x100u;
        vm.mmu_exception_sr = (uint16_t)(0x2700u + depth);
        vm.exception_entry_vector = (uint8_t)(56u + depth);
        CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
        CHECK(vm.exception_depth == (uint8_t)(depth + 1u));
    }

    {
        const uint32_t sp_before = vm.m68k.a[7];
        const uint8_t depth_before = vm.exception_depth;
        CHECK(amivm_m68k_stack_mmu_exception(&vm) != 0);
        CHECK(vm.exception_depth == depth_before);
        CHECK(vm.m68k.a[7] == sp_before);
    }

    for (uint8_t depth = max_depth; depth > 0u; --depth) {
        CHECK(amivm_m68k_rte_mmu_exception(&vm) == 0);
        CHECK(vm.exception_depth == (uint8_t)(depth - 1u));
    }

    CHECK(vm.exception_depth == 0u);
    CHECK(vm.m68k.a[7] == initial_sp);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_exception_stack_atomicity(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x00001000u;
    uint8_t before[64];
    uint8_t after[64];
    uint8_t value;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = initial_sp;
    vm.m68k.pc = 0x00123456u;
    vm.m68k.sr = 0x2700u;
    vm.mmu_exception_pc = vm.m68k.pc;
    vm.mmu_exception_sr = vm.m68k.sr;
    vm.exception_entry_vector = 56u;

    for (size_t i = 0u; i < sizeof before; ++i) {
        CHECK(amivm_read8(&vm, initial_sp - (uint32_t)sizeof before + (uint32_t)i, &value));
        before[i] = value;
    }

    CHECK(amivm_m68k_set_exception_frame_type(&vm, AMIVM_FRAME_68040_ACCESS) == 0);
    CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);

    /*
     * The next frame is deliberately placed where the requested frame cannot
     * fit.  The implementation must not leave a partial frame behind.
     */
    vm.m68k.a[7] = 20u;
    vm.exception_entry_vector = 57u;
    vm.mmu_exception_pc = 0x00222222u;
    vm.mmu_exception_sr = 0x2701u;

    CHECK(amivm_m68k_stack_mmu_exception(&vm) != 0);
    CHECK(vm.exception_stack_fault);
    CHECK(vm.m68k.a[7] == 20u);

    for (size_t i = 0u; i < sizeof after; ++i) {
        CHECK(amivm_read8(&vm, 20u - (uint32_t)sizeof after + (uint32_t)i, &value));
        after[i] = value;
    }
    CHECK(memcmp(before, after, sizeof before) == 0);

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
    if (test_memory_map() != 0 || test_devices() != 0 ||
        test_machine_dump_profile() != 0 || test_exception_frame_lifecycle() != 0 ||
        test_nested_exception_frames() != 0 || test_nested_exception_negative_cases() != 0 ||
        test_exception_depth_boundary() != 0 || test_exception_stack_atomicity() != 0 ||
        test_config() != 0) {
        return 1;
    }
    puts("AmiVM M2.92 nested exception-frame tests: PASS");
    return 0;
}
