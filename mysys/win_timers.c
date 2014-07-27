/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#if defined (_WIN32)
#include "my_pthread.h"     /* my_thread_init, my_thread_end */
#include "my_sys.h"         /* my_message_local */
#include "my_timer.h"       /* my_timer_t */
#include <errno.h>
#include <windows.h>        /* Timer Queue and IO completion port functions */

#define  TIMER_EXPIRED    1

// Timer notifier thread id.
static pthread_t timer_notify_thread;

// IO completion port handle
HANDLE io_compl_port= 0;

// Timer queue handle
HANDLE timer_queue= 0;

/**
  Callback function registered to execute on timer expiration.

  @param  timer_data             timer data passed to function.
  @param  timer_or_wait_fired    flag to represent timer fired or signalled.

  @remark this function is executed in timer owner thread when timer
          expires.
*/
static void CALLBACK
timer_callback_function(PVOID timer_data, BOOLEAN timer_or_wait_fired __attribute__((unused)))
{
  my_timer_t *timer= (my_timer_t *)timer_data;
  assert(timer != NULL);
  PostQueuedCompletionStatus(io_compl_port, TIMER_EXPIRED, (ULONG_PTR)timer, 0);
}


/**
  Timer expiration notification thread.

  @param arg  Unused.
*/
static void*
timer_notify_thread_func(void *arg __attribute__((unused)))
{
  DWORD timer_state;
  ULONG_PTR compl_key;
  LPOVERLAPPED overlapped;
  my_timer_t *timer;

  my_thread_init();

  while(1)
  {
    // Get IO Completion status.
    if (GetQueuedCompletionStatus(io_compl_port, &timer_state, &compl_key,
                                  &overlapped, INFINITE) == 0)
      break;

    timer= (my_timer_t*)compl_key;

    // Timer is cancelled.
    if (timer->id == 0)
      continue;

    timer->id= 0;
    timer->notify_function(timer);
  }

  my_thread_end();

  return NULL;
}


/**
  Cancel timer set.

  @param timer Timer Object.

  @return 0 On Success
          1 On error.
*/
static int
cancel_timer(my_timer_t *timer)
{
  int ret_val;
  HANDLE id;

  assert(timer != 0);
  assert(timer_queue != 0);

  if (timer->id)
  {
    id= timer->id;
    timer->id= 0;

    ret_val= DeleteTimerQueueTimer(timer_queue, id, NULL);

    // Timer deletion from queue failed and there are no outstanding callback
    // functions for this timer.
    if (ret_val == 0 && GetLastError() != ERROR_IO_PENDING)
      return -1;
  }

  return 0;
}


/**
  Initialize internal components.

  @return 0 On success
          -1 On error.
*/
int
my_timer_initialize(void)
{
  // Create timer queue.
  timer_queue= CreateTimerQueue();
  if (!timer_queue)
  {
    my_message_local(ERROR_LEVEL, "Failed to create Timer Queue (errno= %d)",
                     errno);
    goto err;
  }

  // Create IO completion port.
  io_compl_port= CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
  if (!io_compl_port)
  {
    my_message_local(ERROR_LEVEL,
                     "Failed to create IO completion port (errno= %d)",
                     errno);
    goto err;
  }

  if (mysql_thread_create(key_thread_timer_notifier, &timer_notify_thread, 0,
                          timer_notify_thread_func, 0))
  {
    my_message_local(ERROR_LEVEL,
                     "Failed to create timer notify thread (errno= %d).",
                     errno);
    goto err;
  }

  return 0;

err:
  if (timer_queue)
  {
    DeleteTimerQueueEx(timer_queue, NULL);
    timer_queue= 0;
  }

  if (io_compl_port)
  {
    CloseHandle(io_compl_port);
    io_compl_port= 0;
  }

  return -1;
}


/**
  Release any resources that were allocated as part of initialization.
*/
void
my_timer_deinitialize()
{
  if (timer_queue)
  {
    DeleteTimerQueueEx(timer_queue, NULL);
    timer_queue= 0;
  }

  if (io_compl_port)
  {
    CloseHandle(io_compl_port);
    io_compl_port= 0;
  }

  pthread_join(timer_notify_thread, NULL);
}


/**
  Create a timer object.

  @param  timer   Timer object.

  @return On success, 0.
          On error, -1.
*/
int
my_timer_create(my_timer_t *timer)
{
  assert(timer_queue != 0);
  timer->id= 0;
  return 0;
}


/**
  Set the time until the next expiration of the timer.

  @param  timer   Timer object.
  @param  time    Amount of time (in milliseconds) before the timer expires.

  @return On success, 0.
          On error, -1.
*/
int
my_timer_set(my_timer_t *timer, unsigned long time)
{
  assert(timer != NULL);
  assert(timer->id == 0);
  assert(timer_queue != 0);

  if (CreateTimerQueueTimer(&timer->id, timer_queue,
                            timer_callback_function, timer, time, 0,
                            WT_EXECUTEONLYONCE) == 0)
    return -1;

  return 0;
}


/**
  Cancel the timer.

  @param  timer   Timer object.
  @param  state   The state of the timer at the time of cancellation, either
                  signaled (false) or nonsignaled (true).

  @return On success, 0.
          On error, 1. 
*/
int
my_timer_cancel(my_timer_t *timer, int *state)
{
  if (cancel_timer(timer) != 0)
    return -1;

  /**
    Since timer is removed after expiration or cancellation, setting state to
    0 to avoid reusing of timer.
  */
  *state= 0;

  return 0;
}


/**
  Delete a timer object.

  @param  timer   Timer Object.
*/
void
my_timer_delete(my_timer_t *timer)
{
  cancel_timer(timer);
}
#endif
