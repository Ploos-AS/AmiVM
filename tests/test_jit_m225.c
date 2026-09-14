#include "jit.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

#define SR_X 0x0010u
#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u
#define SR_S 0x2000u

static int run_case(const uint16_t *words, size_t word_count,
                    uint32_t start_pc, uint32_t expect_d0,
                    uint16_t initial_sr, uint16_t expect_sr,
                    uint32_t expect_pc)
{
    struct amivm_ir_block block;
    struct amivm_jit_code code;
    struct amivm_cpu_state cpu;
    int rc;

    amivm_ir_block_init(&block, start_pc);
    CHECK(amivm_ir_decode_words(&block, words, word_count) == 0);
    rc = amivm_jit_compile(&block, &code);
    if (amivm_jit_host_arch() != AMIVM_JIT_ARCH_X86_64)
        return rc == AMIVM_JIT_UNSUPPORTED ? 0 : 1;

    CHECK(rc == AMIVM_JIT_OK);
    CHECK(code.guest_instructions == word_count);
    CHECK(code.guest_start_pc == start_pc);
    CHECK(code.guest_end_pc == expect_pc);

#if defined(__x86_64__) && defined(__linux__)
    memset(&cpu, 0, sizeof(cpu));
    cpu.d[0] = 0xaaaaaaaau;
    cpu.sr = initial_sr;
    cpu.pc = 0xdeadbeefu;
    CHECK(amivm_jit_execute(&code, &cpu) == 1);
    CHECK(cpu.d[0] == expect_d0);
    CHECK(cpu.sr == expect_sr);
    CHECK(cpu.pc == expect_pc);
#endif
    return 0;
}

int main(void)
{
    const uint16_t moveq_pos[] = {0x7001u};
    const uint16_t moveq_zero[] = {0x7000u};
    const uint16_t moveq_neg[] = {0x70ffu};
    const uint16_t clr[] = {0x4280u};
    const uint16_t mixed[] = {0x7001u, 0x4280u, 0x4e71u};

    CHECK(run_case(moveq_pos, 1u, 0x1000u, 1u,
                   (uint16_t)(SR_S | SR_X | SR_Z | SR_V | SR_C),
                   (uint16_t)(SR_S | SR_X), 0x1002u) == 0);
    CHECK(run_case(moveq_zero, 1u, 0x1100u, 0u,
                   (uint16_t)(SR_S | SR_X | SR_N | SR_V | SR_C),
                   (uint16_t)(SR_S | SR_X | SR_Z), 0x1102u) == 0);
    CHECK(run_case(moveq_neg, 1u, 0x1200u, 0xffffffffu,
                   (uint16_t)(SR_S | SR_X | SR_Z | SR_V | SR_C),
                   (uint16_t)(SR_S | SR_X | SR_N), 0x1202u) == 0);
    CHECK(run_case(clr, 1u, 0x1300u, 0u,
                   (uint16_t)(SR_S | SR_X | SR_N | SR_V | SR_C),
                   (uint16_t)(SR_S | SR_X | SR_Z), 0x1302u) == 0);
    CHECK(run_case(mixed, 3u, 0x1400u, 0u,
                   (uint16_t)(SR_S | SR_X | SR_N | SR_V | SR_C),
                   (uint16_t)(SR_S | SR_X | SR_Z), 0x1406u) == 0);

    puts("AmiVM M2.25 MOVEQ/CLR.L native lowering tests: PASS");
    return 0;
}
