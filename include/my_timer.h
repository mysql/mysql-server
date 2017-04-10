/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MY_TIMER_H
#define MY_TIMER_H

/**
  @file include/my_timer.h
*/

#include "my_config.h"
#include "my_macros.h"

struct st_my_timer;

/* POSIX timers API. */
#ifdef HAVE_POSIX_TIMERS
# include <time.h>  /* timer_t */

  typedef timer_t   os_timer_t;
#elif defined(HAVE_KQUEUE_TIMERS)
# include <sys/types.h> /* uintptr_t */

  typedef uintptr_t os_timer_t;
#elif defined(_WIN32)
  typedef struct st_os_timer
  {
    HANDLE timer_handle;
    bool timer_state;
  } os_timer_t;
#endif

typedef struct st_my_timer my_timer_t;

/* Non-copyable timer object. */
struct st_my_timer
{
  /* Timer ID used to identify the timer in timer requests. */
  os_timer_t id;

  /** Timer expiration notification function. */
  void (*notify_function)(my_timer_t *);
};

C_MODE_START

/* Initialize internal components. */
int my_timer_initialize(void);

/* Release any resources acquired. */
void my_timer_deinitialize(void);

/* Create a timer object. */
int my_timer_create(my_timer_t *timer);

/* Set the time (in milliseconds) until the next expiration of the timer. */
int my_timer_set(my_timer_t *timer, unsigned long time);

/* Cancel the timer */
int my_timer_cancel(my_timer_t *timer, int *state);

/* Delete a timer object. */
void my_timer_delete(my_timer_t *timer);

C_MODE_END

#endif /* MY_TIMER_H */
