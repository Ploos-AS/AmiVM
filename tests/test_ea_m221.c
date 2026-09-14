#include "ea.h"

#include <stdio.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void)
{
    struct amivm_ea_index_extension ea;

    /* Brief: D1.L*2, displacement -4. */
    {
        const uint16_t words[] = {0x1afcu};
        CHECK(amivm_ea_parse_index_extension(words, 1u, &ea) == AMIVM_EA_PARSE_OK);
        CHECK(ea.full_format == 0u);
        CHECK(ea.index_is_addr == 0u);
        CHECK(ea.index_reg == 1u);
        CHECK(ea.index_long == 1u);
        CHECK(ea.index_scale == 2u);
        CHECK(ea.brief_displacement == -4);
        CHECK(ea.words_consumed == 1u);
    }

    /* Full: A2.W*4, null base displacement, no indirect. */
    {
        const uint16_t words[] = {0xa510u};
        CHECK(amivm_ea_parse_index_extension(words, 1u, &ea) == AMIVM_EA_PARSE_OK);
        CHECK(ea.full_format == 1u);
        CHECK(ea.index_is_addr == 1u);
        CHECK(ea.index_reg == 2u);
        CHECK(ea.index_long == 0u);
        CHECK(ea.index_scale == 4u);
        CHECK(ea.base_displacement_size == 0u);
        CHECK(ea.indirect_mode == AMIVM_EA_INDIRECT_NONE);
        CHECK(ea.words_consumed == 1u);
    }

    /* Full: index suppressed, word base displacement -16. */
    {
        const uint16_t words[] = {0x0160u, 0xfff0u};
        CHECK(amivm_ea_parse_index_extension(words, 2u, &ea) == AMIVM_EA_PARSE_OK);
        CHECK(ea.index_suppress == 1u);
        CHECK(ea.base_displacement_size == 2u);
        CHECK(ea.base_displacement == -16);
        CHECK(ea.words_consumed == 2u);
    }

    /* Full: base suppressed, long base displacement. */
    {
        const uint16_t words[] = {0x01b0u, 0x1234u, 0x5678u};
        CHECK(amivm_ea_parse_index_extension(words, 3u, &ea) == AMIVM_EA_PARSE_OK);
        CHECK(ea.base_suppress == 1u);
        CHECK(ea.base_displacement_size == 4u);
        CHECK((uint32_t)ea.base_displacement == 0x12345678u);
        CHECK(ea.words_consumed == 3u);
    }

    /* Full preindexed with word outer displacement. */
    {
        const uint16_t words[] = {0x0122u, 0x0010u, 0xfffcu};
        CHECK(amivm_ea_parse_index_extension(words, 3u, &ea) == AMIVM_EA_PARSE_OK);
        CHECK(ea.base_displacement_size == 2u);
        CHECK(ea.base_displacement == 16);
        CHECK(ea.indirect_mode == AMIVM_EA_INDIRECT_PREINDEXED);
        CHECK(ea.outer_displacement_size == 2u);
        CHECK(ea.outer_displacement == -4);
        CHECK(ea.words_consumed == 3u);
    }

    /* Full postindexed with long outer displacement. */
    {
        const uint16_t words[] = {0x0117u, 0x0000u, 0x1000u};
        CHECK(amivm_ea_parse_index_extension(words, 3u, &ea) == AMIVM_EA_PARSE_OK);
        CHECK(ea.base_displacement_size == 0u);
        CHECK(ea.indirect_mode == AMIVM_EA_INDIRECT_POSTINDEXED);
        CHECK(ea.outer_displacement_size == 4u);
        CHECK(ea.outer_displacement == 0x1000);
        CHECK(ea.words_consumed == 3u);
    }

    /* Reserved base-displacement code 00. */
    {
        const uint16_t words[] = {0x0100u};
        CHECK(amivm_ea_parse_index_extension(words, 1u, &ea) == AMIVM_EA_PARSE_RESERVED);
    }

    /* Reserved IIS 100. */
    {
        const uint16_t words[] = {0x0114u};
        CHECK(amivm_ea_parse_index_extension(words, 1u, &ea) == AMIVM_EA_PARSE_RESERVED);
    }

    /* Truncated long base displacement. */
    {
        const uint16_t words[] = {0x0130u, 0x1234u};
        CHECK(amivm_ea_parse_index_extension(words, 2u, &ea) == AMIVM_EA_PARSE_TRUNCATED);
    }

    puts("AmiVM M2.21 full-extension EA parser tests: PASS");
    return 0;
}
