/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

#include <global.h>

#if defined(THREAD) && !defined(DONT_USE_THR_ALARM)
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

static my_bool alarm_aborted=1;
my_bool thr_alarm_inited=0;

#if !defined(__WIN__) && !defined(__EMX__)

static pthread_mutex_t LOCK_alarm;
static sigset_t full_signal_set;
static QUEUE alarm_queue;
pthread_t alarm_thread;

#ifdef USE_ALARM_THREAD
static pthread_cond_t COND_alarm;
static void *alarm_handler(void *arg);
#define reschedule_alarms() pthread_cond_signal(&COND_alarm)
#else
#define reschedule_alarms() pthread_kill(alarm_thread,THR_SERVER_ALARM)
#endif

#if THR_CLIENT_ALARM != SIGALRM || defined(USE_ALARM_THREAD)
static sig_handler thread_alarm(int sig __attribute__((unused)));
#endif

static int compare_ulong(void *not_used __attribute__((unused)),
			 byte *a_ptr,byte* b_ptr)
{
  ulong a=*((ulong*) a_ptr),b= *((ulong*) b_ptr);
  return (a < b) ? -1  : (a == b) ? 0 : 1;
}

void init_thr_alarm(uint max_alarms)
{
  sigset_t s;
  DBUG_ENTER("init_thr_alarm");
  alarm_aborted=0;
  init_queue(&alarm_queue,max_alarms+1,offsetof(ALARM,expire_time),0,
	     compare_ulong,NullS);
  sigfillset(&full_signal_set);			/* Neaded to block signals */
  pthread_mutex_init(&LOCK_alarm,MY_MUTEX_INIT_FAST);
#if THR_CLIENT_ALARM != SIGALRM || defined(USE_ALARM_THREAD)
#if defined(HAVE_mit_thread)
  sigset(THR_CLIENT_ALARM,thread_alarm);	/* int. thread system calls */
#else
  {
    struct sigaction sact;
    sact.sa_flags = 0;
    sact.sa_handler = thread_alarm;
    sigaction(THR_CLIENT_ALARM, &sact, (struct sigaction*) 0);
  }
#endif
#endif
  sigemptyset(&s);
  sigaddset(&s, THR_SERVER_ALARM);
  alarm_thread=pthread_self();
#if defined(USE_ALARM_THREAD)
  {
    pthread_attr_t thr_attr;
    pthread_attr_init(&thr_attr);
    pthread_cond_init(&COND_alarm,NULL);
    pthread_attr_setscope(&thr_attr,PTHREAD_SCOPE_PROCESS);
    pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&thr_attr,8196);

    my_pthread_attr_setprio(&thr_attr,100);	/* Very high priority */
    VOID(pthread_create(&alarm_thread,&thr_attr,alarm_handler,NULL));
    VOID(pthread_attr_destroy(&thr_attr));
  }
#elif defined(USE_ONE_SIGNAL_HAND)
  pthread_sigmask(SIG_BLOCK, &s, NULL);		/* used with sigwait() */
#if THR_SERVER_ALARM == THR_CLIENT_ALARM
  sigset(THR_CLIENT_ALARM,process_alarm);	/* Linuxthreads */
  pthread_sigmask(SIG_UNBLOCK, &s, NULL);
#endif
#else
  pthread_sigmask(SIG_UNBLOCK, &s, NULL);
  sigset(THR_SERVER_ALARM,process_alarm);
#endif
  DBUG_VOID_RETURN;
}

/*
** Request alarm after sec seconds.
** A pointer is returned with points to a non-zero int when the alarm has been
** given. This can't be called from the alarm-handling thread.
** Returns 0 if no more alarms are allowed (aborted by process)
*/

bool thr_alarm(thr_alarm_t *alrm, uint sec, ALARM *alarm_data)
{
  ulong now;
  sigset_t old_mask;
  my_bool reschedule;
  DBUG_ENTER("thr_alarm");
  DBUG_PRINT("enter",("thread: %s  sec: %d",my_thread_name(),sec));

  now=(ulong) time((time_t*) 0);
  pthread_sigmask(SIG_BLOCK,&full_signal_set,&old_mask);
  pthread_mutex_lock(&LOCK_alarm);	/* Lock from threads & alarms */
  if (alarm_aborted)
  {					/* No signal thread */
    DBUG_PRINT("info", ("alarm aborted"));
    pthread_mutex_unlock(&LOCK_alarm);
    pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
    DBUG_RETURN(1);
  }
  if (alarm_queue.elements == alarm_queue.max_elements)
  {
    DBUG_PRINT("info", ("alarm queue full"));
    fprintf(stderr,"Warning: thr_alarm queue is full\n");
    pthread_mutex_unlock(&LOCK_alarm);
    pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
    DBUG_RETURN(1);
  }
  reschedule= (!alarm_queue.elements ||
	      (int) (((ALARM*) queue_top(&alarm_queue))->expire_time - now) >
	      (int) sec);
  if (!alarm_data)
  {
    if (!(alarm_data=(ALARM*) my_malloc(sizeof(ALARM),MYF(MY_WME))))
    {
      DBUG_PRINT("info", ("failed my_malloc()"));
      pthread_mutex_unlock(&LOCK_alarm);
      pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
      DBUG_RETURN(1);
    }
    alarm_data->malloced=1;
  }
  else
    alarm_data->malloced=0;
  alarm_data->expire_time=now+sec;
  alarm_data->alarmed=0;
  alarm_data->thread=pthread_self();
  queue_insert(&alarm_queue,(byte*) alarm_data);

  /* Reschedule alarm if the current one has more than sec left */
  if (reschedule)
  {
    DBUG_PRINT("info", ("reschedule"));
    if (pthread_equal(pthread_self(),alarm_thread))
      alarm(sec);				/* purecov: inspected */
    else
      reschedule_alarms();			/* Reschedule alarms */
  }
  pthread_mutex_unlock(&LOCK_alarm);
  pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
  (*alrm)= &alarm_data->alarmed;
  DBUG_RETURN(0);
}


/*
** Remove alarm from list of alarms
*/

void thr_end_alarm(thr_alarm_t *alarmed)
{
  ALARM *alarm_data;
  sigset_t old_mask;
  uint i;
  bool found=0;
  DBUG_ENTER("thr_end_alarm");

  pthread_sigmask(SIG_BLOCK,&full_signal_set,&old_mask);
  pthread_mutex_lock(&LOCK_alarm);

  alarm_data= (ALARM*) ((byte*) *alarmed - offsetof(ALARM,alarmed));
  for (i=0 ; i < alarm_queue.elements ; i++)
  {
    if ((ALARM*) queue_element(&alarm_queue,i) == alarm_data)
    {
      queue_remove(&alarm_queue,i),MYF(0);
      if (alarm_data->malloced)
	my_free((gptr) alarm_data,MYF(0));
      found=1;
      break;
    }
  }
  if (!found)
  {
#ifdef MAIN
    printf("Warning: Didn't find alarm %lx in queue of %d alarms\n",
	   (long) *alarmed, alarm_queue.elements);
#endif
    DBUG_PRINT("warning",("Didn't find alarm %lx in queue\n",*alarmed));
  }
  if (alarm_aborted && !alarm_queue.elements)
    delete_queue(&alarm_queue);
  pthread_mutex_unlock(&LOCK_alarm);
  pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
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
  ALARM *alarm_data;
  DBUG_ENTER("process_alarm");
  DBUG_PRINT("info",("sig: %d active alarms: %d",sig,alarm_queue.elements));

#if THR_SERVER_ALARM == THR_CLIENT_ALARM
  if (!pthread_equal(pthread_self(),alarm_thread))
  {
#if defined(MAIN) && !defined(__bsdi__)
    printf("thread_alarm\n"); fflush(stdout);
#endif
#ifdef DONT_REMEMBER_SIGNAL
    sigset(THR_CLIENT_ALARM,process_alarm);	/* int. thread system calls */
#endif
    DBUG_VOID_RETURN;
  }
#endif

#if defined(MAIN) && !defined(__bsdi__)
  printf("process_alarm\n"); fflush(stdout);
#endif
#ifndef USE_ALARM_THREAD
  pthread_sigmask(SIG_SETMASK,&full_signal_set,&old_mask);
  pthread_mutex_lock(&LOCK_alarm);
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
	    pthread_kill(alarm_data->thread, THR_CLIENT_ALARM))
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
      ulong now=(ulong) time((time_t*) 0);
      ulong next=now+10-(now%10);
      while ((alarm_data=(ALARM*) queue_top(&alarm_queue))->expire_time <= now)
      {
	alarm_data->alarmed=1;			/* Info to thread */
	DBUG_PRINT("info",("sending signal to waiting thread"));
	if (pthread_equal(alarm_data->thread,alarm_thread) ||
	    pthread_kill(alarm_data->thread, THR_CLIENT_ALARM))
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
      }
#endif
    }
  }
#ifndef USE_ALARM_THREAD
#if defined(DONT_REMEMBER_SIGNAL) && !defined(USE_ONE_SIGNAL_HAND)
  sigset(THR_SERVER_ALARM,process_alarm);
#endif
  pthread_mutex_unlock(&LOCK_alarm);
  pthread_sigmask(SIG_SETMASK,&old_mask,NULL);
#endif
  DBUG_VOID_RETURN;
}


/*
** Shedule all alarms now.
** When all alarms are given, Free alarm memory and don't allow more alarms.
*/

void end_thr_alarm(void)
{
  DBUG_ENTER("end_thr_alarm");
  pthread_mutex_lock(&LOCK_alarm);
  if (!alarm_aborted)
  {
    DBUG_PRINT("info",("Resheduling %d waiting alarms",alarm_queue.elements));
    alarm_aborted=1;				/* mark aborted */
    if (!alarm_queue.elements)
      delete_queue(&alarm_queue);
    if (pthread_equal(pthread_self(),alarm_thread))
      alarm(1);					/* Shut down everything soon */
    else
      reschedule_alarms();
  }
  pthread_mutex_unlock(&LOCK_alarm);
  DBUG_VOID_RETURN;
}


/*
** Remove another thread from the alarm
*/

void thr_alarm_kill(pthread_t thread_id)
{
  uint i;
  pthread_mutex_lock(&LOCK_alarm);
  for (i=0 ; i < alarm_queue.elements ; i++)
  {
    if (pthread_equal(((ALARM*) queue_element(&alarm_queue,i))->thread,
		      thread_id))
    {
      ALARM *tmp=(ALARM*) queue_remove(&alarm_queue,i);
      tmp->expire_time=0;
      queue_insert(&alarm_queue,(byte*) tmp);
      reschedule_alarms();
      break;
    }
  }
  pthread_mutex_unlock(&LOCK_alarm);
}


/*
**  This is here for thread to get interruptet from read/write/fcntl
**  ARGSUSED
*/

#if THR_CLIENT_ALARM != SIGALRM || defined(USE_ALARM_THREAD)
static sig_handler thread_alarm(int sig)
{
#ifdef MAIN
  printf("thread_alarm\n"); fflush(stdout);
#endif
#ifdef DONT_REMEMBER_SIGNAL
  sigset(sig,thread_alarm);		/* int. thread system calls */
#endif
}
#endif


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
  pthread_mutex_lock(&LOCK_alarm);
  for (;;)
  {
    if (alarm_queue.elements)
    {
      ulong sleep_time,now=time((time_t*) 0);
      if (alarm_aborted)
	sleep_time=now+1;
      else
	sleep_time= ((ALARM*) queue_top(&alarm_queue))->expire_time;
      if (sleep_time > now)
      {
	abstime.tv_sec=sleep_time;
	abstime.tv_nsec=0;
	if ((error=pthread_cond_timedwait(&COND_alarm,&LOCK_alarm,&abstime)) &&
	    error != ETIME && error != ETIMEDOUT)
	{
#ifdef MAIN
	  printf("Got error: %d from ptread_cond_timedwait (errno: %d)\n",
		 error,errno);
#endif
	}
      }
    }
    else if (alarm_aborted)
      break;
    else if ((error=pthread_cond_wait(&COND_alarm,&LOCK_alarm)))
    {
#ifdef MAIN
      printf("Got error: %d from ptread_cond_wait (errno: %d)\n",
	     error,errno);
#endif
    }
    process_alarm(0);
  }
  bzero((char*) &alarm_thread,sizeof(alarm_thread)); /* For easy debugging */
  pthread_mutex_unlock(&LOCK_alarm);
  pthread_exit(0);
  return 0;					/* Impossible */
}
#endif /* USE_ALARM_THREAD */

/*****************************************************************************
**  thr_alarm for OS/2
*****************************************************************************/

#elif defined(__EMX__)

#define INCL_BASE
#define INCL_NOPMAPI
#include <os2.h>

static pthread_mutex_t LOCK_alarm;
static sigset_t full_signal_set;
static QUEUE alarm_queue;
pthread_t alarm_thread;

#ifdef USE_ALARM_THREAD
static pthread_cond_t COND_alarm;
static void *alarm_handler(void *arg);
#define reschedule_alarms() pthread_cond_signal(&COND_alarm)
#else
#define reschedule_alarms() pthread_kill(alarm_thread,THR_SERVER_ALARM)
#endif

sig_handler process_alarm(int sig __attribute__((unused)))
{
  sigset_t old_mask;
  ALARM *alarm_data;
  DBUG_PRINT("info",("sig: %d active alarms: %d",sig,alarm_queue.elements));
}


/*
** Remove another thread from the alarm
*/

void thr_alarm_kill(pthread_t thread_id)
{
  uint i;

  pthread_mutex_lock(&LOCK_alarm);
  for (i=0 ; i < alarm_queue.elements ; i++)
  {
    if (pthread_equal(((ALARM*) queue_element(&alarm_queue,i))->thread,
		      thread_id))
    {
      ALARM *tmp=(ALARM*) queue_remove(&alarm_queue,i);
      tmp->expire_time=0;
      queue_insert(&alarm_queue,(byte*) tmp);
      reschedule_alarms();
      break;
    }
  }
  pthread_mutex_unlock(&LOCK_alarm);
}

bool thr_alarm(thr_alarm_t *alrm, uint sec, ALARM *alarm)
{
  APIRET rc;
  if (alarm_aborted)
  {
    alarm->alarmed.crono=0;
    alarm->alarmed.event=0;
    return 1;
  }
  if (rc = DosCreateEventSem(NULL,(HEV *) &alarm->alarmed.event,
			     DC_SEM_SHARED,FALSE))
  {
    printf("Error creating event semaphore! [%d] \n",rc);
    alarm->alarmed.crono=0;
    alarm->alarmed.event=0;
    return 1;
  }
  if (rc = DosAsyncTimer((long) sec*1000L, (HSEM) alarm->alarmed.event,
			 (HTIMER *) &alarm->alarmed.crono))
  {
    printf("Error starting async timer! [%d] \n",rc);
    DosCloseEventSem((HEV) alarm->alarmed.event);
    alarm->alarmed.crono=0;
    alarm->alarmed.event=0;
    return 1;
  } /* endif */
  (*alrm)= &alarm->alarmed;
  return 1;
}


bool thr_got_alarm(thr_alarm_t *alrm_ptr)
{
  thr_alarm_t alrm= *alrm_ptr;
  APIRET rc;

  if (alrm->crono)
  {
    rc = DosWaitEventSem((HEV) alrm->event, SEM_IMMEDIATE_RETURN);
    if (rc == 0) {
      DosCloseEventSem((HEV) alrm->event);
      alrm->crono = 0;
      alrm->event = 0;
    } /* endif */
  }
  return !alrm->crono || alarm_aborted;
}


void thr_end_alarm(thr_alarm_t *alrm_ptr)
{
  thr_alarm_t alrm= *alrm_ptr;
  if (alrm->crono)
  {
    DosStopTimer((HTIMER) alrm->crono);
    DosCloseEventSem((HEV) alrm->event);
    alrm->crono = 0;
    alrm->event = 0;
  }
}

void end_thr_alarm(void)
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

/*****************************************************************************
**  thr_alarm for win95
*****************************************************************************/

#else /* __WIN__ */

void thr_alarm_kill(pthread_t thread_id)
{
  /* Can't do this yet */
}

sig_handler process_alarm(int sig __attribute__((unused)))
{
  /* Can't do this yet */
}


bool thr_alarm(thr_alarm_t *alrm, uint sec, ALARM *alarm)
{
  if (alarm_aborted)
  {
    alarm->alarmed.crono=0;
    return 1;
  }
  if (!(alarm->alarmed.crono=SetTimer((HWND) NULL,0, sec*1000,
				      (TIMERPROC) NULL)))
    return 1;
  (*alrm)= &alarm->alarmed;
  return 0;
}


bool thr_got_alarm(thr_alarm_t *alrm_ptr)
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
  if (alrm->crono)
  {
    KillTimer(NULL, alrm->crono);
    alrm->crono = 0;
  }
}

void end_thr_alarm(void)
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

#endif /* __WIN__ */

#endif /* THREAD */


/****************************************************************************
** Handling of MAIN
***************************************************************************/

#ifdef MAIN
#if defined(THREAD) && !defined(DONT_USE_THR_ALARM)

static pthread_cond_t COND_thread_count;
static pthread_mutex_t LOCK_thread_count;
static uint thread_count;

#ifdef HPUX
typedef int * fd_set_ptr;
#else
typedef fd_set * fd_set_ptr;
#endif /* HPUX */

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
    start_time=time((time_t*) 0);
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
	    VOID(getchar());			/* Somebody was playing */
	  }
	}
      }
    }
    printf("Thread: %s  Slept for %d (%d) sec\n",my_thread_name(),
	   (int) (time((time_t*) 0)-start_time), wait_time); fflush(stdout);
    thr_end_alarm(&got_alarm);
    fflush(stdout);
  }
  pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  VOID(pthread_cond_signal(&COND_thread_count)); /* Tell main we are ready */
  pthread_mutex_unlock(&LOCK_thread_count);
  free((gptr) arg);
  return 0;
}

#ifdef USE_ONE_SIGNAL_HAND
static sig_handler print_signal_warning(int sig)
{
  printf("Warning: Got signal %d from thread %s\n",sig,my_thread_name());
  fflush(stdout);
#ifdef DONT_REMEMBER_SIGNAL
  sigset(sig,print_signal_warning);		/* int. thread system calls */
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
  pthread_mutex_lock(&LOCK_thread_count);	/* Required by bsdi */
  VOID(pthread_cond_signal(&COND_thread_count)); /* Tell main we are ready */
  pthread_mutex_unlock(&LOCK_thread_count);

  sigemptyset(&set);				/* Catch all signals */
  sigaddset(&set,SIGINT);
  sigaddset(&set,SIGQUIT);
  sigaddset(&set,SIGTERM);
#if THR_CLIENT_ALARM != SIGHUP
  sigaddset(&set,SIGHUP);
#endif
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
	 THR_SERVER_ALARM,THR_CLIENT_ALARM);
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
      end_thr_alarm();
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
  MY_INIT(argv[0]);

  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '#')
    DBUG_PUSH(argv[1]+2);

  pthread_mutex_init(&LOCK_thread_count,MY_MUTEX_INIT_FAST);
  pthread_cond_init(&COND_thread_count,NULL);

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
  sigdelset(&set,THR_CLIENT_ALARM);
  (void) pthread_sigmask(SIG_SETMASK,&set,NULL);
#ifdef NOT_USED
  sigemptyset(&set);
  sigaddset(&set,THR_CLIENT_ALARM);
  VOID(pthread_sigmask(SIG_UNBLOCK, &set, (sigset_t*) 0));
#endif

  pthread_attr_init(&thr_attr);
  pthread_attr_setscope(&thr_attr,PTHREAD_SCOPE_PROCESS);
  pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED);
  pthread_attr_setstacksize(&thr_attr,65536L);

  /* Start signal thread and wait for it to start */
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  pthread_create(&tid,&thr_attr,signal_hand,NULL);
  VOID(pthread_cond_wait(&COND_thread_count,&LOCK_thread_count));
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  DBUG_PRINT("info",("signal thread created"));

  thr_setconcurrency(3);
  pthread_attr_setscope(&thr_attr,PTHREAD_SCOPE_PROCESS);
  printf("Main thread: %s\n",my_thread_name());
  for (i=0 ; i < 2 ; i++)
  {
    param=(int*) malloc(sizeof(int));
    *param= i;
    pthread_mutex_lock(&LOCK_thread_count);
    if ((error=pthread_create(&tid,&thr_attr,test_thread,(void*) param)))
    {
      printf("Can't create thread %d, error: %d\n",i,error);
      exit(1);
    }
    thread_count++;
    pthread_mutex_unlock(&LOCK_thread_count);
  }

  pthread_attr_destroy(&thr_attr);
  pthread_mutex_lock(&LOCK_thread_count);
  while (thread_count)
  {
    VOID(pthread_cond_wait(&COND_thread_count,&LOCK_thread_count));
    if (thread_count == 1)
    {
      printf("Calling end_thr_alarm. This should cancel the last thread\n");
      end_thr_alarm();
    }
  }
  pthread_mutex_unlock(&LOCK_thread_count);
  printf("Test succeeded\n");
  return 0;
}

#else /* THREAD */

int main(int argc __attribute__((unused)),char **argv __attribute__((unused)))
{
#ifndef THREAD
  printf("thr_alarm disabled because we are not using threads\n");
#else
  printf("thr_alarm disabled with DONT_USE_THR_ALARM\n");
#endif
  exit(1);
}

#endif /* THREAD */
#endif /* MAIN */
