#include "rom_source.h"

int amivm_rom_source_validate(const struct amivm_rom_source_desc *source)
{
    if (!source || !source->image || source->size < 8u || !source->name)
        return 1;

    switch (source->kind) {
    case AMIVM_ROM_EXTERNAL:
    case AMIVM_ROM_AROS:
        return 0;
    default:
        return 1;
    }
}
