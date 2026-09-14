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
    uint8_t byte;

    /* MOVE.L 4(A0,D1.L*2),D0 */
    const uint16_t load_idx[] = {0x2030u, 0x1a04u};
    /* MOVE.W 6(PC,D2.W*4),D3 */
    const uint16_t pc_idx[] = {0x363bu, 0x2406u};
    /* MOVE.B D4,-2(A1,A2.W) */
    const uint16_t store_idx[] = {0x1384u, 0xa0feu};
    /* MOVEA.W 0(A0,D1.W),A2 */
    const uint16_t movea_w[] = {0x3470u, 0x1000u};
    /* MOVEA.L 4(A0,D1.L*8),A5 */
    const uint16_t movea_l[] = {0x2a70u, 0x1e04u};
    /* LEA 8(A3,A1.L*2),A4 */
    const uint16_t lea_idx[] = {0x49f3u, 0x9a08u};
    /* LEA -4(PC,D0.W),A6 */
    const uint16_t lea_pc[] = {0x4dfbu, 0x00fcu};
    /* Full extension marker: intentionally unsupported in M2.20. */
    const uint16_t full_ext[] = {0x2030u, 0x0100u};

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    memset(&cpu, 0, sizeof(cpu));

    cpu.a[0] = AMIVM_RAM_BASE + 0x100u;
    cpu.d[1] = 3u;
    CHECK(write32(&vm, cpu.a[0] + 10u, 0x11223344u));
    amivm_ir_block_init(&block, 0x1000u);
    CHECK(amivm_ir_decode_words(&block, load_idx, 2u) == 0);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_D8_AN_XN);
    CHECK(block.ops[0].index_reg == 1u);
    CHECK(block.ops[0].index_is_addr == 0u);
    CHECK(block.ops[0].index_long == 1u);
    CHECK(block.ops[0].index_scale == 2u);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.d[0] == 0x11223344u);
    CHECK(cpu.pc == 0x1004u);

    /* PC base is extension-word address (instruction PC + 2). */
    memset(&cpu, 0, sizeof(cpu));
    cpu.d[2] = 2u;
    CHECK(amivm_write8(&vm, 0x2002u + 6u + 8u, 0x80u));
    CHECK(amivm_write8(&vm, 0x2002u + 6u + 9u, 0x01u));
    amivm_ir_block_init(&block, 0x2000u);
    CHECK(amivm_ir_decode_words(&block, pc_idx, 2u) == 0);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_PC_D8_XN);
    CHECK(block.ops[0].index_scale == 4u);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK((cpu.d[3] & 0xffffu) == 0x8001u);
    CHECK((cpu.sr & SR_N) != 0u);

    memset(&cpu, 0, sizeof(cpu));
    cpu.a[1] = AMIVM_RAM_BASE + 0x300u;
    cpu.a[2] = 5u;
    cpu.d[4] = 0xa5u;
    amivm_ir_block_init(&block, 0x3000u);
    CHECK(amivm_ir_decode_words(&block, store_idx, 2u) == 0);
    CHECK(block.ops[0].index_is_addr == 1u);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(amivm_read8(&vm, AMIVM_RAM_BASE + 0x303u, &byte));
    CHECK(byte == 0xa5u);

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = SR_X | SR_N | SR_Z | SR_V | SR_C;
    cpu.a[0] = AMIVM_RAM_BASE + 0x400u;
    cpu.d[1] = 2u;
    CHECK(amivm_write8(&vm, cpu.a[0] + 2u, 0xffu));
    CHECK(amivm_write8(&vm, cpu.a[0] + 3u, 0x80u));
    amivm_ir_block_init(&block, 0x4000u);
    CHECK(amivm_ir_decode_words(&block, movea_w, 2u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_MOVEA_W);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.a[2] == 0xffffff80u);
    CHECK(cpu.sr == (SR_X | SR_N | SR_Z | SR_V | SR_C));

    cpu.a[0] = AMIVM_RAM_BASE + 0x500u;
    cpu.d[1] = 1u;
    CHECK(write32(&vm, cpu.a[0] + 12u, 0xcafebabeu));
    amivm_ir_block_init(&block, 0x5000u);
    CHECK(amivm_ir_decode_words(&block, movea_l, 2u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_MOVEA_L);
    CHECK(block.ops[0].index_scale == 8u);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.a[5] == 0xcafebabeu);

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = SR_Z;
    cpu.a[3] = AMIVM_RAM_BASE + 0x600u;
    cpu.a[1] = 3u;
    amivm_ir_block_init(&block, 0x6000u);
    CHECK(amivm_ir_decode_words(&block, lea_idx, 2u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_LEA);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.a[4] == AMIVM_RAM_BASE + 0x60eu);
    CHECK(cpu.sr == SR_Z);

    memset(&cpu, 0, sizeof(cpu));
    cpu.d[0] = 6u;
    amivm_ir_block_init(&block, 0x7000u);
    CHECK(amivm_ir_decode_words(&block, lea_pc, 2u) == 0);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_PC_D8_XN);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.a[6] == 0x7004u);

    amivm_ir_block_init(&block, 0x8000u);
    CHECK(amivm_ir_decode_words(&block, full_ext, 2u) == 1);
    CHECK(block.ops[0].opcode == AMIVM_IR_EXIT);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.20 indexed/MOVEA/LEA tests: PASS");
    return 0;
}
