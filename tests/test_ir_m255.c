#include "ir.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s\n", #x); return 1; } } while (0)

static int put32(struct amivm_vm *vm, uint32_t address, uint32_t value)
{
    unsigned i;
    for (i = 0u; i < 4u; ++i) {
        unsigned shift = (3u - i) * 8u;
        if (!amivm_write8(vm, address + i, (uint8_t)(value >> shift))) return 0;
    }
    return 1;
}

static uint32_t get32(struct amivm_vm *vm, uint32_t address)
{
    uint8_t b[4];
    unsigned i;
    for (i = 0u; i < 4u; ++i)
        if (!amivm_read8(vm, address + i, &b[i])) return 0xffffffffu;
    return ((uint32_t)b[0] << 24u) | ((uint32_t)b[1] << 16u) |
           ((uint32_t)b[2] << 8u) | (uint32_t)b[3];
}

int main(void)
{
    struct amivm_config cfg;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_ir_block block;
    uint32_t sp = AMIVM_RAM_BASE + 0x7000u;
    uint32_t table;

    /* D2.l, full, word BD, preindexed, word OD. */
    const uint16_t jsr_pre[] = {0x4eb3u, 0x2922u, 0x0020u, 0x0010u};
    /* D1.l, full, word BD, postindexed, word OD. */
    const uint16_t jmp_post[] = {0x4ef4u, 0x1926u, 0x0030u, 0xfff0u};

    amivm_config_init(&cfg);
    cfg.ram_size = 2u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &cfg) == 0);
    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = 0x2000u;
    cpu.a[7] = sp;
    cpu.isp = sp;

    cpu.a[3] = AMIVM_RAM_BASE + 0x1000u;
    cpu.d[2] = 4u;
    table = cpu.a[3] + 0x20u + 4u;
    CHECK(put32(&vm, table, 0x20000000u));
    amivm_ir_block_init(&block, 0x4000u);
    CHECK(amivm_ir_decode_words(&block, jsr_pre, 4u) == 0);
    CHECK(block.ops[0].indirect_mode == AMIVM_EA_INDIRECT_PREINDEXED);
    CHECK(block.ops[0].instruction_bytes == 8u);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == 0x20000010u);
    CHECK(cpu.a[7] == sp - 4u);
    CHECK(get32(&vm, sp - 4u) == 0x4008u);

    cpu.a[7] = sp;
    cpu.isp = sp;
    cpu.a[4] = AMIVM_RAM_BASE + 0x2000u;
    cpu.d[1] = 8u;
    table = cpu.a[4] + 0x30u;
    CHECK(put32(&vm, table, 0x30000000u));
    amivm_ir_block_init(&block, 0x5000u);
    CHECK(amivm_ir_decode_words(&block, jmp_post, 4u) == 0);
    CHECK(block.ops[0].indirect_mode == AMIVM_EA_INDIRECT_POSTINDEXED);
    CHECK(block.ops[0].instruction_bytes == 8u);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == 0x2ffffff8u);
    CHECK(cpu.a[7] == sp);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.55 memory-indirect indexed control-flow tests: PASS");
    return 0;
}
