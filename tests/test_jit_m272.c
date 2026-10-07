#include "jit.h"
#include "jit_helpers.h"
#include "vm.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
#if defined(__x86_64__) && defined(__linux__)
    struct amivm_ir_block b;
    struct amivm_jit_code j;
    struct amivm_cpu_state cpu;
    struct amivm_vm vm;
    struct amivm_jit_context ctx;

    memset(&b, 0, sizeof(b));
    memset(&cpu, 0, sizeof(cpu));
    memset(&vm, 0, sizeof(vm));
    if (amivm_vm_init(&vm, NULL) != 0) { fprintf(stderr, "VM init failed\\n"); return 1; }
    cpu.pc = 0xa000u;
    cpu.sr = 0x2000u;

    amivm_jit_context_init(&ctx, &cpu, &vm);
    cpu.a[7] = AMIVM_RAM_BASE + 0x800u;

    b.guest_start_pc = 0xa000u;
    b.guest_end_pc = 0xa002u;
    b.op_count = 1u;
    b.terminates = 1;
    b.ops[0].opcode = AMIVM_IR_BSR;
    b.ops[0].guest_pc = 0xa000u;
    b.ops[0].imm = 0x20;
    b.ops[0].instruction_bytes = 2u;
    if (amivm_jit_compile(&b, &j) != AMIVM_JIT_OK || !j.requires_context) { fprintf(stderr, "BSR compile failed rc=%d context=%d\\n", amivm_jit_compile(&b, &j), j.requires_context); return 1; }
    { int rc = amivm_jit_execute_context(&j, &cpu, &ctx); if (rc != 0) { fprintf(stderr, "BSR rc=%d pc=%08x sp=%08x\\n", rc, cpu.pc, cpu.a[7]); return 1; } }
    if (cpu.pc != 0xa022u || cpu.a[7] != AMIVM_RAM_BASE + 0x7fcu) { fprintf(stderr, "BSR state pc=%08x sp=%08x\\n", cpu.pc, cpu.a[7]); return 1; }

    memset(&b, 0, sizeof(b));
    b.guest_start_pc = 0xa022u;
    b.guest_end_pc = 0xa024u;
    b.op_count = 1u;
    b.terminates = 1;
    b.ops[0].opcode = AMIVM_IR_RTS;
    if (amivm_jit_compile(&b, &j) != AMIVM_JIT_OK || !j.requires_context) { fprintf(stderr, "RTS compile failed context=%d\\n", j.requires_context); return 1; }
    { int rc = amivm_jit_execute_context(&j, &cpu, &ctx); if (rc != 0) { fprintf(stderr, "RTS rc=%d pc=%08x sp=%08x\\n", rc, cpu.pc, cpu.a[7]); return 1; } }
    if (cpu.pc != 0xa002u || cpu.a[7] != AMIVM_RAM_BASE + 0x800u) { fprintf(stderr, "RTS state pc=%08x sp=%08x\\n", cpu.pc, cpu.a[7]); return 1; }

    amivm_vm_destroy(&vm);
#endif
    puts("AmiVM M2.72 x86-64 BSR/RTS lowering: PASS");
    return 0;
}
