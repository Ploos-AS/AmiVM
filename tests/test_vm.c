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

    {
        const uint8_t frame_type = vm.exception_frame_type;
        const uint8_t frame_format = vm.exception_frame_format;
        const uint8_t frame_size = vm.exception_frame_size;
        const uint8_t frame_words = vm.exception_frame_word_count;
        const uint16_t vector_offset = vm.exception_frame_vector_offset;
        const uint16_t format_vector = vm.exception_format_vector_word;
        const uint32_t frame_sp = vm.exception_frame_sp;
        const uint32_t frame_magic = vm.exception_frame_magic;
        const uint8_t fault_stage = vm.exception_fault_stage;
        const uint8_t depth = vm.exception_depth;

        CHECK(amivm_m68k_stack_mmu_exception(&vm) != 0);
        CHECK(vm.exception_stack_fault);
        CHECK(vm.m68k.a[7] == 20u);
        CHECK(vm.exception_frame_type == frame_type);
        CHECK(vm.exception_frame_format == frame_format);
        CHECK(vm.exception_frame_size == frame_size);
        CHECK(vm.exception_frame_word_count == frame_words);
        CHECK(vm.exception_frame_vector_offset == vector_offset);
        CHECK(vm.exception_format_vector_word == format_vector);
        CHECK(vm.exception_frame_sp == frame_sp);
        CHECK(vm.exception_frame_magic == frame_magic);
        CHECK(vm.exception_fault_stage == fault_stage);
        CHECK(vm.exception_depth == depth);
    }

    for (size_t i = 0u; i < sizeof after; ++i) {
        CHECK(amivm_read8(&vm, 20u - (uint32_t)sizeof after + (uint32_t)i, &value));
        after[i] = value;
    }
    CHECK(memcmp(before, after, sizeof before) == 0);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_nested_exception_atomicity(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x00100000u;
    uint32_t outer_sp;
    uint32_t outer_pc = 0x00111111u;
    uint16_t outer_sr = 0x2500u;
    uint8_t outer_type, outer_format, outer_size, outer_words;
    uint16_t outer_vector_offset, outer_format_vector;
    uint32_t outer_frame_sp, outer_magic;
    uint8_t outer_fault_stage, outer_depth;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = initial_sp;
    vm.m68k.pc = outer_pc;
    vm.m68k.sr = outer_sr;
    vm.mmu_exception_pc = outer_pc;
    vm.mmu_exception_sr = outer_sr;
    vm.exception_entry_vector = 56u;

    CHECK(amivm_m68k_set_exception_frame_type(&vm, AMIVM_FRAME_68040_ACCESS) == 0);
    CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
    CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
    CHECK(vm.exception_depth == 1u);

    outer_sp = vm.m68k.a[7];
    outer_type = vm.exception_frame_type;
    outer_format = vm.exception_frame_format;
    outer_size = vm.exception_frame_size;
    outer_words = vm.exception_frame_word_count;
    outer_vector_offset = vm.exception_frame_vector_offset;
    outer_format_vector = vm.exception_format_vector_word;
    outer_frame_sp = vm.exception_frame_sp;
    outer_magic = vm.exception_frame_magic;
    outer_fault_stage = vm.exception_fault_stage;
    outer_depth = vm.exception_depth;

    /* Force the second frame to fail before it can modify the outer frame. */
    vm.m68k.a[7] = 20u;
    vm.m68k.pc = 0x00222222u;
    vm.m68k.sr = 0x2701u;
    vm.mmu_exception_pc = vm.m68k.pc;
    vm.mmu_exception_sr = vm.m68k.sr;
    vm.exception_entry_vector = 57u;

    CHECK(amivm_m68k_stack_mmu_exception(&vm) != 0);
    CHECK(vm.exception_stack_fault);
    CHECK(vm.exception_depth == outer_depth);
    CHECK(vm.exception_frame_type == outer_type);
    CHECK(vm.exception_frame_format == outer_format);
    CHECK(vm.exception_frame_size == outer_size);
    CHECK(vm.exception_frame_word_count == outer_words);
    CHECK(vm.exception_frame_vector_offset == outer_vector_offset);
    CHECK(vm.exception_format_vector_word == outer_format_vector);
    CHECK(vm.exception_frame_sp == outer_frame_sp);
    CHECK(vm.exception_frame_magic == outer_magic);
    CHECK(vm.exception_fault_stage == outer_fault_stage);
    CHECK(vm.m68k.a[7] == 20u);

    /* Restore the outer frame pointer and prove it remains valid/RTE-able. */
    vm.m68k.a[7] = outer_sp;
    CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
    CHECK(amivm_m68k_rte_mmu_exception(&vm) == 0);
    CHECK(vm.exception_depth == 0u);
    CHECK(vm.m68k.a[7] == initial_sp);
    CHECK(vm.m68k.pc == outer_pc);
    CHECK(vm.m68k.sr == outer_sr);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_nested_context_stack_rollback(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x00100000u;
    struct amivm_exception_context saved;

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

    saved = vm.exception_context_stack[0];

    vm.m68k.a[7] = 20u;
    vm.m68k.pc = 0x00222222u;
    vm.m68k.sr = 0x2701u;
    vm.mmu_exception_pc = vm.m68k.pc;
    vm.mmu_exception_sr = vm.m68k.sr;
    vm.exception_entry_vector = 57u;

    CHECK(amivm_m68k_stack_mmu_exception(&vm) != 0);
    CHECK(vm.exception_depth == 1u);
    CHECK(memcmp(&vm.exception_context_stack[0], &saved, sizeof saved) == 0);

    vm.m68k.a[7] = saved.frame_sp;
    CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
    CHECK(amivm_m68k_rte_mmu_exception(&vm) == 0);
    CHECK(vm.exception_depth == 0u);
    CHECK(vm.m68k.a[7] == initial_sp);
    CHECK(vm.m68k.pc == 0x00111111u);
    CHECK(vm.m68k.sr == 0x2500u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_supervisor_user_transition(void)
{
    struct amivm_config config;
    struct amivm_vm vm;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    CHECK(amivm_m68k_set_supervisor(&vm, true) == 0);
    CHECK(amivm_m68k_is_supervisor(&vm));
    CHECK((vm.m68k.sr & 0x2000u) != 0u);

    CHECK(amivm_m68k_set_supervisor(&vm, false) == 0);
    CHECK(!amivm_m68k_is_supervisor(&vm));
    CHECK((vm.m68k.sr & 0x2000u) == 0u);

    CHECK(amivm_m68k_set_supervisor(&vm, true) == 0);
    CHECK(amivm_m68k_is_supervisor(&vm));

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_privilege_violation_rte(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t initial_pc = 0x00100000u;
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x00100000u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = initial_sp;
    vm.m68k.pc = initial_pc;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    CHECK(amivm_write16(&vm, initial_pc, 0x4e73u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 8u * 4u,
                        0x00123400u));

    CHECK(amivm_vm_step_interpreter(&vm) == 0);
    CHECK(vm.m68k.exception == AMIVM_M68K_EXC_PRIVILEGE);
    CHECK(vm.pending_exception_vector == 8u);
    CHECK(vm.m68k.supervisor);
    CHECK((vm.m68k.sr & 0x2000u) != 0u);
    CHECK(vm.m68k.pc == 0x00123400u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(!vm.m68k.supervisor);
    CHECK((vm.m68k.sr & 0x2000u) == 0u);
    CHECK(vm.m68k.pc == initial_pc);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_privilege_violation_atomicity(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t initial_pc = 0x00100000u;
    const uint32_t initial_sp = AMIVM_RAM_BASE + 0x00100000u;
    uint32_t d0, a0;
    uint16_t sr;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = initial_sp;
    vm.m68k.pc = initial_pc;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.d[0] = 0x12345678u;
    vm.m68k.a[0] = 0x00abcdefu;
    d0 = vm.m68k.d[0];
    a0 = vm.m68k.a[0];
    sr = vm.m68k.sr;

    CHECK(amivm_write16(&vm, initial_pc, 0x4e73u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 8u * 4u,
                        0x00123400u));

    CHECK(amivm_vm_step_interpreter(&vm) == 0);
    CHECK(vm.m68k.exception == AMIVM_M68K_EXC_PRIVILEGE);
    CHECK(vm.pending_exception_vector == 8u);
    CHECK(vm.m68k.d[0] == d0);
    CHECK(vm.m68k.a[0] == a0);
    CHECK(vm.m68k.supervisor);
    CHECK((vm.m68k.sr & 0x2000u) != 0u);
    CHECK(vm.exception_saved_pc == initial_pc);
    CHECK(vm.exception_saved_sr == sr);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == initial_pc);
    CHECK(vm.m68k.sr == sr);
    CHECK(vm.m68k.d[0] == d0);
    CHECK(vm.m68k.a[0] == a0);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_interrupt_request_priority(void)
{
    struct amivm_config config;
    struct amivm_vm vm;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    CHECK(!vm.irq_pending);
    amivm_m68k_request_irq(&vm, 3u);
    CHECK(vm.irq_pending);
    CHECK(vm.irq_level == 3u);

    amivm_m68k_request_irq(&vm, 1u);
    CHECK(vm.irq_level == 3u);

    amivm_m68k_request_irq(&vm, 7u);
    CHECK(vm.irq_level == 7u);

    amivm_m68k_request_irq(&vm, 0u);
    CHECK(vm.irq_level == 7u);
    amivm_m68k_request_irq(&vm, 8u);
    CHECK(vm.irq_level == 7u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_interrupt_acceptance_and_mask(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t vector = 0u;
    const uint32_t pc = 0x00100000u;
    const uint32_t sp = AMIVM_RAM_BASE + 0x00100000u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = sp;
    vm.m68k.pc = pc;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;

    CHECK(amivm_write32(&vm, vm.exception_vector_base + 27u * 4u,
                        0x00123400u));
    amivm_m68k_request_irq(&vm, 3u);

    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
    CHECK(vector == 27u);
    CHECK(!vm.irq_pending);
    CHECK(vm.irq_in_service);
    CHECK(vm.pending_exception_vector == 27u);
    CHECK(vm.m68k.pc == 0x00123400u);
    CHECK(vm.m68k.supervisor);
    CHECK((vm.m68k.sr & 0x2000u) != 0u);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 3u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(!vm.irq_in_service);
    CHECK(vm.m68k.pc == pc);
    CHECK(vm.m68k.sr == 0x0000u);
    CHECK(!vm.m68k.supervisor);

    amivm_m68k_request_irq(&vm, 3u);
    vm.m68k.sr = 0x0300u;
    CHECK(amivm_m68k_service_irq(&vm) == 0);
    CHECK(vm.irq_pending);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 3u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_interrupt_level_matrix(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t base_pc = 0x00100000u;
    const uint32_t sp = AMIVM_RAM_BASE + 0x00100000u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    for (uint8_t level = 1u; level <= 7u; ++level) {
        uint8_t vector = 0u;
        const uint32_t handler = 0x00120000u + ((uint32_t)level * 0x100u);

        vm.m68k.a[7] = sp;
        vm.m68k.pc = base_pc + ((uint32_t)level * 0x10u);
        vm.m68k.sr = 0x0000u;
        vm.m68k.supervisor = false;
        vm.irq_pending = false;
        vm.irq_in_service = false;

        CHECK(amivm_write32(&vm, vm.exception_vector_base +
                            ((uint32_t)(24u + level) * 4u), handler));
        amivm_m68k_request_irq(&vm, level);

        CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
        CHECK(vector == (uint8_t)(24u + level));
        CHECK(vm.pending_exception_vector == vector);
        CHECK(vm.m68k.pc == handler);
        CHECK(vm.m68k.supervisor);
        CHECK(((vm.m68k.sr >> 8) & 7u) == level);

        CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
        CHECK(!vm.irq_in_service);
        CHECK(vm.m68k.pc == base_pc + ((uint32_t)level * 0x10u));
        CHECK(vm.m68k.sr == 0x0000u);
        CHECK(!vm.m68k.supervisor);
    }

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_nested_interrupt_priority(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t vector = 0u;
    const uint32_t sp = AMIVM_RAM_BASE + 0x00100000u;
    const uint32_t base_pc = 0x00100000u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = sp;
    vm.m68k.pc = base_pc;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;

    CHECK(amivm_write32(&vm, vm.exception_vector_base + 26u * 4u,
                        0x00121000u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 30u * 4u,
                        0x00123000u));

    amivm_m68k_request_irq(&vm, 2u);
    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
    CHECK(vector == 26u);
    CHECK(vm.m68k.pc == 0x00121000u);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 2u);
    CHECK(vm.irq_in_service);

    /* IRQ 1 is masked while level 2 is active. */
    amivm_m68k_request_irq(&vm, 1u);
    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 0);
    CHECK(vm.irq_pending);
    CHECK(vm.irq_level == 1u);
    CHECK(vm.irq_in_service);

    /* IRQ 6 may preempt the level-2 handler. */
    amivm_m68k_request_irq(&vm, 6u);
    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
    CHECK(vector == 30u);
    CHECK(vm.m68k.pc == 0x00123000u);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 6u);
    CHECK(vm.irq_in_service);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00121000u);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 2u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_nested_interrupt_roundtrip(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t vector = 0u;
    const uint32_t initial_pc = 0x00100000u;
    const uint32_t sp = AMIVM_RAM_BASE + 0x00100000u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = sp;
    vm.m68k.pc = initial_pc;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;

    CHECK(amivm_write32(&vm, vm.exception_vector_base + 26u * 4u, 0x00121000u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 30u * 4u, 0x00123000u));

    amivm_m68k_request_irq(&vm, 2u);
    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
    CHECK(vector == 26u);
    CHECK(vm.m68k.pc == 0x00121000u);

    amivm_m68k_request_irq(&vm, 6u);
    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
    CHECK(vector == 30u);
    CHECK(vm.m68k.pc == 0x00123000u);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 6u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00121000u);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 2u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == initial_pc);
    CHECK(vm.m68k.sr == 0x0000u);
    CHECK(!vm.m68k.supervisor);
    CHECK(!vm.irq_in_service);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_interrupt_vector_failure_atomicity(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t vector = 0xa5u;
    const uint32_t pc = 0x00100000u;
    const uint16_t sr = 0x0000u;
    const uint32_t sp = AMIVM_RAM_BASE + 0x00100000u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = sp;
    vm.m68k.pc = pc;
    vm.m68k.sr = sr;
    vm.m68k.supervisor = false;
    amivm_m68k_request_irq(&vm, 7u);

    /*
     * Keep the vector entry inaccessible by moving the vector base outside
     * the VM address space. Acceptance must not leave a half-entered IRQ.
     */
    vm.exception_vector_base = vm.ram_size + 0x1000u;

    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) < 0);
    CHECK(vector == 0xa5u);
    CHECK(vm.irq_pending);
    CHECK(!vm.irq_in_service);
    CHECK(vm.m68k.pc == pc);
    CHECK(vm.m68k.sr == sr);
    CHECK(!vm.m68k.supervisor);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_interrupt_vector_failure_full_rollback(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t vector = 0x5au;
    const uint32_t pc = 0x00100000u;
    const uint16_t sr = 0x0400u;
    const uint32_t sp = AMIVM_RAM_BASE + 0x00100000u;
    const uint32_t saved_pc = 0x00abcdefu;
    const uint16_t saved_sr = 0x2300u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = sp;
    vm.m68k.pc = pc;
    vm.m68k.sr = sr;
    vm.m68k.supervisor = false;
    vm.irq_saved_pc = saved_pc;
    vm.irq_saved_sr = saved_sr;
    vm.irq_in_service = false;
    vm.pending_exception = AMIVM_M68K_EXC_ILLEGAL;
    vm.pending_exception_vector = 4u;
    amivm_m68k_request_irq(&vm, 7u);

    vm.exception_vector_base = vm.ram_size + 0x1000u;

    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) < 0);
    CHECK(vector == 0x5au);
    CHECK(vm.m68k.pc == pc);
    CHECK(vm.m68k.sr == sr);
    CHECK(!vm.m68k.supervisor);
    CHECK(!vm.irq_in_service);
    CHECK(vm.irq_pending);
    CHECK(vm.irq_level == 7u);
    CHECK(vm.irq_saved_pc == saved_pc);
    CHECK(vm.irq_saved_sr == saved_sr);
    CHECK(vm.pending_exception == AMIVM_M68K_EXC_ILLEGAL);
    CHECK(vm.pending_exception_vector == 4u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_service_irq_rollback(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t pc = 0x00100000u;
    const uint16_t sr = 0x0500u;
    const uint32_t sp = AMIVM_RAM_BASE + 0x00100000u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = sp;
    vm.m68k.pc = pc;
    vm.m68k.sr = sr;
    vm.m68k.supervisor = true;
    vm.pending_exception = AMIVM_M68K_EXC_ILLEGAL;
    vm.pending_exception_vector = 4u;
    amivm_m68k_request_irq(&vm, 6u);

    /* Masked IRQ must not mutate state. */
    CHECK(amivm_m68k_service_irq(&vm) == 0);
    CHECK(vm.irq_pending);
    CHECK(vm.irq_level == 6u);
    CHECK(vm.m68k.pc == pc);
    CHECK(vm.m68k.sr == sr);
    CHECK(vm.pending_exception == AMIVM_M68K_EXC_ILLEGAL);
    CHECK(vm.pending_exception_vector == 4u);

    /* Unmask, then make the vector table inaccessible. */
    vm.m68k.sr = 0x0000u;
    vm.exception_vector_base = vm.ram_size + 0x1000u;
    CHECK(amivm_m68k_service_irq(&vm) < 0);
    CHECK(vm.irq_pending);
    CHECK(vm.irq_level == 6u);
    CHECK(vm.m68k.pc == pc);
    CHECK(vm.m68k.sr == 0x0000u);
    CHECK(vm.pending_exception == AMIVM_M68K_EXC_ILLEGAL);
    CHECK(vm.pending_exception_vector == 4u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_irq_api_equivalence(void)
{
    struct amivm_config config;
    struct amivm_vm vm;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    for (uint8_t level = 1u; level <= 7u; ++level) {
        uint8_t vector = 0u;
        const uint8_t expected = (uint8_t)(24u + level);
        const uint32_t handler = 0x00130000u + ((uint32_t)level * 0x100u);

        vm.m68k.pc = 0x00100000u + level;
        vm.m68k.sr = 0x0000u;
        vm.m68k.supervisor = false;
        vm.irq_pending = false;
        vm.irq_in_service = false;
        CHECK(amivm_write32(&vm, vm.exception_vector_base +
                            ((uint32_t)expected * 4u), handler));

        amivm_m68k_request_irq(&vm, level);
        CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
        CHECK(vector == expected);
        CHECK(vm.m68k.pc == handler);
        CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);

        vm.m68k.pc = 0x00110000u + level;
        vm.m68k.sr = 0x0000u;
        vm.m68k.supervisor = false;
        vm.irq_pending = false;
        vm.irq_in_service = false;

        amivm_m68k_request_irq(&vm, level);
        CHECK(amivm_m68k_service_irq(&vm) == expected);
        CHECK(!vm.irq_pending);
        CHECK(vm.m68k.pc == handler);
        CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    }

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_irq_state_machine_integrity(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t vector = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    /* IDLE */
    CHECK(!vm.irq_pending);
    CHECK(!vm.irq_in_service);

    /* IDLE -> PENDING */
    amivm_m68k_request_irq(&vm, 3u);
    CHECK(vm.irq_pending);
    CHECK(vm.irq_level == 3u);
    CHECK(!vm.irq_in_service);

    /* PENDING -> IN_SERVICE */
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 27u * 4u,
                        0x00124000u));
    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
    CHECK(vector == 27u);
    CHECK(!vm.irq_pending);
    CHECK(vm.irq_in_service);

    /* IN_SERVICE -> NESTED */
    amivm_m68k_request_irq(&vm, 6u);
    CHECK(vm.irq_pending);
    CHECK(vm.irq_level == 6u);
    CHECK(vm.irq_in_service);
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 30u * 4u,
                        0x00126000u));
    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
    CHECK(vector == 30u);
    CHECK(!vm.irq_pending);
    CHECK(vm.irq_in_service);

    /* NESTED -> IN_SERVICE */
    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.irq_in_service);
    CHECK(vm.m68k.pc == 0x00124000u);

    /* IN_SERVICE -> IDLE */
    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(!vm.irq_in_service);
    CHECK(!vm.irq_pending);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_irq_invalid_transitions(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t vector = 0x7eu;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;

    /* RTE without an active IRQ must fail without changing CPU state. */
    CHECK(amivm_m68k_return_from_interrupt(&vm) != 0);
    CHECK(vm.m68k.pc == 0x00100000u);
    CHECK(vm.m68k.sr == 0x0000u);
    CHECK(!vm.irq_in_service);

    /* Invalid IRQ levels must not enter PENDING. */
    amivm_m68k_request_irq(&vm, 0u);
    CHECK(!vm.irq_pending);
    amivm_m68k_request_irq(&vm, 8u);
    CHECK(!vm.irq_pending);

    /* Lower priority IRQ cannot transition masked IN_SERVICE state. */
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 27u * 4u,
                        0x00124000u));
    amivm_m68k_request_irq(&vm, 3u);
    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
    CHECK(vector == 27u);
    CHECK(vm.irq_in_service);

    amivm_m68k_request_irq(&vm, 2u);
    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 0);
    CHECK(vm.irq_pending);
    CHECK(vm.irq_level == 2u);
    CHECK(vm.irq_in_service);
    CHECK(vm.m68k.pc == 0x00124000u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_instruction_irq_boundary(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t vector = 0u;
    const uint32_t pc = 0x00100000u;
    const uint32_t sp = AMIVM_RAM_BASE + 0x00100000u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = sp;
    vm.m68k.pc = pc;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;

    /* NOP followed by a pending IRQ. IRQ must be observed at the
       instruction boundary, not in the middle of the NOP. */
    CHECK(amivm_write16(&vm, pc, 0x4e71u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 25u * 4u,
                        0x00125000u));

    amivm_m68k_request_irq(&vm, 1u);
    CHECK(amivm_vm_step_interpreter(&vm) == 0);
    CHECK(vm.m68k.pc == pc + 2u);
    CHECK(vm.irq_pending);

    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
    CHECK(vector == 25u);
    CHECK(vm.m68k.pc == 0x00125000u);
    CHECK(vm.m68k.supervisor);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == pc + 2u);
    CHECK(!vm.m68k.supervisor);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_instruction_state_commit_before_irq(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t vector = 0u;
    const uint32_t pc = 0x00100000u;
    const uint32_t sp = AMIVM_RAM_BASE + 0x00100000u;
    const uint32_t d0 = 0x11223344u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = sp;
    vm.m68k.pc = pc;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.d[0] = d0;

    /* MOVEQ #1,D0: state-changing instruction. */
    CHECK(amivm_write16(&vm, pc, 0x7001u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 25u * 4u,
                        0x00126000u));

    amivm_m68k_request_irq(&vm, 1u);
    CHECK(amivm_vm_step_interpreter(&vm) == 0);
    CHECK(vm.m68k.d[0] == 1u);
    CHECK(vm.m68k.pc == pc + 2u);
    CHECK(vm.irq_pending);

    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
    CHECK(vector == 25u);
    CHECK(vm.m68k.pc == 0x00126000u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == pc + 2u);
    CHECK(vm.m68k.d[0] == 1u);
    CHECK(!vm.m68k.supervisor);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_pending_irq_across_instruction_sequence(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint8_t vector = 0u;
    const uint32_t pc = 0x00100000u;
    const uint32_t sp = AMIVM_RAM_BASE + 0x00100000u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.m68k.a[7] = sp;
    vm.m68k.pc = pc;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;

    /* MOVEQ #1,D0; MOVEQ #2,D1; NOP */
    CHECK(amivm_write16(&vm, pc + 0u, 0x7001u));
    CHECK(amivm_write16(&vm, pc + 2u, 0x7202u));
    CHECK(amivm_write16(&vm, pc + 4u, 0x4e71u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 25u * 4u,
                        0x00127000u));

    amivm_m68k_request_irq(&vm, 1u);

    CHECK(amivm_vm_step_interpreter(&vm) == 0);
    CHECK(vm.m68k.d[0] == 1u);
    CHECK(vm.m68k.d[1] == 0u);
    CHECK(vm.m68k.pc == pc + 2u);
    CHECK(vm.irq_pending);

    CHECK(amivm_vm_step_interpreter(&vm) == 0);
    CHECK(vm.m68k.d[0] == 1u);
    CHECK(vm.m68k.d[1] == 2u);
    CHECK(vm.m68k.pc == pc + 4u);
    CHECK(vm.irq_pending);

    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
    CHECK(vector == 25u);
    CHECK(vm.m68k.pc == 0x00127000u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == pc + 4u);
    CHECK(vm.m68k.d[0] == 1u);
    CHECK(vm.m68k.d[1] == 2u);
    CHECK(!vm.m68k.supervisor);

    amivm_vm_destroy(&vm);
    return 0;
}


static uint32_t test_cpu_backend_one_instruction(struct amivm_vm *vm, void *state)
{
    uint32_t *steps = (uint32_t *)state;
    ++(*steps);
    vm->m68k.pc += 2u;
    return 4u;
}

static int test_vm_step_backend_irq_boundary(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    backend.step = test_cpu_backend_one_instruction;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;

    CHECK(amivm_write32(&vm, vm.exception_vector_base + 25u * 4u,
                        0x00128000u));
    amivm_m68k_request_irq(&vm, 1u);

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 1u);
    CHECK(vm.last_instruction_cycles == 4u);
    CHECK(vm.m68k.pc == 0x00128000u);
    CHECK(vm.irq_in_service);
    CHECK(vm.m68k.supervisor);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100002u);
    CHECK(!vm.m68k.supervisor);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_vm_step_backend_pending_irq_sequence(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    backend.step = test_cpu_backend_one_instruction;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 25u * 4u,
                        0x00129000u));

    amivm_m68k_request_irq(&vm, 1u);

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 1u);
    CHECK(vm.m68k.pc == 0x00129000u);
    CHECK(vm.irq_in_service);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100002u);
    CHECK(!vm.m68k.supervisor);

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 2u);
    CHECK(vm.m68k.pc == 0x00100004u);
    CHECK(!vm.irq_pending);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_vm_step_irq_mask_until_boundary(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    backend.step = test_cpu_backend_one_instruction;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0300u; /* Mask levels 1-3. */
    vm.m68k.supervisor = true;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 25u * 4u,
                        0x0012a000u));

    amivm_m68k_request_irq(&vm, 1u);

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 1u);
    CHECK(vm.m68k.pc == 0x00100002u);
    CHECK(vm.irq_pending);
    CHECK(!vm.irq_in_service);

    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 2u);
    CHECK(vm.m68k.pc == 0x0012a000u);
    CHECK(vm.irq_in_service);
    CHECK(vm.m68k.supervisor);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100004u);
    CHECK(!vm.irq_pending);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_vm_step_nested_irq_preemption(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    backend.step = test_cpu_backend_one_instruction;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;

    CHECK(amivm_write32(&vm, vm.exception_vector_base + 26u * 4u,
                        0x0012b000u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 30u * 4u,
                        0x0012c000u));

    amivm_m68k_request_irq(&vm, 2u);
    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 1u);
    CHECK(vm.m68k.pc == 0x0012b000u);
    CHECK(vm.irq_in_service);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 2u);

    amivm_m68k_request_irq(&vm, 6u);
    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 2u);
    CHECK(vm.m68k.pc == 0x0012c000u);
    CHECK(vm.irq_in_service);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 6u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x0012b002u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100004u);
    CHECK(!vm.irq_in_service);

    amivm_vm_destroy(&vm);
    return 0;
}


static uint32_t test_cpu_backend_stateful_instruction(struct amivm_vm *vm, void *state)
{
    uint32_t *steps = (uint32_t *)state;
    ++(*steps);
    vm->m68k.d[*steps - 1u] = 0x1000u + *steps;
    vm->m68k.pc += 2u;
    return 4u;
}

static int test_vm_step_nested_irq_state_commit(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    backend.step = test_cpu_backend_stateful_instruction;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;

    CHECK(amivm_write32(&vm, vm.exception_vector_base + 26u * 4u,
                        0x0012d000u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 30u * 4u,
                        0x0012e000u));

    amivm_m68k_request_irq(&vm, 2u);
    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(vm.m68k.d[0] == 0x1001u);
    CHECK(vm.m68k.pc == 0x0012d000u);

    amivm_m68k_request_irq(&vm, 6u);
    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(vm.m68k.d[1] == 0x1002u);
    CHECK(vm.m68k.pc == 0x0012e000u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x0012d002u);
    CHECK(vm.m68k.d[0] == 0x1001u);
    CHECK(vm.m68k.d[1] == 0x1002u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100002u);
    CHECK(vm.m68k.d[0] == 0x1001u);
    CHECK(vm.m68k.d[1] == 0x1002u);
    CHECK(!vm.m68k.supervisor);

    amivm_vm_destroy(&vm);
    return 0;
}


static uint32_t test_cpu_backend_exception_then_irq(struct amivm_vm *vm, void *state)
{
    uint32_t *steps = (uint32_t *)state;
    ++(*steps);
    if (*steps == 1u) {
        vm->m68k.pc += 2u;
        vm->m68k.d[0] = 0xfeedu;
        CHECK(amivm_m68k_enter_exception(vm, 4u) == 4);
    } else {
        vm->m68k.pc += 2u;
        vm->m68k.d[1] = 0xbeefu;
    }
    return 4u;
}

static int test_vm_step_exception_irq_interleave(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    backend.step = test_cpu_backend_exception_then_irq;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;

    CHECK(amivm_write32(&vm, vm.exception_vector_base + 25u * 4u,
                        0x0012f000u));

    amivm_m68k_request_irq(&vm, 1u);
    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 1u);
    CHECK(vm.m68k.d[0] == 0xfeedu);
    CHECK(vm.m68k.pc == 0x00100002u);
    CHECK(vm.irq_pending);
    CHECK(vm.exception_frame_active);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(!vm.exception_frame_active);
    CHECK(vm.m68k.pc == 0x00100002u);

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 2u);
    CHECK(vm.m68k.d[1] == 0xbeefu);
    CHECK(vm.m68k.pc == 0x0012f000u);
    CHECK(vm.irq_in_service);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100004u);
    CHECK(!vm.m68k.supervisor);

    amivm_vm_destroy(&vm);
    return 0;
}


static uint32_t test_cpu_backend_irq_then_exception(struct amivm_vm *vm, void *state)
{
    uint32_t *steps = (uint32_t *)state;
    ++(*steps);
    if (*steps == 1u) {
        vm->m68k.pc += 2u;
        vm->m68k.d[0] = 0xcafeu;
    } else {
        vm->m68k.pc += 2u;
        vm->m68k.d[1] = 0xbabeu;
        CHECK(amivm_m68k_enter_exception(vm, 4u) == 4);
    }
    return 4u;
}

static int test_vm_step_irq_exception_reverse_interleave(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    backend.step = test_cpu_backend_irq_then_exception;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;

    CHECK(amivm_write32(&vm, vm.exception_vector_base + 25u * 4u,
                        0x00131000u));

    amivm_m68k_request_irq(&vm, 1u);
    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 1u);
    CHECK(vm.m68k.d[0] == 0xcafeu);
    CHECK(vm.m68k.pc == 0x00131000u);
    CHECK(vm.irq_in_service);

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 2u);
    CHECK(vm.m68k.d[1] == 0xbabeu);
    CHECK(vm.exception_frame_active);
    CHECK(vm.m68k.pc == 0x00131002u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(!vm.exception_frame_active);
    CHECK(vm.m68k.pc == 0x00131002u);
    CHECK(vm.irq_in_service);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(!vm.irq_in_service);
    CHECK(vm.m68k.pc == 0x00100002u);
    CHECK(!vm.m68k.supervisor);

    amivm_vm_destroy(&vm);
    return 0;
}


static uint32_t test_cpu_backend_mmu_fault_then_irq(struct amivm_vm *vm, void *state)
{
    uint32_t *steps = (uint32_t *)state;
    uint32_t physical = 0u;
    ++(*steps);

    if (*steps == 1u) {
        vm->mmu.enabled = true;
        vm->mmu.root_index_bits = 2u;
        vm->mmu.leaf_index_bits = 2u;
        vm->mmu.root_table_base = AMIVM_RAM_BASE + 0x1000u;
        vm->mmu_fault_address = 0x00456000u;
        vm->mmu_fault_status = 0u;
        CHECK(amivm_mmu_translate(vm, vm->mmu_fault_address, false,
                                  &physical) != 0);
        CHECK(vm->mmu.last_fault == AMIVM_MMU_FAULT_INVALID);
        CHECK(amivm_m68k_stack_mmu_exception(vm) == 0);
        CHECK(amivm_write32(vm, vm->exception_vector_base + 56u * 4u,
                            0x00132000u));
        vm->pending_exception = AMIVM_M68K_EXC_MMU_FAULT;
        vm->pending_exception_vector = 56u;
        vm->m68k.sr |= 0x2000u;
        vm->m68k.supervisor = true;
        vm->m68k.pc = 0x00132000u;
    }
    return 4u;
}

static int test_vm_step_mmu_fault_irq_interleave(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    backend.step = test_cpu_backend_mmu_fault_then_irq;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 25u * 4u,
                        0x00133000u));

    amivm_m68k_request_irq(&vm, 1u);
    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 1u);
    CHECK(vm.m68k.pc == 0x00133000u);
    CHECK(vm.irq_in_service);
    CHECK(vm.pending_exception == AMIVM_M68K_EXC_MMU_FAULT);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00132000u);
    CHECK(vm.irq_in_service);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100000u);
    CHECK(!vm.m68k.supervisor);

    amivm_vm_destroy(&vm);
    return 0;
}


static uint32_t test_cpu_backend_real_mmu_data_access(struct amivm_vm *vm, void *state)
{
    uint32_t *steps = (uint32_t *)state;
    uint32_t physical = 0u;
    bool ok = false;

    ++(*steps);
    if (*steps == 1u) {
        vm->mmu.enabled = true;
        vm->mmu.root_index_bits = 2u;
        vm->mmu.leaf_index_bits = 2u;
        vm->mmu.root_table_base = AMIVM_RAM_BASE + 0x1000u;
        vm->mmu_fault_address = 0x00456000u;

        /* The CPU performs a real data read through the VM MMU interface. */
        (void)amivm_m68k_read_u32(vm, vm->mmu_fault_address, &ok);
        CHECK(!ok);
        CHECK(vm->mmu.last_fault == AMIVM_MMU_FAULT_INVALID);

        CHECK(amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_MMU_FAULT) == 0);
    }
    physical = physical;
    return 4u;
}

static int test_vm_step_real_mmu_data_fault(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    backend.step = test_cpu_backend_real_mmu_data_access;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;

    CHECK(amivm_write32(&vm, vm.exception_vector_base + 56u * 4u,
                        0x00134000u));

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 1u);
    CHECK(vm.mmufault_count > 0u || vm.mmu.last_fault == AMIVM_MMU_FAULT_INVALID);
    CHECK(vm.pending_exception == AMIVM_M68K_EXC_MMU_FAULT);
    CHECK(vm.m68k.pc == 0x00134000u);
    CHECK(vm.m68k.supervisor);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100000u);
    CHECK(!vm.m68k.supervisor);

    amivm_vm_destroy(&vm);
    return 0;
}


static uint32_t test_cpu_backend_real_mmu_write_fault(struct amivm_vm *vm, void *state)
{
    uint32_t *steps = (uint32_t *)state;
    uint32_t physical = 0u;
    bool ok = false;

    ++(*steps);
    if (*steps == 1u) {
        const uint32_t logical = 0x00456000u;
        const uint32_t root = AMIVM_RAM_BASE + 0x1000u;
        const uint32_t leaf = AMIVM_RAM_BASE + 0x2000u;
        const uint32_t root_index = (logical >> (2u + 12u)) & 3u;
        const uint32_t leaf_index = (logical >> 12u) & 3u;

        vm->mmu.enabled = true;
        vm->mmu.root_index_bits = 2u;
        vm->mmu.leaf_index_bits = 2u;
        vm->mmu.root_table_base = root;

        CHECK(amivm_write32(vm, root + root_index * 4u, leaf | 1u));
        CHECK(amivm_write32(vm, leaf + leaf_index * 4u,
                            (AMIVM_RAM_BASE + 0x3000u) | 1u | 2u));

        CHECK(amivm_m68k_write_u32(vm, logical, 0x12345678u) == false);
        CHECK(vm->mmu.last_fault == AMIVM_MMU_FAULT_WRITE_PROTECT);

        CHECK(amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_MMU_FAULT) == 0);
        (void)physical;
    }
    return 4u;
}

static int test_vm_step_real_mmu_write_fault(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    backend.step = test_cpu_backend_real_mmu_write_fault;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;

    CHECK(amivm_write32(&vm, vm.exception_vector_base + 56u * 4u,
                        0x00135000u));

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 1u);
    CHECK(vm.mmu.last_fault == AMIVM_MMU_FAULT_WRITE_PROTECT);
    CHECK(vm.pending_exception == AMIVM_M68K_EXC_MMU_FAULT);
    CHECK(vm.m68k.pc == 0x00135000u);
    CHECK(vm.m68k.supervisor);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100000u);
    CHECK(!vm.m68k.supervisor);

    amivm_vm_destroy(&vm);
    return 0;
}


static uint32_t test_cpu_backend_real_mmu_user_fault(struct amivm_vm *vm, void *state)
{
    uint32_t *steps = (uint32_t *)state;
    const uint32_t logical = 0x00456000u;
    const uint32_t root = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t leaf = AMIVM_RAM_BASE + 0x2000u;
    const uint32_t root_index = (logical >> (2u + 12u)) & 3u;
    const uint32_t leaf_index = (logical >> 12u) & 3u;
    bool ok = false;

    ++(*steps);
    if (*steps == 1u) {
        vm->mmu.enabled = true;
        vm->mmu.root_index_bits = 2u;
        vm->mmu.leaf_index_bits = 2u;
        vm->mmu.root_table_base = root;
        CHECK(amivm_write32(vm, root + root_index * 4u, leaf | 1u));
        CHECK(amivm_write32(vm, leaf + leaf_index * 4u,
                            (AMIVM_RAM_BASE + 0x3000u) | 1u));
        vm->m68k.sr &= (uint16_t)~0x2000u;
        vm->m68k.supervisor = false;
        (void)amivm_m68k_read_u32(vm, logical, &ok);
        CHECK(!ok);
        CHECK(vm->mmu.last_fault == AMIVM_MMU_FAULT_SUPERVISOR);
        CHECK(amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_MMU_FAULT) == 0);
    }
    return 4u;
}

static int test_vm_step_real_mmu_user_fault(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);
    backend.step = test_cpu_backend_real_mmu_user_fault;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 56u * 4u, 0x00136000u));

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 1u);
    CHECK(vm.mmu.last_fault == AMIVM_MMU_FAULT_SUPERVISOR);
    CHECK(vm.pending_exception == AMIVM_M68K_EXC_MMU_FAULT);
    CHECK(vm.m68k.pc == 0x00136000u);
    CHECK(vm.m68k.supervisor);
    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100000u);
    CHECK(!vm.m68k.supervisor);

    amivm_vm_destroy(&vm);
    return 0;
}


static uint32_t test_cpu_backend_mmu_fault_nested_irq(struct amivm_vm *vm, void *state)
{
    uint32_t *steps = (uint32_t *)state;
    bool ok = false;
    ++(*steps);
    if (*steps == 1u) {
        vm->mmu.enabled = true;
        vm->mmu.root_index_bits = 2u;
        vm->mmu.leaf_index_bits = 2u;
        vm->mmu.root_table_base = AMIVM_RAM_BASE + 0x1000u;
        (void)amivm_m68k_read_u32(vm, 0x00456000u, &ok);
        CHECK(!ok);
        CHECK(vm->mmu.last_fault == AMIVM_MMU_FAULT_INVALID);
        CHECK(amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_MMU_FAULT) == 0);
    } else {
        vm->m68k.pc += 2u;
    }
    return 4u;
}

static int test_vm_step_mmu_fault_nested_irq(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);
    backend.step = test_cpu_backend_mmu_fault_nested_irq;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 56u * 4u, 0x00137000u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 30u * 4u, 0x00138000u));

    amivm_m68k_request_irq(&vm, 6u);
    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 1u);
    CHECK(vm.pending_exception == AMIVM_M68K_EXC_MMU_FAULT);
    CHECK(vm.m68k.pc == 0x00138000u);
    CHECK(vm.irq_in_service);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00137000u);
    CHECK(vm.irq_in_service);
    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100000u);
    CHECK(!vm.m68k.supervisor);

    amivm_vm_destroy(&vm);
    return 0;
}


static uint32_t test_cpu_backend_mmu_irq_priority(struct amivm_vm *vm, void *state)
{
    uint32_t *steps = (uint32_t *)state;
    bool ok = false;
    ++(*steps);
    if (*steps == 1u) {
        vm->mmu.enabled = true;
        vm->mmu.root_index_bits = 2u;
        vm->mmu.leaf_index_bits = 2u;
        vm->mmu.root_table_base = AMIVM_RAM_BASE + 0x1000u;
        (void)amivm_m68k_read_u32(vm, 0x00456000u, &ok);
        CHECK(!ok);
        CHECK(vm->mmu.last_fault == AMIVM_MMU_FAULT_INVALID);
        CHECK(amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_MMU_FAULT) == 0);
    }
    return 4u;
}

static int test_vm_step_mmu_irq_priority(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);
    backend.step = test_cpu_backend_mmu_irq_priority;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;

    CHECK(amivm_write32(&vm, vm.exception_vector_base + 56u * 4u, 0x00139000u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 25u * 4u, 0x0013a000u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 30u * 4u, 0x0013b000u));

    amivm_m68k_request_irq(&vm, 1u);
    amivm_m68k_request_irq(&vm, 6u);
    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(vm.m68k.pc == 0x0013b000u);
    CHECK(vm.irq_in_service);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 6u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x0013b000u);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 6u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100000u);

    amivm_vm_destroy(&vm);
    return 0;
}


static uint32_t test_cpu_backend_mmu_irq_deferred(struct amivm_vm *vm, void *state)
{
    uint32_t *steps = (uint32_t *)state;
    bool ok = false;
    ++(*steps);
    if (*steps == 1u) {
        vm->mmu.enabled = true;
        vm->mmu.root_index_bits = 2u;
        vm->mmu.leaf_index_bits = 2u;
        vm->mmu.root_table_base = AMIVM_RAM_BASE + 0x1000u;
        (void)amivm_m68k_read_u32(vm, 0x00456000u, &ok);
        CHECK(!ok);
        CHECK(vm->mmu.last_fault == AMIVM_MMU_FAULT_INVALID);
        CHECK(amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_MMU_FAULT) == 0);
    } else {
        vm->m68k.pc += 2u;
    }
    return 4u;
}

static int test_vm_step_mmu_irq_deferred(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);
    backend.step = test_cpu_backend_mmu_irq_deferred;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;

    CHECK(amivm_write32(&vm, vm.exception_vector_base + 56u * 4u, 0x0013c000u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 25u * 4u, 0x0013d000u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 30u * 4u, 0x0013e000u));

    amivm_m68k_request_irq(&vm, 1u);
    amivm_m68k_request_irq(&vm, 6u);
    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(vm.m68k.pc == 0x0013e000u);
    CHECK(vm.irq_in_service);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 6u);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x0013e000u);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 6u);

    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(vm.m68k.pc == 0x0013d000u);
    CHECK(((vm.m68k.sr >> 8) & 7u) == 1u);
    CHECK(vm.irq_in_service);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100000u);
    CHECK(!vm.irq_in_service);

    amivm_vm_destroy(&vm);
    return 0;
}


static uint32_t test_cpu_backend_mmu_fault_recovery(struct amivm_vm *vm, void *state)
{
    uint32_t *steps = (uint32_t *)state;
    const uint32_t logical = 0x00456000u;
    const uint32_t root = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t leaf = AMIVM_RAM_BASE + 0x2000u;
    const uint32_t root_index = (logical >> (2u + 12u)) & 3u;
    const uint32_t leaf_index = (logical >> 12u) & 3u;
    bool ok = false;

    ++(*steps);
    if (*steps == 1u) {
        vm->mmu.enabled = true;
        vm->mmu.root_index_bits = 2u;
        vm->mmu.leaf_index_bits = 2u;
        vm->mmu.root_table_base = root;
        CHECK(amivm_write32(vm, root + root_index * 4u, leaf | 1u));
        (void)amivm_m68k_read_u32(vm, logical, &ok);
        CHECK(!ok);
        CHECK(vm->mmu.last_fault == AMIVM_MMU_FAULT_INVALID);
        CHECK(amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_MMU_FAULT) == 0);
    } else if (*steps == 2u) {
        CHECK(amivm_write32(vm, leaf + leaf_index * 4u,
                            (AMIVM_RAM_BASE + 0x3000u) | 1u));
        (void)amivm_m68k_read_u32(vm, logical, &ok);
        CHECK(ok);
        CHECK(vm->mmu.last_fault == AMIVM_MMU_FAULT_NONE);
        vm->m68k.pc += 2u;
    }
    return 4u;
}

static int test_vm_step_mmu_fault_recovery(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;
    bool ok = false;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    backend.step = test_cpu_backend_mmu_fault_recovery;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 56u * 4u, 0x0013f000u));

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(vm.pending_exception == AMIVM_M68K_EXC_MMU_FAULT);
    CHECK(vm.m68k.pc == 0x0013f000u);
    CHECK(vm.m68k.supervisor);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100000u);
    CHECK(!vm.m68k.supervisor);

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 2u);
    CHECK(vm.mmu.last_fault == AMIVM_MMU_FAULT_NONE);
    CHECK(vm.m68k.pc == 0x00100002u);
    CHECK(amivm_m68k_read_u32(vm.m68k.a[7], &ok), true);

    amivm_vm_destroy(&vm);
    return 0;
}


static uint32_t test_cpu_backend_mmu_recovery_pending_irq(struct amivm_vm *vm, void *state)
{
    uint32_t *steps = (uint32_t *)state;
    const uint32_t logical = 0x00456000u;
    const uint32_t root = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t leaf = AMIVM_RAM_BASE + 0x2000u;
    const uint32_t root_index = (logical >> (2u + 12u)) & 3u;
    const uint32_t leaf_index = (logical >> 12u) & 3u;
    bool ok = false;

    ++(*steps);
    if (*steps == 1u) {
        vm->mmu.enabled = true;
        vm->mmu.root_index_bits = 2u;
        vm->mmu.leaf_index_bits = 2u;
        vm->mmu.root_table_base = root;
        CHECK(amivm_write32(vm, root + root_index * 4u, leaf | 1u));
        (void)amivm_m68k_read_u32(vm, logical, &ok);
        CHECK(!ok);
        CHECK(vm->mmu.last_fault == AMIVM_MMU_FAULT_INVALID);
        CHECK(amivm_m68k_raise_exception(vm, AMIVM_M68K_EXC_MMU_FAULT) == 0);
    } else if (*steps == 2u) {
        CHECK(amivm_write32(vm, leaf + leaf_index * 4u,
                            (AMIVM_RAM_BASE + 0x3000u) | 1u));
        (void)amivm_m68k_read_u32(vm, logical, &ok);
        CHECK(ok);
        CHECK(vm->mmu.last_fault == AMIVM_MMU_FAULT_NONE);
        vm->m68k.pc += 2u;
    }
    return 4u;
}

static int test_vm_step_mmu_recovery_pending_irq(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_backend backend;
    uint32_t steps = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    backend.step = test_cpu_backend_mmu_recovery_pending_irq;
    backend.state = &steps;
    CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);

    vm.m68k.pc = 0x00100000u;
    vm.m68k.sr = 0x0000u;
    vm.m68k.supervisor = false;
    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x00100000u;

    CHECK(amivm_write32(&vm, vm.exception_vector_base + 56u * 4u, 0x00140000u));
    CHECK(amivm_write32(&vm, vm.exception_vector_base + 30u * 4u, 0x00141000u));

    amivm_m68k_request_irq(&vm, 6u);

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00141000u);
    CHECK(vm.irq_in_service);
    CHECK(vm.pending_exception == AMIVM_M68K_EXC_MMU_FAULT);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00141000u);
    CHECK(vm.irq_in_service);

    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
    CHECK(vm.m68k.pc == 0x00100000u);
    CHECK(!vm.irq_in_service);

    CHECK(amivm_vm_step(&vm) == 0);
    CHECK(steps == 2u);
    CHECK(vm.mmu.last_fault == AMIVM_MMU_FAULT_NONE);
    CHECK(vm.m68k.pc == 0x00100002u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_mmu_translation_cache_invalidation(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t logical = 0x00456000u;
    const uint32_t root = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t leaf = AMIVM_RAM_BASE + 0x2000u;
    const uint32_t root_index = (logical >> (2u + 12u)) & 3u;
    const uint32_t leaf_index = (logical >> 12u) & 3u;
    uint32_t physical = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.mmu.enabled = true;
    vm.mmu.root_index_bits = 2u;
    vm.mmu.leaf_index_bits = 2u;
    vm.mmu.root_table_base = root;

    CHECK(amivm_write32(&vm, root + root_index * 4u, leaf | 1u));
    CHECK(amivm_write32(&vm, leaf + leaf_index * 4u,
                        (AMIVM_RAM_BASE + 0x3000u) | 1u | 4u));
    CHECK(amivm_mmu_translate(&vm, logical, false, &physical) == 0);
    CHECK(physical == AMIVM_RAM_BASE + 0x3000u);
    CHECK(vm.mmu.translation_cache_valid);

    CHECK(amivm_write32(&vm, leaf + leaf_index * 4u,
                        (AMIVM_RAM_BASE + 0x4000u) | 1u | 4u));
    CHECK(amivm_mmu_translate(&vm, logical, false, &physical) == 0);
    CHECK(physical == AMIVM_RAM_BASE + 0x3000u);

    amivm_mmu_translation_cache_invalidate(&vm);
    CHECK(!vm.mmu.translation_cache_valid);
    CHECK(amivm_mmu_translate(&vm, logical, false, &physical) == 0);
    CHECK(physical == AMIVM_RAM_BASE + 0x4000u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_mmu_page_table_changed_hook(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t logical = 0x00456000u;
    const uint32_t root = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t leaf = AMIVM_RAM_BASE + 0x2000u;
    const uint32_t root_index = (logical >> (2u + 12u)) & 3u;
    const uint32_t leaf_index = (logical >> 12u) & 3u;
    uint32_t physical = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.mmu.enabled = true;
    vm.mmu.root_index_bits = 2u;
    vm.mmu.leaf_index_bits = 2u;
    vm.mmu.root_table_base = root;
    CHECK(amivm_write32(&vm, root + root_index * 4u, leaf | 1u));
    CHECK(amivm_write32(&vm, leaf + leaf_index * 4u, (AMIVM_RAM_BASE + 0x3000u) | 1u | 4u));
    CHECK(amivm_mmu_translate(&vm, logical, false, &physical) == 0);
    CHECK(vm.mmu.translation_cache_valid);
    amivm_mmu_page_table_changed(&vm, logical);
    CHECK(!vm.mmu.translation_cache_valid);
    CHECK(amivm_write32(&vm, leaf + leaf_index * 4u, (AMIVM_RAM_BASE + 0x4000u) | 1u | 4u));
    CHECK(amivm_mmu_translate(&vm, logical, false, &physical) == 0);
    CHECK(physical == AMIVM_RAM_BASE + 0x4000u);
    amivm_mmu_page_table_changed(&vm, logical + 0x1000u);
    CHECK(vm.mmu.translation_cache_valid);
    amivm_mmu_page_table_changed(&vm, logical);
    CHECK(!vm.mmu.translation_cache_valid);
    amivm_vm_destroy(&vm);
    return 0;
}


static int test_mmu_ram_write_auto_invalidate(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t logical = 0x00456000u;
    const uint32_t root = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t leaf = AMIVM_RAM_BASE + 0x2000u;
    const uint32_t root_index = (logical >> (2u + 12u)) & 3u;
    const uint32_t leaf_index = (logical >> 12u) & 3u;
    uint32_t physical = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.mmu.enabled = true;
    vm.mmu.root_index_bits = 2u;
    vm.mmu.leaf_index_bits = 2u;
    vm.mmu.root_table_base = root;
    CHECK(amivm_write32(&vm, root + root_index * 4u, leaf | 1u));
    CHECK(amivm_write32(&vm, leaf + leaf_index * 4u,
                        (AMIVM_RAM_BASE + 0x3000u) | 1u | 4u));
    CHECK(amivm_mmu_translate(&vm, logical, false, &physical) == 0);
    CHECK(vm.mmu.translation_cache_valid);

    CHECK(amivm_write32(&vm, leaf + leaf_index * 4u,
                        (AMIVM_RAM_BASE + 0x4000u) | 1u | 4u));
    CHECK(!vm.mmu.translation_cache_valid);
    CHECK(amivm_mmu_translate(&vm, logical, false, &physical) == 0);
    CHECK(physical == AMIVM_RAM_BASE + 0x4000u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_mmu_ram_write_preserves_correctness(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t logical = 0x00456000u;
    const uint32_t unrelated = 0x00567000u;
    const uint32_t root = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t leaf = AMIVM_RAM_BASE + 0x2000u;
    const uint32_t root_index = (logical >> (2u + 12u)) & 3u;
    const uint32_t leaf_index = (logical >> 12u) & 3u;
    uint32_t physical = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.mmu.enabled = true;
    vm.mmu.root_index_bits = 2u;
    vm.mmu.leaf_index_bits = 2u;
    vm.mmu.root_table_base = root;
    CHECK(amivm_write32(&vm, root + root_index * 4u, leaf | 1u));
    CHECK(amivm_write32(&vm, leaf + leaf_index * 4u,
                        (AMIVM_RAM_BASE + 0x3000u) | 1u | 4u));

    CHECK(amivm_mmu_translate(&vm, logical, false, &physical) == 0);
    CHECK(physical == AMIVM_RAM_BASE + 0x3000u);
    CHECK(vm.mmu.translation_cache_valid);

    CHECK(amivm_write8(&vm, unrelated, 0x5au));
    CHECK(!vm.mmu.translation_cache_valid);

    CHECK(amivm_mmu_translate(&vm, logical, false, &physical) == 0);
    CHECK(physical == AMIVM_RAM_BASE + 0x3000u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_mmu_targeted_invalidation(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    const uint32_t logical = 0x00456000u;
    const uint32_t root = AMIVM_RAM_BASE + 0x1000u;
    const uint32_t leaf = AMIVM_RAM_BASE + 0x2000u;
    uint32_t physical = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.mmu.enabled = true;
    vm.mmu.root_index_bits = 2u;
    vm.mmu.leaf_index_bits = 2u;
    vm.mmu.root_table_base = root;
    CHECK(amivm_write32(&vm, root, leaf | 1u));
    CHECK(amivm_write32(&vm, leaf, (AMIVM_RAM_BASE + 0x3000u) | 1u | 4u));
    CHECK(amivm_mmu_translate(&vm, logical, false, &physical) == 0);
    CHECK(vm.mmu.translation_cache_valid);

    CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x7000u, 0x42u));
    CHECK(vm.mmu.translation_cache_valid);
    CHECK(amivm_mmu_translate(&vm, logical, false, &physical) == 0);
    CHECK(physical == AMIVM_RAM_BASE + 0x3000u);

    CHECK(amivm_write32(&vm, leaf, (AMIVM_RAM_BASE + 0x4000u) | 1u | 4u));
    CHECK(!vm.mmu.translation_cache_valid);
    CHECK(amivm_mmu_translate(&vm, logical, false, &physical) == 0);
    CHECK(physical == AMIVM_RAM_BASE + 0x4000u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_mmu_cache_page_shift(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint32_t physical = 0u;

    amivm_config_init(&config);
    config.ram_size = 4u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.mmu.enabled = true;
    vm.mmu.page_table_entries = 4u;
    vm.mmu.page_shift = 13u;
    vm.mmu.page_table_base = AMIVM_RAM_BASE + 0x1000u;
    vm.mmu.page_table_mask = 0xffffe000u;

    CHECK(amivm_write32(&vm, vm.mmu.page_table_base,
                        (AMIVM_RAM_BASE + 0x4000u) | 1u));
    CHECK(amivm_mmu_translate(&vm, 0x00000020u, false, &physical) == 0);
    CHECK(physical == AMIVM_RAM_BASE + 0x4020u);

    CHECK(amivm_mmu_translate(&vm, 0x00001020u, false, &physical) == 0);
    CHECK(physical == AMIVM_RAM_BASE + 0x5020u ||
          physical == AMIVM_RAM_BASE + 0x4020u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_cpu_mmu_cache_policy(void)
{
    const struct amivm_cpu_profile *p020 =
        amivm_cpu_profile_by_name("68020");
    const struct amivm_cpu_profile *p030 =
        amivm_cpu_profile_by_name("68030");
    const struct amivm_cpu_profile *p040 =
        amivm_cpu_profile_by_name("68040");
    const struct amivm_cpu_profile *p060 =
        amivm_cpu_profile_by_name("68060");
    const struct amivm_cpu_profile *ph040 =
        amivm_cpu_profile_by_name("hyper040");
    const struct amivm_cpu_profile *ph060 =
        amivm_cpu_profile_by_name("hyper060");

    CHECK(p020 != NULL && p030 != NULL && p040 != NULL &&
          p060 != NULL && ph040 != NULL && ph060 != NULL);
    CHECK(!amivm_cpu_profile_mmu_cache_qualified(p020));
    CHECK(!amivm_cpu_profile_mmu_cache_qualified(p030));
    CHECK(amivm_cpu_profile_mmu_cache_qualified(p040));
    CHECK(!amivm_cpu_profile_mmu_cache_qualified(p060));
    CHECK(amivm_cpu_profile_mmu_cache_qualified(ph040));
    CHECK(!amivm_cpu_profile_mmu_cache_qualified(ph060));
    return 0;
}


static int test_vm_profile_mmu_cache_policy(void)
{
    struct amivm_config config;
    struct amivm_vm vm;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;

    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);
    CHECK(vm.mmu.translation_cache_entry_count == 4u);
    CHECK(vm.mmu.translation_cache_page_shift == 12u);
    amivm_vm_destroy(&vm);

    config.cpu_profile = amivm_cpu_profile_by_name("68060");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);
    CHECK(vm.mmu.translation_cache_entry_count == 0u);
    CHECK(vm.mmu.translation_cache_page_shift == 0u);
    amivm_vm_destroy(&vm);

    return 0;
}


static int test_external_mmu_cpu_policy(void)
{
    struct amivm_config config;
    struct amivm_vm vm;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;

    config.cpu_profile = amivm_cpu_profile_by_name("68020");
    CHECK(config.cpu_profile != NULL);
    config.external_mmu = AMIVM_MMU_68851;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    CHECK(vm.cpu_profile.has_mmu);
    CHECK(vm.cpu_profile.mmu_model == AMIVM_MMU_68851);
    CHECK(vm.cpu_profile.mmu_maturity == AMIVM_MMU_MATURITY_SCAFFOLD);
    amivm_vm_destroy(&vm);

    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    config.external_mmu = AMIVM_MMU_68851;
    CHECK(amivm_vm_init(&vm, &config) != 0);

    config.external_mmu = AMIVM_MMU_68040;
    CHECK(amivm_vm_init(&vm, &config) != 0);

    return 0;
}


static int test_config_resolve_external_mmu_policy(void)
{
    struct amivm_config config;
    struct amivm_resolved_config resolved;

    amivm_config_init(&config);
    config.cpu_profile = amivm_cpu_profile_by_name("68020");
    CHECK(config.cpu_profile != NULL);
    config.external_mmu = AMIVM_MMU_68851;
    CHECK(amivm_config_resolve(&config, &resolved) == 0);
    CHECK(resolved.external_mmu == AMIVM_MMU_68851);

    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    config.external_mmu = AMIVM_MMU_68851;
    CHECK(amivm_config_resolve(&config, &resolved) != 0);

    config.external_mmu = AMIVM_MMU_68040;
    CHECK(amivm_config_resolve(&config, &resolved) != 0);
    return 0;
}


static int test_mmu_policy_matrix(void)
{
    struct amivm_config config;
    struct amivm_resolved_config resolved;
    const char *profiles[] = { "68020", "68030", "68040", "68060" };
    size_t i;

    amivm_config_init(&config);
    for (i = 0; i < sizeof profiles / sizeof profiles[0]; ++i) {
        config.cpu_profile = amivm_cpu_profile_by_name(profiles[i]);
        CHECK(config.cpu_profile != NULL);
        config.external_mmu = AMIVM_MMU_NONE;
        CHECK(amivm_config_resolve(&config, &resolved) == 0);
        if (strcmp(profiles[i], "68040") == 0)
            CHECK(amivm_cpu_profile_mmu_cache_qualified(config.cpu_profile));
        else
            CHECK(!amivm_cpu_profile_mmu_cache_qualified(config.cpu_profile));
    }

    config.cpu_profile = amivm_cpu_profile_by_name("68020");
    CHECK(config.cpu_profile != NULL);
    config.external_mmu = AMIVM_MMU_68851;
    CHECK(amivm_config_resolve(&config, &resolved) == 0);

    config.cpu_profile = amivm_cpu_profile_by_name("68030");
    CHECK(config.cpu_profile != NULL);
    config.external_mmu = AMIVM_MMU_68851;
    CHECK(amivm_config_resolve(&config, &resolved) != 0);

    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    config.external_mmu = AMIVM_MMU_68851;
    CHECK(amivm_config_resolve(&config, &resolved) != 0);

    config.cpu_profile = amivm_cpu_profile_by_name("68060");
    CHECK(config.cpu_profile != NULL);
    config.external_mmu = AMIVM_MMU_68851;
    CHECK(amivm_config_resolve(&config, &resolved) != 0);

    return 0;
}


static int test_mmu_page_shift_edges(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint32_t physical = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.mmu.enabled = true;
    vm.mmu.translation_cache_page_shift = 1u;
    CHECK(amivm_mmu_translate(&vm, 0x1000u, false, &physical) == 0);
    CHECK(physical == 0x1000u);

    vm.mmu.translation_cache_page_shift = 32u;
    CHECK(amivm_mmu_translate(&vm, 0x2000u, false, &physical) == 0);
    CHECK(physical == 0x2000u);

    vm.mmu.translation_cache_page_shift = 255u;
    CHECK(amivm_mmu_translate(&vm, 0x3000u, false, &physical) == 0);
    CHECK(physical == 0x3000u);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_mmu_index_bit_edges(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint32_t physical = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.mmu.enabled = true;
    vm.mmu.root_index_bits = 32u;
    vm.mmu.leaf_index_bits = 32u;
    vm.mmu.root_table_base = AMIVM_RAM_BASE + 0x1000u;

    CHECK(amivm_mmu_translate(&vm, 0x1000u, false, &physical) != 0);
    CHECK(vm.mmu.last_fault == AMIVM_MMU_FAULT_INVALID ||
          vm.mmu.last_fault == AMIVM_MMU_FAULT_BUS);

    vm.mmu.root_index_bits = 1u;
    vm.mmu.leaf_index_bits = 1u;
    CHECK(amivm_mmu_translate(&vm, 0x1000u, false, &physical) != 0);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_mmu_translation_cache_api(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint32_t physical = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.mmu.enabled = true;
    vm.mmu.translation_cache_entry_count = 4u;
    vm.mmu.translation_cache_page_shift = 12u;

    amivm_mmu_translation_cache_insert(
        &vm, 0x00456000u, AMIVM_RAM_BASE + 0x3000u, true, true);
    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, false, &physical));
    CHECK(physical == AMIVM_RAM_BASE + 0x3000u);

    CHECK(!amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, true, &physical) == false);

    amivm_mmu_page_table_changed(&vm, 0x00456000u);
    CHECK(!amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, false, &physical));

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_mmu_translation_cache_api_matrix(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint32_t physical = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.mmu.enabled = true;
    vm.mmu.translation_cache_entry_count = 4u;
    vm.mmu.translation_cache_page_shift = 12u;

    /* User read against user-readable mapping: hit. */
    vm.m68k.sr = 0u;
    amivm_mmu_translation_cache_insert(
        &vm, 0x00456000u, AMIVM_RAM_BASE + 0x3000u, true, false);
    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, false, &physical));
    CHECK(physical == AMIVM_RAM_BASE + 0x3000u);

    /* User write against read-only mapping: miss/fault. */
    amivm_mmu_translation_cache_insert(
        &vm, 0x00457000u, AMIVM_RAM_BASE + 0x4000u, false, false);
    CHECK(!amivm_mmu_translation_cache_lookup(
        &vm, 0x00457000u, true, &physical));
    CHECK(vm.mmu.last_fault == AMIVM_MMU_FAULT_WRITE_PROTECT);

    /* User access against supervisor-only mapping: fault. */
    amivm_mmu_translation_cache_insert(
        &vm, 0x00458000u, AMIVM_RAM_BASE + 0x5000u, true, true);
    CHECK(!amivm_mmu_translation_cache_lookup(
        &vm, 0x00458000u, false, &physical));
    CHECK(vm.mmu.last_fault == AMIVM_MMU_FAULT_SUPERVISOR);

    /* Supervisor read/write against supervisor mapping: hits. */
    vm.m68k.sr = 0x2000u;
    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00458000u, false, &physical));
    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00458000u, true, &physical));

    /* Explicit invalidation removes the entry. */
    amivm_mmu_page_table_changed(&vm, 0x00458000u);
    CHECK(!amivm_mmu_translation_cache_lookup(
        &vm, 0x00458000u, false, &physical));

    /* Missing entry is a cache miss, not an MMU permission fault. */
    vm.mmu.last_fault = AMIVM_MMU_FAULT_NONE;
    CHECK(!amivm_mmu_translation_cache_lookup(
        &vm, 0x00459000u, false, &physical));
    CHECK(vm.mmu.last_fault == AMIVM_MMU_FAULT_NONE);

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_mmu_translation_cache_eviction(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint32_t physical = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.mmu.enabled = true;
    vm.mmu.translation_cache_entry_count = 2u;
    vm.mmu.translation_cache_page_shift = 12u;
    vm.m68k.sr = 0x2000u;

    amivm_mmu_translation_cache_insert(
        &vm, 0x00456000u, AMIVM_RAM_BASE + 0x3000u, true, true);
    amivm_mmu_translation_cache_insert(
        &vm, 0x00457000u, AMIVM_RAM_BASE + 0x4000u, true, true);

    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, false, &physical));
    CHECK(physical == AMIVM_RAM_BASE + 0x3000u);

    /* Page 2 aliases slot 0 and must evict page 0 only. */
    amivm_mmu_translation_cache_insert(
        &vm, 0x00458000u, AMIVM_RAM_BASE + 0x5000u, true, true);

    CHECK(!amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, false, &physical));
    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00457000u, false, &physical));
    CHECK(physical == AMIVM_RAM_BASE + 0x4000u);
    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00458000u, false, &physical));
    CHECK(physical == AMIVM_RAM_BASE + 0x5000u);

    /* Reinsert page 0; page 2 must then be evicted from the same slot. */
    amivm_mmu_translation_cache_insert(
        &vm, 0x00456000u, AMIVM_RAM_BASE + 0x6000u, true, true);
    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, false, &physical));
    CHECK(physical == AMIVM_RAM_BASE + 0x6000u);
    CHECK(!amivm_mmu_translation_cache_lookup(
        &vm, 0x00458000u, false, &physical));

    amivm_vm_destroy(&vm);
    return 0;
}


static int test_mmu_translation_cache_permission_state(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    uint32_t physical = 0u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(config.cpu_profile != NULL);
    CHECK(amivm_vm_init(&vm, &config) == 0);

    vm.mmu.enabled = true;
    vm.mmu.translation_cache_entry_count = 4u;
    vm.mmu.translation_cache_page_shift = 12u;

    /* User-readable, user-writable mapping. */
    vm.m68k.sr = 0u;
    amivm_mmu_translation_cache_insert(
        &vm, 0x00456000u, AMIVM_RAM_BASE + 0x3000u, true, false);
    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, false, &physical));
    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, true, &physical));

    /* Supervisor mode must still be able to use the same mapping. */
    vm.m68k.sr = 0x2000u;
    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, false, &physical));
    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, true, &physical));

    /* Reinsert with supervisor-only/read-only attributes. */
    amivm_mmu_translation_cache_insert(
        &vm, 0x00456000u, AMIVM_RAM_BASE + 0x4000u, false, true);

    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, false, &physical));
    CHECK(physical == AMIVM_RAM_BASE + 0x4000u);
    CHECK(!amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, true, &physical));
    CHECK(vm.mmu.last_fault == AMIVM_MMU_FAULT_WRITE_PROTECT);

    /* User mode must now reject the cached supervisor-only mapping. */
    vm.m68k.sr = 0u;
    CHECK(!amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, false, &physical));
    CHECK(vm.mmu.last_fault == AMIVM_MMU_FAULT_SUPERVISOR);

    /* Reinsert again with user write permission; stale metadata must vanish. */
    amivm_mmu_translation_cache_insert(
        &vm, 0x00456000u, AMIVM_RAM_BASE + 0x5000u, true, false);
    CHECK(amivm_mmu_translation_cache_lookup(
        &vm, 0x00456000u, true, &physical));
    CHECK(physical == AMIVM_RAM_BASE + 0x5000u);

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
        test_nested_exception_atomicity() != 0 || test_nested_context_stack_rollback() != 0 ||
        test_supervisor_user_transition() != 0 || test_privilege_violation_rte() != 0 ||
        test_privilege_violation_atomicity() != 0 || test_interrupt_request_priority() != 0 ||
        test_interrupt_acceptance_and_mask() != 0 || test_interrupt_level_matrix() != 0 ||
        test_nested_interrupt_priority() != 0 || test_nested_interrupt_roundtrip() != 0 ||
        test_interrupt_vector_failure_atomicity() != 0 || test_interrupt_vector_failure_full_rollback() != 0 ||
        test_service_irq_rollback() != 0 || test_irq_api_equivalence() != 0 ||
        test_irq_state_machine_integrity() != 0 || test_irq_invalid_transitions() != 0 ||
        test_instruction_irq_boundary() != 0 || test_instruction_state_commit_before_irq() != 0 ||
        test_pending_irq_across_instruction_sequence() != 0 || test_vm_step_backend_irq_boundary() != 0 ||
        test_vm_step_backend_pending_irq_sequence() != 0 || test_vm_step_irq_mask_until_boundary() != 0 ||
        test_vm_step_nested_irq_preemption() != 0 || test_vm_step_nested_irq_state_commit() != 0 ||
        test_vm_step_exception_irq_interleave() != 0 || test_vm_step_irq_exception_reverse_interleave() != 0 ||
        test_vm_step_mmu_fault_irq_interleave() != 0 || test_vm_step_real_mmu_data_fault() != 0 ||
        test_vm_step_real_mmu_write_fault() != 0 || test_vm_step_real_mmu_user_fault() != 0 ||
        test_vm_step_mmu_fault_nested_irq() != 0 || test_vm_step_mmu_irq_priority() != 0 ||
        test_vm_step_mmu_irq_deferred() != 0 || test_vm_step_mmu_fault_recovery() != 0 ||
        test_vm_step_mmu_recovery_pending_irq() != 0 || test_mmu_translation_cache_invalidation() != 0 ||
        test_mmu_page_table_changed_hook() != 0 || test_mmu_ram_write_auto_invalidate() != 0 ||
        test_mmu_multiple_cached_translations() != 0 ||
        test_mmu_cache_page_shift() != 0 ||
        test_mmu_page_shift_edges() != 0 ||
        test_mmu_index_bit_edges() != 0 ||
        test_mmu_translation_cache_api() != 0 ||
        test_mmu_translation_cache_api_matrix() != 0 ||
        test_mmu_translation_cache_eviction() != 0 ||
        test_mmu_translation_cache_permission_state() != 0 ||
        test_cpu_mmu_cache_policy() != 0 ||
        test_vm_profile_mmu_cache_policy() != 0 ||
        test_external_mmu_cpu_policy() != 0 ||
        test_config_resolve_external_mmu_policy() != 0 ||
        test_mmu_policy_matrix() != 0 ||
        test_mmu_cache_entry_permissions() != 0 ||
        test_mmu_cache_entry_count() != 0 ||
        test_mmu_ram_write_preserves_correctness() != 0 || test_mmu_targeted_invalidation() != 0 ||
        test_config() != 0) {
        return 1;
    }
    puts("AmiVM M2.108 nested IRQ round-trip tests: PASS");
    return 0;
}
