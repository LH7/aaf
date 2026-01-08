#include <immintrin.h>

int is_all_zeros_array_avx2(const void *data, size_t size)
{
    const char *p = (const char *)data;
    const char *end = p + size;
    const size_t cmp_op = 4;

    for (; ((uintptr_t)p & (uintptr_t)(sizeof(__m256i) - 1)) && p < end; p++)
        if (*p) return 0;

    for (; p + cmp_op * sizeof(__m256i) <= end; p += cmp_op * sizeof(__m256i)) {
        __m256i t = _mm256_load_si256((__m256i*)(p));
        for (size_t j = 1; j < cmp_op; j++)
            t = _mm256_or_si256(t, _mm256_load_si256((__m256i*)(p + (j * sizeof(__m256i)))));
        if (!_mm256_testz_si256(t, t))
            return 0;
    }

    for (; p < end; p++)
        if (*p) return 0;

    return 1;
}
