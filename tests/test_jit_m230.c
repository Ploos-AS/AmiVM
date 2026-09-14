#include "jit.h"

#include <stdio.h>
#include <string.h>

#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

struct branch_case {
    uint8_t condition;
    uint16_t taken_sr;
    uint16_t fallthrough_sr;
};

static int run_branch(uint16_t word, uint16_t sr, uint32_t expected_pc)
{
    struct amivm_ir_block block;
    struct amivm_jit_code code;
    struct amivm_cpu_state cpu;
    int rc;

    amivm_ir_block_init(&block, 0x00004000u);
    CHECK(amivm_ir_decode_words(&block, &word, 1u) == 0);
    CHECK(block.op_count == 1u);
    CHECK(block.terminates == 1);

    rc = amivm_jit_compile(&block, &code);
    if (amivm_jit_host_arch() != AMIVM_JIT_ARCH_X86_64) {
        CHECK(rc == AMIVM_JIT_UNSUPPORTED);
        return 0;
    }
    CHECK(rc == AMIVM_JIT_OK);
    CHECK(code.guest_instructions == 1u);
    CHECK(code.guest_start_pc == 0x00004000u);
    CHECK(code.guest_end_pc == 0x00004002u);

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

static int run_flag_to_branch(void)
{
    const uint16_t words[] = {
        0x7000u, /* MOVEQ #0,D0 -> Z=1 */
        0x6704u  /* BEQ.s +4 */
    };
    struct amivm_ir_block block;
    struct amivm_jit_code code;
    struct amivm_cpu_state cpu;
    int rc;

    amivm_ir_block_init(&block, 0x00005000u);
    CHECK(amivm_ir_decode_words(&block, words, 2u) == 0);
    CHECK(block.op_count == 2u);
    CHECK(block.terminates == 1);

    rc = amivm_jit_compile(&block, &code);
    if (amivm_jit_host_arch() != AMIVM_JIT_ARCH_X86_64) {
        CHECK(rc == AMIVM_JIT_UNSUPPORTED);
        return 0;
    }
    CHECK(rc == AMIVM_JIT_OK);

#if defined(__x86_64__) && defined(__linux__)
    memset(&cpu, 0, sizeof(cpu));
    cpu.pc = 0x00005000u;
    CHECK(amivm_jit_execute(&code, &cpu) == 1);
    CHECK(cpu.d[0] == 0u);
    CHECK((cpu.sr & SR_Z) != 0u);
    CHECK(cpu.pc == 0x00005008u);
#endif
    return 0;
}

int main(void)
{
    static const struct branch_case cases[] = {
        {2u, 0u, SR_C},            /* HI / LS */
        {3u, SR_C, 0u},
        {4u, 0u, SR_C},            /* CC / CS */
        {5u, SR_C, 0u},
        {6u, 0u, SR_Z},            /* NE / EQ */
        {7u, SR_Z, 0u},
        {8u, 0u, SR_V},            /* VC / VS */
        {9u, SR_V, 0u},
        {10u, 0u, SR_N},           /* PL / MI */
        {11u, SR_N, 0u},
        {12u, SR_N | SR_V, SR_N},  /* GE: N == V */
        {13u, SR_N, SR_N | SR_V},  /* LT: N != V */
        {14u, SR_N | SR_V, SR_Z},  /* GT: !Z && N == V */
        {15u, SR_Z, SR_N | SR_V}   /* LE: Z || N != V */
    };
    size_t i;

    CHECK(run_branch(0x6006u, 0x2715u, 0x00004008u) == 0); /* BRA.s +6 */

    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint16_t word = (uint16_t)(0x6006u | ((uint16_t)cases[i].condition << 8u));
        CHECK(run_branch(word, cases[i].taken_sr, 0x00004008u) == 0);
        CHECK(run_branch(word, cases[i].fallthrough_sr, 0x00004002u) == 0);
    }

    CHECK(run_flag_to_branch() == 0);

    puts("AmiVM M2.30 native BRA/Bcc JIT tests: PASS");
    return 0;
}
