#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

static int run_block(struct amivm_ir_block *block, struct amivm_cpu_state *cpu,
                     struct amivm_vm *vm)
{
    struct amivm_jit_code code;
    struct amivm_jit_context context;

    CHECK(amivm_jit_compile(block, &code) == AMIVM_JIT_OK);
    CHECK(code.requires_context == 1);
    CHECK(code.guest_instructions == block->op_count);
    CHECK(amivm_jit_execute(&code, cpu) == AMIVM_JIT_INVALID);
    amivm_jit_context_init(&context, cpu, vm);
    return amivm_jit_execute_context(&code, cpu, &context);
}

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_cpu_state cpu;
    struct amivm_ir_block block;
    uint32_t sp = AMIVM_RAM_BASE + 0x2000u;

    if (amivm_jit_host_arch() != AMIVM_JIT_ARCH_X86_64) {
        puts("AmiVM M2.44 mixed native BSR/RTS tests: SKIP (non-x86-64 host)");
        return 0;
    }
#if !defined(__x86_64__) || !defined(__linux__)
    puts("AmiVM M2.44 mixed native BSR/RTS tests: SKIP (execution unavailable)");
    return 0;
#endif

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    CHECK(amivm_vm_init(&vm, &config) == 0);
    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = 0x2000u;
    cpu.a[7] = sp;
    cpu.isp = sp;

    /* MOVEQ #7,D0 followed by BSR.s. The ordinary native prefix must execute
       before the context-aware call helper, and the stacked return is P+2. */
    amivm_ir_block_init(&block, 0x4000u);
    block.op_count = 2u;
    block.guest_end_pc = 0x4004u;
    block.terminates = 1;
    block.ops[0].opcode = AMIVM_IR_MOVEQ;
    block.ops[0].guest_pc = 0x4000u;
    block.ops[0].reg = 0u;
    block.ops[0].imm = 7;
    block.ops[0].instruction_bytes = 2u;
    block.ops[1].opcode = AMIVM_IR_BSR;
    block.ops[1].guest_pc = 0x4002u;
    block.ops[1].imm = 0x10;
    block.ops[1].instruction_bytes = 2u;
    CHECK(run_block(&block, &cpu, &vm) == 1);
    CHECK(cpu.d[0] == 7u);
    CHECK(cpu.pc == 0x4014u);
    CHECK(cpu.a[7] == sp - 4u && cpu.isp == sp - 4u);

    /* ADDQ.L #1,D0 followed by RTS. RTS must consume the return address just
       stacked by BSR while preserving the prefix result. */
    amivm_ir_block_init(&block, 0x5000u);
    block.op_count = 2u;
    block.guest_end_pc = 0x5004u;
    block.terminates = 1;
    block.ops[0].opcode = AMIVM_IR_ADDQ_L;
    block.ops[0].guest_pc = 0x5000u;
    block.ops[0].reg = 0u;
    block.ops[0].imm = 1;
    block.ops[0].instruction_bytes = 2u;
    block.ops[1].opcode = AMIVM_IR_RTS;
    block.ops[1].guest_pc = 0x5002u;
    block.ops[1].instruction_bytes = 2u;
    CHECK(run_block(&block, &cpu, &vm) == 1);
    CHECK(cpu.d[0] == 8u);
    CHECK(cpu.pc == 0x4004u);
    CHECK(cpu.a[7] == sp && cpu.isp == sp);

    amivm_vm_destroy(&vm);
    puts("AmiVM M2.44 native prefix BSR/RTS tests: PASS");
    return 0;
}
