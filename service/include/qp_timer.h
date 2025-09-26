#ifndef _Q_PERF_TIME_H
#define _Q_PERF_TIME_H

#include <windows.h>

void qp_timer_init();
LONGLONG qp_time_get();
LONGLONG qp_timer_diff_100ns(LONGLONG start_time, LONGLONG end_time);

#endif
