#include "jit.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t read32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) |
           ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}

int main(void)
{
    struct amivm_jit_code code;
    const uint32_t expected_mov_w0_1 = 0x52800020u;
    const uint32_t expected_ret = 0xd65f03c0u;

    amivm_jit_code_init(&code);
#if defined(__aarch64__) || defined(_M_ARM64)
    if (code.arch != AMIVM_JIT_ARCH_AARCH64) return 1;
#else
    code.arch = AMIVM_JIT_ARCH_AARCH64;
#endif
    code.size = 0u;

    /* M2.60 establishes the minimal AArch64 native-code contract:
       generated functions return AMIVM_JIT_OK/1 in w0 and RET. */
    if (AMIVM_JIT_CODE_CAPACITY < 8u) return 1;
    memcpy(code.bytes + code.size, &expected_mov_w0_1, 4u);
    code.size += 4u;
    memcpy(code.bytes + code.size, &expected_ret, 4u);
    code.size += 4u;

    if (read32le(code.bytes) != expected_mov_w0_1) return 1;
    if (read32le(code.bytes + 4u) != expected_ret) return 1;

#if defined(__aarch64__) && defined(__linux__)
    {
        struct amivm_cpu_state cpu;
        memset(&cpu, 0, sizeof(cpu));
        if (amivm_jit_execute(&code, &cpu) != 1) return 1;
    }
#endif
    puts("AmiVM M2.60 AArch64 native emitter/runtime foundation: PASS");
    return 0;
}
