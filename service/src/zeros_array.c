#include <stdint.h>

int is_all_zeros_array_avx2(const void *data, size_t size);
int is_all_zeros_array_avx512(const void *data, size_t size);

static int is_all_zeros_array_native(const void *data, size_t size)
{
    size_t i, j;
    const size_t cmp_op = 16;

    for (i = 0; i + sizeof(size_t)*cmp_op <= size; i += sizeof(size_t)*cmp_op) {
        for (j = 0; j < cmp_op; j++)
            if (((size_t*)((uint8_t *)data + i))[j]) return 0;
    }

    for (; i < size; i++) {
        if (((uint8_t *)data)[i]) return 0;
    }

    return 1;
}

int is_all_zeros_array(const void *data, size_t size)
{
    static int (*is_all_zeros_array_ptr)(const void *, size_t) = NULL;
    if (is_all_zeros_array_ptr == NULL) {
        if (__builtin_cpu_supports("avx512f")) {
            is_all_zeros_array_ptr = is_all_zeros_array_avx512;
        } else if (__builtin_cpu_supports("avx2")) {
            is_all_zeros_array_ptr = is_all_zeros_array_avx2;
        } else {
            is_all_zeros_array_ptr = is_all_zeros_array_native;
        }
    }
    return is_all_zeros_array_ptr(data, size);
}
