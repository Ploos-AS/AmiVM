#include "cpu.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK failed: %s\n",#x);return 1;}}while(0)
int main(void){
 struct amivm_cpu_state cpu;memset(&cpu,0,sizeof cpu);
 cpu.pmmu_atc[0].valid=true;cpu.pmmu_atc[0].function_code=1u;
 cpu.pmmu_atc[1].valid=true;cpu.pmmu_atc[1].function_code=5u;
 cpu.pmmu_atc[2].valid=true;cpu.pmmu_atc[2].function_code=2u;
 amivm_pmmu51_atc_flush_fc(&cpu,1u,7u);
 CHECK(!cpu.pmmu_atc[0].valid);CHECK(cpu.pmmu_atc[1].valid);CHECK(cpu.pmmu_atc[2].valid);
 cpu.pmmu_atc[0].valid=true;cpu.pmmu_atc[0].function_code=1u;
 amivm_pmmu51_atc_flush_fc(&cpu,1u,3u);
 CHECK(!cpu.pmmu_atc[0].valid);CHECK(!cpu.pmmu_atc[1].valid);CHECK(cpu.pmmu_atc[2].valid);
 puts("AmiVM M2.108 MC68851 selective FC ATC flush: PASS");return 0;
}