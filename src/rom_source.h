#ifndef AMIVM_ROM_SOURCE_H
#define AMIVM_ROM_SOURCE_H

#include <stddef.h>
#include <stdint.h>

enum amivm_rom_source {
    AMIVM_ROM_EXTERNAL = 0,
    AMIVM_ROM_AROS = 1
};

struct amivm_rom_source_desc {
    enum amivm_rom_source kind;
    const uint8_t *image;
    size_t size;
    const char *name;
};

int amivm_rom_source_validate(const struct amivm_rom_source_desc *source);

#endif
