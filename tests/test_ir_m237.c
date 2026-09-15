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

static int write_long(struct amivm_vm *vm, uint32_t address, uint32_t value)
{
    return amivm_write8(vm, address, (uint8_t)(value >> 24u)) &&
           amivm_write8(vm, address + 1u, (uint8_t)(value >> 16u)) &&
           amivm_write8(vm, address + 2u, (uint8_t)(value >> 8u)) &&
           amivm_write8(vm, address + 3u, (uint8_t)value) ? 0 : -1;
}

static int run_rts(uint16_t sr)
{
    const uint16_t words[] = { 0x4e75u };
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_ir_block block;
    uint32_t initial_sp = AMIVM_RAM_BASE + 0x1000u;
    uint32_t return_pc = 0x00123456u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    CHECK(write_long(&vm, initial_sp, return_pc) == 0);

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = sr;
    cpu.a[7] = initial_sp;
    if ((sr & 0x2000u) == 0u) cpu.usp = initial_sp;
    else if ((sr & 0x1000u) != 0u) cpu.msp = initial_sp;
    else cpu.isp = initial_sp;

    amivm_ir_block_init(&block, 0x00004000u);
    CHECK(amivm_ir_decode_words(&block, words, 1u) == 0);
    CHECK(block.op_count == 1u);
    CHECK(block.ops[0].opcode == AMIVM_IR_RTS);
    CHECK(block.ops[0].instruction_bytes == 2u);
    CHECK(block.terminates == 1);

    CHECK(amivm_ir_execute(&block, &cpu, &vm) == 1);
    CHECK(cpu.pc == return_pc);
    CHECK(cpu.a[7] == initial_sp + 4u);
    if ((sr & 0x2000u) == 0u) CHECK(cpu.usp == cpu.a[7]);
    else if ((sr & 0x1000u) != 0u) CHECK(cpu.msp == cpu.a[7]);
    else CHECK(cpu.isp == cpu.a[7]);

    amivm_vm_destroy(&vm);
    return 0;
}

static int fault_preserves_stack(void)
{
    const uint16_t words[] = { 0x4e75u };
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_ir_block block;
    uint32_t initial_sp = AMIVM_RAM_BASE - 4u;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = 0x2000u;
    cpu.a[7] = initial_sp;
    cpu.isp = initial_sp;

    amivm_ir_block_init(&block, 0x00004000u);
    CHECK(amivm_ir_decode_words(&block, words, 1u) == 0);
    CHECK(amivm_ir_execute(&block, &cpu, &vm) == -4);
    CHECK(cpu.a[7] == initial_sp);
    CHECK(cpu.isp == initial_sp);

    amivm_vm_destroy(&vm);
    return 0;
}

int main(void)
{
    CHECK(run_rts(0x0000u) == 0); /* USP */
    CHECK(run_rts(0x2000u) == 0); /* ISP */
    CHECK(run_rts(0x3000u) == 0); /* MSP */
    CHECK(fault_preserves_stack() == 0);
    puts("AmiVM M2.37 RTS IR stack tests: PASS");
    return 0;
}
