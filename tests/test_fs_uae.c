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
            }
            CHECK(vm.aga.beam_h == 20u);
            CHECK(vm.aga.display_active);
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
