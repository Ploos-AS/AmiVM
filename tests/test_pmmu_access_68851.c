#include "cpu.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK failed: %s\n",#x);return 1;}}while(0)
int main(void){
 struct amivm_cpu_state cpu; memset(&cpu,0,sizeof cpu);
 CHECK(amivm_pmmu51_get_access_level(&cpu)==0u);
 for(uint8_t i=0;i<8u;i++){CHECK(amivm_pmmu51_set_access_level(&cpu,i));CHECK(amivm_pmmu51_get_access_level(&cpu)==i);}
 CHECK(!amivm_pmmu51_set_access_level(&cpu,8u));CHECK(amivm_pmmu51_get_access_level(&cpu)==7u);
 CHECK(!amivm_pmmu51_set_access_level(NULL,0u));CHECK(amivm_pmmu51_get_access_level(NULL)==0u);
 puts("AmiVM M2.102 68851 access-level input: PASS");return 0;
}