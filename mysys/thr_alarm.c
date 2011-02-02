/* Copyright (C) 2000 MySQL AB, 2008-2009 Sun Microsystems, Inc

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

/* To avoid problems with alarms in debug code, we disable DBUG here */
#define FORCE_DBUG_OFF
#include "mysys_priv.h"
#include <my_global.h>

#if !defined(DONT_USE_THR_ALARM)
#include <errno.h>
#include <my_pthread.h>
#include <signal.h>
#include <my_sys.h>
#include <m_string.h>
#include <queues.h>
#include "thr_alarm.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>				/* AIX needs this for fd_set */
#endif

#ifndef ETIME
#define ETIME ETIMEDOUT
#endif

uint thr_client_alarm;
static int alarm_aborted=1;			/* No alarm thread */
my_bool thr_alarm_inited= 0;
volatile my_bool alarm_thread_running= 0;
time_t next_alarm_expire_time= ~ (time_t) 0;
static sig_handler process_alarm_part2(int sig);

#if !defined(__WIN__)

static mysql_mutex_t LOCK_alarm;
static mysql_cond_t COND_alarm;
static sigset_t full_signal_set;
static QUEUE alarm_queue;
static uint max_used_alarms=0;
pthread_t alarm_thread;

#ifdef USE_ALARM_THREAD
static void *alarm_handler(void *arg);
#define reschedule_alarms() mysql_cond_signal(&COND_alarm)
#else
#define reschedule_alarms() pthread_kill(alarm_thread,THR_SERVER_ALARM)
#endif

static sig_handler thread_alarm(int sig __attribute__((unused)));

static int compare_ulong(void *not_used __attribute__((unused)),
			 uchar *a_ptr,uchar* b_ptr)
{
  ulong a=*((ulong*) a_ptr),b= *((ulong*) b_ptr);
  return (a < b) ? -1  : (a == b) ? 0 : 1;
}

void init_thr_alarm(uint max_alarms)
{
  sigset_t s;
  DBUG_ENTER("init_thr_alarm");
  alarm_aborted=0;
  next_alarm_expire_time= ~ (time_t) 0;
  init_queue(&alarm_queue,max_alarms+1,offsetof(ALARM,expire_time),0,
	     compare_ulong,NullS);
  sigfillset(&full_signal_set);			/* Neaded to block signals */
  mysql_mutex_init(key_LOCK_alarm, &LOCK_alarm, MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_alarm, &COND_alarm, NULL);
  if (thd_lib_detected == THD_LIB_LT)
    thr_client_alarm= SIGALRM;
  else
    thr_client_alarm= SIGUSR1;
#ifndef USE_ALARM_THREAD
  if (thd_lib_detected != THD_LIB_LT)
#endif
  {
    my_sigset(thr_client_alarm, thread_alarm);
  }
  sigemptyset(&s);
  sigaddset(&s, THR_SERVER_ALARM);
  alarm_thread=pthread_self();
#if defined(USE_ALARM_THREAD)
  {
    pthread_attr_t thr_attr;
    pthread_attr_init(&thr_attr);
    pthread_attr_setscope(&thr_attr,PTHREAD_SCOPE_PROCESS);
    pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&thr_attr,8196);
    mysql_thread_create(key_thread_alarm,
                        &alarm_thread, &thr_attr, alarm_handler, NULL);
    pthread_attr_destroy(&thr_attr);
  }
#elif defined(USE_ONE_SIGNAL_HAND)
  pthread_sigmask(SIG_BLOCK, &s, NULL);		/* used with sigwait() */
  if (thd_lib_detected == THD_LIB_LT)
  {
    my_sigset(thr_client_alarm, process_alarm);        /* Linuxthreads */
    pthread_sigmask(SIG_UNBLOCK, &s, NULL);
  }
#else
  my_sigset(THR_SERVER_ALARM, process_alarm);
  pthread_sigmask(SIG_UNBLOCK, &s, NULL);
#endif
  DBUG_VOID_RETURN;
}


void resize_thr_alarm(uint max_alarms)
{
  mysql_mutex_lock(&LOCK_alarm);
  /*
    It's ok not to shrink the queue as there may be more pending alarms than
    than max_alarms
  */
  if (alarm_queue.elements < max_alarms)
    resize_queue(&alarm_queue,max_alarms+1);
  mysql_mutex_unlock(&LOCK_alarm);
}


/*
  Request alarm after sec seconds.

  SYNOPSIS
    thr_alarm()
    alrm		Pointer to alarm detection
    alarm_data		Structure to store in alarm queue

  NOTES
    This function can't be called from the alarm-handling thread.

  RETURN VALUES
    0 ok
    1 If no more alarms are allowed (aborted by process)

    Stores in first argument a pointer to a non-zero int which is set to 0
    when the alarm has been given
*/

my_bool thr_alarm(thr_alarm_t *alrm, uint sec, ALARM *alarm_data)
{
  time_t now;
#ifndef USE_ONE_SIGNAL_HAND
  sigset_t old_mask;
#endif
  my_bool reschedule;
  struct st_my_thread_var *current_my_thread_var= my_thread_var;
  DBUG_ENTER("thr_alarm");
  DBUG_PRINT("enter",("thread: %s  sec: %d",my_thread_name(),sec));

  now= my_time(0);
#ifndef USE_ONE_SIGNAL_HAND
  pthread_sigmask(SIG_BLOCK,&full_signal_set,&old_mask);
#endif
  mysql_mutex_lock(&LOCK_alarm);        /* Lock from threads & alarms */
  if (alarm_aborted > 0)
  {					/* No signal thread */
    DBUG_PRINT("info", ("alarm aborted"));
    *alrm= 0;					/* No alarm */
    mysql_mutex_unlock(&LOCK_alarm);
#ifndef USE_ONE_SIGNAL_HAND
    pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
#endif
    DBUG_RETURN(1);
  }
  if (alarm_aborted < 0)
    sec= 1;					/* Abort mode */

  if (alarm_queue.elements >= max_used_alarms)
  {
    if (alarm_queue.elements == alarm_queue.max_elements)
    {
      DBUG_PRINT("info", ("alarm queue full"));
      fprintf(stderr,"Warning: thr_alarm queue is full\n");
      *alrm= 0;					/* No alarm */
      mysql_mutex_unlock(&LOCK_alarm);
#ifndef USE_ONE_SIGNAL_HAND
      pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
#endif
      DBUG_RETURN(1);
    }
    max_used_alarms=alarm_queue.elements+1;
  }
  reschedule= (ulong) next_alarm_expire_time > (ulong) now + sec;
  if (!alarm_data)
  {
    if (!(alarm_data=(ALARM*) my_malloc(sizeof(ALARM),MYF(MY_WME))))
    {
      DBUG_PRINT("info", ("failed my_malloc()"));
      *alrm= 0;					/* No alarm */
      mysql_mutex_unlock(&LOCK_alarm);
#ifndef USE_ONE_SIGNAL_HAND
      pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
#endif
      DBUG_RETURN(1);
    }
    alarm_data->malloced=1;
  }
  else
    alarm_data->malloced=0;
  alarm_data->expire_time=now+sec;
  alarm_data->alarmed=0;
  alarm_data->thread=    current_my_thread_var->pthread_self;
  alarm_data->thread_id= current_my_thread_var->id;
  queue_insert(&alarm_queue,(uchar*) alarm_data);

  /* Reschedule alarm if the current one has more than sec left */
  if (reschedule)
  {
    DBUG_PRINT("info", ("reschedule"));
    if (pthread_equal(pthread_self(),alarm_thread))
    {
      alarm(sec);				/* purecov: inspected */
      next_alarm_expire_time= now + sec;
    }
    else
      reschedule_alarms();			/* Reschedule alarms */
  }
  mysql_mutex_unlock(&LOCK_alarm);
#ifndef USE_ONE_SIGNAL_HAND
  pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
#endif
  (*alrm)= &alarm_data->alarmed;
  DBUG_RETURN(0);
}


/*
  Remove alarm from list of alarms
*/

void thr_end_alarm(thr_alarm_t *alarmed)
{
  ALARM *alarm_data;
#ifndef USE_ONE_SIGNAL_HAND
  sigset_t old_mask;
#endif
  uint i, found=0;
  DBUG_ENTER("thr_end_alarm");

#ifndef USE_ONE_SIGNAL_HAND
  pthread_sigmask(SIG_BLOCK,&full_signal_set,&old_mask);
#endif
  mysql_mutex_lock(&LOCK_alarm);

  alarm_data= (ALARM*) ((uchar*) *alarmed - offsetof(ALARM,alarmed));
  for (i=0 ; i < alarm_queue.elements ; i++)
  {
    if ((ALARM*) queue_element(&alarm_queue,i) == alarm_data)
    {
      queue_remove(&alarm_queue,i),MYF(0);
      if (alarm_data->malloced)
	my_free(alarm_data);
      found++;
#ifdef DBUG_OFF
      break;
#endif
    }
  }
  DBUG_ASSERT(!*alarmed || found == 1);
  if (!found)
  {
    if (*alarmed)
      fprintf(stderr,"Warning: Didn't find alarm 0x%lx in queue of %d alarms\n",
	      (long) *alarmed, alarm_queue.elements);
    DBUG_PRINT("warning",("Didn't find alarm 0x%lx in queue\n",
			  (long) *alarmed));
  }
  mysql_mutex_unlock(&LOCK_alarm);
#ifndef USE_ONE_SIGNAL_HAND
  pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
#endif
  DBUG_VOID_RETURN;
}

/*
  Come here when some alarm in queue is due.
  Mark all alarms with are finnished in list.
  Shedule alarms to be sent again after 1-10 sec (many alarms at once)
  If alarm_aborted is set then all alarms are given and resent
  every second.
*/

sig_handler process_alarm(int sig __attribute__((unused)))
{
  sigset_t old_mask;
/*
  This must be first as we can't call DBUG inside an alarm for a normal thread
*/

  if (thd_lib_detected == THD_LIB_LT &&
      !pthread_equal(pthread_self(),alarm_thread))
  {
#if defined(MAIN) && !defined(__bsdi__)
    printf("thread_alarm in process_alarm\n"); fflush(stdout);
#endif
#ifdef SIGNAL_HANDLER_RESET_ON_DELIVERY
    my_sigset(thr_client_alarm, process_alarm);	/* int. thread system calls */
#endif
    return;
  }

  /*
    We have to do do the handling of the alarm in a sub function,
    because otherwise we would get problems with two threads calling
    DBUG_... functions at the same time (as two threads may call
    process_alarm() at the same time
  */

#ifndef USE_ALARM_THREAD
  pthread_sigmask(SIG_SETMASK,&full_signal_set,&old_mask);
  mysql_mutex_lock(&LOCK_alarm);
#endif
  process_alarm_part2(sig);
#ifndef USE_ALARM_THREAD
#if defined(SIGNAL_HANDLER_RESET_ON_DELIVERY) && !defined(USE_ONE_SIGNAL_HAND)
  my_sigset(THR_SERVER_ALARM,process_alarm);
#endif
  mysql_mutex_unlock(&LOCK_alarm);
  pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
#endif
  return;
}


static sig_handler process_alarm_part2(int sig __attribute__((unused)))
{
  ALARM *alarm_data;
  DBUG_ENTER("process_alarm");
  DBUG_PRINT("info",("sig: %d  active alarms: %d",sig,alarm_queue.elements));

#if defined(MAIN) && !defined(__bsdi__)
  printf("process_alarm\n"); fflush(stdout);
#endif
  if (alarm_queue.elements)
  {
    if (alarm_aborted)
    {
      uint i;
      for (i=0 ; i < alarm_queue.elements ;)
      {
	alarm_data=(ALARM*) queue_element(&alarm_queue,i);
	alarm_data->alarmed=1;			/* Info to thread */
	if (pthread_equal(alarm_data->thread,alarm_thread) ||
	    pthread_kill(alarm_data->thread, thr_client_alarm))
	{
#ifdef MAIN
	  printf("Warning: pthread_kill couldn't find thread!!!\n");
#endif
	  queue_remove(&alarm_queue,i);		/* No thread. Remove alarm */
	}
	else
	  i++;					/* Signal next thread */
      }
#ifndef USE_ALARM_THREAD
      if (alarm_queue.elements)
	alarm(1);				/* Signal soon again */
#endif
    }
    else
    {
      ulong now=(ulong) my_time(0);
      ulong next=now+10-(now%10);
      while ((alarm_data=(ALARM*) queue_top(&alarm_queue))->expire_time <= now)
      {
	alarm_data->alarmed=1;			/* Info to thread */
	DBUG_PRINT("info",("sending signal to waiting thread"));
	if (pthread_equal(alarm_data->thread,alarm_thread) ||
	    pthread_kill(alarm_data->thread, thr_client_alarm))
	{
#ifdef MAIN
	  printf("Warning: pthread_kill couldn't find thread!!!\n");
#endif
	  queue_remove(&alarm_queue,0);		/* No thread. Remove alarm */
	  if (!alarm_queue.elements)
	    break;
	}
	else
	{
	  alarm_data->expire_time=next;
	  queue_replaced(&alarm_queue);
	}
      }
#ifndef USE_ALARM_THREAD
      if (alarm_queue.elements)
      {
#ifdef __bsdi__
	alarm(0);				/* Remove old alarm */
#endif
	alarm((uint) (alarm_data->expire_time-now));
        next_alarm_expire_time= alarm_data->expire_time;
      }
#endif
    }
  }
  else
  {
    /*
      Ensure that next time we call thr_alarm(), we will schedule a new alarm
    */
    next_alarm_expire_time= ~(time_t) 0;
  }
  DBUG_VOID_RETURN;
}


/*
  Schedule all alarms now and optionally free all structures

  SYNPOSIS
    end_thr_alarm()
      free_structures		Set to 1 if we should free memory used for
				the alarm queue.
				When we call this we should KNOW that there
				is no active alarms
  IMPLEMENTATION
    Set alarm_abort to -1 which will change the behavior of alarms as follows:
    - All old alarms will be rescheduled at once
    - All new alarms will be rescheduled to one second
*/

void end_thr_alarm(my_bool free_structures)
{
  DBUG_ENTER("end_thr_alarm");
  if (alarm_aborted != 1)			/* If memory not freed */
  {
    mysql_mutex_lock(&LOCK_alarm);
    DBUG_PRINT("info",("Resheduling %d waiting alarms",alarm_queue.elements));
    alarm_aborted= -1;				/* mark aborted */
    if (alarm_queue.elements || (alarm_thread_running && free_structures))
    {
      if (pthread_equal(pthread_self(),alarm_thread))
	alarm(1);				/* Shut down everything soon */
      else
	reschedule_alarms();
    }
    if (free_structures)
    {
      struct timespec abstime;

      DBUG_ASSERT(!alarm_queue.elements);

      /* Wait until alarm thread dies */
      set_timespec(abstime, 10);		/* Wait up to 10 seconds */
      while (alarm_thread_running)
      {
        int error= mysql_cond_timedwait(&COND_alarm, &LOCK_alarm, &abstime);
	if (error == ETIME || error == ETIMEDOUT)
	  break;				/* Don't wait forever */
      }
      delete_queue(&alarm_queue);
      alarm_aborted= 1;
      mysql_mutex_unlock(&LOCK_alarm);
      if (!alarm_thread_running)              /* Safety */
      {
        mysql_mutex_destroy(&LOCK_alarm);
        mysql_cond_destroy(&COND_alarm);
      }
    }
    else
      mysql_mutex_unlock(&LOCK_alarm);
  }
  DBUG_VOID_RETURN;
}


/*
  Remove another thread from the alarm
*/

void thr_alarm_kill(my_thread_id thread_id)
{
  uint i;
  if (alarm_aborted)
    return;
  mysql_mutex_lock(&LOCK_alarm);
  for (i=0 ; i < alarm_queue.elements ; i++)
  {
    if (((ALARM*) queue_element(&alarm_queue,i))->thread_id == thread_id)
    {
      ALARM *tmp=(ALARM*) queue_remove(&alarm_queue,i);
      tmp->expire_time=0;
      queue_insert(&alarm_queue,(uchar*) tmp);
      reschedule_alarms();
      break;
    }
  }
  mysql_mutex_unlock(&LOCK_alarm);
}


void thr_alarm_info(ALARM_INFO *info)
{
  mysql_mutex_lock(&LOCK_alarm);
  info->next_alarm_time= 0;
  info->max_used_alarms= max_used_alarms;
  if ((info->active_alarms=  alarm_queue.elements))
  {
    ulong now=(ulong) my_time(0);
    long time_diff;
    ALARM *alarm_data= (ALARM*) queue_top(&alarm_queue);
    time_diff= (long) (alarm_data->expire_time - now);
    info->next_alarm_time= (ulong) (time_diff < 0 ? 0 : time_diff);
  }
  mysql_mutex_unlock(&LOCK_alarm);
}

/*
  This is here for thread to get interruptet from read/write/fcntl
  ARGSUSED
*/


static sig_handler thread_alarm(int sig __attribute__((unused)))
{
#ifdef MAIN
  printf("thread_alarm\n"); fflush(stdout);
#endif
#ifdef SIGNAL_HANDLER_RESET_ON_DELIVERY
  my_sigset(sig,thread_alarm);		/* int. thread system calls */
#endif
}


#ifdef HAVE_TIMESPEC_TS_SEC
#define tv_sec ts_sec
#define tv_nsec ts_nsec
#endif

/* set up a alarm thread with uses 'sleep' to sleep between alarms */

#ifdef USE_ALARM_THREAD
static void *alarm_handler(void *arg __attribute__((unused)))
{
  int error;
  struct timespec abstime;
#ifdef MAIN
  puts("Starting alarm thread");
#endif
  my_thread_init();
  alarm_thread_running= 1;
  mysql_mutex_lock(&LOCK_alarm);
  for (;;)
  {
    if (alarm_queue.elements)
    {
      ulong sleep_time,now= my_time(0);
      if (alarm_aborted)
	sleep_time=now+1;
      else
	sleep_time= ((ALARM*) queue_top(&alarm_queue))->expire_time;
      if (sleep_time > now)
      {
	abstime.tv_sec=sleep_time;
	abstime.tv_nsec=0;
        next_alarm_expire_time= sleep_time;
        if ((error= mysql_cond_timedwait(&COND_alarm, &LOCK_alarm, &abstime)) &&
	    error != ETIME && error != ETIMEDOUT)
	{
#ifdef MAIN
	  printf("Got error: %d from ptread_cond_timedwait (errno: %d)\n",
		 error,errno);
#endif
	}
      }
    }
    else if (alarm_aborted == -1)
      break;
    else
    {
      next_alarm_expire_time= ~ (time_t) 0;
      if ((error= mysql_cond_wait(&COND_alarm, &LOCK_alarm)))
      {
#ifdef MAIN
        printf("Got error: %d from ptread_cond_wait (errno: %d)\n",
               error,errno);
#endif
      }
    }
    process_alarm(0);
  }
  bzero((char*) &alarm_thread,sizeof(alarm_thread)); /* For easy debugging */
  alarm_thread_running= 0;
  mysql_cond_signal(&COND_alarm);
  mysql_mutex_unlock(&LOCK_alarm);
  pthread_exit(0);
  return 0;					/* Impossible */
}
#endif /* USE_ALARM_THREAD */

/*****************************************************************************
  thr_alarm for win95
*****************************************************************************/

#else /* __WIN__ */

void thr_alarm_kill(my_thread_id thread_id)
{
  /* Can't do this yet */
}

sig_handler process_alarm(int sig __attribute__((unused)))
{
  /* Can't do this yet */
}


my_bool thr_alarm(thr_alarm_t *alrm, uint sec, ALARM *alarm)
{
  (*alrm)= &alarm->alarmed;
  if (alarm_aborted)
  {
    alarm->alarmed.crono=0;
    return 1;
  }
  if (!(alarm->alarmed.crono=SetTimer((HWND) NULL,0, sec*1000,
				      (TIMERPROC) NULL)))
    return 1;
  return 0;
}


my_bool thr_got_alarm(thr_alarm_t *alrm_ptr)
{
  thr_alarm_t alrm= *alrm_ptr;
  MSG msg;
  if (alrm->crono)
  {
    PeekMessage(&msg,NULL,WM_TIMER,WM_TIMER,PM_REMOVE) ;
    if (msg.message == WM_TIMER || alarm_aborted)
    {
      KillTimer(NULL, alrm->crono);
      alrm->crono = 0;
    }
  }
  return !alrm->crono || alarm_aborted;
}


void thr_end_alarm(thr_alarm_t *alrm_ptr)
{
  thr_alarm_t alrm= *alrm_ptr;
  /* alrm may be zero if thr_alarm aborted with an error */
  if (alrm && alrm->crono)

  {
    KillTimer(NULL, alrm->crono);
    alrm->crono = 0;
  }
}

void end_thr_alarm(my_bool free_structures)
{
  DBUG_ENTER("end_thr_alarm");
  alarm_aborted=1;				/* No more alarms */
  DBUG_VOID_RETURN;
}

void init_thr_alarm(uint max_alarm)
{
  DBUG_ENTER("init_thr_alarm");
  alarm_aborted=0;				/* Yes, Gimmie alarms */
  DBUG_VOID_RETURN;
}

void thr_alarm_info(ALARM_INFO *info)
{
  bzero((char*) info, sizeof(*info));
}

void resize_thr_alarm(uint max_alarms)
{
}

#endif /* __WIN__ */

#endif

/****************************************************************************
  Handling of test case (when compiled with -DMAIN)
***************************************************************************/

#ifdef MAIN
#if !defined(DONT_USE_THR_ALARM)

static mysql_cond_t COND_thread_count;
static mysql_mutex_t LOCK_thread_count;
static uint thread_count;

#ifdef HPUX10
typedef int * fd_set_ptr;
#else
typedef fd_set * fd_set_ptr;
#endif /* HPUX10 */

static void *test_thread(void *arg)
{
  int i,param=*((int*) arg),wait_time,retry;
  time_t start_time;
  thr_alarm_t got_alarm;
  fd_set fd;
  FD_ZERO(&fd);
  my_thread_init();
  printf("Thread %d (%s) started\n",param,my_thread_name()); fflush(stdout);
  for (i=1 ; i <= 10 ; i++)
  {
    wait_time=param ? 11-i : i;
    start_time= my_time(0);
    if (thr_alarm(&got_alarm,wait_time,0))
    {
      printf("Thread: %s  Alarms aborted\n",my_thread_name());
      break;
    }
    if (wait_time == 3)
    {
      printf("Thread: %s  Simulation of no alarm needed\n",my_thread_name());
      fflush(stdout);
    }
    else
    {
      for (retry=0 ; !thr_got_alarm(&got_alarm) && retry < 10 ; retry++)
      {
	printf("Thread: %s  Waiting %d sec\n",my_thread_name(),wait_time);
	select(0,(fd_set_ptr) &fd,0,0,0);
      }
      if (!thr_got_alarm(&got_alarm))
      {
	printf("Thread: %s  didn't get an alarm. Aborting!\n",
	       my_thread_name());
	break;
      }
      if (wait_time == 7)
      {						/* Simulate alarm-miss */
	fd_set readFDs;
	uint max_connection=fileno(stdin);
	FD_ZERO(&readFDs);
	FD_SET(max_connection,&readFDs);
	retry=0;
	for (;;)
	{
	  printf("Thread: %s  Simulating alarm miss\n",my_thread_name());
	  fflush(stdout);
	  if (select(max_connection+1, (fd_set_ptr) &readFDs,0,0,0) < 0)
	  {
	    if (errno == EINTR)
	      break;				/* Got new interrupt */
	    printf("Got errno: %d from select.  Retrying..\n",errno);
	    if (retry++ >= 3)
	    {
	      printf("Warning:  Interrupt of select() doesn't set errno!\n");
	      break;
	    }
	  }
	  else					/* This shouldn't happen */
	  {
	    if (!FD_ISSET(max_connection,&readFDs))
	    {
	      printf("Select interrupted, but errno not set\n");
	      fflush(stdout);
	      if (retry++ >= 3)
		break;
	      continue;
	    }
	    (void) getchar();			/* Somebody was playing */
	  }
	}
      }
    }
    printf("Thread: %s  Slept for %d (%d) sec\n",my_thread_name(),
	   (int) (my_time(0)-start_time), wait_time); fflush(stdout);
    thr_end_alarm(&got_alarm);
    fflush(stdout);
  }
  mysql_mutex_lock(&LOCK_thread_count);
  thread_count--;
  mysql_cond_signal(&COND_thread_count); /* Tell main we are ready */
  mysql_mutex_unlock(&LOCK_thread_count);
  free((uchar*) arg);
  return 0;
}

#ifdef USE_ONE_SIGNAL_HAND
static sig_handler print_signal_warning(int sig)
{
  printf("Warning: Got signal %d from thread %s\n",sig,my_thread_name());
  fflush(stdout);
#ifdef SIGNAL_HANDLER_RESET_ON_DELIVERY
  my_sigset(sig,print_signal_warning);		/* int. thread system calls */
#endif
  if (sig == SIGALRM)
    alarm(2);					/* reschedule alarm */
}
#endif /* USE_ONE_SIGNAL_HAND */


static void *signal_hand(void *arg __attribute__((unused)))
{
  sigset_t set;
  int sig,error,err_count=0;;

  my_thread_init();
  pthread_detach_this_thread();
  init_thr_alarm(10);				/* Setup alarm handler */
  mysql_mutex_lock(&LOCK_thread_count);         /* Required by bsdi */
  mysql_cond_signal(&COND_thread_count);        /* Tell main we are ready */
  mysql_mutex_unlock(&LOCK_thread_count);

  sigemptyset(&set);				/* Catch all signals */
  sigaddset(&set,SIGINT);
  sigaddset(&set,SIGQUIT);
  sigaddset(&set,SIGTERM);
  sigaddset(&set,SIGHUP);
#ifdef SIGTSTP
  sigaddset(&set,SIGTSTP);
#endif
#ifdef USE_ONE_SIGNAL_HAND
  sigaddset(&set,THR_SERVER_ALARM);		/* For alarms */
  puts("Starting signal and alarm handling thread");
#else
  puts("Starting signal handling thread");
#endif
  printf("server alarm: %d  thread alarm: %d\n",
         THR_SERVER_ALARM, thr_client_alarm);
  DBUG_PRINT("info",("Starting signal and alarm handling thread"));
  for(;;)
  {
    while ((error=my_sigwait(&set,&sig)) == EINTR)
      printf("sigwait restarted\n");
    if (error)
    {
      fprintf(stderr,"Got error %d from sigwait\n",error);
      if (err_count++ > 5)
	exit(1);				/* Too many errors in test */
      continue;
    }
#ifdef USE_ONE_SIGNAL_HAND
    if (sig != THR_SERVER_ALARM)
#endif
      printf("Main thread: Got signal %d\n",sig);
    switch (sig) {
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
    case SIGHUP:
      printf("Aborting nicely\n");
      end_thr_alarm(0);
      break;
#ifdef SIGTSTP
    case SIGTSTP:
      printf("Aborting\n");
      exit(1);
      return 0;					/* Keep some compilers happy */
#endif
#ifdef USE_ONE_SIGNAL_HAND
     case THR_SERVER_ALARM:
       process_alarm(sig);
      break;
#endif
    }
  }
}


int main(int argc __attribute__((unused)),char **argv __attribute__((unused)))
{
  pthread_t tid;
  pthread_attr_t thr_attr;
  int i,*param,error;
  sigset_t set;
  ALARM_INFO alarm_info;
  MY_INIT(argv[0]);

  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '#')
  {
    DBUG_PUSH(argv[1]+2);
  }
  mysql_mutex_init(0, &LOCK_thread_count, MY_MUTEX_INIT_FAST);
  mysql_cond_init(0, &COND_thread_count, NULL);

  /* Start a alarm handling thread */
  sigemptyset(&set);
  sigaddset(&set,SIGINT);
  sigaddset(&set,SIGQUIT);
  sigaddset(&set,SIGTERM);
  sigaddset(&set,SIGHUP);
  signal(SIGTERM,SIG_DFL);			/* If it's blocked by parent */
#ifdef SIGTSTP
  sigaddset(&set,SIGTSTP);
#endif
  sigaddset(&set,THR_SERVER_ALARM);
  sigdelset(&set, thr_client_alarm);
  (void) pthread_sigmask(SIG_SETMASK,&set,NULL);

  pthread_attr_init(&thr_attr);
  pthread_attr_setscope(&thr_attr,PTHREAD_SCOPE_PROCESS);
  pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED);
  pthread_attr_setstacksize(&thr_attr,65536L);

  /* Start signal thread and wait for it to start */
  mysql_mutex_lock(&LOCK_thread_count);
  mysql_thread_create(0,
                      &tid, &thr_attr, signal_hand, NULL);
  mysql_cond_wait(&COND_thread_count, &LOCK_thread_count);
  mysql_mutex_unlock(&LOCK_thread_count);
  DBUG_PRINT("info",("signal thread created"));

  thr_setconcurrency(3);
  pthread_attr_setscope(&thr_attr,PTHREAD_SCOPE_PROCESS);
  printf("Main thread: %s\n",my_thread_name());
  for (i=0 ; i < 2 ; i++)
  {
    param=(int*) malloc(sizeof(int));
    *param= i;
    mysql_mutex_lock(&LOCK_thread_count);
    if ((error= mysql_thread_create(0,
                                    &tid, &thr_attr, test_thread,
                                    (void*) param)))
    {
      printf("Can't create thread %d, error: %d\n",i,error);
      exit(1);
    }
    thread_count++;
    mysql_mutex_unlock(&LOCK_thread_count);
  }

  pthread_attr_destroy(&thr_attr);
  mysql_mutex_lock(&LOCK_thread_count);
  thr_alarm_info(&alarm_info);
  printf("Main_thread:  Alarms: %u  max_alarms: %u  next_alarm_time: %lu\n",
	 alarm_info.active_alarms, alarm_info.max_used_alarms,
	 alarm_info.next_alarm_time);
  while (thread_count)
  {
    mysql_cond_wait(&COND_thread_count, &LOCK_thread_count);
    if (thread_count == 1)
    {
      printf("Calling end_thr_alarm. This should cancel the last thread\n");
      end_thr_alarm(0);
    }
  }
  mysql_mutex_unlock(&LOCK_thread_count);
  thr_alarm_info(&alarm_info);
  end_thr_alarm(1);
  printf("Main_thread:  Alarms: %u  max_alarms: %u  next_alarm_time: %lu\n",
	 alarm_info.active_alarms, alarm_info.max_used_alarms,
	 alarm_info.next_alarm_time);
  printf("Test succeeded\n");
  return 0;
}

#else /* !defined(DONT_USE_ALARM_THREAD) */

int main(int argc __attribute__((unused)),char **argv __attribute__((unused)))
{
  printf("thr_alarm disabled with DONT_USE_THR_ALARM\n");
  exit(1);
}

#endif /* !defined(DONT_USE_ALARM_THREAD) */
#endif /* MAIN */
