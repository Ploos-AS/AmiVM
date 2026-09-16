#include "jit.h"
#include "jit_helpers.h"

#include <stdint.h>
#include <string.h>

int amivm_jit_compile_base(const struct amivm_ir_block *block,
                           struct amivm_jit_code *code);

#define BASE_FALLTHROUGH_EPILOGUE_SIZE 16u

static int put8(struct amivm_jit_code *code, uint8_t value)
{
    if (code->size >= AMIVM_JIT_CODE_CAPACITY) return AMIVM_JIT_NO_SPACE;
    code->bytes[code->size++] = value;
    return AMIVM_JIT_OK;
}

static int put32(struct amivm_jit_code *code, uint32_t value)
{
    unsigned i;
    for (i = 0u; i < 4u; ++i) {
        int rc = put8(code, (uint8_t)(value >> (i * 8u)));
        if (rc != AMIVM_JIT_OK) return rc;
    }
    return AMIVM_JIT_OK;
}

static int put64(struct amivm_jit_code *code, uint64_t value)
{
    unsigned i;
    for (i = 0u; i < 8u; ++i) {
        int rc = put8(code, (uint8_t)(value >> (i * 8u)));
        if (rc != AMIVM_JIT_OK) return rc;
    }
    return AMIVM_JIT_OK;
}

#define HELPER_ADDRESS(name, type, target) \
static int name(uint64_t *address) \
{ \
    type fn = target; \
    uintptr_t bits = 0u; \
    if (address == NULL || sizeof(fn) != sizeof(bits)) return AMIVM_JIT_UNSUPPORTED; \
    memcpy(&bits, &fn, sizeof(bits)); \
    *address = (uint64_t)bits; \
    return AMIVM_JIT_OK; \
}

typedef int (*helper_ctx_u32_u32)(struct amivm_jit_context *, uint32_t, uint32_t);
typedef int (*helper_ctx_u32)(struct amivm_jit_context *, uint32_t);
typedef int (*helper_ctx)(struct amivm_jit_context *);
HELPER_ADDRESS(bsr_helper_address, helper_ctx_u32_u32, amivm_jit_helper_bsr)
HELPER_ADDRESS(rts_helper_address, helper_ctx, amivm_jit_helper_rts)
HELPER_ADDRESS(jsr_helper_address, helper_ctx_u32_u32, amivm_jit_helper_jsr_an)
HELPER_ADDRESS(jmp_helper_address, helper_ctx_u32, amivm_jit_helper_jmp_an)

static int emit_call_tail(struct amivm_jit_code *code, uint64_t helper)
{
    int rc;
    rc = put8(code, 0x48u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xb8u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put64(code, helper); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x48u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x83u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xecu); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x08u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xffu); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xd0u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x48u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x83u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xc4u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x08u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x85u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xc0u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x78u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x05u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xb8u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put32(code, 1u); if (rc != AMIVM_JIT_OK) return rc;
    return put8(code, 0xc3u);
}

static int prepare_prefix(const struct amivm_ir_block *block,
                          struct amivm_jit_code *code)
{
    struct amivm_ir_block prefix;
    size_t prefix_count;
    int rc;

    if (block->op_count == 1u) {
        amivm_jit_code_init(code);
        if (code->arch != AMIVM_JIT_ARCH_X86_64) return AMIVM_JIT_UNSUPPORTED;
        return AMIVM_JIT_OK;
    }
    prefix = *block;
    prefix_count = block->op_count - 1u;
    prefix.op_count = prefix_count;
    prefix.terminates = 0;
    prefix.guest_end_pc = block->ops[prefix_count].guest_pc;
    rc = amivm_jit_compile_base(&prefix, code);
    if (rc != AMIVM_JIT_OK) return rc;
    if (code->size < BASE_FALLTHROUGH_EPILOGUE_SIZE) return AMIVM_JIT_INVALID;
    code->size -= BASE_FALLTHROUGH_EPILOGUE_SIZE;
    return AMIVM_JIT_OK;
}

static void finish_control_code(const struct amivm_ir_block *block,
                                struct amivm_jit_code *code)
{
    code->requires_context = 1;
    code->guest_instructions = block->op_count;
    code->guest_start_pc = block->guest_start_pc;
    code->guest_end_pc = block->guest_end_pc;
}

static int compile_bsr(const struct amivm_ir_block *block, struct amivm_jit_code *code)
{
    const struct amivm_ir_op *op = &block->ops[block->op_count - 1u];
    uint64_t helper;
    uint32_t return_pc = op->guest_pc + op->instruction_bytes;
    uint32_t target_pc = op->guest_pc + 2u + (uint32_t)op->imm;
    int rc = bsr_helper_address(&helper);
    if (rc != AMIVM_JIT_OK) return rc;
    rc = prepare_prefix(block, code); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x48u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x89u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xf7u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xbeu); if (rc != AMIVM_JIT_OK) return rc;
    rc = put32(code, return_pc); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xbau); if (rc != AMIVM_JIT_OK) return rc;
    rc = put32(code, target_pc); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit_call_tail(code, helper); if (rc != AMIVM_JIT_OK) return rc;
    finish_control_code(block, code);
    return AMIVM_JIT_OK;
}

static int compile_rts(const struct amivm_ir_block *block, struct amivm_jit_code *code)
{
    uint64_t helper;
    int rc = rts_helper_address(&helper);
    if (rc != AMIVM_JIT_OK) return rc;
    rc = prepare_prefix(block, code); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x48u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x89u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xf7u); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit_call_tail(code, helper); if (rc != AMIVM_JIT_OK) return rc;
    finish_control_code(block, code);
    return AMIVM_JIT_OK;
}

static int compile_jsr(const struct amivm_ir_block *block, struct amivm_jit_code *code)
{
    const struct amivm_ir_op *op = &block->ops[block->op_count - 1u];
    uint64_t helper;
    uint32_t return_pc = op->guest_pc + op->instruction_bytes;
    int rc;
    if (op->ea_mode != AMIVM_IR_EA_AN || op->reg >= 8u) return AMIVM_JIT_UNSUPPORTED;
    rc = jsr_helper_address(&helper); if (rc != AMIVM_JIT_OK) return rc;
    rc = prepare_prefix(block, code); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x48u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x89u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xf7u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xbeu); if (rc != AMIVM_JIT_OK) return rc;
    rc = put32(code, return_pc); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xbau); if (rc != AMIVM_JIT_OK) return rc;
    rc = put32(code, op->reg); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit_call_tail(code, helper); if (rc != AMIVM_JIT_OK) return rc;
    finish_control_code(block, code);
    return AMIVM_JIT_OK;
}

static int compile_jmp(const struct amivm_ir_block *block, struct amivm_jit_code *code)
{
    const struct amivm_ir_op *op = &block->ops[block->op_count - 1u];
    uint64_t helper;
    int rc;
    if (op->ea_mode != AMIVM_IR_EA_AN || op->reg >= 8u) return AMIVM_JIT_UNSUPPORTED;
    rc = jmp_helper_address(&helper); if (rc != AMIVM_JIT_OK) return rc;
    rc = prepare_prefix(block, code); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x48u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0x89u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xf7u); if (rc != AMIVM_JIT_OK) return rc;
    rc = put8(code, 0xbeu); if (rc != AMIVM_JIT_OK) return rc;
    rc = put32(code, op->reg); if (rc != AMIVM_JIT_OK) return rc;
    rc = emit_call_tail(code, helper); if (rc != AMIVM_JIT_OK) return rc;
    finish_control_code(block, code);
    return AMIVM_JIT_OK;
}

int amivm_jit_compile(const struct amivm_ir_block *block, struct amivm_jit_code *code)
{
    const struct amivm_ir_op *terminal;
    if (block == NULL || code == NULL || block->op_count == 0u) return AMIVM_JIT_INVALID;
    terminal = &block->ops[block->op_count - 1u];
    if (terminal->opcode == AMIVM_IR_BSR) return compile_bsr(block, code);
    if (terminal->opcode == AMIVM_IR_RTS) return compile_rts(block, code);
    if (terminal->opcode == AMIVM_IR_JSR) return compile_jsr(block, code);
    if (terminal->opcode == AMIVM_IR_JMP) return compile_jmp(block, code);
    return amivm_jit_compile_base(block, code);
}
