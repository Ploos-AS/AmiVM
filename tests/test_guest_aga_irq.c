#include "boot.h"
#include "rom_boot.h"
#include "vm.h"

#define VECTOR_BASE (AMIVM_RAM_BASE + 0x400u)
#define HANDLER     (AMIVM_RAM_BASE + 0x100u)
#define RESULT      (AMIVM_RAM_BASE + 0x120u)

static const unsigned char rom[] = {
    0x10,0x00,0x20,0x00,             /* SP = RAM + $2000 */
    0x00,0xF0,0x00,0x08,             /* PC = ROM + $08 */

    /* MOVE.L #HANDLER,VECTOR+27*4 */
    0x21,0xFC,0x10,0x00,0x01,0x00,
    0x10,0x00,0x04,0x6C,

    /* MOVE.B #1,AGA VBlank enable */
    0x13,0xFC,0x00,0x01,0xFF,0x00,0x50,0x0D,

    /* idle */
    0x4E,0x71,
    0x4E,0x71,
    0x4E,0x71
};

/* MOVE.L #1,RESULT ; RTE */
static const unsigned char handler[] = {
    0x23,0xFC,0x00,0x00,0x00,0x01,
    0x10,0x00,0x01,0x20,
    0x4E,0x73
};

int main(void)
{
    struct amivm_config config;
    struct amivm_vm vm;
    struct amivm_boot_config boot = { AMIVM_BOOT_AMIGA_ROM, 0u, 0u, 0u };
    uint32_t vector_value = 0u;
    uint32_t result = 0u;
    unsigned i;

    amivm_config_init(&config);
    config.ram_size = 2u * 1024u * 1024u;
    if (amivm_vm_init(&vm, &config) != 0)
        return 1;

    if (amivm_rom_install(&vm, rom, sizeof(rom)) != 0)
        goto fail;

    for (i = 0u; i < sizeof(handler); ++i)
        if (!amivm_write8(&vm, HANDLER + i, handler[i]))
            goto fail;

    if (amivm_boot_prepare(&vm, &boot) != 0)
        goto fail;

    for (i = 0u; i < 1000000u; ++i) {
        if (amivm_vm_step(&vm) != 0)
            goto fail;
        if (amivm_m68k_read_u32(&vm, RESULT, &result) == 0)
            goto fail;
        if (result == 1u)
            break;
    }

    if (amivm_m68k_read_u32(&vm, VECTOR_BASE + 27u * 4u, &vector_value) != 0)
        goto fail;
    if (vector_value != HANDLER || result != 1u)
        goto fail;

    amivm_vm_destroy(&vm);
    return 0;

fail:
    amivm_vm_destroy(&vm);
    return 1;
}
