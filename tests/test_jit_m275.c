#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
#if defined(__x86_64__) && defined(__linux__)
    struct amivm_ir_block b = {0}; struct amivm_jit_code j;
    struct amivm_cpu_state cpu = {0}; struct amivm_vm vm = {0};
    struct amivm_jit_context ctx; struct amivm_config config;
    amivm_config_init(&config); config.ram_size = 2u * 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0) return 1;
    amivm_jit_context_init(&ctx, &cpu, &vm);
    cpu.a[4] = 0xb000u; cpu.pc = 0xa000u; cpu.sr = 0x2000u;
    b.guest_start_pc = 0xa000u; b.guest_end_pc = 0xa004u; b.op_count = 1u; b.terminates = 1;
    b.ops[0].opcode = AMIVM_IR_JMP; b.ops[0].ea_mode = AMIVM_IR_EA_D16_AN;
    b.ops[0].reg = 4u; b.ops[0].imm = 0x0010u; b.ops[0].guest_pc = 0xa000u;
    b.ops[0].instruction_bytes = 4u;
    if (amivm_jit_compile(&b, &j) != AMIVM_JIT_OK || !j.requires_context) return 1;
    if (amivm_jit_execute_context(&j, &cpu, &ctx) != 1) return 1;
    if (cpu.pc != 0xb010u) return 1;
    amivm_vm_destroy(&vm);
#endif
    puts("AmiVM M2.75 x86-64 JMP d16(An): PASS"); return 0;
}
