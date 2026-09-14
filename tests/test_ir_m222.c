#include "ir.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

#define SR_X 0x0010u
#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u

static int write32(struct amivm_vm *vm, uint32_t addr, uint32_t value)
{
    return amivm_write8(vm, addr, (uint8_t)(value >> 24u)) &&
           amivm_write8(vm, addr + 1u, (uint8_t)(value >> 16u)) &&
           amivm_write8(vm, addr + 2u, (uint8_t)(value >> 8u)) &&
           amivm_write8(vm, addr + 3u, (uint8_t)value);
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_ir_block block;

    /* MOVE.L ([bd,A0,D1.L*2] direct),D0; full format, word BD. */
    const uint16_t move_direct[] = {0x2030u, 0x1b20u, 0x0004u};
    /* MOVE.L full format with base+index suppressed and long BD,D1. */
    const uint16_t move_suppressed[] = {0x2230u, 0x01f0u, 0x1000u, 0x0200u};
    /* MOVE.L preindexed memory indirect, word BD + word OD,D2. */
    const uint16_t move_pre[] = {0x2430u, 0x1b22u, 0x0010u, 0xfffcu};
    /* MOVE.L postindexed memory indirect, word BD + word OD,D3. */
    const uint16_t move_post[] = {0x2630u, 0x1b26u, 0x0020u, 0x0004u};
    /* MOVEA.L full direct, base/index suppressed, long BD,A4. */
    const uint16_t movea_full[] = {0x2870u, 0x01f0u, 0x1000u, 0x0700u};
    /* LEA preindexed memory indirect, word BD + word OD,A5. */
    const uint16_t lea_full[] = {0x4bf0u, 0x0122u, 0x0010u, 0xfff8u};

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    memset(&cpu, 0, sizeof(cpu));
    cpu.a[0] = AMIVM_RAM_BASE + 0x100u;
    cpu.d[1] = 3u;
    CHECK(write32(&vm, cpu.a[0] + 10u, 0x11223344u));
    amivm_ir_block_init(&block, 0x1000u);
    CHECK(amivm_ir_decode_words(&block, move_direct, 3u) == 0);
    CHECK(block.ops[0].full_format == 1u);
    CHECK(block.ops[0].base_displacement == 4);
    CHECK(block.ops[0].instruction_bytes == 6u);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.d[0] == 0x11223344u);
    CHECK(cpu.pc == 0x1006u);

    memset(&cpu, 0, sizeof(cpu));
    CHECK(write32(&vm, AMIVM_RAM_BASE + 0x200u, 0x55667788u));
    amivm_ir_block_init(&block, 0x2000u);
    CHECK(amivm_ir_decode_words(&block, move_suppressed, 4u) == 0);
    CHECK(block.ops[0].base_suppress == 1u);
    CHECK(block.ops[0].index_suppress == 1u);
    CHECK(block.ops[0].instruction_bytes == 8u);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.d[1] == 0x55667788u);

    memset(&cpu, 0, sizeof(cpu));
    cpu.a[0] = AMIVM_RAM_BASE + 0x300u;
    cpu.d[1] = 2u;
    CHECK(write32(&vm, cpu.a[0] + 0x14u, AMIVM_RAM_BASE + 0x500u));
    CHECK(write32(&vm, AMIVM_RAM_BASE + 0x4fcu, 0xaabbccddu));
    amivm_ir_block_init(&block, 0x3000u);
    CHECK(amivm_ir_decode_words(&block, move_pre, 4u) == 0);
    CHECK(block.ops[0].indirect_mode == AMIVM_EA_INDIRECT_PREINDEXED);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.d[2] == 0xaabbccddu);

    memset(&cpu, 0, sizeof(cpu));
    cpu.a[0] = AMIVM_RAM_BASE + 0x400u;
    cpu.d[1] = 3u;
    CHECK(write32(&vm, cpu.a[0] + 0x20u, AMIVM_RAM_BASE + 0x600u));
    CHECK(write32(&vm, AMIVM_RAM_BASE + 0x60au, 0x10203040u));
    amivm_ir_block_init(&block, 0x4000u);
    CHECK(amivm_ir_decode_words(&block, move_post, 4u) == 0);
    CHECK(block.ops[0].indirect_mode == AMIVM_EA_INDIRECT_POSTINDEXED);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.d[3] == 0x10203040u);

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = SR_X | SR_N | SR_Z | SR_V | SR_C;
    CHECK(write32(&vm, AMIVM_RAM_BASE + 0x700u, 0xcafebabeu));
    amivm_ir_block_init(&block, 0x5000u);
    CHECK(amivm_ir_decode_words(&block, movea_full, 4u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_MOVEA_L);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.a[4] == 0xcafebabeu);
    CHECK(cpu.sr == (SR_X | SR_N | SR_Z | SR_V | SR_C));

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = SR_Z;
    cpu.a[0] = AMIVM_RAM_BASE + 0x800u;
    cpu.d[0] = 2u;
    CHECK(write32(&vm, cpu.a[0] + 0x12u, AMIVM_RAM_BASE + 0x900u));
    amivm_ir_block_init(&block, 0x6000u);
    CHECK(amivm_ir_decode_words(&block, lea_full, 4u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_LEA);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.a[5] == AMIVM_RAM_BASE + 0x8f8u);
    CHECK(cpu.sr == SR_Z);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.22 full-extension IR tests: PASS");
    return 0;
}
