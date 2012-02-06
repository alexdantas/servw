/** @file timer.c
 *
 *  Definition of the timer functions.
 */

#include <stdio.h>
#include <time.h>
#include "timer.h"

/* Clock types for the timer:
 *
 * CLOCK_REALTIME            System-wide real-time clock.
 * CLOCK_MONOTONIC           Clock that represents monotonic time since some
 *                           unspecified starting point.
 * CLOCK_MONOTONIC_RAW       Similar to CLOCK_MONOTONIC, but provides access
 *                           to raw hardware-based time. (Linux-specific).
 * CLOCK_PROCESS_CPUTIME_ID  High-resolution per-process timer from the CPU.
 * CLOCK_THREAD_CPUTIME_ID   Thread-specific CPU-time clock.
 */

 #define CLOCK_TYPE      CLOCK_MONOTONIC



/** Uses clock_gettime() to mark the number of seconds and nanoseconds
 *  between the Epoch and now.
 */
static int get_time (struct timespec* tv)
{
  return clock_gettime(CLOCK_TYPE, tv);
}

/** Returns the delta between the start and end of the timer.
 *
 *  @note If timer_start() and timer_stop() doesn't get called
 *        before this, the results are unpredictable.
 */
float timer_delta (struct timert* t)
{
  int sec    = (t->end.tv_sec) - (t->start.tv_sec);

  float nsec = (t->end.tv_nsec - t->start.tv_nsec) / 1e9;

  return (sec + nsec);
}


/** Records the current time as a start point.
 *
 *  Remember to call timer_stop() to record the diff.
 */
int timer_start (struct timert* t)
{
  return get_time (&(t->start));
}

/** Records the current time as a stop point.
 *
 *  Remember to call timer_delta() to retrieve the timer diff.
 */
int timer_stop (struct timert* t)
{
  return get_time (&(t->end));
}


