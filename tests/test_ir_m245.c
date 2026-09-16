#include "ir.h"
#include "vm.h"

#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s\n", #x); return 1; } } while (0)

static uint32_t read_be32(struct amivm_vm *vm, uint32_t address)
{
    uint8_t b0, b1, b2, b3;
    if (!amivm_read8(vm, address, &b0) || !amivm_read8(vm, address + 1u, &b1) ||
        !amivm_read8(vm, address + 2u, &b2) || !amivm_read8(vm, address + 3u, &b3))
        return 0xffffffffu;
    return ((uint32_t)b0 << 24u) | ((uint32_t)b1 << 16u) |
           ((uint32_t)b2 << 8u) | (uint32_t)b3;
}

int main(void)
{
    struct amivm_ir_block block;
    struct amivm_cpu_state cpu = {0};
    struct amivm_config config;
    struct amivm_vm vm;
    const uint16_t jsr_words[] = { 0x4e92u }; /* JSR (A2) */
    const uint16_t jmp_words[] = { 0x4ed3u }; /* JMP (A3) */
    uint32_t sp = AMIVM_RAM_BASE + 0x1000u;

    amivm_config_init(&config);
    config.ram_size = 1u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    cpu.sr = 0x2000u;
    cpu.a[7] = sp;
    cpu.isp = sp;
    cpu.a[2] = 0x12345678u;

    amivm_ir_block_init(&block, 0x4000u);
    CHECK(amivm_ir_decode_words(&block, jsr_words, 1u) == 0);
    CHECK(block.op_count == 1u);
    CHECK(block.ops[0].opcode == AMIVM_IR_JSR);
    CHECK(block.ops[0].reg == 2u);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_AN);
    CHECK(block.terminates != 0);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == 0x12345678u);
    CHECK(cpu.a[7] == sp - 4u);
    CHECK(cpu.isp == sp - 4u);
    CHECK(read_be32(&vm, sp - 4u) == 0x4002u);

    cpu.a[3] = 0x89abcdefu;
    amivm_ir_block_init(&block, 0x5000u);
    CHECK(amivm_ir_decode_words(&block, jmp_words, 1u) == 0);
    CHECK(block.op_count == 1u);
    CHECK(block.ops[0].opcode == AMIVM_IR_JMP);
    CHECK(block.ops[0].reg == 3u);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_AN);
    CHECK(block.terminates != 0);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == 0x89abcdefu);
    CHECK(cpu.a[7] == sp - 4u);

    /* Stack failure must preserve JSR-visible state. */
    cpu.pc = 0x6000u;
    cpu.a[7] = AMIVM_RAM_BASE;
    cpu.isp = cpu.a[7];
    cpu.a[2] = 0x11112222u;
    amivm_ir_block_init(&block, 0x6000u);
    CHECK(amivm_ir_decode_words(&block, jsr_words, 1u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == -4);
    CHECK(cpu.pc == 0x6000u);
    CHECK(cpu.a[7] == AMIVM_RAM_BASE);

    amivm_vm_destroy(&vm);
    puts("M2.45 dynamic JSR/JMP IR PASS");
    return 0;
}
