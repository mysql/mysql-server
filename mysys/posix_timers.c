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


#include "my_global.h"
#include "my_thread.h"      /* my_thread_init, my_thread_end */
#include "my_sys.h"         /* my_message_local */
#include "my_timer.h"       /* my_timer_t */

#include <string.h>         /* memset */
#include <signal.h>

#if defined(HAVE_SIGEV_THREAD_ID)
#include <sys/syscall.h>    /* SYS_gettid */

#ifndef sigev_notify_thread_id
#define sigev_notify_thread_id   _sigev_un._tid
#endif

#define MY_TIMER_EVENT_SIGNO  (SIGRTMIN)
#define MY_TIMER_KILL_SIGNO   (SIGRTMIN+1)

/* Timer thread ID (TID). */
static pid_t timer_notify_thread_id;

#elif defined(HAVE_SIGEV_PORT)
#include <port.h>

int port_id= -1;

#endif

/* Timer thread object. */
static my_thread_handle timer_notify_thread;

#if defined(HAVE_SIGEV_THREAD_ID)
/**
  Timer expiration notification thread.

  @param  arg   Barrier object.
*/

static void *
timer_notify_thread_func(void *arg)
{
  sigset_t set;
  siginfo_t info;
  my_timer_t *timer;
  pthread_barrier_t *barrier= arg;

  my_thread_init();

  sigemptyset(&set);
  sigaddset(&set, MY_TIMER_EVENT_SIGNO);
  sigaddset(&set, MY_TIMER_KILL_SIGNO);

  /* Get the thread ID of the current thread. */
  timer_notify_thread_id= (pid_t) syscall(SYS_gettid);

  /* Wake up parent thread, timer_notify_thread_id is available. */
  pthread_barrier_wait(barrier);

  while (1)
  {
    if (sigwaitinfo(&set, &info) < 0)
      continue;

    if (info.si_signo == MY_TIMER_EVENT_SIGNO)
    {
      timer= (my_timer_t*)info.si_value.sival_ptr;
      timer->notify_function(timer);
    }
    else if (info.si_signo == MY_TIMER_KILL_SIGNO)
      break;
  }

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
  pthread_barrier_t barrier;

  if (pthread_barrier_init(&barrier, NULL, 2))
  {
    my_message_local(ERROR_LEVEL,
                     "Failed to initialize pthread barrier. errno=%d", errno);
    return -1;
  }

  if (mysql_thread_create(key_thread_timer_notifier, &timer_notify_thread,
                          NULL, timer_notify_thread_func, &barrier))
  {
    my_message_local(ERROR_LEVEL,
                     "Failed to create timer notify thread (errno= %d).",
                     errno);
    pthread_barrier_destroy(&barrier);
    return -1;
  }

  pthread_barrier_wait(&barrier);
  pthread_barrier_destroy(&barrier);

  return 0;
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
  sigset_t set, old_set;

  if (sigfillset(&set))
  {
    my_message_local(ERROR_LEVEL,
                     "Failed to intialize signal set (errno=%d).", errno);
    return -1;
  }

  /*
    Temporarily block all signals. New thread will inherit signal
    mask of the current thread.
  */
  if (pthread_sigmask(SIG_BLOCK, &set, &old_set))
    return -1;

  /* Create a helper thread. */
  rc= start_helper_thread();

  /* Restore the signal mask. */
  pthread_sigmask(SIG_SETMASK, &old_set, NULL);

  return rc;
}


/**
  Release any resources that were allocated as part of initialization.
*/

void
my_timer_deinitialize(void)
{
  /* Kill helper thread. */
  pthread_kill(timer_notify_thread.thread, MY_TIMER_KILL_SIGNO);

  /* Wait for helper thread termination. */
  my_thread_join(&timer_notify_thread, NULL);
}


/**
  Create a timer object.

  @param  timer   Location where the timer ID is returned.

  @return On success, 0.
          On error, -1 is returned, and errno is set to indicate the error.
*/

int
my_timer_create(my_timer_t *timer)
{
  struct sigevent sigev;

  memset(&sigev, 0, sizeof(sigev));

  sigev.sigev_value.sival_ptr= timer;
  sigev.sigev_signo= MY_TIMER_EVENT_SIGNO;
  sigev.sigev_notify= SIGEV_SIGNAL | SIGEV_THREAD_ID;
  sigev.sigev_notify_thread_id= timer_notify_thread_id;

  return timer_create(CLOCK_MONOTONIC, &sigev, &timer->id);
}
#elif defined(HAVE_SIGEV_PORT)
/**
  Timer expiration notification thread.

  @param  arg   Barrier object.
*/

static void *
timer_notify_thread_func(void *arg MY_ATTRIBUTE((unused)))
{
  port_event_t port_event;
  my_timer_t *timer;

  my_thread_init();

  while (1)
  {
    if (port_get(port_id, &port_event, NULL))
      break;

    if (port_event.portev_source != PORT_SOURCE_TIMER)
      continue;

    timer= (my_timer_t*)port_event.portev_user;
    timer->notify_function(timer);
  }

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
  if (mysql_thread_create(key_thread_timer_notifier, &timer_notify_thread,
                          NULL, timer_notify_thread_func, NULL))
  {
    my_message_local(ERROR_LEVEL,
                     "Failed to create timer notify thread (errno= %d).",
                     errno);
    return -1;
  }

  return 0;
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

  if ((port_id= port_create()) < 0)
  {
    my_message_local(ERROR_LEVEL, "Failed to create port (errno= %d).", errno);
    return -1;
  }

  /* Create a helper thread. */
  rc= start_helper_thread();

  return rc;
}


/**
  Release any resources that were allocated as part of initialization.
*/

void
my_timer_deinitialize(void)
{
  DBUG_ASSERT(port_id >= 0);

  // close port
  close(port_id);

  /* Wait for helper thread termination. */
  my_thread_join(&timer_notify_thread, NULL);
}


/**
  Create a timer object.

  @param  timer   Location where the timer ID is returned.

  @return On success, 0.
          On error, -1 is returned, and errno is set to indicate the error.
*/

int
my_timer_create(my_timer_t *timer)
{
  struct sigevent sigev;
  port_notify_t port_notify;

  port_notify.portnfy_port= port_id;
  port_notify.portnfy_user= timer;

  memset(&sigev, 0, sizeof(sigev));
  sigev.sigev_value.sival_ptr= &port_notify;
  sigev.sigev_notify= SIGEV_PORT;

  return timer_create(CLOCK_REALTIME, &sigev, &timer->id);
}
#endif


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
  const struct itimerspec spec= {
    .it_interval= {.tv_sec= 0, .tv_nsec= 0},
    .it_value= {.tv_sec= time / 1000,
                .tv_nsec= (time % 1000) * 1000000}
  };

  return timer_settime(timer->id, 0, &spec, NULL);
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
  struct itimerspec old_spec;

  /* A zeroed initial expiration value disarms the timer. */
  const struct timespec zero_time= { .tv_sec= 0, .tv_nsec= 0 };
  const struct itimerspec zero_spec= { .it_value= zero_time };

  /*
    timer_settime returns the amount of time before the timer
    would have expired or zero if the timer was disarmed.
  */
  if (! (status= timer_settime(timer->id, 0, &zero_spec, &old_spec)))
    *state= (old_spec.it_value.tv_sec || old_spec.it_value.tv_nsec);

  return status;
}


/**
  Delete a timer object.

  @param  timer   Timer object.
*/

void
my_timer_delete(my_timer_t *timer)
{
  timer_delete(timer->id);
}

