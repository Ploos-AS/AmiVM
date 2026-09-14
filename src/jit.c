#if defined(__linux__)
#define _GNU_SOURCE
#endif

#include "jit.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(__x86_64__) && defined(__linux__)
#include <sys/mman.h>
#endif

#define SR_N 0x0008u
#define SR_Z 0x0004u
#define SR_V 0x0002u
#define SR_C 0x0001u
#define SR_NZVC (SR_N | SR_Z | SR_V | SR_C)

static int emit8(struct amivm_jit_code *code, uint8_t value)
{
    if (code->size >= AMIVM_JIT_CODE_CAPACITY) return AMIVM_JIT_NO_SPACE;
    code->bytes[code->size++] = value;
    return AMIVM_JIT_OK;
}

static int emit16(struct amivm_jit_code *code, uint16_t value)
{
    int rc;
    rc = emit8(code, (uint8_t)value);
    if (rc != AMIVM_JIT_OK) return rc;
    return emit8(code, (uint8_t)(value >> 8u));
}

static int emit32(struct amivm_jit_code *code, uint32_t value)
{
    unsigned i;
    for (i = 0u; i < 4u; ++i) {
        int rc = emit8(code, (uint8_t)(value >> (i * 8u)));
        if (rc != AMIVM_JIT_OK) return rc;
    }
    return AMIVM_JIT_OK;
}

static int emit_mov32_state_imm(struct amivm_jit_code *code,
                                size_t offset, uint32_t value)
{
    int rc;
    if (offset > UINT32_MAX) return AMIVM_JIT_UNSUPPORTED;
    /* mov dword ptr [rdi + disp32], imm32 */
    rc = emit8(code, 0xc7u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x87u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit32(code, (uint32_t)offset); if (rc != AMIVM_JIT_OK) return rc;
    return emit32(code, value);
}

static int emit_and16_state_imm(struct amivm_jit_code *code,
                                size_t offset, uint16_t value)
{
    int rc;
    if (offset > UINT32_MAX) return AMIVM_JIT_UNSUPPORTED;
    /* and word ptr [rdi + disp32], imm16 */
    rc = emit8(code, 0x66u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x81u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xa7u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit32(code, (uint32_t)offset); if (rc != AMIVM_JIT_OK) return rc;
    return emit16(code, value);
}

static int emit_or16_state_imm(struct amivm_jit_code *code,
                               size_t offset, uint16_t value)
{
    int rc;
    if (offset > UINT32_MAX) return AMIVM_JIT_UNSUPPORTED;
    /* or word ptr [rdi + disp32], imm16 */
    rc = emit8(code, 0x66u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x81u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0x8fu); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit32(code, (uint32_t)offset); if (rc != AMIVM_JIT_OK) return rc;
    return emit16(code, value);
}

static int emit_nz_flags(struct amivm_jit_code *code, uint32_t value)
{
    const size_t sr_offset = offsetof(struct amivm_cpu_state, sr);
    uint16_t flags = 0u;
    int rc;

    if (value == 0u) flags |= SR_Z;
    if ((value & 0x80000000u) != 0u) flags |= SR_N;

    rc = emit_and16_state_imm(code, sr_offset, (uint16_t)~SR_NZVC);
    if (rc != AMIVM_JIT_OK) return rc;
    if (flags != 0u) return emit_or16_state_imm(code, sr_offset, flags);
    return AMIVM_JIT_OK;
}

int amivm_jit_host_arch(void)
{
#if defined(__x86_64__) || defined(_M_X64)
    return AMIVM_JIT_ARCH_X86_64;
#else
    return AMIVM_JIT_ARCH_NONE;
#endif
}

void amivm_jit_code_init(struct amivm_jit_code *code)
{
    if (code == NULL) return;
    memset(code, 0, sizeof(*code));
    code->arch = (enum amivm_jit_arch)amivm_jit_host_arch();
}

int amivm_jit_compile(const struct amivm_ir_block *block,
                      struct amivm_jit_code *code)
{
    size_t i;
    uint32_t final_pc;
    int rc;

    if (block == NULL || code == NULL || block->op_count == 0u)
        return AMIVM_JIT_INVALID;
    amivm_jit_code_init(code);
    if (code->arch != AMIVM_JIT_ARCH_X86_64) return AMIVM_JIT_UNSUPPORTED;

    /* M2.25 lowers side-effect-complete straight-line NOP/MOVEQ/CLR.L blocks.
       Everything else remains on the IR interpreter path. */
    for (i = 0u; i < block->op_count; ++i) {
        const struct amivm_ir_op *op = &block->ops[i];
        uint32_t value;
        size_t d_offset;

        switch (op->opcode) {
        case AMIVM_IR_NOP:
            break;
        case AMIVM_IR_MOVEQ:
            if (op->reg >= 8u) return AMIVM_JIT_INVALID;
            value = (uint32_t)op->imm;
            d_offset = offsetof(struct amivm_cpu_state, d) + ((size_t)op->reg * sizeof(uint32_t));
            rc = emit_mov32_state_imm(code, d_offset, value);
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit_nz_flags(code, value);
            if (rc != AMIVM_JIT_OK) return rc;
            break;
        case AMIVM_IR_CLR_L:
            if (op->reg >= 8u) return AMIVM_JIT_INVALID;
            d_offset = offsetof(struct amivm_cpu_state, d) + ((size_t)op->reg * sizeof(uint32_t));
            rc = emit_mov32_state_imm(code, d_offset, 0u);
            if (rc != AMIVM_JIT_OK) return rc;
            rc = emit_nz_flags(code, 0u);
            if (rc != AMIVM_JIT_OK) return rc;
            break;
        default:
            return AMIVM_JIT_UNSUPPORTED;
        }
    }

    final_pc = block->ops[block->op_count - 1u].guest_pc +
               block->ops[block->op_count - 1u].instruction_bytes;
    rc = emit_mov32_state_imm(code, offsetof(struct amivm_cpu_state, pc), final_pc);
    if (rc != AMIVM_JIT_OK) return rc;

    /* mov eax, 1 ; successful block execution */
    rc = emit8(code, 0xb8u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit32(code, 1u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit8(code, 0xc3u); if (rc != AMIVM_JIT_OK) return rc;

    code->guest_instructions = block->op_count;
    code->guest_start_pc = block->guest_start_pc;
    code->guest_end_pc = final_pc;
    return AMIVM_JIT_OK;
}

int amivm_jit_execute(const struct amivm_jit_code *code,
                      struct amivm_cpu_state *cpu)
{
#if defined(__x86_64__) && defined(__linux__)
    typedef int (*jit_fn)(struct amivm_cpu_state *);
    void *mapping;
    void *entry;
    jit_fn fn;
    int rc;

    if (code == NULL || cpu == NULL || code->size == 0u ||
        code->arch != AMIVM_JIT_ARCH_X86_64) return AMIVM_JIT_INVALID;

    mapping = mmap(NULL, code->size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) return AMIVM_JIT_EXEC_UNAVAILABLE;
    memcpy(mapping, code->bytes, code->size);
    if (mprotect(mapping, code->size, PROT_READ | PROT_EXEC) != 0) {
        (void)munmap(mapping, code->size);
        return AMIVM_JIT_EXEC_UNAVAILABLE;
    }

    if (sizeof(fn) != sizeof(entry)) {
        (void)munmap(mapping, code->size);
        return AMIVM_JIT_EXEC_UNAVAILABLE;
    }
    entry = mapping;
    memcpy(&fn, &entry, sizeof(fn));
    rc = fn(cpu);
    (void)munmap(mapping, code->size);
    return rc;
#else
    (void)code;
    (void)cpu;
    return AMIVM_JIT_EXEC_UNAVAILABLE;
#endif
}
