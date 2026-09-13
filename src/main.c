#include <stdio.h>
#include <string.h>

#define AMIVM_VERSION "0.0.1-m0"

static void dump_machine(void)
{
    puts("machine=AmiVM-Hyper/040");
    puts("cpu=m68040");
    puts("mmu=required");
    puts("fpu=required");
    puts("endianness=big");
    puts("devices=vmserial,vmblock,vmnet,vmgfx,vmfs,vmaudio");
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("AmiVM %s\n", AMIVM_VERSION);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--dump-machine") == 0) {
        dump_machine();
        return 0;
    }

    puts("AmiVM M0 host skeleton");
    puts("Use --version or --dump-machine.");
    return 0;
}
