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
#define SR_S 0x2000u

static int branch_case(uint8_t condition, uint16_t sr, int taken)
{
    struct amivm_ir_block block;
    struct amivm_cpu_state cpu;
    uint16_t words[1];

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = sr;
    words[0] = (uint16_t)(0x6000u | ((uint16_t)condition << 8u) | 0x02u);
    amivm_ir_block_init(&block, 0x9000u + (uint32_t)condition * 0x10u);
    CHECK(amivm_ir_decode_words(&block, words, 1u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_BRANCH_CC);
    CHECK(block.ops[0].condition == condition);
    CHECK(amivm_ir_execute(&block, &cpu, NULL) == 1);
    CHECK(cpu.pc == block.guest_start_pc + (taken ? 4u : 2u));
    return 0;
}

static int write32_vm(struct amivm_vm *vm, uint32_t addr, uint32_t value)
{
    return amivm_write8(vm, addr, (uint8_t)(value >> 24u)) &&
           amivm_write8(vm, addr + 1u, (uint8_t)(value >> 16u)) &&
           amivm_write8(vm, addr + 2u, (uint8_t)(value >> 8u)) &&
           amivm_write8(vm, addr + 3u, (uint8_t)value);
}

static int read32_vm(struct amivm_vm *vm, uint32_t addr, uint32_t *value)
{
    uint8_t b0, b1, b2, b3;
    if (!amivm_read8(vm, addr, &b0) || !amivm_read8(vm, addr + 1u, &b1) ||
        !amivm_read8(vm, addr + 2u, &b2) || !amivm_read8(vm, addr + 3u, &b3)) return 0;
    *value = ((uint32_t)b0 << 24u) | ((uint32_t)b1 << 16u) |
             ((uint32_t)b2 << 8u) | (uint32_t)b3;
    return 1;
}

int main(void)
{
    struct amivm_ir_block block;
    struct amivm_cpu_state cpu;
    const uint16_t hot_block[] = {0x4e71u, 0x7480u, 0x6002u};
    const uint16_t loop_block[] = {0x60feu};
    const uint16_t unsupported[] = {0x4e72u};
    const uint16_t flags_block[] = {0x7000u, 0x6602u};
    const uint16_t arithmetic_block[] = {0x7001u, 0x5280u, 0x5380u, 0x4a80u, 0x6780u};
    const uint16_t clear_branch[] = {0x4280u, 0x6702u};
    const uint16_t negative_branch[] = {0x70ffu, 0x6b02u};
    const uint16_t overflow_add[] = {0x5280u};
    const uint16_t reg_alu[] = {
        0x7005u, 0x7203u, 0xd081u, 0x9081u,
        0xb081u, 0xc081u, 0x8081u, 0xb380u
    };

    memset(&cpu, 0, sizeof(cpu));
    amivm_ir_block_init(&block, 0x1000u);
    CHECK(amivm_ir_decode_words(&block, hot_block, 3u) == 0);
    CHECK(block.op_count == 3u);
    CHECK(block.terminates == 1);
    CHECK(block.guest_end_pc == 0x1006u);
    CHECK(block.ops[0].opcode == AMIVM_IR_NOP);
    CHECK(block.ops[1].opcode == AMIVM_IR_MOVEQ);
    CHECK(block.ops[1].reg == 2u);
    CHECK(block.ops[1].imm == -128);
    CHECK(block.ops[2].opcode == AMIVM_IR_BRANCH);
    CHECK(block.ops[2].imm == 2);
    CHECK(amivm_ir_execute(&block, &cpu, NULL) == 1);
    CHECK(cpu.d[2] == 0xffffff80u);
    CHECK((cpu.sr & SR_N) != 0u);
    CHECK((cpu.sr & SR_Z) == 0u);
    CHECK(cpu.pc == 0x1008u);

    amivm_ir_block_init(&block, 0x2000u);
    CHECK(amivm_ir_decode_words(&block, loop_block, 1u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu, NULL) == 1);
    CHECK(cpu.pc == 0x2000u);

    amivm_ir_block_init(&block, 0x3000u);
    CHECK(amivm_ir_decode_words(&block, unsupported, 1u) == 1);
    CHECK(block.op_count == 1u);
    CHECK(block.ops[0].opcode == AMIVM_IR_EXIT);
    CHECK(amivm_ir_execute(&block, &cpu, NULL) == 2);
    CHECK(cpu.pc == 0x3000u);

    memset(&cpu, 0, sizeof(cpu));
    amivm_ir_block_init(&block, 0x4000u);
    CHECK(amivm_ir_decode_words(&block, flags_block, 2u) == 0);
    CHECK(block.ops[1].opcode == AMIVM_IR_BRANCH_CC);
    CHECK(block.ops[1].condition == AMIVM_IR_CC_NE);
    CHECK(amivm_ir_execute(&block, &cpu, NULL) == 1);
    CHECK((cpu.sr & SR_Z) != 0u);
    CHECK(cpu.pc == 0x4004u);

    memset(&cpu, 0, sizeof(cpu));
    amivm_ir_block_init(&block, 0x5000u);
    CHECK(amivm_ir_decode_words(&block, arithmetic_block, 5u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu, NULL) == 1);
    CHECK(cpu.d[0] == 1u);
    CHECK((cpu.sr & (SR_N | SR_Z | SR_V | SR_C)) == 0u);
    CHECK(cpu.pc == 0x500au);

    cpu.sr = SR_X | SR_N | SR_V | SR_C;
    cpu.d[0] = 0xffffffffu;
    amivm_ir_block_init(&block, 0x6000u);
    CHECK(amivm_ir_decode_words(&block, clear_branch, 2u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu, NULL) == 1);
    CHECK(cpu.d[0] == 0u);
    CHECK((cpu.sr & SR_Z) != 0u);
    CHECK((cpu.sr & (SR_N | SR_V | SR_C)) == 0u);
    CHECK((cpu.sr & SR_X) != 0u);
    CHECK(cpu.pc == 0x6006u);

    memset(&cpu, 0, sizeof(cpu));
    amivm_ir_block_init(&block, 0x7000u);
    CHECK(amivm_ir_decode_words(&block, negative_branch, 2u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu, NULL) == 1);
    CHECK((cpu.sr & SR_N) != 0u);
    CHECK(cpu.pc == 0x7006u);

    memset(&cpu, 0, sizeof(cpu));
    cpu.d[0] = 0x7fffffffu;
    amivm_ir_block_init(&block, 0x8000u);
    CHECK(amivm_ir_decode_words(&block, overflow_add, 1u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu, NULL) == 0);
    CHECK(cpu.d[0] == 0x80000000u);
    CHECK((cpu.sr & SR_N) != 0u);
    CHECK((cpu.sr & SR_V) != 0u);
    CHECK((cpu.sr & SR_C) == 0u);

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = SR_X;
    amivm_ir_block_init(&block, 0xa000u);
    CHECK(amivm_ir_decode_words(&block, reg_alu, sizeof(reg_alu) / sizeof(reg_alu[0])) == 0);
    CHECK(block.ops[2].opcode == AMIVM_IR_ADD_L);
    CHECK(block.ops[2].reg == 0u && block.ops[2].src_reg == 1u);
    CHECK(block.ops[7].opcode == AMIVM_IR_EOR_L);
    CHECK(amivm_ir_execute(&block, &cpu, NULL) == 0);
    CHECK(cpu.d[0] == 0u);
    CHECK(cpu.d[1] == 3u);
    CHECK((cpu.sr & SR_Z) != 0u);
    CHECK((cpu.sr & (SR_N | SR_V | SR_C | SR_X)) == 0u);

    CHECK(branch_case(AMIVM_IR_CC_HI, 0u, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_LS, SR_C, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_CC, 0u, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_CS, SR_C, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_NE, 0u, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_EQ, SR_Z, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_VC, 0u, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_VS, SR_V, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_PL, 0u, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_MI, SR_N, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_GE, SR_N | SR_V, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_LT, SR_N, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_GT, 0u, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_LE, SR_Z, 1) == 0);
    CHECK(branch_case(AMIVM_IR_CC_HI, SR_Z, 0) == 0);
    CHECK(branch_case(AMIVM_IR_CC_GT, SR_Z, 0) == 0);

    /* M2.18: MOVE.L memory forms: (An), (An)+, -(An), d16(An). */
    {
        struct amivm_config config;
        struct amivm_vm vm;
        const uint16_t load_an[] = {0x2010u};
        const uint16_t store_post[] = {0x22c1u};
        const uint16_t load_pre[] = {0x2422u};
        const uint16_t load_disp[] = {0x262bu, 0x0004u};
        const uint16_t store_disp[] = {0x2943u, 0x0008u};
        const uint16_t store_a7_pre[] = {0x2f00u};
        uint32_t value;

        amivm_config_init(&config);
        config.ram_size = 1024u * 1024u;
        CHECK(amivm_vm_init(&vm, &config) == 0);
        memset(&cpu, 0, sizeof(cpu));
        cpu.sr = SR_S;

        cpu.a[0] = AMIVM_RAM_BASE + 0x100u;
        CHECK(write32_vm(&vm, cpu.a[0], 0x11223344u));
        amivm_ir_block_init(&block, 0xb000u);
        CHECK(amivm_ir_decode_words(&block, load_an, 1u) == 0);
        CHECK(block.ops[0].opcode == AMIVM_IR_LOAD_L);
        CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_AN);
        CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
        CHECK(cpu.d[0] == 0x11223344u);
        CHECK(cpu.a[0] == AMIVM_RAM_BASE + 0x100u);

        cpu.a[1] = AMIVM_RAM_BASE + 0x200u;
        cpu.d[1] = 0xaabbccddu;
        amivm_ir_block_init(&block, 0xb100u);
        CHECK(amivm_ir_decode_words(&block, store_post, 1u) == 0);
        CHECK(block.ops[0].opcode == AMIVM_IR_STORE_L);
        CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_POSTINC);
        CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
        CHECK(read32_vm(&vm, AMIVM_RAM_BASE + 0x200u, &value));
        CHECK(value == 0xaabbccddu);
        CHECK(cpu.a[1] == AMIVM_RAM_BASE + 0x204u);

        cpu.a[2] = AMIVM_RAM_BASE + 0x304u;
        CHECK(write32_vm(&vm, AMIVM_RAM_BASE + 0x300u, 0x55667788u));
        amivm_ir_block_init(&block, 0xb200u);
        CHECK(amivm_ir_decode_words(&block, load_pre, 1u) == 0);
        CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
        CHECK(cpu.d[2] == 0x55667788u);
        CHECK(cpu.a[2] == AMIVM_RAM_BASE + 0x300u);

        cpu.a[3] = AMIVM_RAM_BASE + 0x400u;
        CHECK(write32_vm(&vm, AMIVM_RAM_BASE + 0x404u, 0x10203040u));
        amivm_ir_block_init(&block, 0xb300u);
        CHECK(amivm_ir_decode_words(&block, load_disp, 2u) == 0);
        CHECK(block.guest_end_pc == 0xb304u);
        CHECK(block.ops[0].imm == 4);
        CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
        CHECK(cpu.d[3] == 0x10203040u);
        CHECK(cpu.pc == 0xb304u);

        cpu.a[4] = AMIVM_RAM_BASE + 0x500u;
        cpu.d[3] = 0xdeadbeefu;
        amivm_ir_block_init(&block, 0xb400u);
        CHECK(amivm_ir_decode_words(&block, store_disp, 2u) == 0);
        CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
        CHECK(read32_vm(&vm, AMIVM_RAM_BASE + 0x508u, &value));
        CHECK(value == 0xdeadbeefu);

        cpu.a[7] = AMIVM_RAM_BASE + 0x604u;
        cpu.isp = cpu.a[7];
        cpu.d[0] = 0xcafebabeu;
        amivm_ir_block_init(&block, 0xb500u);
        CHECK(amivm_ir_decode_words(&block, store_a7_pre, 1u) == 0);
        CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
        CHECK(cpu.a[7] == AMIVM_RAM_BASE + 0x600u);
        CHECK(cpu.isp == cpu.a[7]);
        CHECK(read32_vm(&vm, cpu.a[7], &value));
        CHECK(value == 0xcafebabeu);

        {
            const uint32_t srp = AMIVM_RAM_BASE + 0x4000u;
            const uint32_t l2 = AMIVM_RAM_BASE + 0x5000u;
            const uint32_t page = AMIVM_RAM_BASE + 0x9000u;
            const uint32_t logical = 0x00400100u;
            CHECK(write32_vm(&vm, srp + 4u, l2 | 1u));
            CHECK(write32_vm(&vm, l2, page | 1u));
            CHECK(write32_vm(&vm, page + 0x100u, 0x0badf00du));
            memset(&cpu, 0, sizeof(cpu));
            cpu.sr = SR_S;
            cpu.srp = srp;
            cpu.tc = 0x80000000u;
            cpu.a[0] = logical;
            amivm_ir_block_init(&block, 0xb600u);
            CHECK(amivm_ir_decode_words(&block, load_an, 1u) == 0);
            CHECK(amivm_ir_execute(&block, &cpu, &vm) == 0);
            CHECK(cpu.d[0] == 0x0badf00du);
        }

        amivm_vm_destroy(&vm);
    }

    puts("AmiVM M2.18 IR memory/effective-address tests: PASS");
    return 0;
}
