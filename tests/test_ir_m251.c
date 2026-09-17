#include "ir.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed: %s\n",#x); return 1; } } while (0)
static uint32_t be32(struct amivm_vm *v,uint32_t a){uint8_t b[4];unsigned i;for(i=0;i<4u;i++)if(!amivm_read8(v,a+i,&b[i]))return 0xffffffffu;return((uint32_t)b[0]<<24u)|((uint32_t)b[1]<<16u)|((uint32_t)b[2]<<8u)|b[3];}
int main(void){
 struct amivm_config cfg;struct amivm_vm vm;struct amivm_cpu_state cpu;struct amivm_ir_block b;uint32_t sp=AMIVM_RAM_BASE+0x3000u;
 /* D2.w*2 + (-16), A4.l*4 + 32, D1.l + 48, A3.w*2 - 32. */
 const uint16_t jsr_an[]={0x4eb3u,0x2200u|0x00f0u};
 const uint16_t jmp_an[]={0x4ef5u,0xcc00u|0x0020u};
 const uint16_t jsr_pc[]={0x4ebbu,0x1800u|0x0030u};
 const uint16_t jmp_pc[]={0x4efbu,0xb200u|0x00e0u};
 amivm_config_init(&cfg);cfg.ram_size=1u*1024u*1024u;CHECK(amivm_vm_init(&vm,&cfg)==0);
 memset(&cpu,0,sizeof(cpu));cpu.sr=0x2000u;cpu.a[7]=sp;cpu.isp=sp;
 cpu.a[3]=0x20001000u;cpu.d[2]=3u;amivm_ir_block_init(&b,0x4000u);CHECK(amivm_ir_decode_words(&b,jsr_an,2u)==0);CHECK(b.ops[0].ea_mode==AMIVM_IR_EA_D8_AN_XN);CHECK(b.ops[0].index_reg==2u);CHECK(b.ops[0].index_scale==2u);CHECK(amivm_ir_execute(&b,&cpu,&vm)==1);CHECK(cpu.pc==0x20000ff6u);CHECK(be32(&vm,sp-4u)==0x4004u);
 cpu.a[7]=sp;cpu.isp=sp;cpu.a[5]=0x30000000u;cpu.a[4]=5u;amivm_ir_block_init(&b,0x5000u);CHECK(amivm_ir_decode_words(&b,jmp_an,2u)==0);CHECK(b.ops[0].index_is_addr==1u);CHECK(b.ops[0].index_long==1u);CHECK(b.ops[0].index_scale==4u);CHECK(amivm_ir_execute(&b,&cpu,&vm)==1);CHECK(cpu.pc==0x30000034u);CHECK(cpu.a[7]==sp);
 cpu.d[1]=7u;amivm_ir_block_init(&b,0x6000u);CHECK(amivm_ir_decode_words(&b,jsr_pc,2u)==0);CHECK(b.ops[0].ea_mode==AMIVM_IR_EA_PC_D8_XN);CHECK(b.ops[0].index_long==1u);CHECK(amivm_ir_execute(&b,&cpu,&vm)==1);CHECK(cpu.pc==0x6039u);CHECK(be32(&vm,sp-4u)==0x6004u);
 cpu.a[7]=sp;cpu.isp=sp;cpu.a[3]=2u;amivm_ir_block_init(&b,0x7000u);CHECK(amivm_ir_decode_words(&b,jmp_pc,2u)==0);CHECK(amivm_ir_execute(&b,&cpu,&vm)==1);CHECK(cpu.pc==0x6fe6u);CHECK(cpu.a[7]==sp);
 amivm_vm_destroy(&vm);puts("AmiVM M2.51 indexed JSR/JMP IR tests: PASS");return 0;
}
