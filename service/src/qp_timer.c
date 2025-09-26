#include "qp_timer.h"

static LARGE_INTEGER qp_timer_freq;

void qp_timer_init()
{
    QueryPerformanceFrequency(&qp_timer_freq);
}

LONGLONG qp_time_get()
{
    LARGE_INTEGER cur_counter;
    QueryPerformanceCounter(&cur_counter);
    return cur_counter.QuadPart;
}

LONGLONG qp_timer_diff_100ns(LONGLONG start_time, LONGLONG end_time)
{
    return ((end_time - start_time) * 10000000LL) / qp_timer_freq.QuadPart;
}
