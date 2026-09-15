#include "ir.h"
#include "jit.h"

#include <stdio.h>
#include <string.h>

#define SR_Z 0x0004u

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static int run_word_branch(uint16_t opcode, int16_t displacement,
                           uint16_t sr, uint32_t expected_pc)
{
    const uint16_t words[] = { opcode, (uint16_t)displacement };
    struct amivm_ir_block block;
    struct amivm_jit_code code;
    struct amivm_cpu_state cpu;
    int rc;

    amivm_ir_block_init(&block, 0x00004000u);
    CHECK(amivm_ir_decode_words(&block, words, 2u) == 0);
    CHECK(block.op_count == 1u);
    CHECK(block.terminates == 1);
    CHECK(block.guest_end_pc == 0x00004004u);
    CHECK(block.ops[0].instruction_bytes == 4u);

    memset(&cpu, 0, sizeof(cpu));
    cpu.sr = sr;
    CHECK(amivm_ir_execute(&block, &cpu, NULL) == 1);
    CHECK(cpu.pc == expected_pc);
    CHECK(cpu.sr == sr);

    rc = amivm_jit_compile(&block, &code);
    if (amivm_jit_host_arch() != AMIVM_JIT_ARCH_X86_64) {
        CHECK(rc == AMIVM_JIT_UNSUPPORTED);
        return 0;
    }
    CHECK(rc == AMIVM_JIT_OK);
    CHECK(code.guest_instructions == 1u);
    CHECK(code.guest_start_pc == 0x00004000u);
    CHECK(code.guest_end_pc == 0x00004004u);

#if defined(__x86_64__) && defined(__linux__)
    memset(&cpu, 0, sizeof(cpu));
    cpu.pc = 0xdeadbeefu;
    cpu.sr = sr;
    CHECK(amivm_jit_execute(&code, &cpu) == 1);
    CHECK(cpu.pc == expected_pc);
    CHECK(cpu.sr == sr);
#endif
    return 0;
}

static int qualification_edges(void)
{
    struct amivm_ir_block block;
    const uint16_t truncated[] = { 0x6000u };

    /* M2.35 still owns the truncated word-displacement fallback boundary.
     * BSR.w moved into supported IR in M2.36 and is qualified there. */
    amivm_ir_block_init(&block, 0x00005000u);
    CHECK(amivm_ir_decode_words(&block, truncated, 1u) == 1);
    CHECK(block.op_count == 1u);
    CHECK(block.ops[0].opcode == AMIVM_IR_EXIT);
    return 0;
}

int main(void)
{
    /* Branch base is opcode PC + 2 for the 16-bit displacement form. */
    CHECK(run_word_branch(0x6000u, 0x0010, 0x2715u, 0x00004012u) == 0);
    CHECK(run_word_branch(0x6000u, -2, 0x2715u, 0x00004000u) == 0);

    /* BEQ.w: taken uses disp16, not-taken skips the extension word. */
    CHECK(run_word_branch(0x6700u, 0x0010, SR_Z, 0x00004012u) == 0);
    CHECK(run_word_branch(0x6700u, 0x0010, 0u, 0x00004004u) == 0);

    /* BNE.w negative displacement qualifies sign extension. */
    CHECK(run_word_branch(0x6600u, -2, 0u, 0x00004000u) == 0);
    CHECK(run_word_branch(0x6600u, -2, SR_Z, 0x00004004u) == 0);

    CHECK(qualification_edges() == 0);

    puts("AmiVM M2.35 word-displacement BRA/Bcc IR/JIT tests: PASS");
    return 0;
}
