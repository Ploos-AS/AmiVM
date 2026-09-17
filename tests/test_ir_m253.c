#include "ir.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed: %s\n",#x); return 1; } } while (0)
static uint32_t be32(struct amivm_vm *v,uint32_t a){uint8_t b[4];unsigned i;for(i=0;i<4u;i++)if(!amivm_read8(v,a+i,&b[i]))return 0xffffffffu;return((uint32_t)b[0]<<24u)|((uint32_t)b[1]<<16u)|((uint32_t)b[2]<<8u)|b[3];}
int main(void){struct amivm_config cfg;struct amivm_vm vm;struct amivm_cpu_state cpu;struct amivm_ir_block b;uint32_t sp=AMIVM_RAM_BASE+0x3000u;
/* full extension: D2.l*4, base displacement word +0x20, no memory indirect */
const uint16_t jsr_an[]={0x4eb3u,0x2d20u,0x0020u};
/* full extension: A1.w*2, PC base, base displacement word -0x10 */
const uint16_t jmp_pc[]={0x4efbu,0x9320u,0xfff0u};
/* full extension with base suppression, D1.l*1, null base displacement */
const uint16_t jmp_bs[]={0x4ef4u,0x1990u};
/* full extension with index suppression, A6 base, word displacement +0x44 */
const uint16_t jsr_is[]={0x4eb6u,0x0160u,0x0044u};
amivm_config_init(&cfg);cfg.ram_size=2u*1024u*1024u;CHECK(amivm_vm_init(&vm,&cfg)==0);memset(&cpu,0,sizeof(cpu));cpu.sr=0x2000u;cpu.a[7]=sp;cpu.isp=sp;
cpu.a[3]=0x20000000u;cpu.d[2]=5u;amivm_ir_block_init(&b,0x4000u);CHECK(amivm_ir_decode_words(&b,jsr_an,3u)==0);CHECK(b.ops[0].full_format==1u);CHECK(b.ops[0].instruction_bytes==6u);CHECK(b.ops[0].base_displacement==0x20);CHECK(amivm_ir_execute(&b,&cpu,&vm)==1);CHECK(cpu.pc==0x20000034u);CHECK(be32(&vm,sp-4u)==0x4006u);
cpu.a[7]=sp;cpu.isp=sp;cpu.a[1]=3u;amivm_ir_block_init(&b,0x5000u);CHECK(amivm_ir_decode_words(&b,jmp_pc,3u)==0);CHECK(b.ops[0].base_displacement==-16);CHECK(b.ops[0].instruction_bytes==6u);CHECK(amivm_ir_execute(&b,&cpu,&vm)==1);CHECK(cpu.pc==0x4ff8u);CHECK(cpu.a[7]==sp);
cpu.d[1]=0x12345678u;amivm_ir_block_init(&b,0x6000u);CHECK(amivm_ir_decode_words(&b,jmp_bs,2u)==0);CHECK(b.ops[0].base_suppress==1u);CHECK(b.ops[0].base_displacement==0);CHECK(amivm_ir_execute(&b,&cpu,&vm)==1);CHECK(cpu.pc==0x12345678u);
cpu.a[7]=sp;cpu.isp=sp;cpu.a[6]=0x30000000u;amivm_ir_block_init(&b,0x7000u);CHECK(amivm_ir_decode_words(&b,jsr_is,3u)==0);CHECK(b.ops[0].index_suppress==1u);CHECK(amivm_ir_execute(&b,&cpu,&vm)==1);CHECK(cpu.pc==0x30000044u);CHECK(be32(&vm,sp-4u)==0x7006u);
amivm_vm_destroy(&vm);puts("AmiVM M2.53 full indexed JSR/JMP reference tests: PASS");return 0;}
