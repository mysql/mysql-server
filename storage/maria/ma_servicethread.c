/*
   Copyright (c) 2009, 2011, Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "maria_def.h"
#include "ma_servicethread.h"

/**
   Initializes the service thread

   @param control        control block

   @return Operation status
    @retval 0 OK
    @retval 1 error
*/

int ma_service_thread_control_init(MA_SERVICE_THREAD_CONTROL *control)
{
  int res= 0;
  DBUG_ENTER("ma_service_thread_control_init");
  DBUG_PRINT("init", ("control 0x%lx", (ulong) control));
  control->inited= TRUE;
  control->killed= FALSE;
  res= (mysql_mutex_init(key_SERVICE_THREAD_CONTROL_lock,
                         control->LOCK_control, MY_MUTEX_INIT_SLOW) ||
        mysql_cond_init(key_SERVICE_THREAD_CONTROL_cond,
                        control->COND_control, 0));
  DBUG_PRINT("info", ("init: %s", (res ? "Error" : "OK")));
  DBUG_RETURN(res);
}


/**
   Kill the service thread

   @param control        control block

   @note The service thread should react on condition and status equal
   THREAD_DYING, by setting status THREAD_DEAD, and issuing message to
   control thread via condition and exiting. The base way to do so is using
   my_service_thread_sleep() and my_service_thread_signal_end()
*/

void ma_service_thread_control_end(MA_SERVICE_THREAD_CONTROL *control)
{
  DBUG_ENTER("ma_service_thread_control_end");
  DBUG_PRINT("init", ("control 0x%lx", (ulong) control));
  DBUG_ASSERT(control->inited);
  mysql_mutex_lock(control->LOCK_control);
  if (!control->killed)
  {
    DBUG_PRINT("info",("killing Maria background thread"));
    control->killed= TRUE; /* kill it */
    mysql_cond_broadcast(control->COND_control);
    mysql_mutex_unlock(control->LOCK_control);
    DBUG_PRINT("info", ("waiting for Maria background thread to die"));
    pthread_join(control->thread, NULL);
  }
  else
    mysql_mutex_unlock(control->LOCK_control);
  mysql_mutex_destroy(control->LOCK_control);
  mysql_cond_destroy(control->COND_control);
  control->inited= FALSE;
  DBUG_VOID_RETURN;
}


/**
   Sleep for given number of nanoseconds with reaction on thread kill

   @param control        control block
   @param sleep_time     time of sleeping

   @return Operation status
    @retval FALSE Time out
    @retval TRUE  Thread should be killed
*/

my_bool my_service_thread_sleep(MA_SERVICE_THREAD_CONTROL *control,
                                ulonglong sleep_time)
{
  struct timespec abstime;
  my_bool res= FALSE;
  DBUG_ENTER("my_service_thread_sleep");
  DBUG_PRINT("init", ("control 0x%lx", (ulong) control));
  mysql_mutex_lock(control->LOCK_control);
  if (control->killed)
  {
    mysql_mutex_unlock(control->LOCK_control);
    DBUG_RETURN(TRUE);
  }
#if 0 /* good for testing, to do a lot of checkpoints, finds a lot of bugs */
  mysql_mutex_unlock(&control->LOCK_control);
  my_sleep(100000); /* a tenth of a second */
  mysql_mutex_lock(&control->LOCK_control);
#else
    /* To have a killable sleep, we use timedwait like our SQL GET_LOCK() */
  DBUG_PRINT("info", ("sleeping %llu nano seconds", sleep_time));
  if (sleep_time)
  {
    set_timespec_nsec(abstime, sleep_time);
    mysql_cond_timedwait(control->COND_control,
                           control->LOCK_control, &abstime);
  }
#endif
  if (control->killed)
    res= TRUE;
  mysql_mutex_unlock(control->LOCK_control);
  DBUG_RETURN(res);
}
