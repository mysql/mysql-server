/* ==== sleep.c ============================================================
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : All the appropriate sleep routines.
 *
 *  1.00 93/12/28 proven
 *      -Started coding this file.
 *
 *	1.36 94/06/04 proven
 *		-Use new timer structure pthread_timer, that uses seconds
 *		-nano seconds. Rewrite all routines completely.
 *
 *	1.38 94/06/13 proven
 *		-switch pthread_timer to timespec
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/compat.h>

struct pthread * pthread_sleep = NULL;

/* ==========================================================================
 * sleep_compare_time()
 */
/* static inline int sleep_compare_time(struct timespec * time1, 
  struct timespec * time2) */
static int sleep_compare_time(struct timespec * time1, struct timespec * time2)
{
	if ((time1->tv_sec < time2->tv_sec) || 
	  ((time1->tv_sec == time2->tv_sec) && (time1->tv_nsec < time2->tv_nsec))) {
		return(-1);
	}
	if ((time1->tv_sec == time2->tv_sec) && (time1->tv_nsec == time2->tv_nsec)){
		return(0);
	}
	return(1);
}

/* ==========================================================================
 * machdep_stop_timer()
 *
 * Returns the time left on the timer.
 */
static struct itimerval timestop = { { 0, 0 }, { 0, 0 } };

void machdep_stop_timer(struct timespec *current)
{
	struct itimerval timenow;

	setitimer(ITIMER_REAL, & timestop, & timenow);
	__pthread_signal_delete(SIGALRM);
	if (current) {
		current->tv_nsec = timenow.it_value.tv_usec * 1000;
		current->tv_sec = timenow.it_value.tv_sec;
	}
}

/* ==========================================================================
 * machdep_start_timer()
 */
int machdep_start_timer(struct timespec *current, struct timespec *wakeup)
{
	struct itimerval timeout;

	timeout.it_value.tv_usec = (wakeup->tv_nsec - current->tv_nsec) / 1000;
	timeout.it_value.tv_sec = wakeup->tv_sec - current->tv_sec;
	timeout.it_interval.tv_usec = 0;
	timeout.it_interval.tv_sec = 0;
	if (timeout.it_value.tv_usec < 0) {
		timeout.it_value.tv_usec += 1000000;
		timeout.it_value.tv_sec--;
	}

	if (((long) timeout.it_value.tv_sec >= 0) &&
	    ((timeout.it_value.tv_usec) || (timeout.it_value.tv_sec))) {
	  if (setitimer(ITIMER_REAL, & timeout, NULL) < 0)
	  {
	    fprintf(stderr,"Got error %d from setitimer with:\n\
	            wakeup:   tv_sec: %ld  tv_nsec: %ld\n\
		    current:  tv_sec: %ld  tv_nsec: %ld\n\
                    argument: tv_sec: %ld  tv_usec: %ld\n",
		    errno,
		    wakeup->tv_sec, wakeup->tv_nsec,
		    current->tv_sec, current->tv_nsec,
		    timeout.it_value.tv_sec, timeout.it_value.tv_usec);
	    PANIC();
	  }
	} else {
		/*
		 * There is no time on the timer.
		 * This shouldn't happen,
		 * but isn't fatal.
		 */
		sig_handler_fake(SIGALRM);
	}
	return(OK);
}

/* ==========================================================================
 * sleep_schedule()
 *
 * Assumes that the current thread is the thread to be scheduled
 * and that the kthread is already locked.
 */
void sleep_schedule(struct timespec *current_time, struct timespec *new_time)
{
	struct pthread * pthread_sleep_current, * pthread_sleep_prev;

	/* Record the new time as the current thread's wakeup time. */
	pthread_run->wakeup_time = *new_time;

	/* any threads? */
	if (pthread_sleep_current = pthread_sleep) {
		if (sleep_compare_time(&(pthread_sleep_current->wakeup_time),
		  new_time) <= 0) {
			/* Don't need to restart timer */
			while (pthread_sleep_current->sll) {

				pthread_sleep_prev =  pthread_sleep_current;
				pthread_sleep_current = pthread_sleep_current->sll;
				
				if (sleep_compare_time(&(pthread_sleep_current->wakeup_time),
				  new_time) > 0) {
					pthread_run->sll = pthread_sleep_current;
					pthread_sleep_prev->sll = pthread_run;
					return;
				}
			} 

			/* No more threads in queue, attach pthread_run to end of list */
			pthread_sleep_current->sll = pthread_run;
			pthread_run->sll = NULL;

		} else {
			/* Start timer and enqueue thread */
			machdep_start_timer(current_time, new_time);
			pthread_run->sll = pthread_sleep_current;
			pthread_sleep = pthread_run;
		}
	} else {
		/* Start timer and enqueue thread */
		machdep_start_timer(current_time, new_time);
		pthread_sleep = pthread_run;
		pthread_run->sll = NULL;
	}
}

/* ==========================================================================
 * sleep_wakeup()
 *
 * This routine is called by the interrupt handler, which has already
 * locked the current kthread. Since all threads on this list are owned
 * by the current kthread, rescheduling won't be a problem.
 */
int sleep_spurious_wakeup = 0;
int sleep_wakeup()
{
	struct pthread *pthread_sleep_next;
	struct timespec current_time;
	int ret = 0;

	if (pthread_sleep == NULL) {
		return(NOTOK);
	} 

	machdep_gettimeofday(&current_time);
    if (sleep_compare_time(&(pthread_sleep->wakeup_time), &current_time) > 0) {
        machdep_start_timer(&current_time, &(pthread_sleep->wakeup_time));
        sleep_spurious_wakeup++;
        return(OK);
    }

	do {
		if (pthread_sleep->pthread_priority > ret) {
			ret = pthread_sleep->pthread_priority;
		}

		/*
		 * Clean up removed thread and start it running again. 
		 *
		 * Note: It is VERY important to remove the thread form the
		 * current queue before putting it on the run queue.
		 * Both queues use pthread_sleep->next, and the thread that points
		 * to pthread_sleep should point to pthread_sleep->next then
		 * pthread_sleep should be put on the run queue.
		 */
		if ((SET_PF_DONE_EVENT(pthread_sleep)) == OK) {
			if (pthread_sleep->queue)
				pthread_queue_remove(pthread_sleep->queue, pthread_sleep);
			pthread_prio_queue_enq(pthread_current_prio_queue, pthread_sleep);
			pthread_sleep->state = PS_RUNNING;
		} 

		pthread_sleep_next = pthread_sleep->sll;
		pthread_sleep->sll = NULL;

		if ((pthread_sleep = pthread_sleep_next) == NULL) {
			/* No more threads on sleep queue */
			return(ret);
		}
	} while (sleep_compare_time(&(pthread_sleep->wakeup_time), &(current_time)) <= 0);
		
	/* Start timer for next time interval */
	machdep_start_timer(&current_time, &(pthread_sleep->wakeup_time));
	return(ret);
}


/* ==========================================================================
 * __sleep()
 */
void __sleep(struct timespec * time_to_sleep)
{
	struct pthread *pthread_sleep_prev;
	struct timespec current_time, wakeup_time;

	pthread_sched_prevent();

	/* Get real time */
	machdep_gettimeofday(&current_time);
	wakeup_time.tv_sec = current_time.tv_sec + time_to_sleep->tv_sec;
	wakeup_time.tv_nsec = current_time.tv_nsec + time_to_sleep->tv_nsec;

	sleep_schedule(&current_time, &wakeup_time);

	/* Reschedule thread */
	SET_PF_WAIT_EVENT(pthread_run);
	SET_PF_AT_CANCEL_POINT(pthread_run); /* This is a cancel point */
	pthread_resched_resume(PS_SLEEP_WAIT);
	CLEAR_PF_AT_CANCEL_POINT(pthread_run); /* No longer at cancel point */
	CLEAR_PF_DONE_EVENT(pthread_run);

	/* Return actual time slept */
	time_to_sleep->tv_sec = pthread_run->wakeup_time.tv_sec;
	time_to_sleep->tv_nsec = pthread_run->wakeup_time.tv_nsec;
}

/* ==========================================================================
 * pthread_nanosleep()
 */
unsigned int pthread_nanosleep(unsigned int nseconds)
{
	struct timespec time_to_sleep;

	if (nseconds) {
		time_to_sleep.tv_nsec = nseconds;
		time_to_sleep.tv_sec = 0;
		__sleep(&time_to_sleep);
		nseconds = time_to_sleep.tv_nsec;
	}
	return(nseconds);
}

/* ==========================================================================
 * usleep()
 */
void usleep(unsigned int useconds)
{
	struct timespec time_to_sleep;

	if (useconds) {
		time_to_sleep.tv_nsec = (useconds % 1000000) * 1000;
		time_to_sleep.tv_sec = useconds / 1000000;
		__sleep(&time_to_sleep);
	}
}

/* ==========================================================================
 * sleep()
 */
unsigned int sleep(unsigned int seconds)
{
	struct timespec time_to_sleep;

	if (seconds) {
		time_to_sleep.tv_sec = seconds;
		time_to_sleep.tv_nsec = 0;
		__sleep(&time_to_sleep);
		seconds = time_to_sleep.tv_sec;
	}
	return(seconds);
}

/* ==========================================================================
 * sleep_cancel()
 *
 * Cannot be called while kernel is locked.
 * Does not wake sleeping thread up, just remove it from the sleep queue.
 */
int sleep_cancel(struct pthread * pthread)
{
  struct timespec current_time, delta_time;
  struct pthread * pthread_last;
  int rval = NOTOK;

  /* Lock sleep queue, Note this may be on a different kthread queue */
  pthread_sched_prevent();

  if (pthread_sleep) {
    if (pthread == pthread_sleep) {
      rval = OK;
      machdep_stop_timer(&delta_time);
      if (pthread_sleep = pthread_sleep->sll) {
	current_time.tv_sec 	= delta_time.tv_sec;
	current_time.tv_nsec 	= delta_time.tv_nsec;
	current_time.tv_sec 	+= pthread_sleep->wakeup_time.tv_sec;
	current_time.tv_nsec 	+= pthread_sleep->wakeup_time.tv_nsec;
	while (current_time.tv_nsec > 1000000000) {
	  current_time.tv_nsec -= 1000000000;
	  current_time.tv_sec++;
	}
	machdep_start_timer(&(current_time), 
			    &(pthread_sleep->wakeup_time));
      }
    } else {
      for (pthread_last = pthread_sleep; pthread_last;
	   pthread_last = pthread_last->sll) {
	if (pthread_last->sll == pthread) {
	  pthread_last->sll = pthread->sll;
	  rval = OK;
	  break;
	}
      }
    }
  }

  pthread_sched_resume();
  pthread->sll = NULL;
  return(rval);
}
