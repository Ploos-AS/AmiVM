#include "jit.h"
#include <stdio.h>
int main(void)
{
    int arch = amivm_jit_host_arch();
#if defined(__aarch64__) || defined(_M_ARM64)
    if (arch != AMIVM_JIT_ARCH_AARCH64) return 1;
#elif defined(__x86_64__) || defined(_M_X64)
    if (arch != AMIVM_JIT_ARCH_X86_64) return 1;
#else
    if (arch != AMIVM_JIT_ARCH_NONE) return 1;
#endif
    puts("AmiVM M2.59 host architecture detection: PASS");
    return 0;
}
