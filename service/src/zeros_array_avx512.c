#include <immintrin.h>

int is_all_zeros_array_avx512(const void *data, size_t size)
{
    const char *p = (const char *)data;
    const char  *end = p + size;
    const size_t cmp_op = 4;

    for (; ((uintptr_t)p & (uintptr_t)(sizeof(__m512i) - 1)) && p < end; p++)
        if (*p) return 0;

    for (; p + cmp_op * sizeof(__m512i) <= end; p += cmp_op * sizeof(__m512i)) {
        __m512i t = _mm512_load_si512((__m512i*)(p));
        for (size_t j = 1; j < cmp_op; j++)
            t = _mm512_or_si512(t, _mm512_load_si512((__m512i*)(p + (j * sizeof(__m512i)))));
        if (_mm512_cmpneq_epi64_mask(t, _mm512_setzero_si512()))
            return 0;
    }

    for (; p < end; p++)
        if (*p) return 0;

    return 1;
}
