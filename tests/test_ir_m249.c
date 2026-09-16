#include "ir.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s\n", #x); return 1; } } while (0)

static uint32_t read_be32(struct amivm_vm *vm, uint32_t address)
{
    uint8_t b[4];
    unsigned i;
    for (i = 0u; i < 4u; ++i) {
        if (!amivm_read8(vm, address + i, &b[i])) return 0xffffffffu;
    }
    return ((uint32_t)b[0] << 24u) | ((uint32_t)b[1] << 16u) |
           ((uint32_t)b[2] << 8u) | (uint32_t)b[3];
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_ir_block block;
    const uint16_t jsr_d16[] = {0x4eabu, 0xfff0u};
    const uint16_t jmp_d16[] = {0x4eedu, 0x0120u};
    const uint16_t jsr_pc[] = {0x4ebau, 0x0030u};
    const uint16_t jmp_pc[] = {0x4efau, 0xffe0u};
    uint32_t sp = AMIVM_RAM_BASE + 0x3000u;

    amivm_config_init(&config);
    config.ram_size = 1u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = 0x2000u;
    cpu.a[7] = sp;
    cpu.isp = sp;

    cpu.a[3] = 0x20001000u;
    amivm_ir_block_init(&block, 0x4000u);
    CHECK(amivm_ir_decode_words(&block, jsr_d16, 2u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_JSR);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_D16_AN);
    CHECK(block.ops[0].reg == 3u);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == 0x20000ff0u);
    CHECK(read_be32(&vm, sp - 4u) == 0x4004u);

    cpu.a[7] = sp;
    cpu.isp = sp;
    cpu.a[5] = 0x30000000u;
    amivm_ir_block_init(&block, 0x5000u);
    CHECK(amivm_ir_decode_words(&block, jmp_d16, 2u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_JMP);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_D16_AN);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == 0x30000120u);
    CHECK(cpu.a[7] == sp);

    amivm_ir_block_init(&block, 0x6000u);
    CHECK(amivm_ir_decode_words(&block, jsr_pc, 2u) == 0);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_PC_D16);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == 0x6032u);
    CHECK(read_be32(&vm, sp - 4u) == 0x6004u);

    cpu.a[7] = sp;
    cpu.isp = sp;
    amivm_ir_block_init(&block, 0x7000u);
    CHECK(amivm_ir_decode_words(&block, jmp_pc, 2u) == 0);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_PC_D16);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == 0x6fe2u);
    CHECK(cpu.a[7] == sp);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.49 d16(An)/PC-relative JSR/JMP IR tests: PASS");
    return 0;
}
