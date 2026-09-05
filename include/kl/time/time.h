#ifndef KL_TIME_H
#define KL_TIME_H

typedef double (*KL_GetTimeFunction)();

/**
* @brief the current monotonic time, in seconds
**/
double kl_time_now_s__monotonic();

/**
* @brief the current monotonic time, in milliseconds
**/
double kl_time_now_ms__monotonic();

#endif
