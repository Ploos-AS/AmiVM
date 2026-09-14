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

#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u
#define SR_S 0x2000u

static int read8_vm(struct amivm_vm *vm, uint32_t addr, uint8_t *value)
{
    return amivm_read8(vm, addr, value) ? 1 : 0;
}

static int read16_vm(struct amivm_vm *vm, uint32_t addr, uint16_t *value)
{
    uint8_t hi, lo;
    if (!amivm_read8(vm, addr, &hi) || !amivm_read8(vm, addr + 1u, &lo)) return 0;
    *value = (uint16_t)(((uint16_t)hi << 8u) | lo);
    return 1;
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_ir_block block;
    uint8_t byte_value;
    uint16_t word_value;

    const uint16_t load_b_post[] = {0x1018u};             /* MOVE.B (A0)+,D0 */
    const uint16_t load_w_pre[] = {0x3221u};              /* MOVE.W -(A1),D1 */
    const uint16_t store_b_a7_post[] = {0x1ec2u};         /* MOVE.B D2,(A7)+ */
    const uint16_t store_w_pre[] = {0x3d03u};             /* MOVE.W D3,-(A6) */
    const uint16_t load_w_absw[] = {0x3838u, 0x1234u};    /* MOVE.W $1234.w,D4 */
    const uint16_t store_l_absl[] = {0x23c5u, 0x1000u, 0x0800u}; /* MOVE.L D5,$10000800 */
    const uint16_t load_b_pcd16[] = {0x1c3au, 0x0100u};   /* MOVE.B 256(PC),D6 */

    amivm_config_init(&config);
    config.ram_size = 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = SR_S;

    cpu.a[0] = AMIVM_RAM_BASE + 0x100u;
    cpu.d[0] = 0xaabbcc00u;
    CHECK(amivm_write8(&vm, cpu.a[0], 0x80u));
    amivm_ir_block_init(&block, 0x1000u);
    CHECK(amivm_ir_decode_words(&block, load_b_post, 1u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_LOAD_B);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.d[0] == 0xaabbcc80u);
    CHECK(cpu.a[0] == AMIVM_RAM_BASE + 0x101u);
    CHECK((cpu.sr & SR_N) != 0u);
    CHECK((cpu.sr & (SR_Z | SR_V | SR_C)) == 0u);

    cpu.a[1] = AMIVM_RAM_BASE + 0x204u;
    cpu.d[1] = 0xdead0000u;
    CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x202u, 0x00u));
    CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x203u, 0x00u));
    amivm_ir_block_init(&block, 0x1100u);
    CHECK(amivm_ir_decode_words(&block, load_w_pre, 1u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_LOAD_W);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.d[1] == 0xdead0000u);
    CHECK(cpu.a[1] == AMIVM_RAM_BASE + 0x202u);
    CHECK((cpu.sr & SR_Z) != 0u);

    cpu.a[7] = AMIVM_RAM_BASE + 0x300u;
    cpu.isp = cpu.a[7];
    cpu.d[2] = 0x123456a5u;
    amivm_ir_block_init(&block, 0x1200u);
    CHECK(amivm_ir_decode_words(&block, store_b_a7_post, 1u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(read8_vm(&vm, AMIVM_RAM_BASE + 0x300u, &byte_value));
    CHECK(byte_value == 0xa5u);
    CHECK(cpu.a[7] == AMIVM_RAM_BASE + 0x302u);
    CHECK(cpu.isp == cpu.a[7]);

    cpu.a[6] = AMIVM_RAM_BASE + 0x404u;
    cpu.d[3] = 0x89abcdefu;
    amivm_ir_block_init(&block, 0x1300u);
    CHECK(amivm_ir_decode_words(&block, store_w_pre, 1u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.a[6] == AMIVM_RAM_BASE + 0x402u);
    CHECK(read16_vm(&vm, cpu.a[6], &word_value));
    CHECK(word_value == 0xcdefu);

    amivm_ir_block_init(&block, 0x1400u);
    CHECK(amivm_ir_decode_words(&block, load_w_absw, 2u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_LOAD_W);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_ABS_W);
    CHECK(block.ops[0].imm == 0x1234);
    CHECK(block.guest_end_pc == 0x1404u);

    cpu.d[5] = 0xfeedfaceu;
    amivm_ir_block_init(&block, 0x1500u);
    CHECK(amivm_ir_decode_words(&block, store_l_absl, 3u) == 0);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_ABS_L);
    CHECK(block.guest_end_pc == 0x1506u);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(read16_vm(&vm, AMIVM_RAM_BASE + 0x800u, &word_value));
    CHECK(word_value == 0xfeedu);
    CHECK(read16_vm(&vm, AMIVM_RAM_BASE + 0x802u, &word_value));
    CHECK(word_value == 0xfaceu);

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = SR_S;
    cpu.d[6] = 0xffffff00u;
    amivm_ir_block_init(&block, AMIVM_RAM_BASE + 0x600u);
    CHECK(amivm_ir_decode_words(&block, load_b_pcd16, 2u) == 0);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_PC_D16);
    CHECK(amivm_write8(&vm, AMIVM_RAM_BASE + 0x702u, 0x7fu));
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
    CHECK(cpu.d[6] == 0xffffff7fu);
    CHECK(cpu.pc == AMIVM_RAM_BASE + 0x604u);
    CHECK((cpu.sr & (SR_N | SR_Z | SR_V | SR_C)) == 0u);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.19 byte/word/absolute/PC-relative IR tests: PASS");
    return 0;
}
