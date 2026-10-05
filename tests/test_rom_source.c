#include "rom_source.h"

static const unsigned char rom[] = {
    0x00,0x10,0x00,0x00,
    0x00,0xF0,0x00,0x08,
    0x4E,0x71
};

int main(void)
{
    struct amivm_rom_source_desc aros = {
        AMIVM_ROM_AROS, rom, sizeof(rom), "aros-m68k"
    };
    struct amivm_rom_source_desc external = {
        AMIVM_ROM_EXTERNAL, rom, sizeof(rom), "external"
    };

    if (amivm_rom_source_validate(&aros) != 0)
        return 1;
    if (amivm_rom_source_validate(&external) != 0)
        return 2;
    return 0;
}
