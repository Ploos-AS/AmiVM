#include "ir.h"
#include "jit.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #x, __FILE__, __LINE__); \
    return 1; \
} } while (0)

static int same_arch_state(const struct amivm_cpu_state *a,
                           const struct amivm_cpu_state *b)
{
    return memcmp(a->d, b->d, sizeof a->d) == 0 &&
           memcmp(a->a, b->a, sizeof a->a) == 0 &&
           a->pc == b->pc && a->sr == b->sr;
}

int main(void)
{
    struct amivm_ir_block block;
    struct amivm_jit_code code;
    struct amivm_cpu_state reference_cpu;
    struct amivm_cpu_state jit_cpu;
    const uint16_t words[] = {
        0x7005u, /* MOVEQ #5,D0 */
        0x72fdu, /* MOVEQ #-3,D1 */
        0x5280u, /* ADDQ.L #1,D0 */
        0x5381u, /* SUBQ.L #1,D1 */
        0x4a80u, /* TST.L D0 */
        0x4e71u  /* NOP */
    };
    int ir_rc;
    int jit_rc;

    memset(&reference_cpu, 0, sizeof reference_cpu);
    memset(&jit_cpu, 0, sizeof jit_cpu);
    amivm_ir_block_init(&block, 0x1000u);

    CHECK(amivm_ir_decode_words(&block, words,
                                sizeof words / sizeof words[0]) == 0);
    CHECK(block.op_count != 0u);
    CHECK(amivm_jit_compile(&block, &code) == AMIVM_JIT_OK);
    CHECK(code.arch == amivm_jit_host_arch());

    ir_rc = amivm_ir_execute(&block, &reference_cpu, NULL);
    jit_rc = amivm_jit_execute(&code, &jit_cpu);

    CHECK(ir_rc == jit_rc);
    CHECK(same_arch_state(&reference_cpu, &jit_cpu));

    puts("AmiVM M2.80 JIT/reference equivalence: PASS");
    return 0;
}
