/** @file timer.c
 *
 *  Definition of the timer functions.
 */

#include <stdio.h>
#include <sys/time.h>
#include "timer.h"


/** Uses clock_gettime() to mark the number of seconds and nanoseconds
 *  between the Epoch and now.
 */
static int get_time (struct timeval* tv)
{
  return gettimeofday(tv, NULL);
}

float get_seconds (struct timeval* tv)
{
  int sec = tv->tv_sec;
  float usec = tv->tv_usec / 1e6;

  return (sec + usec);
}

/** Returns the delta between the start and end of the timer.
 *
 *  @note If timer_start() and timer_stop() doesn't get called
 *        before this, the results are unpredictable.
 */
float timer_delta (struct timert* t)
{
  timersub (&(t->end), &(t->start), &(t->delta));

  return get_seconds (&(t->delta));
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


