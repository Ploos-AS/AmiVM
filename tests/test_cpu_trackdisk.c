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
    /* enable TrackDisk IRQ before issuing command */
    put16(vm.ram + 34u, 0x7001u);
    put16(vm.ram + 36u, 0x13c0u); put32(vm.ram + 38u, AMIVM_TRACKDISK_BASE + 10u);
    /* MOVE.B D0,$00f03007 / READ_SECTOR */
    put16(vm.ram + 42u, 0x13c0u); put32(vm.ram + 44u, AMIVM_TRACKDISK_BASE + 7u);

    cpu.pc = pc;
    cpu.sr = 0x2700u;
    for (unsigned i = 0; i < 10u; ++i)
        CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);

    CHECK(vm.trackdisk.dma_address == AMIVM_RAM_BASE + 0x2000u);
    CHECK(vm.trackdisk.track == 0u);
    CHECK(vm.trackdisk.head == 0u);
    CHECK(vm.trackdisk.sector == 0u);
    CHECK(vm.trackdisk.command == AMIVM_TRACKDISK_READ_SECTOR);
    CHECK(vm.trackdisk.status == 0x01u);
    CHECK((vm.irq.pending & (1u << 3)) != 0u);
    {
        uint8_t v = 0u;
        CHECK(amivm_read8(&vm, AMIVM_IRQ_BASE + 0u, &v) && (v & 0x08u));
        CHECK(amivm_read8(&vm, AMIVM_IRQ_BASE + 1u, &v) && (v & 0x08u));
        CHECK(amivm_read8(&vm, AMIVM_IRQ_BASE + 2u, &v) && (v & 0x08u));
        CHECK(amivm_write8(&vm, AMIVM_IRQ_BASE + 2u, 0x08u));
        CHECK((vm.irq.pending & (1u << 3)) == 0u);
    }
    amivm_raise_irq(&vm, 3u);
    CHECK(vm.ram[0x2000u] == 0x44u);

    /* Autovector level 3 = vector 27; handler contains RTE. */
    {
        uint32_t handler = AMIVM_RAM_BASE + 0x3000u;
        uint32_t vector_addr = cpu.vbr + (27u * 4u);
        CHECK(amivm_write8(&vm, vector_addr + 0u, (uint8_t)(handler >> 24)));
        CHECK(amivm_write8(&vm, vector_addr + 1u, (uint8_t)(handler >> 16)));
        CHECK(amivm_write8(&vm, vector_addr + 2u, (uint8_t)(handler >> 8)));
        CHECK(amivm_write8(&vm, vector_addr + 3u, (uint8_t)handler));
        CHECK(amivm_write8(&vm, handler + 0u, 0x4eu));
        CHECK(amivm_write8(&vm, handler + 1u, 0x73u));
        cpu.sr = 0x2000u;
        cpu.pc = AMIVM_RAM_BASE + 0x1000u;
        amivm_irq_enable(&vm, 3u, true);
        amivm_raise_irq(&vm, 3u);
        CHECK(amivm_cpu_step(&cpu, &vm, backend) == 2);
        CHECK(cpu.pc == handler);
        CHECK(cpu.last_exception_vector == 27u);
        CHECK((vm.irq.pending & (1u << 3)) == 0u);
        CHECK(amivm_cpu_step(&cpu, &vm, backend) == 1);
        CHECK(cpu.pc == AMIVM_RAM_BASE + 0x1000u);
        CHECK(cpu.sr == 0x2000u);
    }
    CHECK(vm.ram[0x2001u] == 0x4fu);
    CHECK(vm.ram[0x2002u] == 0x53u);

    amivm_irq_enable(&vm, 6u, true);
    vm.timer.period = 10u;
    vm.timer.enabled = true;
    vm.timer.irq_enable = true;
    vm.timer.periodic = true;
    amivm_timer_tick(&vm, 9u);
    CHECK(!amivm_irq_is_pending(&vm, 6u));
    amivm_timer_tick(&vm, 1u);
    CHECK(amivm_irq_is_pending(&vm, 6u));
    CHECK(vm.timer.enabled);
    CHECK(vm.timer.counter == 0u);
    amivm_clear_irq(&vm, 6u);
    vm.timer.periodic = false;
    vm.timer.counter = 0u;
    amivm_timer_tick(&vm, 10u);
    CHECK(amivm_irq_is_pending(&vm, 6u));
    CHECK(!vm.timer.enabled);

    amivm_vm_destroy(&vm);
    remove("m2_150.adf");
    puts("AmiVM M2.155 TrackDisk plus timer generic IRQ qualification qualification: PASS");
    return 0;
}
