#include "jit.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void)
{
    struct amivm_ir_block block;
    struct amivm_jit_code code;
    struct amivm_cpu_state cpu;
    const uint16_t nop_words[] = {0x4e71u, 0x4e71u};
    const uint16_t moveq_words[] = {0x7001u};
    int rc;

    amivm_ir_block_init(&block, 0x00001000u);
    CHECK(amivm_ir_decode_words(&block, nop_words, 2u) == 0);
    CHECK(block.op_count == 2u);

    rc = amivm_jit_compile(&block, &code);
    if (amivm_jit_host_arch() == AMIVM_JIT_ARCH_X86_64) {
        CHECK(rc == AMIVM_JIT_OK);
        CHECK(code.arch == AMIVM_JIT_ARCH_X86_64);
        CHECK(code.size == 16u);
        CHECK(code.guest_instructions == 2u);
        CHECK(code.guest_start_pc == 0x00001000u);
        CHECK(code.guest_end_pc == 0x00001004u);
        CHECK(code.bytes[0] == 0xc7u);
        CHECK(code.bytes[1] == 0x87u);
        CHECK(code.bytes[10] == 0xb8u);
        CHECK(code.bytes[15] == 0xc3u);

#if defined(__x86_64__) && defined(__linux__)
        memset(&cpu, 0, sizeof(cpu));
        cpu.pc = 0xdeadbeefu;
        CHECK(amivm_jit_execute(&code, &cpu) == 1);
        CHECK(cpu.pc == 0x00001004u);
#endif
    } else {
        CHECK(rc == AMIVM_JIT_UNSUPPORTED);
    }

    /* Unsupported guest semantics must remain on the IR interpreter path. */
    amivm_ir_block_init(&block, 0x00002000u);
    CHECK(amivm_ir_decode_words(&block, moveq_words, 1u) == 0);
    CHECK(block.op_count == 1u);
    CHECK(amivm_jit_compile(&block, &code) == AMIVM_JIT_UNSUPPORTED);

    puts("AmiVM M2.24 native JIT foundation tests: PASS");
    return 0;
}
