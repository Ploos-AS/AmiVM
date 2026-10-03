#include "fs_uae.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); return 1; } } while (0)

int main(void)
{
    const char *path = "/tmp/amivm-fsuae-test.conf";
    FILE *f = fopen(path, "w");
    struct amivm_config config;
    struct amivm_fsuae_report report;

    CHECK(f != NULL);
    fputs("amiga_model=A1200\n", f);
    fputs("cpu=68040\n", f);
    fputs("chip_memory=2\n", f);
    fputs("fast_memory=32\n", f);
    fputs("accelerator=68030\n", f);
    fputs("fpu_model=68030\n", f);
    fputs("mmu_model=68851\n", f);
    fputs("uae_cpu_speed=real\n", f);
    fputs("uae_cpu_multiplier=2\n", f);
    fputs("fullscreen=1\n", f);
    fputs("jit_compiler=1\n", f);
    fputs("floppy_image_0=disk.adf\n", f);
    fputs("hard_drive_0=DH0\n", f);
    fputs("bsdsocket_library=1\n", f);
    fputs("mystery_option=1\n", f);
    fclose(f);

    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, false) == 0);
    CHECK(config.cpu_profile != NULL);
    CHECK(strcmp(config.cpu_profile->name, "68040") == 0);
    CHECK(config.external_mmu == AMIVM_MMU_68851);
    CHECK(config.ram_size == 34u * 1024u * 1024u);
    CHECK(config.chip_ram_size == 2u * 1024u * 1024u);
    CHECK(config.fast_ram_size == 32u * 1024u * 1024u);
    {
        struct amivm_resolved_config resolved;
        struct amivm_vm vm;
        CHECK(amivm_config_resolve(&config, &resolved) == 0);
        CHECK(amivm_vm_init(&vm, &config) == 0);
        CHECK(vm.machine == AMIVM_MACHINE_A1200);
        CHECK(vm.chip_ram_size == 2u * 1024u * 1024u);
        CHECK(vm.fast_ram_size == 32u * 1024u * 1024u);
        CHECK(vm.device_count == 6u);
        CHECK(vm.devices[0].instantiated);
        CHECK(vm.devices[0].enabled);
        CHECK(strcmp(vm.devices[4].desc->name, "aga") == 0);
        CHECK(strcmp(vm.devices[5].desc->name, "ide") == 0);
        CHECK(amivm_write8(&vm, vm.devices[4].desc->base, 0x12u));
        CHECK(amivm_write8(&vm, vm.devices[4].desc->base + 1u, 0x34u));
        { uint8_t v = 0u; CHECK(amivm_read8(&vm, vm.devices[4].desc->base, &v)); CHECK(v == 0x12u); }
        { uint8_t v = 0u; CHECK(amivm_read8(&vm, vm.devices[4].desc->base + 1u, &v)); CHECK(v == 0x34u); }
        CHECK(amivm_write8(&vm, vm.devices[4].desc->base + 6u, 0x01u));
        { uint8_t v = 0u; CHECK(amivm_read8(&vm, vm.devices[4].desc->base + 7u, &v)); CHECK(v == 0x01u); }
        CHECK(amivm_write8(&vm, vm.devices[5].desc->base + 1u, 0x34u));
        { uint8_t v = 0u; CHECK(amivm_read8(&vm, vm.devices[5].desc->base + 1u, &v)); CHECK(v == 0x34u); }
        {
            uint8_t v = 0u;
            CHECK(amivm_read8(&vm, vm.devices[4].desc->base + 8u, &v));
            CHECK(v == 0u);
            amivm_vm_account_instruction(&vm, 20u);
            CHECK(vm.last_instruction_cycles == 20u);
            CHECK(vm.cpu_cycles == 20u);
            CHECK(vm.chipset_cycles == 20u);
            {
                static uint32_t fake_cpu_step(struct amivm_vm *v, void *state)
                {
                    (void)v; (void)state; return 12u;
                }
                struct amivm_cpu_backend backend = {
                    "test-cpu", fake_cpu_step, NULL
                };
                vm.cpu_cycles = 0u;
                vm.chipset_cycles = 0u;
                CHECK(amivm_vm_attach_cpu_backend(&vm, &backend) == 0);
                CHECK(amivm_vm_step(&vm) == 0);
                CHECK(vm.last_instruction_cycles == 12u);
                CHECK(vm.cpu_cycles == 12u);
                CHECK(vm.chipset_cycles == 12u);
                amivm_m68k_reset(&vm, 0x00f80000u, 0x2700u);
                CHECK(vm.m68k.pc == 0x00f80000u);
                CHECK(vm.m68k.sr == 0x2700u);
                CHECK(amivm_m68k_is_supervisor(&vm));
                CHECK(amivm_m68k_set_supervisor(&vm, false) == 0);
                CHECK(!amivm_m68k_is_supervisor(&vm));
                CHECK((vm.m68k.sr & 0x2000u) == 0u);
                CHECK(amivm_m68k_set_supervisor(&vm, true) == 0);
                CHECK(amivm_m68k_is_supervisor(&vm));
                CHECK((vm.m68k.sr & 0x2000u) != 0u);
                vm.m68k.pc = AMIVM_RAM_BASE;
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE, 0x4eu));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 1u, 0x73u));
                CHECK(amivm_m68k_set_supervisor(&vm, false) == 0);
                CHECK(amivm_m68k_execute_one(&vm) == AMIVM_M68K_EXC_PRIVILEGE);
                CHECK(vm.pending_exception == AMIVM_M68K_EXC_PRIVILEGE);
                vm.m68k.pc = AMIVM_RAM_BASE;
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE, 0x4eu));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 1u, 0x40u));
                CHECK(amivm_m68k_set_supervisor(&vm, false) == 0);
                CHECK(amivm_m68k_execute_one(&vm) == 0);
                CHECK(vm.exception_frame_active);
                CHECK(vm.exception_saved_pc == AMIVM_RAM_BASE + 2u);
                CHECK(vm.pending_exception_vector == 32u);
                CHECK(vm.m68k.supervisor);
                CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
                CHECK(vm.m68k.a[7] >= vm.exception_frame_sp);
                vm.m68k.a[7] = vm.exception_frame_sp;
                vm.exception_frame_active = true;
                CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                vm.exception_frame_format = 0u;
                vm.exception_frame_size = 0u;
                vm.exception_frame_word_count = 0u;
                CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                CHECK(vm.exception_frame_format == 2u);
                CHECK(vm.exception_frame_word_count == 8u);
                CHECK(vm.exception_frame_size == 16u);
                CHECK(vm.exception_frame_format == 2u);
                CHECK(vm.exception_frame_word_count == 8u);
                CHECK((vm.exception_format_vector_word >> 12) == 2u);
                CHECK(vm.m68k.pc == AMIVM_RAM_BASE + 2u);
                CHECK(!vm.exception_frame_active);
                vm.m68k.exception = AMIVM_M68K_EXC_NONE;
                vm.m68k.pc = AMIVM_RAM_BASE;
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE, 0x4eu));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 1u, 0x45u));
                CHECK(amivm_m68k_execute_one(&vm) == 0);
                CHECK(vm.pending_exception_vector == 0u);
                vm.m68k.pc = 0x00111111u;
                vm.m68k.sr = 0x0000u;
                vm.m68k.supervisor = false;
                CHECK(amivm_m68k_enter_exception(&vm, AMIVM_M68K_EXC_ILLEGAL,
                                                   vm.m68k.pc) == 4);
                CHECK(vm.exception_frame_active);
                CHECK(vm.exception_saved_pc == 0x00111111u);
                CHECK(vm.exception_saved_sr == 0x0000u);
                CHECK(vm.m68k.supervisor);
                vm.m68k.pc = 0x00f00000u;
                CHECK(amivm_m68k_set_supervisor(&vm, true) == 0);
                CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
                CHECK(vm.m68k.pc == 0x00111111u);
                {
                    uint32_t physical = 0u;
                    CHECK(amivm_mmu_translate(&vm, 0x00123456u, false, &physical) == 0);
                    CHECK(physical == 0x00123456u);
                    vm.mmu.enabled = true;
                    CHECK(amivm_mmu_translate(&vm, 0x00abcdefu, true, &physical) == 0);
                    CHECK(physical == 0x00abcdefu);
                    CHECK(vm.mmu.last_logical == 0x00abcdefu);
                    CHECK(vm.mmu.last_write);
                }
                CHECK(vm.m68k.sr == 0x0000u);
                CHECK(!vm.m68k.supervisor);
                CHECK(!vm.exception_frame_active);
                CHECK(amivm_m68k_raise_exception(&vm, AMIVM_M68K_EXC_ILLEGAL) == 4);
                CHECK(vm.pending_exception == AMIVM_M68K_EXC_ILLEGAL);
                CHECK(vm.pending_exception_vector == 4u);
                CHECK((vm.m68k.sr & 0x2000u) != 0u);
                vm.m68k.sr = 0x2000u;
                amivm_m68k_request_irq(&vm, 4u);
                CHECK(vm.irq_pending);
                CHECK(vm.irq_level == 4u);
                CHECK(amivm_m68k_service_irq(&vm) == 28);
                CHECK(!vm.irq_pending);
                CHECK(((vm.m68k.sr >> 8) & 7u) == 4u);
                amivm_m68k_request_irq(&vm, 3u);
                CHECK(amivm_m68k_service_irq(&vm) == 0);
                CHECK(vm.irq_pending);
                for (size_t r = 0; r < 8u; ++r) {
                    CHECK(vm.m68k.d[r] == 0u);
                    CHECK(vm.m68k.a[r] == 0u);
                }
            }
            CHECK(vm.aga.beam_h == 20u);
            CHECK(vm.aga.display_active);
            vm.aga.vblank_irq_enable = true;
            vm.m68k.sr = 0x2000u;
            vm.aga.beam_h = 226u;
            vm.aga.beam_v = 19u;
            amivm_vm_advance_cycles(&vm, 1u);
            CHECK(vm.irq_pending);
            CHECK(vm.irq_level == 3u);
            {
                uint8_t vector = 0u;
                CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
                CHECK(vector == 27u);
                CHECK(!vm.irq_pending);
                CHECK(vm.irq_in_service);
                CHECK(((vm.m68k.sr >> 8) & 7u) == 3u);
                CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 0);
                vm.m68k.pc = 0x00123456u;
                vm.m68k.sr = 0x2000u;
                amivm_m68k_request_irq(&vm, 3u);
                CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
                CHECK(vm.irq_saved_pc == 0x00123456u);
                CHECK(vm.irq_saved_sr == 0x2000u);
                vm.m68k.pc = 0x00abcdefu;
                CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
                CHECK(vm.m68k.pc == 0x00123456u);
        {
            uint32_t physical = 0u;
            bool ok = true;
            uint8_t value = 0u;
            vm.mmu.enabled = true;
            CHECK(amivm_mmu_translate(&vm, AMIVM_RAM_BASE + 0x100u, false, &physical) == 0);
            CHECK(physical == AMIVM_RAM_BASE + 0x100u);
            CHECK(vm.mmu.last_logical == AMIVM_RAM_BASE + 0x100u);
            CHECK(vm.mmu.last_physical == physical);
            vm.mmu.tt0 = AMIVM_RAM_BASE | 1u | 0x00000200u;
            CHECK(amivm_mmu_tt_match(&vm.mmu, AMIVM_RAM_BASE + 0x1234u, false) == 1);
            CHECK(amivm_mmu_tt_match(&vm.mmu, AMIVM_RAM_BASE + 0x1234u, true) == 1);
            CHECK(amivm_mmu_tt_match(&vm.mmu, AMIVM_ROM_BASE, false) == 0);
            CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x100u, 0x5au));
            CHECK(amivm_read8(&vm, AMIVM_RAM_BASE + 0x100u, &value));
            CHECK(value == 0x5au);
            CHECK(vm.mmu.enabled);
            vm.mmu.tt0 = 0u;
            CHECK(amivm_mmu_configure_two_level(&vm, AMIVM_RAM_BASE + 0x3000u, 4u, 4u) == 0);
            CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x3000u, 0x00u));
            CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x3001u, 0x00u));
            CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x3002u, 0x80u));
            CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x3003u, 0x01u));
            CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x4000u, 0x80u));
            CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x4001u, 0x00u));
            CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x4002u, 0x00u));
            CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x4003u, 0x01u));
            {
                uint32_t physical = 0u;
                CHECK(amivm_mmu_translate(&vm, 0x00001234u, false, &physical) == 0);
                CHECK(physical == 0x80001234u);
            }
            vm.mmu.root_index_bits = 0u;
            vm.mmu.leaf_index_bits = 0u;
            CHECK(amivm_mmu_configure_page_table(&vm, AMIVM_RAM_BASE + 0x1000u, 0xffffffffu, 12u, 4u) == 0);
            CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x1000u + 1u, 0x80u));
            CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x1000u + 2u, 0x10u));
            CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x1000u + 3u, 0x01u));
            {
                uint32_t physical = 0u;
                CHECK(amivm_mmu_translate(&vm, AMIVM_RAM_BASE + 0x123u, false, &physical) == 0);
                CHECK(physical == (0x80000000u | 0x123u));
            }
            vm.mmu.page_table_entries = 0u;
            vm.mmu.test_page_valid = true;
            vm.mmu.test_logical_page = AMIVM_RAM_BASE + 0x4000u;
            vm.mmu.test_physical_page = AMIVM_RAM_BASE + 0x8000u;
            vm.m68k.sr = 0x2000u;
            vm.mmu.test_write_protect = false;
            {
                uint32_t physical = 0u;
                CHECK(amivm_mmu_translate(&vm, AMIVM_RAM_BASE + 0x4567u, false, &physical) == 0);
                CHECK(physical == AMIVM_RAM_BASE + 0x8567u);
            }
            vm.m68k.sr = 0x0000u;
            vm.mmu.test_write_protect = false;
            {
                uint32_t physical = 0u;
                CHECK(amivm_mmu_translate(&vm, AMIVM_RAM_BASE + 0x4567u, false, &physical) != 0);
                CHECK(vm.mmu.last_fault == AMIVM_MMU_FAULT_SUPERVISOR);
            }
            vm.mmu.mmu_supervisor = true;
            vm.mmu.test_write_protect = true;
            {
                uint32_t physical = 0u;
                CHECK(amivm_mmu_translate(&vm, AMIVM_RAM_BASE + 0x4567u, true, &physical) != 0);
                CHECK(vm.mmu.last_fault == AMIVM_MMU_FAULT_WRITE_PROTECT);
            }
            vm.m68k.exception = AMIVM_M68K_EXC_NONE;
            {
                uint8_t value = 0u;
                CHECK(!amivm_read8(&vm, AMIVM_RAM_BASE + 0x200u, &value));
                CHECK(vm.mmu.last_fault == AMIVM_MMU_FAULT_INVALID);
                CHECK(vm.mmu_fault_address == AMIVM_RAM_BASE + 0x200u);
                CHECK((vm.mmu_fault_status & 16u) != 0u);
                CHECK(vm.m68k.exception == AMIVM_M68K_EXC_MMU_FAULT);
                CHECK(vm.mmu_exception_pc == vm.m68k.pc);
                CHECK(vm.mmu_exception_sr == vm.m68k.sr);
                CHECK(vm.exception_fault_address == vm.mmu_fault_address);
                CHECK(vm.exception_fault_status == vm.mmu_fault_status);
                vm.m68k.a[7] = AMIVM_RAM_BASE + 0x2000u;
                CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                CHECK(vm.m68k.a[7] == AMIVM_RAM_BASE + 0x1ff0u);
                CHECK(vm.exception_frame_sp == vm.m68k.a[7]);
                CHECK(vm.exception_frame_size == 16u);
                CHECK(amivm_m68k_set_exception_frame_type(&vm, 0u) == 0);
                CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                CHECK(vm.exception_frame_size == 8u);
                CHECK(vm.exception_frame_format == 0u);
                CHECK(amivm_m68k_set_cpu_model(&vm, 2u) == 0);
                CHECK(amivm_m68k_set_exception_frame_type(&vm, 2u) == 0);
                CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                CHECK(vm.exception_frame_format == 0u);
                CHECK(amivm_m68k_set_cpu_model(&vm, 4u) == 0);
                CHECK(amivm_m68k_set_cpu_profile(&vm, 0u, 0u) == 0);
                CHECK(vm.exception_frame_class == 0u);
                CHECK(amivm_m68k_set_cpu_profile(&vm, 2u, 0u) == 0);
                CHECK(vm.exception_frame_class == 1u);
                CHECK(amivm_m68k_set_cpu_profile(&vm, 1u, 0u) == 0);
                CHECK(vm.exception_frame_class == 0u);
                vm.exception_frame_type = AMIVM_FRAME_68000_SHORT;
                CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                CHECK(vm.exception_frame_format == 0x0Au);
                CHECK(vm.exception_frame_type == AMIVM_FRAME_68030_SHORT_BUS);
                CHECK(vm.exception_frame_size == 32u);
                CHECK(vm.exception_internal_state[0] == vm.m68k.pc);
                CHECK(vm.exception_internal_state[1] == vm.m68k.sr);
                CHECK(vm.exception_internal_state[2] == vm.exception_fault_address);
                CHECK(vm.exception_internal_state[3] == vm.exception_fault_status);
                CHECK(amivm_m68k_read_u32(&vm, vm.exception_frame_sp + 8u, &(bool){true}) ==
                       vm.exception_internal_state[0]);
                CHECK(amivm_m68k_read_u32(&vm, vm.exception_frame_sp + 12u, &(bool){true}) ==
                       vm.exception_internal_state[1]);
                CHECK(amivm_m68k_read_u32(&vm, vm.exception_frame_sp + 16u, &(bool){true}) ==
                       vm.exception_internal_state[2]);
                CHECK(amivm_m68k_read_u32(&vm, vm.exception_frame_sp + 20u, &(bool){true}) ==
                       vm.exception_internal_state[3]);
                vm.exception_bus_fault_in_progress = true;
                CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                CHECK(vm.exception_frame_type == AMIVM_FRAME_68030_LONG_BUS);
                CHECK(vm.exception_frame_format == 0x0Bu);
                CHECK(vm.exception_frame_size == 92u);
                {
                    const struct amivm_m68k_frame_layout *long_layout =
                        amivm_m68k_frame_layout(AMIVM_FRAME_68030_LONG_BUS);
                    CHECK(long_layout != NULL);
                    CHECK(long_layout->internal_state_offset == 8u);
                    CHECK(long_layout->internal_state_words == 34u);
                    CHECK(vm.exception_internal_state[0] == vm.m68k.pc);
                    CHECK(vm.exception_internal_state[33] == vm.m68k.pc + 132u);
                    CHECK(amivm_m68k_frame_descriptor_for_format(0x02u) == AMIVM_FRAME_68020_BUS);
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68020, 0u) == 0);
                 CHECK(vm.cpu_profile.exception_frame_family == AMIVM_FRAME_FAMILY_68020);
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68030, 0u) == 0);
                 CHECK(amivm_m68k_enter_exception(&vm, 10u) != 0 || vm.exception_entry_vector == 10u);
                 CHECK(vm.m68k.a[7] == 0u || vm.exception_stack_fault);
                 CHECK(!vm.exception_entry_active);
                 CHECK(amivm_m68k_decode_frame_format(&vm, 0x7000u) != 0);
                 CHECK(amivm_m68k_decode_frame_format(&vm, 0xA000u) == 0);
                 CHECK(amivm_m68k_decode_frame_format(&vm, 0xA001u) != 0);
                 vm.exception_internal_state[0] = 0x11223344u;
                 vm.exception_internal_state[1] = 0x55667788u;
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68030, 0u) == 0);
                 CHECK(amivm_m68k_set_exception_frame_type(&vm, AMIVM_FRAME_68030_SHORT_BUS) == 0);
                 CHECK(vm.exception_frame_type == AMIVM_FRAME_68030_SHORT_BUS);
                 CHECK(amivm_m68k_frame_layout(vm.exception_frame_type)->has_030_short_state);
                 CHECK(amivm_m68k_set_exception_frame_type(&vm, AMIVM_FRAME_68040_ACCESS) != 0);
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68030, 0u) == 0);
                 CHECK(vm.cpu_profile.exception_frame_family == AMIVM_FRAME_FAMILY_68030);
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68040, 0u) == 0);
                 CHECK(vm.cpu_profile.exception_frame_family == AMIVM_FRAME_FAMILY_68040);
                 vm.cpu_profile.exception_frame_family = (enum amivm_exception_frame_family)0xFFu;
                 CHECK(amivm_m68k_stack_mmu_exception(&vm) != 0);
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68040, 0u) == 0);
                 vm.cpu_profile.exception_frame_family = (enum amivm_exception_frame_family)0xFFu;
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68060, 0u) == 0);
                 CHECK(vm.cpu_profile.exception_frame_family == AMIVM_FRAME_FAMILY_68060);
                 CHECK(vm.exception_frame_type == AMIVM_FRAME_INVALID);
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68060, 0u) == 0);
                 CHECK(vm.cpu_profile.exception_frame_family == AMIVM_FRAME_FAMILY_68060);
                 vm.exception_bus_fault_in_progress = false;
                 CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                 CHECK(vm.exception_frame_type == AMIVM_FRAME_68020_BUS);
                 CHECK(vm.exception_frame_format == 0x02u);
                 CHECK(vm.exception_frame_size == 12u);
                 vm.exception_frame_active = true;
                 vm.exception_frame_size = 32u;
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68040, 7u) != 0);
                 CHECK(vm.cpu_submodel != 7u);
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68040, 0u) != 0);
                 vm.exception_frame_active = false;
                 vm.exception_frame_size = 0u;
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68040, 1u) == 0);
                 CHECK(vm.cpu_submodel == 1u);
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68030, 2u) == 0);
                 CHECK(vm.cpu_submodel == 2u);
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68030, 0u) == 0);
                 vm.exception_bus_fault_in_progress = false;
                 CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                 CHECK(vm.exception_frame_type == AMIVM_FRAME_68030_SHORT_BUS);
                 CHECK(vm.exception_frame_format == 0x0Au);
                 CHECK(vm.exception_frame_type == AMIVM_FRAME_68030_SHORT_BUS);
                 vm.exception_bus_fault_in_progress = true;
                 CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                 CHECK(vm.exception_frame_type == AMIVM_FRAME_68030_LONG_BUS);
                 CHECK(vm.exception_frame_format == 0x0Bu);
                 CHECK(vm.exception_frame_size == 92u);
                 vm.exception_bus_fault_in_progress = false;
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68040, 0u) == 0);
                 CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                 CHECK(vm.exception_frame_type == AMIVM_FRAME_68040_ACCESS);
                 CHECK(vm.exception_frame_format == 0x07u);
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68060, 0u) == 0);
                 CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                 CHECK(vm.exception_frame_type == AMIVM_FRAME_68060_ACCESS);
                 CHECK(vm.exception_frame_format == 0x04u);
                 CHECK(vm.cpu_profile.exception_frame_family == AMIVM_FRAME_FAMILY_68060);
                 CHECK(amivm_m68k_frame_layout(vm.exception_frame_type)->has_fault_address);
                 CHECK(amivm_m68k_frame_layout(vm.exception_frame_type)->has_fault_status);
                 CHECK(vm.cpu_profile.exception_frame_family == AMIVM_FRAME_FAMILY_68060);
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_HYPER040, 0u) == 0);
                 CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                 CHECK(vm.exception_frame_type == AMIVM_FRAME_68040_ACCESS);
                 CHECK(vm.exception_frame_format == 0x07u);
                 CHECK(vm.cpu_profile.exception_frame_family == AMIVM_FRAME_FAMILY_68040);
                 CHECK(amivm_m68k_frame_descriptor_for_format(0x07u) == AMIVM_FRAME_68040_ACCESS);
                 CHECK(amivm_m68k_frame_descriptor_for_format(0x04u) == AMIVM_FRAME_68060_ACCESS);
                 CHECK(amivm_m68k_frame_descriptor_for_format(0x09u) ==
                       AMIVM_FRAME_68030_COPROC_MID);
                 {
                     CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68030, 0u) == 0);
                     vm.exception_frame_type = AMIVM_FRAME_68030_COPROC_MID;
                     vm.exception_frame_format = 0x09u;
                     vm.exception_frame_size = 20u;
                     vm.m68k.a[7] = 0x1000u;
                     CHECK(amivm_m68k_write_u32(&vm, 0x1000u, vm.m68k.pc) == 1);
                     CHECK(amivm_write8(&vm, 0x1004u, (uint8_t)(vm.m68k.sr >> 8)));
                     CHECK(amivm_write8(&vm, 0x1005u, (uint8_t)vm.m68k.sr));
                     CHECK(amivm_m68k_write_u32(&vm, 0x1008u, 0x11223344u));
                     CHECK(amivm_m68k_write_u32(&vm, 0x100Cu, 0x55667788u));
                     vm.exception_internal_state[0] = 0u;
                     vm.exception_internal_state[1] = 0u;
                     CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                 vm.exception_frame_vector_offset++;
                 CHECK(amivm_m68k_validate_exception_frame(&vm) != 0);
                 vm.exception_frame_vector_offset--;
                 vm.exception_format_vector_word ^= 1u;
                 CHECK(amivm_m68k_validate_exception_frame(&vm) != 0);
                 vm.exception_format_vector_word ^= 1u;
                 CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                 CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                 CHECK(amivm_m68k_frame_layout(vm.exception_frame_type)->preserves_internal_state);
                 vm.exception_frame_size++;
                 CHECK(amivm_m68k_validate_exception_frame(&vm) != 0);
                 vm.exception_frame_size--;
                 vm.exception_frame_word_count++;
                 CHECK(amivm_m68k_validate_exception_frame(&vm) != 0);
                 vm.exception_frame_word_count--;
                 CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                 CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                 vm.exception_frame_format ^= 1u;
                 CHECK(amivm_m68k_validate_exception_frame(&vm) != 0);
                 vm.exception_frame_format ^= 1u;
                 CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                 CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                                      {
                     uint32_t saved_sp = vm.m68k.a[7];
                     uint32_t saved_pc = vm.mmu_exception_pc;
                     uint16_t saved_sr = vm.mmu_exception_sr;
                     CHECK(amivm_m68k_validate_exception_internal_state(
                         &vm, amivm_m68k_frame_layout(vm.exception_frame_type),
                         vm.m68k.a[7]) == 0);
                     CHECK(vm.exception_frame_sp == vm.m68k.a[7]);
                     {
                         uint32_t pc_before = vm.m68k.pc;
                         uint16_t sr_before = vm.m68k.sr;
                         uint32_t fault_before = vm.mmu_fault_address;
                         vm.m68k.a[7] = vm.exception_frame_sp + 1u;
                         CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                         CHECK(vm.m68k.pc == pc_before);
                         CHECK(vm.m68k.sr == sr_before);
                         CHECK(vm.mmu_fault_address == fault_before);
                         CHECK(vm.exception_frame_sp != 0u);
                         vm.m68k.a[7] = vm.exception_frame_sp;
                     }
                     CHECK(amivm_m68k_rte_mmu_exception(&vm) == 0);
                     CHECK(vm.m68k.pc == saved_pc);
                     CHECK(vm.m68k.sr == saved_sr);
                     CHECK(vm.m68k.a[7] >= saved_sp);
                     CHECK(vm.m68k.a[7] == 0x1014u);
                     CHECK(vm.exception_internal_state[0] == 0x11223344u);
                     CHECK(vm.exception_internal_state[1] == 0x55667788u);
                 }
                 {
                     CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68030, 0u) == 0);
                     vm.exception_bus_fault_in_progress = false;
                     vm.m68k.a[7] = 0x1200u;
                     vm.m68k.pc = 0x12345678u;
                     vm.m68k.sr = 0x2700u;
                     vm.exception_fault_address = 0x00abcdefu;
                     vm.exception_fault_status = 0x00001234u;
                     CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                     CHECK(vm.exception_frame_type == AMIVM_FRAME_68030_SHORT_BUS);
                     CHECK(vm.exception_frame_format == 0x0Au);
                     CHECK(vm.exception_frame_size == 32u);
                     CHECK(vm.exception_frame_format <= 0x0Bu);
                 CHECK(amivm_m68k_frame_layout(vm.exception_frame_type)->implemented);
                 CHECK(amivm_m68k_frame_layout(vm.exception_frame_type)->words ==
                       vm.exception_frame_word_count);
                 CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                     {
                         uint8_t saved = 0u;
                         CHECK(amivm_read8(&vm, vm.exception_frame_sp + 6u, &saved));
                         CHECK(amivm_write8(&vm, vm.exception_frame_sp + 6u,
                                            (uint8_t)(saved ^ 0x10u)));
                         CHECK(amivm_m68k_validate_exception_frame(&vm) != 0);
                         CHECK(amivm_write8(&vm, vm.exception_frame_sp + 6u, saved));
                         CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                     }
                 }
                 {
                     const struct amivm_m68k_frame_layout *l9 =
                         amivm_m68k_frame_layout(AMIVM_FRAME_68030_COPROC_MID);
                     CHECK(l9 != NULL);
                     CHECK(l9->words == 10u);
                     CHECK(l9->has_format_vector);
                     CHECK(l9->format_offset == 6u);
                     CHECK(l9->internal_state_offset == 8u);
                     CHECK(l9->internal_state_words == 2u);
                     CHECK(!l9->semantically_decoded);
                     CHECK(l9->implemented);
                     CHECK(l9->semantically_decoded == false);
                     CHECK((unsigned)l9->internal_state_offset +
                           (unsigned)l9->internal_state_words * 4u <=
                           (unsigned)l9->words * 2u);
                     CHECK(l9->has_fault_address == false);
                     CHECK(l9->has_fault_status == false);
                     CHECK(amivm_m68k_frame_layout(AMIVM_FRAME_68020_BUS)->format_offset == 6u);
                     CHECK(amivm_m68k_frame_layout(AMIVM_FRAME_68040_ACCESS)->format_offset == 6u);
                     CHECK(!l9->has_fault_address);
                     CHECK(!l9->has_fault_status);
                 }
                 CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                    {
                        uint8_t saved = 0u;
                        CHECK(amivm_read8(&vm, vm.exception_frame_sp + 8u, &saved));
                        CHECK(amivm_write8(&vm, vm.exception_frame_sp + 8u,
                                           (uint8_t)(saved ^ 0x01u)));
                        CHECK(amivm_m68k_validate_exception_frame(&vm) != 0);
                        CHECK(amivm_write8(&vm, vm.exception_frame_sp + 8u, saved));
                        CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                    }
                }
                CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                {
                    uint32_t long_sp = vm.exception_frame_sp;
                    vm.exception_frame_active = true;
                    vm.m68k.a[7] = long_sp;
                    CHECK(amivm_m68k_write_u32(&vm, long_sp + 8u, 0xCAFEBABEu));
                    CHECK(amivm_m68k_write_u32(&vm, long_sp + 12u, 0x13572468u));
                    CHECK(amivm_m68k_write_u32(&vm, long_sp + 16u, 0x24681357u));
                    CHECK(amivm_m68k_write_u32(&vm, long_sp + 20u, 0x89ABCDEFu));
                    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
                    CHECK(vm.m68k.a[7] == long_sp + 92u);
                    CHECK(vm.exception_internal_state[0] == 0xCAFEBABEu);
                    CHECK(vm.exception_internal_state[1] == 0x13572468u);
                    CHECK(vm.exception_internal_state[2] == 0x24681357u);
                    CHECK(vm.exception_internal_state[3] == 0x89ABCDEFu);
                    CHECK(!vm.exception_frame_active);
                }
                vm.exception_bus_fault_in_progress = false;
                CHECK(amivm_m68k_set_cpu_profile(&vm, 3u, 0u) == 0);
                CHECK(vm.exception_frame_class == 1u);
                CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                CHECK(vm.exception_frame_format == 0x04u);
                CHECK(vm.exception_frame_type == AMIVM_FRAME_68060_ACCESS);
                CHECK(vm.exception_frame_size == 16u);
                CHECK(vm.exception_frame_format == 0x04u);
                CHECK(vm.exception_fslw == vm.exception_fault_status);
                {
                    bool ok = false;
                    uint32_t access_address =
                        amivm_m68k_read_u32(&vm, vm.exception_frame_sp + 8u, &ok);
                    CHECK(ok);
                    CHECK(access_address == vm.exception_060_access_address);
                    CHECK(access_address == vm.exception_fault_address);
                    CHECK(amivm_m68k_read_u32(&vm, vm.exception_frame_sp + 12u, &ok) ==
                          vm.exception_fslw);
                    CHECK(ok);
                    {
                        uint8_t saved = 0u;
                        CHECK(amivm_read8(&vm, vm.exception_frame_sp + 12u, &saved));
                        CHECK(amivm_write8(&vm, vm.exception_frame_sp + 12u,
                                           (uint8_t)(saved ^ 0x01u)));
                        CHECK(amivm_m68k_validate_exception_frame(&vm) != 0);
                        CHECK(amivm_write8(&vm, vm.exception_frame_sp + 12u, saved));
                        CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                    }
                }
                {
                    uint32_t access_sp = vm.exception_frame_sp;
                    uint32_t expected_sp = access_sp + 16u;
                    CHECK(amivm_m68k_rte_mmu_exception(&vm) == 0);
                    CHECK(vm.m68k.a[7] == expected_sp);
                    CHECK(vm.exception_frame_size == 0u);
                    CHECK(vm.exception_frame_type == AMIVM_FRAME_68000_SHORT);
                    CHECK(vm.exception_frame_format == 0u);
                 CHECK(vm.exception_frame_type == AMIVM_FRAME_68000_SHORT);
                    CHECK(vm.exception_frame_word_count == 0u);
                    CHECK(vm.exception_fslw == vm.exception_fault_status);
                }
                CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68040, 0u) == 0);
                 CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                 CHECK(vm.exception_frame_type == AMIVM_FRAME_68040_ACCESS);
                 CHECK(vm.exception_frame_format == 0x07u);
                 CHECK(vm.exception_frame_size == 60u);
                 {
                     bool ok = false;
                     uint8_t shi = 0u, slo = 0u;
                     uint32_t access_address =
                         amivm_m68k_read_u32(&vm, vm.exception_frame_sp + 20u, &ok);
                     CHECK(ok);
                     CHECK(access_address == vm.exception_fault_address);
                     CHECK(amivm_read8(&vm, vm.exception_frame_sp + 10u, &shi));
                     CHECK(amivm_read8(&vm, vm.exception_frame_sp + 11u, &slo));
                     CHECK((((uint32_t)shi << 8) | slo) ==
                           (vm.exception_fault_status & 0xffffu));
                 }
                 CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                 {
                     uint8_t saved = 0u;
                     CHECK(amivm_read8(&vm, vm.exception_frame_sp + 10u, &saved));
                     CHECK(amivm_write8(&vm, vm.exception_frame_sp + 10u,
                                        (uint8_t)(saved ^ 0x01u)));
                     CHECK(amivm_m68k_validate_exception_frame(&vm) != 0);
                     CHECK(amivm_write8(&vm, vm.exception_frame_sp + 10u, saved));
                     vm.exception_internal_state[0] = 0x11223344u;
                     vm.exception_internal_state[1] = 0x55667788u;
                     CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                     CHECK(amivm_m68k_read_u32(&vm, vm.exception_frame_sp + 8u, &(bool){true}) == 0x11223344u);
                     CHECK(amivm_m68k_read_u32(&vm, vm.exception_frame_sp + 12u, &(bool){true}) == 0x55667788u);
                     CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                     {
                         uint8_t saved = 0u;
                         CHECK(amivm_read8(&vm, vm.exception_frame_sp + 8u, &saved));
                         CHECK(amivm_write8(&vm, vm.exception_frame_sp + 8u,
                                            (uint8_t)(saved ^ 0x01u)));
                         CHECK(amivm_m68k_validate_exception_frame(&vm) != 0);
                         CHECK(amivm_write8(&vm, vm.exception_frame_sp + 8u, saved));
                         CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                     }
                 {
                     uint32_t access_sp = vm.exception_frame_sp;
                     CHECK(amivm_m68k_rte_mmu_exception(&vm) == 0);
                     CHECK(vm.m68k.a[7] == access_sp + 60u);
                     CHECK(vm.exception_frame_size == 0u);
                 }
                 CHECK(amivm_m68k_set_cpu_profile(&vm, AMIVM_CPU_68020, 0u) == 0);
                 CHECK(vm.exception_frame_class == 1u);
                 CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                 CHECK(vm.exception_frame_type == AMIVM_FRAME_68020_BUS);
                 CHECK(vm.exception_frame_format == 0x02u);
                 CHECK(vm.exception_frame_size == 12u);
                 CHECK(vm.exception_format_vector_word ==
                       (uint16_t)(0x2000u | (vm.exception_entry_vector & 0x0fffu)));
                 {
                     const struct amivm_m68k_frame_layout *l20 =
                         amivm_m68k_frame_layout(AMIVM_FRAME_68020_BUS);
                     CHECK(l20 != NULL);
                     CHECK(l20->words == 6u);
                     CHECK(l20->has_format_vector);
                     CHECK(l20->format_offset == 6u);
                     CHECK(!l20->has_fault_address);
                     CHECK(!l20->has_fault_status);
                     CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                 }
                 CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                 {
                     uint32_t frame_sp = vm.exception_frame_sp;
                     CHECK(amivm_m68k_rte_mmu_exception(&vm) == 0);
                     CHECK(vm.m68k.a[7] == frame_sp + 12u);
                     CHECK(vm.exception_frame_size == 0u);
                 }
                 CHECK(amivm_cpu_profile_by_id(AMIVM_CPU_68020)->mmu_model == AMIVM_MMU_68851);
                 CHECK(amivm_cpu_profile_by_id(AMIVM_CPU_68030)->mmu_model == AMIVM_MMU_68030);
                 CHECK(amivm_cpu_profile_by_id(AMIVM_CPU_68040)->mmu_model == AMIVM_MMU_68040);
                 CHECK(amivm_cpu_profile_by_id(AMIVM_CPU_68060)->mmu_model == AMIVM_MMU_68060);
                 CHECK(amivm_cpu_profile_by_id(AMIVM_CPU_68030)->fpu_model == AMIVM_FPU_NONE);
                 CHECK(amivm_cpu_profile_by_id(AMIVM_CPU_68040)->fpu_model == AMIVM_FPU_68040);
                 CHECK(amivm_cpu_profile_by_id(AMIVM_CPU_68060)->fpu_model == AMIVM_FPU_68060);
                 {
                     static const uint8_t qualification_frames[] = {
                         AMIVM_FRAME_68000_SHORT,
                         AMIVM_FRAME_68020_BUS,
                         AMIVM_FRAME_68030_SHORT_BUS,
                         AMIVM_FRAME_68030_LONG_BUS,
                         AMIVM_FRAME_68040_ACCESS,
                         AMIVM_FRAME_68060_ACCESS
                     };
                     size_t qi;
                     for (qi = 0u; qi < sizeof(qualification_frames); ++qi) {
                         const struct amivm_m68k_frame_layout *ql =
                             amivm_m68k_frame_layout(qualification_frames[qi]);
                         CHECK(ql != NULL);
                         CHECK(ql->implemented);
                         CHECK(ql->words > 0u);

                         CHECK((unsigned)ql->words * 2u >= 8u);
                         if (qualification_frames[qi] == AMIVM_FRAME_68030_SHORT_BUS) {
                             CHECK(ql->words == 16u);
                             CHECK(ql->has_fault_address);
                             CHECK(ql->fault_address_offset == 16u);
                             CHECK(ql->internal_state_offset == 8u);
                             CHECK(ql->internal_state_words == 4u);
                         } else if (qualification_frames[qi] == AMIVM_FRAME_68030_LONG_BUS) {
                             CHECK(ql->words == 46u);
                             CHECK(ql->has_fault_address);
                             CHECK(ql->fault_address_offset == 16u);
                             CHECK(ql->internal_state_offset == 8u);
                             CHECK(ql->internal_state_words == 34u);
                         } else if (qualification_frames[qi] == AMIVM_FRAME_68040_ACCESS) {
                             CHECK(ql->words == 30u);
                             CHECK(ql->fault_address_offset == 20u);
                             CHECK(ql->fault_status_offset == 10u);
                             CHECK(ql->fault_status_bytes == 2u);
                         } else if (qualification_frames[qi] == AMIVM_FRAME_68060_ACCESS) {
                             CHECK(ql->words == 8u);
                             CHECK(ql->fault_address_offset == 8u);
                             CHECK(ql->fault_status_offset == 12u);
                             CHECK(ql->fault_status_bytes == 4u);
                         }
                         CHECK((unsigned)ql->fault_address_offset < (unsigned)ql->words * 2u || !ql->has_fault_address);
                         if (ql->has_fault_status)
                             CHECK((unsigned)ql->fault_status_offset +
                                   (unsigned)ql->fault_status_bytes <=
                                   (unsigned)ql->words * 2u);
                         if (ql->internal_state_words)
                             CHECK((unsigned)ql->internal_state_offset +
                                   (unsigned)ql->internal_state_words * 4u <=
                                   (unsigned)ql->words * 2u);
                         if (ql->has_fault_address && ql->internal_state_words)
                             CHECK((unsigned)ql->internal_state_offset +
                                   (unsigned)ql->internal_state_words * 4u <=
                                   (unsigned)ql->fault_address_offset);
                         if (ql->has_fault_address) {
                             CHECK((ql->fault_address_offset & 1u) == 0u);
                             CHECK((unsigned)ql->fault_address_offset + 4u <=
                                   (unsigned)ql->words * 2u);
                         }
                         if (ql->has_fault_status) {
                             CHECK((ql->fault_status_offset & 1u) == 0u);
                             CHECK((unsigned)ql->fault_status_offset +
                                   (unsigned)ql->fault_status_bytes <=
                                   (unsigned)ql->words * 2u);
                             CHECK(ql->fault_status_bytes == 2u ||
                                   ql->fault_status_bytes == 4u);
                         }
                         if (ql->internal_state_words) {
                             CHECK((ql->internal_state_offset & 3u) == 0u);
                             CHECK((unsigned)ql->internal_state_offset +
                                   (unsigned)ql->internal_state_words * 4u <=
                                   (unsigned)ql->words * 2u);
                         }
                         if (ql->has_format_vector) {
                             CHECK((ql->format_offset & 1u) == 0u);
                             CHECK((unsigned)ql->format_offset + 2u <= (unsigned)ql->words * 2u);
                             CHECK((unsigned)ql->format_offset + 2u <=
                                   (unsigned)ql->words * 2u);
                             if (ql->preserves_internal_state)
                              CHECK(ql->internal_state_words != 0u);
                          if (ql->internal_state_words)
                                 CHECK((unsigned)ql->format_offset + 2u <=
                                       (unsigned)ql->internal_state_offset ||
                                       (unsigned)ql->internal_state_offset +
                                       (unsigned)ql->internal_state_words * 4u <=
                                       (unsigned)ql->format_offset);
                             if (ql->has_fault_address)
                              CHECK((ql->fault_address_offset & 1u) == 0u);
                              CHECK((unsigned)ql->fault_address_offset + 4u <= (unsigned)ql->words * 2u);
                                 CHECK((unsigned)ql->format_offset + 2u <=
                                       (unsigned)ql->fault_address_offset ||
                                       (unsigned)ql->fault_address_offset + 4u <=
                                       (unsigned)ql->format_offset);
                             if (ql->has_fault_status)
                              CHECK((ql->fault_status_offset & 1u) == 0u);
                              CHECK(ql->fault_status_bytes == 2u || ql->fault_status_bytes == 4u);
                              CHECK((unsigned)ql->fault_status_offset + (unsigned)ql->fault_status_bytes <= (unsigned)ql->words * 2u);
                                 CHECK((unsigned)ql->format_offset + 2u <=
                                       (unsigned)ql->fault_status_offset ||
                                       (unsigned)ql->fault_status_offset +
                                       (unsigned)ql->fault_status_bytes <=
                                       (unsigned)ql->format_offset);
                         }
                         if (ql->has_fault_status && ql->internal_state_words)
                             CHECK((unsigned)ql->internal_state_offset +
                                   (unsigned)ql->internal_state_words * 4u <=
                                   (unsigned)ql->fault_status_offset ||
                                   (unsigned)ql->fault_status_offset +
                                   (unsigned)ql->fault_status_bytes <=
                                   (unsigned)ql->internal_state_offset);
                     }
                 }
                 CHECK(amivm_m68k_set_cpu_profile(&vm, 4u, 0u) == 0);
                CHECK(vm.exception_frame_class == 2u);
                CHECK(AMIVM_FRAME_68000_SHORT == 0);
                CHECK(AMIVM_FRAME_68020_BUS == 1);
                CHECK(AMIVM_FRAME_68040_MMU == 2);
                {
                    const struct amivm_m68k_frame_layout *l;
                    l = amivm_m68k_frame_layout(AMIVM_FRAME_68000_SHORT);
                    CHECK(l && l->words == 4u && !l->has_fault_address);
                    l = amivm_m68k_frame_layout(AMIVM_FRAME_68020_BUS);
                    CHECK(l && l->words == 6u && l->has_format_vector);
                    l = amivm_m68k_frame_layout(AMIVM_FRAME_68030_COPROC_MID);
                    CHECK(l && l->has_opaque_coproc_state && !l->semantically_decoded);
                    CHECK(l->preserves_internal_state);
                    l = amivm_m68k_frame_layout(AMIVM_FRAME_68040_MMU);
                    CHECK(l && l->words == 8u && l->has_fault_address && l->has_fault_status);
                    CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                    vm.exception_frame_word_count = 4u;
                    CHECK(amivm_m68k_validate_exception_frame(&vm) != 0);
                    vm.exception_frame_word_count = 8u;
                }
                CHECK(amivm_m68k_set_cpu_profile(&vm, 5u, 0u) == 0);
                CHECK(vm.exception_frame_class == 2u);
                CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                CHECK(vm.exception_frame_format == 2u);
                CHECK(vm.exception_frame_vector_offset ==
                CHECK((vm.exception_format_vector_word >> 12) == vm.exception_frame_format);
                CHECK((vm.exception_format_vector_word & 0x0fffu) == vm.exception_entry_vector);
                CHECK(vm.exception_fault_stage == 1u);
                CHECK(amivm_m68k_frame_layout(vm.exception_frame_type)->has_fault_address);
                      (uint16_t)(vm.exception_entry_vector * 4u));
                CHECK(vm.exception_frame_word_count == 4u);
                CHECK(vm.exception_frame_magic == 0x45584632u);
                CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                vm.exception_frame_magic = 0u;
                CHECK(amivm_m68k_validate_exception_frame(&vm) != 0);
                vm.exception_frame_magic = 0x45584632u;
                vm.exception_frame_active = true;
                vm.m68k.stopped = false;
                vm.irq_in_service = false;
                vm.exception_frame_magic = 0u;
                CHECK(amivm_m68k_return_from_interrupt(&vm) != 0);
                CHECK(vm.m68k.stopped);
                vm.m68k.stopped = false;
                vm.exception_frame_active = true;
                vm.m68k.a[7] = vm.exception_frame_sp;
                CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
                CHECK(!vm.m68k.stopped);
                vm.m68k.a[7] = vm.exception_frame_sp;
                CHECK(amivm_m68k_write_u32(&vm, vm.m68k.a[7], 0x00abcdefu));
                CHECK(amivm_write8(&vm, vm.m68k.a[7] + 4u, 0x20u));
                CHECK(amivm_write8(&vm, vm.m68k.a[7] + 5u, 0x00u));
                vm.exception_frame_active = true;
                vm.m68k.pc = 0x00111111u;
                vm.m68k.sr = 0x2700u;
                CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
                CHECK(vm.m68k.pc == 0x00abcdefu);
                CHECK(vm.m68k.sr == 0x2000u);
                CHECK(vm.m68k.a[7] == vm.exception_frame_sp + 0u || vm.exception_frame_sp == 0u);
                CHECK(!vm.exception_frame_active);
                CHECK(vm.exception_frame_size == 0u);
                vm.exception_frame_magic = 0x45584632u;
                vm.m68k.stopped = false;
                vm.exception_frame_active = false;
                vm.exception_frame_magic = 0x45584632u;
                CHECK(amivm_m68k_set_exception_frame_type(&vm, 1u) == 0);
                CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                CHECK(vm.exception_frame_format == 1u);
                CHECK(vm.exception_frame_format == 1u);
                CHECK(vm.exception_frame_word_count == 6u);
                CHECK(amivm_m68k_set_exception_frame_type(&vm, 2u) == 0);
                CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                CHECK(vm.exception_frame_format == 2u);
                CHECK(vm.exception_frame_word_count == 8u);
                CHECK(amivm_m68k_set_exception_frame_type(&vm, 1u) == 0);
                CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                CHECK(vm.exception_frame_size == 12u);
                {
                    uint32_t frame_sp = vm.exception_frame_sp;
                    vm.exception_frame_active = true;
                    vm.m68k.a[7] = frame_sp;
                    CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
                    CHECK(vm.m68k.a[7] == frame_sp + 12u);
                    CHECK(!vm.exception_frame_active);
                }
                {
                    const uint8_t frame_types[] = { 0u, 1u, 2u };
                    const uint8_t frame_sizes[] = { 8u, 12u, 16u };
                    size_t i;
                    for (i = 0u; i < sizeof frame_types / sizeof frame_types[0]; ++i) {
                        uint32_t frame_sp;
                        uint32_t old_sp = vm.m68k.a[7];
                        CHECK(amivm_m68k_set_exception_frame_type(&vm, frame_types[i]) == 0);
                        CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                        CHECK(vm.m68k.a[7] == old_sp - frame_sizes[i]);
                        CHECK(vm.exception_frame_size == frame_sizes[i]);
                        CHECK(vm.exception_frame_word_count ==
                              (uint8_t)(frame_sizes[i] / 2u));
                        {
                            bool ok = false;
                            uint32_t stacked_pc = amivm_m68k_read_u32(&vm, vm.exception_frame_sp, &ok);
                            uint8_t sr_hi = 0u, sr_lo = 0u;
                            CHECK(ok);
                            CHECK(stacked_pc == vm.mmu_exception_pc);
                            CHECK(amivm_read8(&vm, vm.exception_frame_sp + 4u, &sr_hi));
                            CHECK(amivm_read8(&vm, vm.exception_frame_sp + 5u, &sr_lo));
                            CHECK((((uint16_t)sr_hi << 8) | sr_lo) == vm.mmu_exception_sr);
                            if (frame_types[i] != 0u) {
                                uint8_t fv_hi = 0u, fv_lo = 0u;
                                CHECK(amivm_read8(&vm, vm.exception_frame_sp + 6u, &fv_hi));
                                CHECK(amivm_read8(&vm, vm.exception_frame_sp + 7u, &fv_lo));
                                CHECK((((uint16_t)fv_hi << 8) | fv_lo) ==
                                      vm.exception_format_vector_word);
                            }
                            if (frame_types[i] == 2u) {
                                bool fault_ok = false;
                                uint32_t fault_address =
                                    amivm_m68k_read_u32(&vm, vm.exception_frame_sp + 8u, &fault_ok);
                                CHECK(fault_ok);
                                fault_ok = false;
                                uint32_t fault_status =
                                    amivm_m68k_read_u32(&vm, vm.exception_frame_sp + 12u, &fault_ok);
                                CHECK(fault_ok);
                                CHECK(fault_address == vm.exception_fault_address);
                                CHECK(fault_status == vm.exception_fault_status);
                            }
                        }
                        CHECK(amivm_m68k_validate_exception_frame(&vm) == 0);
                        frame_sp = vm.exception_frame_sp;
                        vm.exception_frame_active = true;
                        vm.m68k.a[7] = frame_sp;
                        CHECK(amivm_m68k_return_from_interrupt(&vm) == 0);
                        CHECK(vm.m68k.a[7] == frame_sp + frame_sizes[i]);
                        CHECK(!vm.exception_frame_active);
                    }
                }
                CHECK(amivm_m68k_set_exception_vector_base(&vm, AMIVM_RAM_BASE + 0x3000u) == 0);
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x3000u + 27u * 4u, 0x00u));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x3000u + 27u * 4u + 1u, 0x12u));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x3000u + 27u * 4u + 2u, 0x34u));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x3000u + 27u * 4u + 3u, 0x56u));
                CHECK(amivm_m68k_enter_exception_handler(&vm, 27u) == 0);
                CHECK(vm.m68k.pc == 0x00123456u);
                CHECK(vm.exception_handler_pc == 0x00123456u);
                vm.m68k.pc = 0x00001000u;
                vm.m68k.sr = 0x0000u;
                vm.m68k.a[7] = AMIVM_RAM_BASE + 0x2800u;
                CHECK(amivm_m68k_enter_exception(&vm, 27u) == 0);
                CHECK(vm.m68k.pc == 0x00123456u);
                CHECK((vm.m68k.sr & 0x2000u) != 0u);
                CHECK(!vm.exception_entry_active);
                vm.irq_pending = true;
                vm.irq_level = 3u;
                vm.m68k.sr = 0x2000u;
                {
                    uint8_t vector = 0u;
                    CHECK(amivm_m68k_acknowledge_irq(&vm, &vector) == 1);
                    CHECK(vector == 27u);
                    CHECK(vm.exception_handler_pc == 0x00123456u);
                    vm.m68k.pc = 0x00002000u;
                    vm.m68k.sr = 0x0000u;
                    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x3000u;
                    CHECK(amivm_m68k_enter_trap(&vm, 32u) == 0);
                    CHECK(vm.m68k.pc == 0x00123456u);
                    CHECK((vm.m68k.sr & 0x2000u) != 0u);
                }
                vm.m68k.pc = 0x00abcdefu;
                vm.m68k.sr = 0x0000u;
                CHECK(amivm_m68k_rte_mmu_exception(&vm) == 0);
                CHECK(vm.m68k.pc == vm.mmu_exception_pc);
                CHECK(vm.m68k.sr == vm.mmu_exception_sr);
                CHECK(vm.m68k.a[7] == AMIVM_RAM_BASE + 0x2000u);
                CHECK(vm.exception_frame_size == 0u);
                CHECK(vm.m68k.exception == AMIVM_M68K_EXC_NONE);
                vm.exception_vector_base = AMIVM_RAM_BASE + 0x1000u;
                vm.m68k.pc = AMIVM_RAM_BASE + 0x2200u;
                vm.m68k.sr = 0x0000u;
                CHECK(amivm_m68k_write_u32(&vm, vm.exception_vector_base + 56u * 4u,
                                           AMIVM_RAM_BASE + 0x9000u));
                CHECK(amivm_m68k_exception_enter(&vm, 56u) == 0);
                CHECK(vm.m68k.pc == AMIVM_RAM_BASE + 0x9000u);
                CHECK((vm.m68k.sr & 0x2000u) != 0u);
                CHECK(vm.pending_exception_vector == 56u);
                vm.mmu.enabled = true;
                vm.mmu.root_index_bits = 0u;
                vm.mmu.leaf_index_bits = 0u;
                vm.mmu.page_table_entries = 0u;
                vm.exception_vector_base = AMIVM_RAM_BASE + 0x5000u;
                CHECK(amivm_m68k_write_u32(&vm, vm.exception_vector_base + 56u * 4u,
                                           AMIVM_RAM_BASE + 0x9100u));
                vm.m68k.pc = AMIVM_RAM_BASE + 0x2200u;
                vm.m68k.sr = 0x0000u;
                {
                    uint8_t value = 0u;
                    CHECK(!amivm_read8(&vm, AMIVM_RAM_BASE + 0x2fffu, &value));
                    CHECK(vm.m68k.pc == AMIVM_RAM_BASE + 0x9100u);
                    CHECK(vm.pending_exception_vector == 56u);
                    CHECK(vm.exception_frame_size == 16u);
                    CHECK((vm.m68k.sr & 0x2000u) != 0u);
                    CHECK(vm.exception_frame_sp == vm.m68k.a[7]);
                    CHECK(vm.exception_depth == 1u);
                    vm.m68k.a[7] = 8u;
                    CHECK(amivm_m68k_stack_mmu_exception(&vm) != 0);
                    CHECK(vm.exception_stack_fault);
                    vm.exception_depth = 1u;
                    CHECK(amivm_m68k_enter_mmu_exception(&vm) != 0);
                    CHECK(vm.exception_double_fault);
                    CHECK(vm.exception_halted);
                    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x2000u;
                    vm.m68k.a[7] = AMIVM_RAM_BASE + 0x2400u;
                    vm.mmu_fault_address = AMIVM_RAM_BASE + 0x2345u;
                    vm.mmu_fault_status = 0x12u;
                    {
                        uint32_t outer_sp = vm.exception_frame_sp;
                        uint8_t outer_type = vm.exception_frame_type;
                        uint32_t outer_state0 = vm.exception_internal_state[0];
                        uint32_t outer_state1 = vm.exception_internal_state[1];
                        CHECK(amivm_m68k_stack_mmu_exception(&vm) == 0);
                        CHECK(vm.exception_depth == 2u);
                        CHECK(vm.exception_frame_sp != outer_sp);
                        vm.exception_internal_state[0] = 0xAABBCCDDu;
                        vm.exception_internal_state[1] = 0xEEFF0011u;
                        {
                            uint32_t inner_sp = vm.exception_frame_sp;
                            uint8_t inner_type = vm.exception_frame_type;
                            uint8_t inner_depth = vm.exception_depth;
                            uint32_t inner_state0 = vm.exception_internal_state[0];
                            uint32_t inner_state1 = vm.exception_internal_state[1];
                            vm.m68k.a[7] = inner_sp + 1u;
                            CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                            CHECK(vm.exception_frame_sp == inner_sp);
                            CHECK(vm.exception_frame_type == inner_type);
                            CHECK(vm.exception_depth == inner_depth);
                            CHECK(vm.exception_internal_state[0] == inner_state0);
                            CHECK(vm.exception_internal_state[1] == inner_state1);
                            vm.m68k.a[7] = inner_sp;
                        }
                        CHECK(amivm_m68k_rte_mmu_exception(&vm) == 0);
                        CHECK(vm.exception_depth == 1u);
                        CHECK(vm.exception_frame_sp == outer_sp);
                        CHECK(vm.exception_frame_type == outer_type);
                        CHECK(vm.exception_frame_magic == 0x45584632u);
                        {
                            uint32_t saved_sp = vm.exception_frame_sp;
                            uint8_t saved_depth = vm.exception_depth;
                            uint8_t saved_type = vm.exception_frame_type;
                            vm.exception_context_stack[0].frame_magic = 0u;
                            CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                            vm.exception_context_stack[0].frame_magic = 0x45584632u;
                            vm.exception_context_stack[0].frame_sp |= 1u;
                            CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                            vm.exception_context_stack[0].frame_sp &= ~1u;
                            {
                                uint8_t saved_format = vm.exception_context_stack[0].frame_format;
                                vm.exception_context_stack[0].frame_format = 0x7u;
                                CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                                CHECK(vm.exception_depth == saved_depth);
                                vm.exception_context_stack[0].frame_format = saved_format;
                                {
                                    uint8_t saved_vector = vm.exception_context_stack[0].entry_vector;
                                    vm.exception_context_stack[0].entry_vector ^= 1u;
                                    CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                                    CHECK(vm.exception_depth == saved_depth);
                                    vm.exception_context_stack[0].entry_vector = saved_vector;
                                    {
                                        uint32_t saved_stage =
                                            vm.exception_context_stack[0].fault_stage;
                                        vm.exception_context_stack[0].fault_stage = 2u;
                                        CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                                        CHECK(vm.exception_depth == saved_depth);
                                        vm.exception_context_stack[0].fault_stage = saved_stage;
                                        {
                                            uint32_t saved_status =
                                                vm.exception_context_stack[0].fault_status;
                                            vm.exception_context_stack[0].fault_stage = 1u;
                                            vm.exception_context_stack[0].fault_status = 0u;
                                            CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                                            CHECK(vm.exception_depth == saved_depth);
                                            vm.exception_context_stack[0].fault_status = saved_status;
                                            vm.exception_context_stack[0].fault_stage = saved_stage;
                                            {
                                                uint32_t saved_state0 =
                                                    vm.exception_context_stack[0].internal_state[0];
                                                vm.exception_context_stack[0].internal_state[0] = 0xDEADBEEFu;
                                                CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                                                CHECK(vm.exception_depth == saved_depth);
                                                vm.exception_context_stack[0].internal_state[0] = saved_state0;
                                            {
                                                uint32_t saved_address =
                                                    vm.exception_context_stack[0].fault_address;
                                                vm.exception_context_stack[0].fault_address |= 1u;
                                                CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                                                CHECK(vm.exception_depth == saved_depth);
                                                vm.exception_context_stack[0].fault_address = saved_address;
                                                if (amivm_m68k_frame_layout(
                                                        vm.exception_context_stack[0].frame_type)->fault_status_bytes == 2u) {
                                                    uint32_t saved_status =
                                                        vm.exception_context_stack[0].fault_status;
                                                    vm.exception_context_stack[0].fault_stage = 1u;
                                                    vm.exception_context_stack[0].fault_status = 0x10000u;
                                                    CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                                                    CHECK(vm.exception_depth == saved_depth);
                                                    vm.exception_context_stack[0].fault_status = saved_status;
                                                    {
                                                        const struct amivm_m68k_frame_layout *layout =
                                                            amivm_m68k_frame_layout(vm.exception_context_stack[0].frame_type);
                                                        uint8_t saved_offset = layout->fault_status_offset;
                                                        ((struct amivm_m68k_frame_layout *)layout)->fault_status_offset =
                                                            (uint8_t)(saved_offset | 1u);
                                                        CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                                                        CHECK(vm.exception_depth == saved_depth);
                                                        ((struct amivm_m68k_frame_layout *)layout)->fault_status_offset =
                                                            saved_offset;
                                                        {
                                                        uint8_t saved_offset2 = layout->internal_state_offset;
                                                        ((struct amivm_m68k_frame_layout *)layout)->internal_state_offset =
                                                            (uint8_t)(saved_offset2 | 1u);
                                                        CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                                                        CHECK(vm.exception_depth == saved_depth);
                                                        ((struct amivm_m68k_frame_layout *)layout)->internal_state_offset =
                                                            saved_offset2;
                                                        if (layout->has_fault_status &&
                                                            layout->internal_state_words != 0u) {
                                                            uint8_t saved_is = layout->internal_state_offset;
                                                            uint8_t saved_fs = layout->fault_status_offset;
                                                            ((struct amivm_m68k_frame_layout *)layout)->internal_state_offset =
                                                                saved_fs;
                                                            CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                                                            CHECK(vm.exception_depth == saved_depth);
                                                            ((struct amivm_m68k_frame_layout *)layout)->internal_state_offset =
                                                                saved_is;
                                                        }
                                                    }
                                                }
                                                }
                                            }
                                            }
                                        }
                                    }
                                }
                            }
                            CHECK(amivm_m68k_rte_mmu_exception(&vm) != 0);
                            CHECK(vm.exception_frame_sp == saved_sp);
                            CHECK(vm.exception_depth == saved_depth);
                            CHECK(vm.exception_frame_type == saved_type);
                            vm.exception_context_stack[0].frame_magic = 0x45584632u;
                        }
                        CHECK(vm.exception_internal_state[0] == outer_state0);
                        CHECK(vm.exception_internal_state[1] == outer_state1);
                        CHECK(vm.exception_depth <= 8u);
                        {
                            uint8_t saved_depth = vm.exception_depth;
                            uint32_t saved_sp = vm.exception_frame_sp;
                            uint8_t saved_type = vm.exception_frame_type;
                            uint32_t saved_state0 = vm.exception_internal_state[0];
                            uint32_t saved_state1 = vm.exception_internal_state[1];
                            vm.exception_depth = 8u;
                            CHECK(amivm_m68k_stack_mmu_exception(&vm) != 0);
                            CHECK(vm.exception_depth == 8u);
                            CHECK(vm.exception_frame_sp == saved_sp);
                            CHECK(vm.exception_frame_type == saved_type);
                            CHECK(vm.exception_internal_state[0] == saved_state0);
                            CHECK(vm.exception_internal_state[1] == saved_state1);
                            vm.exception_depth = saved_depth;
                        }
                        CHECK(amivm_m68k_rte_mmu_exception(&vm) == 0);
                        CHECK(vm.exception_depth == 0u);
                        CHECK(vm.exception_frame_sp == 0u);
                    }
                    CHECK(vm.m68k.a[7] == AMIVM_RAM_BASE + 0x23f0u);
                }
                vm.mmu.enabled = false;
            }
            vm.mmu.enabled = false;
        }
                CHECK(vm.m68k.sr == 0x2000u);
                CHECK(!vm.irq_in_service);
                vm.m68k.stopped = false;
                vm.m68k.exception = AMIVM_M68K_EXC_NONE;
                vm.m68k.pc = AMIVM_RAM_BASE;
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE, 0x4eu));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 1u, 0x71u));
                CHECK(amivm_m68k_execute_one(&vm) == 0);
                CHECK(vm.m68k.pc == AMIVM_RAM_BASE + 2u);
                CHECK(vm.last_instruction_cycles == 4u);
                CHECK(vm.m68k.exception == AMIVM_M68K_EXC_NONE);
                vm.m68k.d[0] = 0x80000001u;
                vm.m68k.d[1] = 0u;
                vm.m68k.pc = AMIVM_RAM_BASE;
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE, 0x22u));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 1u, 0x01u));
                CHECK(amivm_m68k_execute_one(&vm) == 0);
                CHECK(vm.m68k.d[1] == 0x80000001u);
                CHECK((vm.m68k.sr & 0x04u) == 0u);
                CHECK((vm.m68k.sr & 0x08u) != 0u);
                vm.m68k.pc = AMIVM_RAM_BASE;
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE, 0x20u));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 1u, 0x3cu));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 2u, 0xdeu));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 3u, 0xadu));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 4u, 0xbeu));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 5u, 0xefu));
                CHECK(amivm_m68k_execute_one(&vm) == 0);
                CHECK(vm.m68k.d[1] == 0xdeadbeefu);
                CHECK(vm.m68k.pc == AMIVM_RAM_BASE + 6u);
                vm.m68k.a[2] = 0x12345678u;
                vm.m68k.pc = AMIVM_RAM_BASE;
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE, 0x24u));
                CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 1u, 0x0au));
                CHECK(amivm_m68k_execute_one(&vm) == 0);
                CHECK(vm.m68k.d[2] == 0x12345678u);
            }
        }
        amivm_vm_destroy(&vm);
        CHECK(amivm_config_resolve(&config, &resolved) == 0);
        CHECK(resolved.machine == AMIVM_MACHINE_A1200);
        CHECK(resolved.machine_profile != NULL);
        CHECK(resolved.hardware.machine == AMIVM_MACHINE_A1200);
        CHECK(resolved.hardware.chipset == AMIVM_CHIPSET_AGA);
        CHECK(resolved.hardware.has_aga);
        CHECK(resolved.hardware.has_ide);
        CHECK(amivm_device_count_for_hardware(&resolved.hardware) == 6u);
        CHECK(resolved.device_count == 6u);
        CHECK(strcmp(resolved.devices[0]->name, "vmserial") == 0);
        CHECK(strcmp(resolved.devices[4]->name, "aga") == 0);
        CHECK(strcmp(resolved.devices[5]->name, "ide") == 0);
        CHECK(resolved.cpu_profile != NULL);
        CHECK(resolved.accelerator_profile != NULL);
        CHECK(resolved.chip_ram_size == 2u * 1024u * 1024u);
        CHECK(resolved.fast_ram_size == 32u * 1024u * 1024u);
        CHECK(resolved.total_ram_size == 34u * 1024u * 1024u);
    }

    f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("amiga_model=A1200\nchip_memory=4M\n", f);
    fclose(f);
    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, false) != 0);
    CHECK(report.unsupported > 0u);

    f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("amiga_model=A1200\nchip_memory=2M\nfast_memory=32M\n", f);
    fclose(f);
    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, false) != 0);
    CHECK(report.unsupported > 0u);

    f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("amiga_model=A1200\naccelerator=68040\nchip_memory=2M\nfast_memory=32M\n", f);
    fclose(f);
    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, false) == 0);
    CHECK(config.accelerator_present);
    CHECK(config.accelerator_profile != NULL);
    CHECK(strcmp(config.accelerator_profile->name, "68040") == 0);
    CHECK(config.accelerator_profile->has_mmu);
    CHECK(config.accelerator_profile->has_fpu);
    CHECK(config.fast_ram_size == 32u * 1024u * 1024u);
    CHECK(report.ignored_speed == 2u);
    CHECK(report.ignored_host == 1u);
    CHECK(report.unsupported == 1u);
    CHECK(strcmp(config.floppy_images[0], "disk.adf") == 0);
    {
        FILE *adf = fopen("disk.adf", "wb");
        uint8_t sector[512] = { 0 };
        CHECK(adf != NULL);
        sector[0] = 0x44u; sector[1] = 0x4fu; sector[2] = 0x53u;
        CHECK(fwrite(sector, 1, sizeof sector, adf) == sizeof sector);
        fclose(adf);
    }
    CHECK(strcmp(config.hard_drives[0], "DH0") == 0);
    {
        struct amivm_vm vm;
        uint8_t readback[3] = { 0 };
        amivm_config_init(&config);
        config.cpu_profile = amivm_cpu_profile_by_name("68040");
        config.floppy_images[0] = "disk.adf";
        CHECK(amivm_vm_init(&vm, &config) == 0);
        CHECK(amivm_media_read(&vm.floppy[0], 0u, readback, sizeof readback) == 0);
        CHECK(readback[0] == 0x44u && readback[1] == 0x4fu && readback[2] == 0x53u);
        CHECK(vm.floppy[0].tracks == 80u);
        CHECK(vm.floppy[0].heads == 2u);
        CHECK(vm.floppy[0].sectors_per_track == 11u);
        CHECK(vm.floppy[0].sector_size == 512u);
        {
            uint8_t sector[512] = { 0 };
            CHECK(amivm_media_seek(&vm.floppy[0], 0u, 0u) == 0);
            CHECK(amivm_media_read_sector(&vm.floppy[0], 0u, sector,
                                          sizeof sector) == 0);
            CHECK(sector[0] == 0x44u && sector[1] == 0x4fu &&
                  sector[2] == 0x53u);
            CHECK(amivm_media_seek(&vm.floppy[0], 79u, 1u) == 0);
            CHECK(amivm_media_read_sector(&vm.floppy[0], 10u, sector,
                                          sizeof sector) == 0);
            CHECK(amivm_media_seek(&vm.floppy[0], 80u, 0u) != 0);
            CHECK(amivm_media_seek(&vm.floppy[0], 0u, 2u) != 0);
            CHECK(amivm_media_write_sector(&vm.floppy[0], 0u, sector,
                                           sizeof sector) != 0);
            vm.trackdisk.dma_address = AMIVM_RAM_BASE + 0x2000u;
            vm.trackdisk.track = 0u;
            vm.trackdisk.head = 0u;
            vm.trackdisk.sector = 0u;
            vm.trackdisk.irq_enable = true;
            CHECK(amivm_trackdisk_command(&vm,
                                          AMIVM_TRACKDISK_READ_SECTOR) == 0);
            CHECK((vm.irq_pending & (1u << 3)) != 0u);
            CHECK(vm.trackdisk.status == 0x01u);
            CHECK(vm.ram[0x2000u] == 0x44u);
            CHECK(vm.ram[0x2001u] == 0x4fu);
            CHECK(amivm_write8(&vm, AMIVM_TRACKDISK_BASE + 0u,
                               0x10u));
            CHECK(amivm_write8(&vm, AMIVM_TRACKDISK_BASE + 1u,
                               0x00u));
            CHECK(amivm_write8(&vm, AMIVM_TRACKDISK_BASE + 2u,
                               0x20u));
            CHECK(amivm_write8(&vm, AMIVM_TRACKDISK_BASE + 3u,
                               0x00u));
            CHECK(amivm_write8(&vm, AMIVM_TRACKDISK_BASE + 4u, 0u));
            CHECK(amivm_write8(&vm, AMIVM_TRACKDISK_BASE + 5u, 0u));
            CHECK(amivm_write8(&vm, AMIVM_TRACKDISK_BASE + 6u, 0u));
            CHECK(amivm_write8(&vm, AMIVM_TRACKDISK_BASE + 10u, 1u));
            CHECK(amivm_write8(&vm, AMIVM_TRACKDISK_BASE + 7u,
                               AMIVM_TRACKDISK_READ_SECTOR));
            {
                uint8_t status = 0u;
                CHECK(amivm_read8(&vm, AMIVM_TRACKDISK_BASE + 8u, &status));
                CHECK(status == 0x01u);
            }
            amivm_clear_irq(&vm, 3u);
            vm.trackdisk.track = 80u;
            CHECK(amivm_trackdisk_command(&vm,
                                          AMIVM_TRACKDISK_SEEK) != 0);
            CHECK(vm.trackdisk.error == 2u);
            CHECK(vm.trackdisk.status == 0x80u);
        }
        amivm_vm_destroy(&vm);
    }

    f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("amiga_model=A1200\n", f);
    fclose(f);
    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, false) == 0);
    CHECK(strcmp(config.cpu_profile->name, "68020") == 0);
    CHECK(config.machine == AMIVM_MACHINE_A1200);
    CHECK(report.supported == 1u);

    f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("amiga_model=A3000\n", f);
    fclose(f);
    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, false) == 0);
    CHECK(config.machine == AMIVM_MACHINE_A3000);
    CHECK(strcmp(config.cpu_profile->name, "68030") == 0);

    f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("amiga_model=A4000\n", f);
    fclose(f);
    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, false) == 0);
    CHECK(config.machine == AMIVM_MACHINE_A4000);
    CHECK(strcmp(config.cpu_profile->name, "68040") == 0);

    f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("amiga_model=A1200\n", f);
    fputs("accelerator=68040\n", f);
    fclose(f);
    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, false) == 0);
    CHECK(config.machine == AMIVM_MACHINE_A1200);
    CHECK(strcmp(config.cpu_profile->name, "68040") == 0);
    CHECK(config.accelerator_present);
    {
        const struct amivm_machine_profile *p =
            amivm_machine_profile_by_id(config.machine);
        CHECK(p != NULL);
        CHECK(p->has_aga);
        CHECK(p->has_ide);
        CHECK(p->default_chip_ram == 2u * 1024u * 1024u);
    }

    f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("amiga_model=A500\n", f);
    fclose(f);
    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, false) == 0);
    CHECK(report.unsupported == 1u);

    amivm_config_init(&config);
    CHECK(amivm_fsuae_load_config(path, &config, &report, true) != 0);

    remove(path);
    remove("disk.adf");
    puts("AmiVM M2.148 TrackDisk MMIO register qualification: PASS");
    return 0;
}
