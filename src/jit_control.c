#include "jit.h"
#include "jit_helpers.h"

#include <stdint.h>
#include <string.h>

int amivm_jit_compile_base(const struct amivm_ir_block *block,
                           struct amivm_jit_code *code);

#define BASE_FALLTHROUGH_EPILOGUE_SIZE 16u

static int put8(struct amivm_jit_code *code, uint8_t value)
{ if (code->size >= AMIVM_JIT_CODE_CAPACITY) return AMIVM_JIT_NO_SPACE; code->bytes[code->size++] = value; return AMIVM_JIT_OK; }
static int put32(struct amivm_jit_code *code, uint32_t value)
{ unsigned i; for (i=0u;i<4u;++i) { int rc=put8(code,(uint8_t)(value>>(i*8u))); if(rc!=AMIVM_JIT_OK)return rc; } return AMIVM_JIT_OK; }
static int put64(struct amivm_jit_code *code, uint64_t value)
{ unsigned i; for (i=0u;i<8u;++i) { int rc=put8(code,(uint8_t)(value>>(i*8u))); if(rc!=AMIVM_JIT_OK)return rc; } return AMIVM_JIT_OK; }
#define HELPER_ADDRESS(name,type,target) static int name(uint64_t *address){type fn=target;uintptr_t bits=0u;if(address==NULL||sizeof(fn)!=sizeof(bits))return AMIVM_JIT_UNSUPPORTED;memcpy(&bits,&fn,sizeof(bits));*address=(uint64_t)bits;return AMIVM_JIT_OK;}
typedef int (*helper_ctx_u32_u32_u32_u32)(struct amivm_jit_context *,uint32_t,uint32_t,uint32_t,uint32_t);
typedef int (*helper_ctx_u32_u32)(struct amivm_jit_context *,uint32_t,uint32_t);
typedef int (*helper_ctx_u32)(struct amivm_jit_context *,uint32_t);
typedef int (*helper_ctx)(struct amivm_jit_context *);
HELPER_ADDRESS(bsr_helper_address,helper_ctx_u32_u32,amivm_jit_helper_bsr)
HELPER_ADDRESS(rts_helper_address,helper_ctx,amivm_jit_helper_rts)
HELPER_ADDRESS(jsr_helper_address,helper_ctx_u32_u32,amivm_jit_helper_jsr_an)
HELPER_ADDRESS(jmp_helper_address,helper_ctx_u32,amivm_jit_helper_jmp_an)
HELPER_ADDRESS(jmp_abs_helper_address,helper_ctx_u32,amivm_jit_helper_jmp_abs)
HELPER_ADDRESS(jsr_d16_helper_address,helper_ctx_u32_u32,amivm_jit_helper_jsr_d16_an)
HELPER_ADDRESS(jmp_d16_helper_address,helper_ctx_u32,amivm_jit_helper_jmp_d16_an)
HELPER_ADDRESS(jsr_indexed_helper_address,helper_ctx_u32_u32,amivm_jit_helper_jsr_indexed)
HELPER_ADDRESS(jmp_indexed_helper_address,helper_ctx_u32_u32,amivm_jit_helper_jmp_indexed)
HELPER_ADDRESS(jsr_full_indexed_helper_address,helper_ctx_u32_u32_u32_u32,amivm_jit_helper_jsr_full_indexed)
HELPER_ADDRESS(jmp_full_indexed_helper_address,helper_ctx_u32_u32_u32_u32,amivm_jit_helper_jmp_full_indexed)
static int emit_call_tail(struct amivm_jit_code *code,uint64_t helper){int rc;
#define EMIT8(v) do{rc=put8(code,(v));if(rc!=AMIVM_JIT_OK)return rc;}while(0)
EMIT8(0x48u);EMIT8(0xb8u);rc=put64(code,helper);if(rc!=AMIVM_JIT_OK)return rc;EMIT8(0x48u);EMIT8(0x83u);EMIT8(0xecu);EMIT8(0x08u);EMIT8(0xffu);EMIT8(0xd0u);EMIT8(0x48u);EMIT8(0x83u);EMIT8(0xc4u);EMIT8(0x08u);EMIT8(0x85u);EMIT8(0xc0u);EMIT8(0x78u);EMIT8(0x05u);EMIT8(0xb8u);rc=put32(code,1u);if(rc!=AMIVM_JIT_OK)return rc;EMIT8(0xc3u);
#undef EMIT8
return AMIVM_JIT_OK;}
static int prepare_prefix(const struct amivm_ir_block *block,struct amivm_jit_code *code){struct amivm_ir_block prefix;size_t n;int rc;if(block->op_count==1u){amivm_jit_code_init(code);return code->arch==AMIVM_JIT_ARCH_X86_64?AMIVM_JIT_OK:AMIVM_JIT_UNSUPPORTED;}prefix=*block;n=block->op_count-1u;prefix.op_count=n;prefix.terminates=0;prefix.guest_end_pc=block->ops[n].guest_pc;rc=amivm_jit_compile_base(&prefix,code);if(rc!=AMIVM_JIT_OK)return rc;if(code->size<BASE_FALLTHROUGH_EPILOGUE_SIZE)return AMIVM_JIT_INVALID;code->size-=BASE_FALLTHROUGH_EPILOGUE_SIZE;return AMIVM_JIT_OK;}
static void finish_control_code(const struct amivm_ir_block *block,struct amivm_jit_code *code){code->requires_context=1;code->guest_instructions=block->op_count;code->guest_start_pc=block->guest_start_pc;code->guest_end_pc=block->guest_end_pc;}
static int emit_context(struct amivm_jit_code *code){int rc=put8(code,0x48u);if(rc!=AMIVM_JIT_OK)return rc;rc=put8(code,0x89u);if(rc!=AMIVM_JIT_OK)return rc;return put8(code,0xf7u);}
static int emit_imm32(struct amivm_jit_code *code,uint8_t opcode,uint32_t value){int rc=put8(code,opcode);if(rc!=AMIVM_JIT_OK)return rc;return put32(code,value);}
static int compile_bsr(const struct amivm_ir_block *block,struct amivm_jit_code *code){const struct amivm_ir_op *op=&block->ops[block->op_count-1u];uint64_t helper;int rc=bsr_helper_address(&helper);if(rc!=AMIVM_JIT_OK)return rc;rc=prepare_prefix(block,code);if(rc!=AMIVM_JIT_OK)return rc;rc=emit_context(code);if(rc!=AMIVM_JIT_OK)return rc;rc=emit_imm32(code,0xbeu,op->guest_pc+op->instruction_bytes);if(rc!=AMIVM_JIT_OK)return rc;rc=emit_imm32(code,0xbau,op->guest_pc+2u+(uint32_t)op->imm);if(rc!=AMIVM_JIT_OK)return rc;rc=emit_call_tail(code,helper);if(rc!=AMIVM_JIT_OK)return rc;finish_control_code(block,code);return AMIVM_JIT_OK;}
static int compile_rts(const struct amivm_ir_block *block,struct amivm_jit_code *code){uint64_t helper;int rc=rts_helper_address(&helper);if(rc!=AMIVM_JIT_OK)return rc;rc=prepare_prefix(block,code);if(rc!=AMIVM_JIT_OK)return rc;rc=emit_context(code);if(rc!=AMIVM_JIT_OK)return rc;rc=emit_call_tail(code,helper);if(rc!=AMIVM_JIT_OK)return rc;finish_control_code(block,code);return AMIVM_JIT_OK;}
static uint32_t encode_d16_an(const struct amivm_ir_op *op){return((uint32_t)op->reg<<16u)|((uint32_t)op->imm&0xffffu);}
static uint32_t scale_shift(uint8_t scale){uint32_t shift=0u,s=(uint32_t)scale;while(s>1u){++shift;s>>=1u;}return shift;}
static uint32_t encode_indexed(const struct amivm_ir_op *op){return((uint32_t)op->imm&0xffu)|((uint32_t)op->index_reg<<8u)|((uint32_t)op->index_is_addr<<11u)|((uint32_t)op->index_long<<12u)|(scale_shift(op->index_scale)<<13u)|((op->ea_mode==AMIVM_IR_EA_PC_D8_XN?1u:0u)<<15u)|((uint32_t)op->reg<<16u);}
static uint32_t encode_full_indexed(const struct amivm_ir_op *op){return((uint32_t)op->index_reg)|((uint32_t)op->index_is_addr<<3u)|((uint32_t)op->index_long<<4u)|(scale_shift(op->index_scale)<<5u)|((op->ea_mode==AMIVM_IR_EA_PC_D8_XN?1u:0u)<<7u)|((uint32_t)op->reg<<8u)|((uint32_t)op->base_suppress<<11u)|((uint32_t)op->index_suppress<<12u)|((uint32_t)op->indirect_mode<<13u)|((uint32_t)op->instruction_bytes<<16u);}
static int compile_jsr(const struct amivm_ir_block *block,struct amivm_jit_code *code){const struct amivm_ir_op *op=&block->ops[block->op_count-1u];uint64_t helper;uint32_t target,third=0u,fourth=0u;int full_args=0;int rc;if(op->ea_mode==AMIVM_IR_EA_AN){if(op->reg>=8u)return AMIVM_JIT_UNSUPPORTED;rc=jsr_helper_address(&helper);target=(uint32_t)op->reg;}else if(op->ea_mode==AMIVM_IR_EA_D16_AN){if(op->reg>=8u)return AMIVM_JIT_UNSUPPORTED;rc=jsr_d16_helper_address(&helper);target=encode_d16_an(op);}else if(op->ea_mode==AMIVM_IR_EA_D8_AN_XN||op->ea_mode==AMIVM_IR_EA_PC_D8_XN){if(op->full_format){rc=jsr_full_indexed_helper_address(&helper);target=encode_full_indexed(op);third=(uint32_t)op->base_displacement;fourth=(uint32_t)op->outer_displacement;full_args=1;}else{rc=jsr_indexed_helper_address(&helper);target=encode_indexed(op);}}else if(op->ea_mode==AMIVM_IR_EA_PC_D16){rc=bsr_helper_address(&helper);target=op->guest_pc+2u+(uint32_t)op->imm;}else if(op->ea_mode==AMIVM_IR_EA_ABS_W||op->ea_mode==AMIVM_IR_EA_ABS_L){rc=bsr_helper_address(&helper);target=(uint32_t)op->imm;}else return AMIVM_JIT_UNSUPPORTED;if(rc!=AMIVM_JIT_OK)return rc;rc=prepare_prefix(block,code);if(rc!=AMIVM_JIT_OK)return rc;rc=emit_context(code);if(rc!=AMIVM_JIT_OK)return rc;rc=emit_imm32(code,0xbeu,op->guest_pc+op->instruction_bytes);if(rc!=AMIVM_JIT_OK)return rc;rc=emit_imm32(code,0xbau,target);if(rc!=AMIVM_JIT_OK)return rc;if(full_args){rc=emit_imm32(code,0xb9u,third);if(rc!=AMIVM_JIT_OK)return rc;rc=put8(code,0x41u);if(rc!=AMIVM_JIT_OK)return rc;rc=emit_imm32(code,0xb8u,fourth);if(rc!=AMIVM_JIT_OK)return rc;}rc=emit_call_tail(code,helper);if(rc!=AMIVM_JIT_OK)return rc;finish_control_code(block,code);return AMIVM_JIT_OK;}
static int compile_jmp(const struct amivm_ir_block *block,struct amivm_jit_code *code){const struct amivm_ir_op *op=&block->ops[block->op_count-1u];uint64_t helper;uint32_t arg,second=0u,third=0u,fourth=0u;int args=1;int rc;if(op->ea_mode==AMIVM_IR_EA_AN){if(op->reg>=8u)return AMIVM_JIT_UNSUPPORTED;rc=jmp_helper_address(&helper);arg=(uint32_t)op->reg;}else if(op->ea_mode==AMIVM_IR_EA_D16_AN){if(op->reg>=8u)return AMIVM_JIT_UNSUPPORTED;rc=jmp_d16_helper_address(&helper);arg=encode_d16_an(op);}else if(op->ea_mode==AMIVM_IR_EA_D8_AN_XN||op->ea_mode==AMIVM_IR_EA_PC_D8_XN){arg=op->guest_pc+2u;if(op->full_format){rc=jmp_full_indexed_helper_address(&helper);second=encode_full_indexed(op);third=(uint32_t)op->base_displacement;fourth=(uint32_t)op->outer_displacement;args=4;}else{rc=jmp_indexed_helper_address(&helper);second=encode_indexed(op);args=2;}}else if(op->ea_mode==AMIVM_IR_EA_PC_D16){rc=jmp_abs_helper_address(&helper);arg=op->guest_pc+2u+(uint32_t)op->imm;}else if(op->ea_mode==AMIVM_IR_EA_ABS_W||op->ea_mode==AMIVM_IR_EA_ABS_L){rc=jmp_abs_helper_address(&helper);arg=(uint32_t)op->imm;}else return AMIVM_JIT_UNSUPPORTED;if(rc!=AMIVM_JIT_OK)return rc;rc=prepare_prefix(block,code);if(rc!=AMIVM_JIT_OK)return rc;rc=emit_context(code);if(rc!=AMIVM_JIT_OK)return rc;rc=emit_imm32(code,0xbeu,arg);if(rc!=AMIVM_JIT_OK)return rc;if(args>=2){rc=emit_imm32(code,0xbau,second);if(rc!=AMIVM_JIT_OK)return rc;}if(args>=3){rc=emit_imm32(code,0xb9u,third);if(rc!=AMIVM_JIT_OK)return rc;}if(args>=4){rc=put8(code,0x41u);if(rc!=AMIVM_JIT_OK)return rc;rc=emit_imm32(code,0xb8u,fourth);if(rc!=AMIVM_JIT_OK)return rc;}rc=emit_call_tail(code,helper);if(rc!=AMIVM_JIT_OK)return rc;finish_control_code(block,code);return AMIVM_JIT_OK;}
int amivm_jit_compile(const struct amivm_ir_block *block,struct amivm_jit_code *code){const struct amivm_ir_op *t;if(block==NULL||code==NULL||block->op_count==0u)return AMIVM_JIT_INVALID;t=&block->ops[block->op_count-1u];if(t->opcode==AMIVM_IR_BSR)return compile_bsr(block,code);if(t->opcode==AMIVM_IR_RTS)return compile_rts(block,code);if(t->opcode==AMIVM_IR_JSR)return compile_jsr(block,code);if(t->opcode==AMIVM_IR_JMP)return compile_jmp(block,code);return amivm_jit_compile_base(block,code);}
