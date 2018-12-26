/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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


#include "my_thread.h"   /* my_thread_init, my_thread_end */
#include "my_sys.h"       /* my_message_local */
#include "my_timer.h"     /* my_timer_t */
#include "my_global.h"

#include <sys/types.h>
#include <sys/event.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

/* Kernel event queue file descriptor. */
static int kq_fd= -1;

/* Timer thread object. */
static my_thread_handle timer_notify_thread;

/**
  Timer expiration notification thread.

  @param  arg   Unused.
*/

static void *
timer_notify_thread_func(void *arg MY_ATTRIBUTE((unused)))
{
  my_timer_t *timer;
  struct kevent kev;

  my_thread_init();

  while (1)
  {
    if (kevent(kq_fd, NULL, 0, &kev, 1, NULL) < 0)
    {
      if (errno == EINTR)
        continue;
      else
      {
        my_message_local(ERROR_LEVEL,
                         "kevent failed with errno= %d,"
                         "exiting timer notifier thread.",
                         errno);
        break;
      }
    }

    if (kev.filter == EVFILT_TIMER)
    {
      timer= kev.udata;
      DBUG_ASSERT(timer->id == kev.ident);
      timer->notify_function(timer);
    }
    else if (kev.filter == EVFILT_USER)
      break;
  }

  close(kq_fd);
  my_thread_end();

  return NULL;
}


/**
  Create a helper thread to dispatch timer expiration notifications.

  @return On success, 0. On error, -1 is returned.
*/

static int
start_helper_thread(void)
{
  struct kevent kev;

  EV_SET(&kev, 0, EVFILT_USER, EV_ADD, 0, 0, 0);

  if (kevent(kq_fd, &kev, 1, NULL, 0, NULL) < 0)
  {
    my_message_local(ERROR_LEVEL, "Failed to create event (errno= %d).", errno);
    return -1;
  }

  return mysql_thread_create(key_thread_timer_notifier, &timer_notify_thread,
                             NULL, timer_notify_thread_func, NULL);
}


/**
  Initialize internal components.

  @return On success, 0.
          On error, -1 is returned, and errno is set to indicate the error.
*/

int
my_timer_initialize(void)
{
  int rc;

  /* Create a file descriptor for event notification. */
  if ((kq_fd= kqueue()) < 0)
  {
    my_message_local(ERROR_LEVEL,
                     "Failed to create fd for event notification (errno= %d).",
                     errno);
    return -1;
  }

  /* Create a helper thread. */
  if ((rc= start_helper_thread()))
  {
    my_message_local(ERROR_LEVEL, "Failed to start timer notify thread.");
    close(kq_fd);
  }

  return rc;
}


/**
  Release any resources that were allocated as part of initialization.
*/

void
my_timer_deinitialize(void)
{
  struct kevent kev;

  EV_SET(&kev, 0, EVFILT_USER, 0, NOTE_TRIGGER, 0, 0);

  if (kevent(kq_fd, &kev, 1, NULL, 0, NULL) < 0)
    my_message_local(ERROR_LEVEL,
                     "Failed to create event to interrupt timer notifier thread"
                     " (errno= %d).", errno);

  my_thread_join(&timer_notify_thread, NULL);
}


/**
  Create a timer object.

  @param  timer   Timer object.

  @return On success, 0.
          On error, -1 is returned, and errno is set to indicate the error.
*/

int
my_timer_create(my_timer_t *timer)
{
  DBUG_ASSERT(kq_fd >= 0);

  timer->id= (uintptr_t) timer;

  return 0;
}


/**
  Set the time until the next expiration of the timer.

  @param  timer   Timer object.
  @param  time    Amount of time (in milliseconds) before the timer expires.

  @return On success, 0.
          On error, -1 is returned, and errno is set to indicate the error.
*/

int
my_timer_set(my_timer_t *timer, unsigned long time)
{
  struct kevent kev;

  EV_SET(&kev, timer->id, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, time, timer);

  return kevent(kq_fd, &kev, 1, NULL, 0, NULL);
}


/**
  Cancel the timer.

  @param  timer   Timer object.
  @param  state   The state of the timer at the time of cancellation, either
                  signaled (false) or nonsignaled (true).

  @return On success, 0.
          On error, -1 is returned, and errno is set to indicate the error.
*/

int
my_timer_cancel(my_timer_t *timer, int *state)
{
  int status;
  struct kevent kev;

  EV_SET(&kev, timer->id, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);

  status= kevent(kq_fd, &kev, 1, NULL, 0, NULL);

  /*
    If the event was retrieved from the kqueue (at which point we
    consider it to be signaled), the timer was automatically deleted.
  */
  if (!status)
    *state= 1;
  else if (errno == ENOENT)
  {
    *state= 0;
    status= 0;
  }

  return status;
}


/**
  Delete a timer object.

  @param  timer   Timer object.
*/

void
my_timer_delete(my_timer_t *timer)
{
  struct kevent kev;

  EV_SET(&kev, timer->id, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);

  kevent(kq_fd, &kev, 1, NULL, 0, NULL);
}

