#include "jit.h"

#include <stdio.h>
#include <string.h>

#define SR_X 0x0010u
#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u
#define SR_XNZVC (SR_X | SR_N | SR_Z | SR_V | SR_C)

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static int run_one(uint16_t word, uint32_t initial_d0, uint16_t initial_sr,
                   uint32_t expected_d0, uint16_t expected_ccr)
{
    struct amivm_ir_block block;
    struct amivm_jit_code code;
    struct amivm_cpu_state cpu;
    int rc;

    amivm_ir_block_init(&block, 0x00003000u);
    CHECK(amivm_ir_decode_words(&block, &word, 1u) == 0);
    CHECK(block.op_count == 1u);

    rc = amivm_jit_compile(&block, &code);
    if (amivm_jit_host_arch() != AMIVM_JIT_ARCH_X86_64) {
        CHECK(rc == AMIVM_JIT_UNSUPPORTED);
        return 0;
    }
    CHECK(rc == AMIVM_JIT_OK);

#if defined(__x86_64__) && defined(__linux__)
    memset(&cpu, 0, sizeof(cpu));
    cpu.d[0] = initial_d0;
    cpu.sr = initial_sr;
    cpu.pc = 0x00003000u;
    CHECK(amivm_jit_execute(&code, &cpu) == 1);
    CHECK(cpu.d[0] == expected_d0);
    CHECK((cpu.sr & SR_XNZVC) == expected_ccr);
    CHECK(cpu.pc == 0x00003002u);
#endif
    return 0;
}

int main(void)
{
    /* TST.L preserves X, clears V/C, and derives N/Z from the operand. */
    CHECK(run_one(0x4a80u, 0u, SR_X | SR_N | SR_V | SR_C,
                  0u, SR_X | SR_Z) == 0);
    CHECK(run_one(0x4a80u, 0x80000000u, SR_X | SR_Z | SR_V | SR_C,
                  0x80000000u, SR_X | SR_N) == 0);

    /* ADDQ.L #1,D0: carry propagates to both C and X. */
    CHECK(run_one(0x5280u, 0xffffffffu, 0u,
                  0u, SR_X | SR_Z | SR_C) == 0);
    /* Signed overflow is reflected in V without carry. */
    CHECK(run_one(0x5280u, 0x7fffffffu, SR_X | SR_Z | SR_C,
                  0x80000000u, SR_N | SR_V) == 0);

    /* SUBQ.L #1,D0: borrow propagates to both C and X. */
    CHECK(run_one(0x5380u, 0u, 0u,
                  0xffffffffu, SR_X | SR_N | SR_C) == 0);
    /* 0x80000000 - 1 overflows signed range without a borrow. */
    CHECK(run_one(0x5380u, 0x80000000u, SR_X | SR_C,
                  0x7fffffffu, SR_V) == 0);

    puts("AmiVM M2.27 TST/ADDQ/SUBQ JIT tests: PASS");
    return 0;
}
