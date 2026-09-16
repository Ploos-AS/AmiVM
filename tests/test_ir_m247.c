#include "ir.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s\n", #x); return 1; } } while (0)

static uint32_t read_be32(struct amivm_vm *vm, uint32_t address)
{
    uint8_t b0, b1, b2, b3;
    if (!amivm_read8(vm, address, &b0) || !amivm_read8(vm, address + 1u, &b1) ||
        !amivm_read8(vm, address + 2u, &b2) || !amivm_read8(vm, address + 3u, &b3)) return 0xffffffffu;
    return ((uint32_t)b0 << 24u) | ((uint32_t)b1 << 16u) |
           ((uint32_t)b2 << 8u) | (uint32_t)b3;
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_ir_block block;
    const uint16_t jsr_absw[] = {0x4eb8u, 0xff00u};
    const uint16_t jmp_absw[] = {0x4ef8u, 0x1234u};
    const uint16_t jsr_absl[] = {0x4eb9u, 0x1234u, 0x5678u};
    const uint16_t jmp_absl[] = {0x4ef9u, 0x89abu, 0xcdefu};
    uint32_t sp = AMIVM_RAM_BASE + 0x2000u;

    amivm_config_init(&config);
    config.ram_size = 1u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = 0x2000u;
    cpu.a[7] = sp;
    cpu.isp = sp;

    amivm_ir_block_init(&block, 0x4000u);
    CHECK(amivm_ir_decode_words(&block, jsr_absw, 2u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_JSR);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_ABS_W);
    CHECK(block.ops[0].instruction_bytes == 4u);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == 0xffffff00u);
    CHECK(read_be32(&vm, sp - 4u) == 0x4004u);

    cpu.a[7] = sp; cpu.isp = sp;
    amivm_ir_block_init(&block, 0x5000u);
    CHECK(amivm_ir_decode_words(&block, jsr_absl, 3u) == 0);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_ABS_L);
    CHECK(block.ops[0].instruction_bytes == 6u);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == 0x12345678u);
    CHECK(read_be32(&vm, sp - 4u) == 0x5006u);

    amivm_ir_block_init(&block, 0x6000u);
    CHECK(amivm_ir_decode_words(&block, jmp_absw, 2u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_JMP);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_ABS_W);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == 0x00001234u);

    amivm_ir_block_init(&block, 0x7000u);
    CHECK(amivm_ir_decode_words(&block, jmp_absl, 3u) == 0);
    CHECK(block.ops[0].opcode == AMIVM_IR_JMP);
    CHECK(block.ops[0].ea_mode == AMIVM_IR_EA_ABS_L);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == 0x89abcdefu);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.47 absolute JSR/JMP IR tests: PASS");
    return 0;
}
