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

static int run_one(uint16_t word, uint32_t initial_d0, uint32_t initial_d1,
                   uint16_t initial_sr, uint32_t expected_d0,
                   uint16_t expected_ccr)
{
    struct amivm_ir_block block;
    struct amivm_jit_code code;
    struct amivm_cpu_state cpu;
    int rc;

    amivm_ir_block_init(&block, 0x00004000u);
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
    cpu.d[1] = initial_d1;
    cpu.sr = initial_sr;
    cpu.pc = 0x00004000u;
    CHECK(amivm_jit_execute(&code, &cpu) == 1);
    CHECK(cpu.d[0] == expected_d0);
    CHECK(cpu.d[1] == initial_d1);
    CHECK((cpu.sr & SR_XNZVC) == expected_ccr);
    CHECK(cpu.pc == 0x00004002u);
#endif
    return 0;
}

int main(void)
{
    /* ADD.L D1,D0. */
    CHECK(run_one(0xd081u, 0xffffffffu, 1u, 0u,
                  0u, SR_X | SR_Z | SR_C) == 0);
    CHECK(run_one(0xd081u, 0x7fffffffu, 1u, SR_X | SR_C,
                  0x80000000u, SR_N | SR_V) == 0);

    /* SUB.L D1,D0. */
    CHECK(run_one(0x9081u, 0u, 1u, 0u,
                  0xffffffffu, SR_X | SR_N | SR_C) == 0);
    CHECK(run_one(0x9081u, 0x80000000u, 1u, SR_X | SR_C,
                  0x7fffffffu, SR_V) == 0);

    /* CMP.L D1,D0 leaves D0 unchanged and preserves X. */
    CHECK(run_one(0xb081u, 0u, 1u, SR_X | SR_Z | SR_V,
                  0u, SR_X | SR_N | SR_C) == 0);

    /* AND/OR/EOR clear V/C, derive N/Z, and preserve X. */
    CHECK(run_one(0xc081u, 0xf0f0f0f0u, 0x0f0f0f0fu,
                  SR_X | SR_N | SR_V | SR_C,
                  0u, SR_X | SR_Z) == 0);
    CHECK(run_one(0x8081u, 0x80000000u, 1u,
                  SR_X | SR_Z | SR_V | SR_C,
                  0x80000001u, SR_X | SR_N) == 0);
    CHECK(run_one(0xb380u, 0xffffffffu, 0xffffffffu,
                  SR_X | SR_N | SR_V | SR_C,
                  0u, SR_X | SR_Z) == 0);

    puts("AmiVM M2.29 register ALU JIT tests: PASS");
    return 0;
}
