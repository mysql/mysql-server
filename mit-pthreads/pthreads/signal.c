/* ==== signal.c ============================================================
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
 * Description : Queue functions.
 *
 *  1.00 93/07/21 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <config.h>
#include <pthread.h>
#include <signal.h>

/* This will force init.o to get dragged in; if you've got support for
   C++ initialization, that'll cause pthread_init to be called at
   program startup automatically, so the application won't need to
   call it explicitly.  */

extern char __pthread_init_hack;
char *__pthread_init_hack_2 = &__pthread_init_hack;

/*
 * Time which select in fd_kern_wait() will sleep.
 * If there are no threads to run we sleep for an hour or until
 * we get an interrupt or an fd thats awakens. To make sure we
 * don't miss an interrupt this variable gets reset too zero in
 * sig_handler_real().
 */
struct timeval __fd_kern_wait_timeout = { 0, 0 };

/*
 * Global for user-kernel lock, and blocked signals
 */

static sig_atomic_t signum_to_process[SIGMAX + 1] = { 0, };
volatile sig_atomic_t sig_to_process = 0;

/* static volatile	sigset_t sig_to_process; */
static volatile	int	sig_count = 0;

static void sig_handler(int signal);
static void set_thread_timer();
static void __cleanup_after_resume( void );
void sig_prevent(void);
void sig_resume(void);

/* ==========================================================================
 * context_switch()
 *
 * This routine saves the current state of the running thread gets
 * the next thread to run and restores it's state. To allow different
 * processors to work with this routine, I allow the machdep_restore_state()
 * to either return or have it return from machdep_save_state with a value
 * other than 0, this is for implementations which use setjmp/longjmp. 
 */
static void context_switch()
{
	struct pthread **current, *next, *last, **dead;

	if (pthread_run->state == PS_RUNNING) {
		/* Put current thread back on the queue */
		pthread_prio_queue_enq(pthread_current_prio_queue, pthread_run);
	}

	/* save floating point registers if necessary */
	if (!(pthread_run->attr.flags & PTHREAD_NOFLOAT)) {
		machdep_save_float_state(pthread_run);
	}
	/* save state of current thread */
	if (machdep_save_state()) {
		return;
	}

	last = pthread_run;

	/* Poll all fds */
	fd_kern_poll();

context_switch_reschedule:;
	/* Are there any threads to run */
	if (pthread_run = pthread_prio_queue_deq(pthread_current_prio_queue)) {
		/* restore floating point registers if necessary */
		if (!(pthread_run->attr.flags & PTHREAD_NOFLOAT)) {
			machdep_restore_float_state();
		}
		uthread_sigmask = &(pthread_run->sigmask);
      	/* restore state of new current thread */
		machdep_restore_state();
    	return;
   	} 

	/* Are there any threads at all */
	for (next = pthread_link_list; next; next = next->pll) {
		if ((next->state != PS_UNALLOCED) && (next->state != PS_DEAD)) {
			sigset_t sig_to_block, oset;

			sigfillset(&sig_to_block);

			/*
			 * Check sig_to_process before calling fd_kern_wait, to handle
			 * things like zero timeouts to select() which would register
			 * a signal with the sig_handler_fake() call.
			 *
			 * This case should ignore SIGVTALRM
			 */
			machdep_sys_sigprocmask(SIG_BLOCK, &sig_to_block, &oset);
			signum_to_process[SIGVTALRM] = 0;
			if (sig_to_process) {
              	/* Process interrupts */
               	/*
               	 * XXX pthread_run should not be set!
				 * Places where it dumps core should be fixed to 
				 * check for the existance of pthread_run --proven
               	 */
               	sig_handler(0);
           	} else {
				machdep_sys_sigprocmask(SIG_UNBLOCK, &sig_to_block, &oset);
				/*
				 * Do a wait, timeout is set to a hour unless we get an 
				 * intr. before the select in wich case it polls.
				 */
				fd_kern_wait();
				machdep_sys_sigprocmask(SIG_BLOCK, &sig_to_block, &oset);
				/* Check for interrupts, but ignore SIGVTALR */
				signum_to_process[SIGVTALRM] = 0;
				if (sig_to_process) {
					/* Process interrupts */
					sig_handler(0); 
				}
			}
			machdep_sys_sigprocmask(SIG_UNBLOCK, &sig_to_block, &oset); 
			goto context_switch_reschedule;
		}
	}

	/* There are no threads alive. */
	pthread_run = last;
	exit(0);
}

#if !defined(HAVE_SYSCALL_SIGSUSPEND) && defined(HAVE_SYSCALL_SIGPAUSE)

/* ==========================================================================
 * machdep_sys_sigsuspend()
 */ 
int machdep_sys_sigsuspend(sigset_t * set)
{
	return(machdep_sys_sigpause(* set));
}

#endif

/* ==========================================================================
 * sig_handler_pause()
 * 
 * Wait until a signal is sent to the process.
 */
void sig_handler_pause()
{
	sigset_t sig_to_block, sig_to_pause, oset;

	sigfillset(&sig_to_block);
	sigemptyset(&sig_to_pause);
	machdep_sys_sigprocmask(SIG_BLOCK, &sig_to_block, &oset);
/*	if (!(SIG_ANY(sig_to_process))) { */
	if (!sig_to_process) {
		machdep_sys_sigsuspend(&sig_to_pause);
	}
	machdep_sys_sigprocmask(SIG_UNBLOCK, &sig_to_block, &oset);
}

/* ==========================================================================
 * context_switch_done()
 *
 * This routine does all the things that are necessary after a context_switch()
 * calls the machdep_restore_state(). DO NOT put this in the context_switch()
 * routine because sometimes the machdep_restore_state() doesn't return
 * to context_switch() but instead ends up in machdep_thread_start() or
 * some such routine, which will need to call this routine and
 * sig_check_and_resume().
 */
void context_switch_done()
{
	/* sigdelset((sigset_t *)&sig_to_process, SIGVTALRM); */
	signum_to_process[SIGVTALRM] = 0;
	set_thread_timer();
}

/* ==========================================================================
 * set_thread_timer()
 *
 * Assums kernel is locked.
 */
static void set_thread_timer()
{
	static int last_sched_attr = SCHED_RR;

	switch (pthread_run->attr.schedparam_policy) {
	case SCHED_RR:
		machdep_set_thread_timer(&(pthread_run->machdep_data));
		break;
	case SCHED_FIFO:
		if (last_sched_attr != SCHED_FIFO) {
			machdep_unset_thread_timer(NULL);
		}
		break;
	case SCHED_IO:
		if ((last_sched_attr != SCHED_IO) && (!sig_count)) {
			machdep_set_thread_timer(&(pthread_run->machdep_data));
		}
		break;
	default:
		machdep_set_thread_timer(&(pthread_run->machdep_data));
		break;
	} 
    last_sched_attr = pthread_run->attr.schedparam_policy;
}

/* ==========================================================================
 * sigvtalrm()
 */
static inline void sigvtalrm() 
{
	if (sig_count) {
		sigset_t sigall, oset;

		sig_count = 0;

		/* Unblock all signals */
		sigemptyset(&sigall);
		machdep_sys_sigprocmask(SIG_SETMASK, &sigall, &oset); 
	}
	context_switch();
	context_switch_done();
}

/* ==========================================================================
 * sigdefault()
 */
static inline void sigdefault(int sig)
{
	int ret;

	ret = pthread_sig_register(sig);
	if (pthread_run && (ret > pthread_run->pthread_priority)) {
		sigvtalrm();
	}
}

/* ==========================================================================
 * sig_handler_switch()
 */
static inline void sig_handler_switch(int sig)
{
	int ret;

			switch(sig) {
			case 0:
				break;
			case SIGVTALRM:
				sigvtalrm();
				break;
			case SIGALRM:
/*		sigdelset((sigset_t *)&sig_to_process, SIGALRM); */
				signum_to_process[SIGALRM] = 0;
				switch (ret = sleep_wakeup()) {
				default:
					if (pthread_run && (ret > pthread_run->pthread_priority)) {
						sigvtalrm();
					}
				case 0:
					break;
				case NOTOK:
			/* Do the registered action, no threads were sleeping */
                                      /* There is a timing window that gets
                                       * here when no threads are on the
                                       * sleep queue.  This is a quick fix.
                                       * The real problem is possibly related
                                       * to heavy use of condition variables
                                       * with time outs.
                                       * (mevans)
                                       *sigdefault(sig);
                                       */
				  break;
				}
				break;
			case SIGCHLD:
/*		sigdelset((sigset_t *)&sig_to_process, SIGCHLD); */
				signum_to_process[SIGCHLD] = 0;
				switch (ret = wait_wakeup()) {
				default:
					if (pthread_run && (ret > pthread_run->pthread_priority)) {
						sigvtalrm();
					}
				case 0:
					break;
				case NOTOK:
			/* Do the registered action, no threads were waiting */
					sigdefault(sig);
					break;
				} 
				break;

#ifdef SIGINFO
			case SIGINFO:
				pthread_dump_info ();
				/* Then fall through, invoking the application's
		   		signal handler after printing our info out.

		   		I'm not convinced that this is right, but I'm not
		   		100% convinced that it is wrong, and this is how
		   		Chris wants it done...  */
#endif

	default:
		/* Do the registered action */
		if (!sigismember(uthread_sigmask, sig)) {
			/*
			 * If the signal isn't masked by the last running thread and
			 * the signal behavior is default or ignore then we can
			 * execute it immediatly. --proven
			 */
			pthread_sig_default(sig);
		}
		signum_to_process[sig] = 0;
		sigdefault(sig);
		break;
	}

}

/* ==========================================================================
 * sig_handler()
 *
 * Process signal that just came in, plus any pending on the signal mask.
 * All of these must be resolved.
 *
 * Assumes the kernel is locked. 
 */
static void sig_handler(int sig)
{
	if (pthread_kernel_lock != 1) {
		PANIC();
	}

	if (sig) { 
		sig_handler_switch(sig);
	}

	while (sig_to_process) {
		for (sig_to_process = 0, sig = 1; sig <= SIGMAX; sig++) {
			if (signum_to_process[sig]) {
				sig_handler_switch(sig);
			}
		}
	}

		
/*
	if (SIG_ANY(sig_to_process)) {
		for (sig = 1; sig <= SIGMAX; sig++) {
			if (sigismember((sigset_t *)&sig_to_process, sig)) {
				goto sig_handler_top;
			}
		}
	}
*/
}

/* ==========================================================================
 * sig_handler_real()
 * 
 * On a multi-processor this would need to use the test and set instruction
 * otherwise the following will work.
 */
void sig_handler_real(int sig)
{
	/*
	 * Get around systems with BROKEN signal handlers.
	 *
	 * Some systems will reissue SIGCHLD if the handler explicitly
	 * clear the signal pending by either doing a wait() or 
	 * ignoring the signal.
	 */
#if defined BROKEN_SIGNALS
	if (sig == SIGCHLD) {
		sigignore(SIGCHLD);
		signal(SIGCHLD, sig_handler_real);
	}
#endif

	if (pthread_kernel_lock) {
		/* sigaddset((sigset_t *)&sig_to_process, sig); */
		__fd_kern_wait_timeout.tv_sec = 0;
		signum_to_process[sig] = 1;
		sig_to_process = 1;
		return;
	}
	pthread_kernel_lock++;

	sig_count++;
	sig_handler(sig);

	/* Handle any signals the current thread might have just gotten */
	if (pthread_run && pthread_run->sigcount) {
		pthread_sig_process();
	}
	pthread_kernel_lock--;
}

/* ==========================================================================
 * sig_handler_fake()
 */
void sig_handler_fake(int sig)
{
	if (pthread_kernel_lock) {
		/* sigaddset((sigset_t *)&sig_to_process, sig); */
		signum_to_process[sig] = 1;
		sig_to_process = 1;
		return;
	}
	pthread_kernel_lock++;
	sig_handler(sig);
	while (!(--pthread_kernel_lock)) {
		if (sig_to_process) {
		/* if (SIG_ANY(sig_to_process)) { */
			pthread_kernel_lock++;
			sig_handler(0);
		} else {
			break;
		}
	}
}

/* ==========================================================================
 * __pthread_signal_delete(int sig)
 *
 * Assumes the kernel is locked.
 */
void __pthread_signal_delete(int sig)
{
		signum_to_process[sig] = 0;
}

/* ==========================================================================
 * pthread_sched_other_resume()
 *
 * Check if thread to be resumed is of higher priority and if so
 * stop current thread and start new thread.
 */
pthread_sched_other_resume(struct pthread * pthread)
{
	pthread->state = PS_RUNNING;
	pthread_prio_queue_enq(pthread_current_prio_queue, pthread);

	if (pthread->pthread_priority > pthread_run->pthread_priority) {
		if (pthread_kernel_lock == 1) {
			sig_handler(SIGVTALRM);
		}
	}

	__cleanup_after_resume();
}

/* ==========================================================================
 * pthread_resched_resume()
 *
 * This routine assumes that the caller is the current pthread, pthread_run
 * and that it has a lock the kernel thread and it wants to reschedule itself.
 */
void pthread_resched_resume(enum pthread_state state)
{
	pthread_run->state = state;

	/* Since we are about to block this thread, lets see if we are
	 * at a cancel point and if we've been cancelled.
	 * Avoid cancelling dead or unalloced threads.
	 */
	if( ! TEST_PF_RUNNING_TO_CANCEL(pthread_run) &&
		TEST_PTHREAD_IS_CANCELLABLE(pthread_run) &&
		state != PS_DEAD && state != PS_UNALLOCED ) {

		/* Set this flag to avoid recursively calling pthread_exit */
		/* We have to set this flag here because we will unlock the
		 * kernel prior to calling pthread_cancel_internal.
		 */
		SET_PF_RUNNING_TO_CANCEL(pthread_run);

		pthread_run->old_state = state;	/* unlock needs this data */
		pthread_sched_resume();			/* Unlock kernel before cancel */
		pthread_cancel_internal( 1 );	/* free locks and exit */
	}

	sig_handler(SIGVTALRM);

	__cleanup_after_resume();
}

/* ==========================================================================
 * pthread_sched_resume()
 */
void pthread_sched_resume()
{
	__cleanup_after_resume();
}

/*----------------------------------------------------------------------
 * Function:	__cleanup_after_resume
 * Purpose:		cleanup kernel locks after a resume
 * Args:		void
 * Returns:		void
 * Notes:
 *----------------------------------------------------------------------*/
static void
__cleanup_after_resume( void )
{
	/* Only bother if we are truely unlocking the kernel */
	while (!(--pthread_kernel_lock)) {
		/* if (SIG_ANY(sig_to_process)) { */
		if (sig_to_process) {
			pthread_kernel_lock++;
			sig_handler(0);
			continue;
		}
		if (pthread_run && pthread_run->sigcount) {
			pthread_kernel_lock++;
			pthread_sig_process();
			continue;
		}
		break;
	}

	if( pthread_run == NULL )
		return;							/* Must be during init processing */

	/* Test for cancel that should be handled now */

	if( ! TEST_PF_RUNNING_TO_CANCEL(pthread_run) &&
		TEST_PTHREAD_IS_CANCELLABLE(pthread_run) ) {
		/* Kernel is already unlocked */
		pthread_cancel_internal( 1 );	/* free locks and exit */
	}
}

/* ==========================================================================
 * pthread_sched_prevent()
 */
void pthread_sched_prevent(void)
{
	pthread_kernel_lock++;
}

/* ==========================================================================
 * sig_init()
 *
 * SIGVTALRM	(NOT POSIX) needed for thread timeslice timeouts.
 *				Since it's not POSIX I will replace it with a 
 *				virtual timer for threads.
 * SIGALRM		(IS POSIX) so some special handling will be
 * 				necessary to fake SIGALRM signals
 */
#ifndef SIGINFO
#define SIGINFO 0
#endif
void sig_init(void)
{
	static const int signum_to_initialize[] = 
					 { SIGCHLD, SIGALRM, SIGVTALRM, SIGINFO, 0 };
	static const int signum_to_ignore[] = { SIGKILL, SIGSTOP, 0 };
	int i, j;

#if defined(HAVE_SYSCALL_SIGACTION) || defined(HAVE_SYSCALL_KSIGACTION)
	struct sigaction act;

	act.sa_handler = sig_handler_real;
	sigemptyset(&(act.sa_mask));
	act.sa_flags = 0;
#endif

	/* Initialize the important signals */
	for (i = 0; signum_to_initialize[i]; i++) {

#if defined(HAVE_SYSCALL_SIGACTION) || defined(HAVE_SYSCALL_KSIGACTION)
		if (sigaction(signum_to_initialize[i], &act, NULL)) {
#else
		if (signal(signum_to_initialize[i], sig_handler_real)) { 
#endif
			PANIC();
		}
	}

	/* Initialize the rest of the signals */
	for (j = 1; j < SIGMAX; j++) {
		for (i = 0; signum_to_initialize[i]; i++) {
			if (signum_to_initialize[i] == j) {
				goto sig_next;
			}
		}
		/* Because Solaris 2.4 can't deal -- proven */
		for (i = 0; signum_to_ignore[i]; i++) {
			if (signum_to_ignore[i] == j) {
				goto sig_next;
			}
		}
		pthread_signal(j, SIG_DFL);
		
#if defined(HAVE_SYSCALL_SIGACTION) || defined(HAVE_SYSCALL_KSIGACTION)
		sigaction(j, &act, NULL);
#else
		signal(j, sig_handler_real);
#endif

		sig_next:;
	}

#if defined BROKEN_SIGNALS 
	signal(SIGCHLD, sig_handler_real);
#endif

}

