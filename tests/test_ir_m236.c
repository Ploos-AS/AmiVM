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

static int read_long(struct amivm_vm *vm, uint32_t address, uint32_t *value)
{
    uint8_t b0, b1, b2, b3;
    if (!amivm_read8(vm, address, &b0) ||
        !amivm_read8(vm, address + 1u, &b1) ||
        !amivm_read8(vm, address + 2u, &b2) ||
        !amivm_read8(vm, address + 3u, &b3)) return -1;
    *value = ((uint32_t)b0 << 24u) | ((uint32_t)b1 << 16u) |
             ((uint32_t)b2 << 8u) | b3;
    return 0;
}

static int run_bsr(const uint16_t *words, size_t word_count,
                   uint32_t expected_target, uint32_t expected_return)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_ir_block block;
    uint32_t stacked;
    uint32_t initial_sp = AMIVM_RAM_BASE + 0x1000u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = 0x2000u;
    cpu.a[7] = initial_sp;
    cpu.isp = initial_sp;

    amivm_ir_block_init(&block, 0x00004000u);
    CHECK(amivm_ir_decode_words(&block, words, word_count) == 0);
    CHECK(block.op_count == 1u);
    CHECK(block.ops[0].opcode == AMIVM_IR_BSR);
    CHECK(block.terminates == 1);

    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == expected_target);
    CHECK(cpu.a[7] == initial_sp - 4u);
    CHECK(cpu.isp == cpu.a[7]);
    CHECK(read_long(&vm, cpu.a[7], &stacked) == 0);
    CHECK(stacked == expected_return);

    amivm_vm_destroy(&vm);
    return 0;
}

int main(void)
{
    const uint16_t short_forward[] = { 0x6110u };
    const uint16_t short_back[] = { 0x61feu };
    const uint16_t word_forward[] = { 0x6100u, 0x0010u };
    const uint16_t word_back[] = { 0x6100u, 0xfffeu };

    CHECK(run_bsr(short_forward, 1u, 0x00004012u, 0x00004002u) == 0);
    CHECK(run_bsr(short_back, 1u, 0x00004000u, 0x00004002u) == 0);
    CHECK(run_bsr(word_forward, 2u, 0x00004012u, 0x00004004u) == 0);
    CHECK(run_bsr(word_back, 2u, 0x00004000u, 0x00004004u) == 0);

    puts("AmiVM M2.36 BSR IR stack tests: PASS");
    return 0;
}
