#include <string.h>
#include <stdint.h>

#define BLOCK_SIZE 4096

int is_all_zeros_array(const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    static const uint8_t zero_block[BLOCK_SIZE] = {0};

    int remaining = size;

    while (remaining >= BLOCK_SIZE) {
        if (memcmp(bytes, zero_block, BLOCK_SIZE) != 0) return 0;
        bytes += BLOCK_SIZE;
        remaining -= BLOCK_SIZE;
    }

    if (remaining > 0) {
        if (memcmp(bytes, zero_block, remaining) != 0) return 0;
    }

    return 1;
}

