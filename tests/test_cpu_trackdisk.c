#include "cpu.h"
#include "vm.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); return 1; } } while (0)

static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static void put32(uint8_t *p, uint32_t v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v;
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    const struct amivm_cpu_backend *backend = amivm_cpu_reference_backend();
    FILE *adf;
    uint8_t sector[512] = { 0 };
    uint32_t pc = AMIVM_RAM_BASE;

    adf = fopen("m2_150.adf", "wb");
    CHECK(adf != NULL);
    sector[0] = 0x44u; sector[1] = 0x4fu; sector[2] = 0x53u;
    CHECK(fwrite(sector, 1, sizeof sector, adf) == sizeof sector);
    CHECK(fseek(adf, (long)(1760u * 1024u) - 1L, SEEK_SET) == 0);
    CHECK(fputc(0, adf) != EOF);
    fclose(adf);

    amivm_config_init(&config);
    config.floppy_images[0] = "m2_150.adf";
    config.ram_size = 2u * 1024u * 1024u;
    config.cpu_profile = amivm_cpu_profile_by_name("68040");
    CHECK(amivm_vm_init(&vm, &config) == 0);
    CHECK(amivm_cpu_reset(&cpu, &vm, backend) == 0);

    /* MOVE.L #$00002000,D0 */
    put16(vm.ram, 0x203cu); put32(vm.ram + 2u, AMIVM_RAM_BASE + 0x2000u);
    /* MOVE.L D0,$00f03000 */
    put16(vm.ram + 6u, 0x23c0u); put32(vm.ram + 8u, AMIVM_TRACKDISK_BASE);
    /* MOVEQ #0,D0 */
    put16(vm.ram + 12u, 0x7000u);
    /* MOVE.B D0,$00f03004 / track */
    put16(vm.ram + 14u, 0x13c0u); put32(vm.ram + 16u, AMIVM_TRACKDISK_BASE + 4u);
    /* MOVE.B D0,$00f03005 / head */
    put16(vm.ram + 20u, 0x13c0u); put32(vm.ram + 22u, AMIVM_TRACKDISK_BASE + 5u);
    /* MOVE.B D0,$00f03006 / sector */
    put16(vm.ram + 26u, 0x13c0u); put32(vm.ram + 28u, AMIVM_TRACKDISK_BASE + 6u);
    /* MOVEQ #1,D0 */
    put16(vm.ram + 32u, 0x7001u);
    /* MOVE.B D0,$00f03007 / READ_SECTOR */
    put16(vm.ram + 34u, 0x13c0u); put32(vm.ram + 36u, AMIVM_TRACKDISK_BASE + 7u);
    /* enable TrackDisk IRQ before issuing command */
    put16(vm.ram + 40u, 0x7001u);
    put16(vm.ram + 42u, 0x13c0u); put32(vm.ram + 44u, AMIVM_TRACKDISK_BASE + 10u);

    cpu.pc = pc;
    cpu.sr = 0x2700u;
    for (unsigned i = 0; i < 11u; ++i)
        CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);

    CHECK(vm.trackdisk.dma_address == AMIVM_RAM_BASE + 0x2000u);
    CHECK(vm.trackdisk.track == 0u);
    CHECK(vm.trackdisk.head == 0u);
    CHECK(vm.trackdisk.sector == 0u);
    CHECK(vm.trackdisk.command == AMIVM_TRACKDISK_READ_SECTOR);
    CHECK(vm.trackdisk.status == 0x01u);
    CHECK((vm.irq_pending & (1u << 3)) != 0u);
    CHECK(vm.ram[0x2000u] == 0x44u);
    CHECK(vm.ram[0x2001u] == 0x4fu);
    CHECK(vm.ram[0x2002u] == 0x53u);

    amivm_vm_destroy(&vm);
    remove("m2_150.adf");
    puts("AmiVM M2.150 CPU-to-MMIO-to-DMA-to-ADF-to-IRQ qualification: PASS");
    return 0;
}
