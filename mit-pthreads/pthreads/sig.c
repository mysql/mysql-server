/* ==== sig.c =======================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@mit.edu
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
 * Description : All the thread signal functions.
 *
 *  1.32 94/06/12 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

#if defined(M_UNIX)
#define signal(A,B) machdep_sys_signal((A),(B))
#endif

extern void sig_handler_real();

struct pthread * pthread_sigwait;
static sigset_t pending_signals;

struct pthread_sigvec {
	void 							(*vector)();
	sigset_t						mask;
	int								flags;
} pthread_sigvec[SIGMAX];

/* ==========================================================================
 * pthread_sig_register()
 *
 * Assumes the kernel is locked.
 */
int pthread_sig_register(int sig)
{
	struct pthread ** pthread_ptr, * pthread;
	int ret; 

	/*
	 * If we have a siginfo structure and the signal is synchronous then
	 * only deliver the signal to the current thread.
	 */

	/* Check waiting threads for delivery */
	for (pthread_ptr = &pthread_sigwait; (*pthread_ptr);
	  pthread_ptr = &((*pthread_ptr)->next)) {
		if (sigismember((*pthread_ptr)->data.sigwait, sig)) {
			pthread=*pthread_ptr;
			*pthread_ptr=(*pthread_ptr)->next;
			
			pthread_prio_queue_enq(pthread_current_prio_queue, pthread);
			ret = pthread->pthread_priority;
			*(int *)(pthread->ret) = sig;
			pthread->state = PS_RUNNING;

			return(ret);
		}
	}

	/* Check current running thread */
	if (pthread_run) {
		if (!sigismember(&pthread_run->sigmask, sig)) {
			sigaddset(&pthread_run->sigpending, sig);
			pthread_run->sigcount++;
			return(0);
		}
	}

	/* Check any running thread */
	for (pthread = pthread_current_prio_queue->next;
	  pthread; pthread = pthread->next) {
		if (!sigismember(&pthread->sigmask, sig)) {
			sigaddset(&pthread->sigpending, sig);
			pthread->sigcount++;
			return(0);
		}
	}

	/* Check any thread */
	for (pthread = pthread_link_list; pthread; pthread = pthread->pll) {
		if (!sigismember(&pthread->sigmask, sig)) {
			sigaddset(&pthread->sigpending, sig);
			pthread->sigcount++;
			return(0);
		}
	}

	sigaddset(&pending_signals, sig);
	return(0);
}

/* ==========================================================================
 * pthread_sig_default()
 */
void pthread_sig_default(int sig)
{
	sigset_t mask, omask;

	if (pthread_sigvec[sig].vector == SIG_DFL) {
		/* Set the signal handler to default before issueing the kill */
		signal(sig, SIG_DFL);
		kill(getpid(), sig);
		sigemptyset(&mask);
		sigaddset(&mask, sig);
		machdep_sys_sigprocmask(SIG_UNBLOCK, &mask, &omask); 
		signal(sig, sig_handler_real);
	}
}

/* ==========================================================================
 * pthread_sig_process()
 *
 * Assumes the kernel is locked.
 */
void pthread_sig_process()
{
  void (*vector)();
  int i, j;

  for (i = 1; i < SIGMAX; i++) {
    if (sigismember(&(pthread_run->sigpending), i)) {
      if (! sigismember(&(pthread_run->sigmask), i)) {
	sigdelset(&(pthread_run->sigpending), i);
	pthread_run->sigcount--;

	if (pthread_sigvec[i].vector == SIG_IGN) {
	  continue;
	} 
	if (pthread_sigvec[i].vector == SIG_DFL) {
	  pthread_sig_default(i);
	  continue;
	} 

	{
	  sigset_t omask;

	  sigemptyset(&omask);
	  /* Save old mask */
	  for (j = 1; j < SIGMAX; j++) {
	    if (sigismember(&(pthread_run->sigmask), j)) {
	      if (sigismember(&(pthread_sigvec[i].mask), j)) 
		sigaddset(&(pthread_run->sigmask), j);
	      sigaddset(&omask, j);
	    }
	  }
	  /* The signal is masked while handling the signal */
	  sigaddset(&(pthread_run->sigmask), i);

	  /* 
	   * Allow interrupts during a signal, 
	   * but not a change in the vector 
	   */
	  vector = pthread_sigvec[i].vector;
	  if (--pthread_kernel_lock) {
	    PANIC();
	  }
	  vector(i);
	  pthread_run->sighandled=1; /* Mark for select; Monty */
	  pthread_kernel_lock++;

	  memcpy(&(pthread_run->sigmask), &omask, sizeof(omask));
	} 
      }
    }
  }
}

/* ==========================================================================
 * pthread_sigmask()
 *
 * It is unclear wheather this call should be implemented as an atomic
 * operation. The resulting mask could be wrong if in the signal
 * handler the thread calls sigprocmask for any signal other than the
 * signal the handler is dealing with.
 */
int pthread_sigmask(int how, const sigset_t *set, sigset_t * oset)
{
	int i;

	if (oset) {
		sigemptyset(oset);
		for (i = 1; i < SIGMAX; i++) {
			if (sigismember(&(pthread_run->sigmask), i)) {
				sigaddset(oset, i);
			}
		}
	}

	if (set) {
		switch(how) {
		case SIG_BLOCK:
			for (i = 1; i < SIGMAX; i++) {
				if (sigismember(set, i)) {
					sigaddset(&(pthread_run->sigmask), i);
				}
			}
			break;
		case SIG_UNBLOCK:
			pthread_sched_prevent();
			for (i = 1; i < SIGMAX; i++) {
				if (sigismember(set, i)) {
					sigdelset(&(pthread_run->sigmask), i);
					if (sigismember(&pending_signals, i)) {
						sigaddset(&(pthread_run->sigpending), i);
						sigdelset(&pending_signals, i);
						pthread_run->sigcount++;
					}
				}
			}
			pthread_sched_resume();
			break;
		case SIG_SETMASK:
			sigfillset(&(pthread_run->sigmask));
			pthread_sched_prevent();
			for (i = 1; i < SIGMAX; i++) {
				if (! sigismember(set, i)) {
					sigdelset(&(pthread_run->sigmask), i);
					if (sigismember(&pending_signals, i)) {
						sigaddset(&(pthread_run->sigpending), i);
						sigdelset(&pending_signals, i);
						pthread_run->sigcount++;
					}
				}
			}
			pthread_sched_resume();
			break;
		default:
			SET_ERRNO(EINVAL);
			return(NOTOK);
		}
	}
	return(OK);
}

int sigprocmask(int how, const sigset_t *set, sigset_t * oset)
{
	return(pthread_sigmask(how, set, oset));
}

/* ==========================================================================
 * sigwait()
 */
int sigwait(const sigset_t * set, int * sig)
{
	int i;

	/* Check that sig is valid */
	*sig = 0;

	pthread_sched_prevent();
	for (i = 1; i < SIGMAX; i++) {
		if (sigismember(set, i)) {
			/* Check personal signals */
			if (sigismember(&(pthread_run->sigpending), i)) {
				sigdelset(&(pthread_run->sigpending), i);
				pthread_sched_resume();
				*sig = i;
				return(OK);
			}
			/* Check kernel signals */
			if (sigismember(&pending_signals, i)) {
				sigdelset(&pending_signals, i);
				pthread_sched_resume();
				*sig = i;
				return(OK);
			}
		}
	}

	/* No pending signals, wait for one */
	pthread_run->next = pthread_sigwait;
	pthread_sigwait = pthread_run;
	pthread_run->data.sigwait = set;
	pthread_run->ret = sig;

	SET_PF_AT_CANCEL_POINT(pthread_run); /* This is a cancel point */
	pthread_resched_resume(PS_SIGWAIT);
	CLEAR_PF_AT_CANCEL_POINT(pthread_run); /* No longer at cancel point */

	return(OK);
}

/* ==========================================================================
 * raise()
 */
int raise(int sig)
{
	return(pthread_kill(pthread_self(), sig));
}

/* ==========================================================================
 * sigsuspend()
 */
int	sigsuspend(const sigset_t * mask)
{
	int ret_sig, ret;
	sigset_t nm, om;

	sigfillset(&nm);
	for(ret_sig = 1; ret_sig < SIGMAX; ret_sig++) {
		if (sigismember(mask, ret_sig)) {
			sigdelset(&nm, ret_sig);
		}
	}
	pthread_sigmask(SIG_BLOCK, &nm, &om);
	if ((ret = sigwait(&nm, &ret_sig)) == OK) {
		sigemptyset(&nm);
		sigaddset(&nm, ret_sig);
		pthread_kill(pthread_self(), ret_sig);
		pthread_sigmask(SIG_UNBLOCK, &nm, NULL);
		/* There is a race condition here, it's not worth worring about */
		pthread_sigmask(SIG_BLOCK, &nm, NULL);
		SET_ERRNO(EINTR);
		ret = NOTOK;
	}
	pthread_sigmask(SIG_SETMASK, &om, NULL);
	return(ret);
}

/* ==========================================================================
 * pthread_signal()
 */
void (*pthread_signal(int sig, void (*dispatch)(int)))()
{
	void (*odispatch)(int);

	odispatch = pthread_sigvec[sig].vector;
	if ((sig > 0) && (sig < SIGMAX)) {
		pthread_sigvec[sig].vector = dispatch;
		sigemptyset(&(pthread_sigvec[sig].mask));
		pthread_sigvec[sig].flags = 0;
	}
	return(odispatch);
}

/* ==========================================================================
 * pthread_sigprocmask()
 */
int pthread_sigaction(int sig, const struct sigaction * act, 
  struct sigaction * oact)
{
	if ((sig > 0) && (sig < SIGMAX)) {
		if (oact) {
			memcpy(&(oact->sa_mask), &(pthread_sigvec[sig].mask), 
			  sizeof(sigset_t));
			oact->sa_handler = pthread_sigvec[sig].vector;
			oact->sa_flags = pthread_sigvec[sig].flags;
		}
		if (act) {
			memcpy(&(pthread_sigvec[sig].mask), &(act->sa_mask), 
			  sizeof(sigset_t));
			pthread_sigvec[sig].vector = act->sa_handler;
			pthread_sigvec[sig].flags = act->sa_flags;
		}
		return(OK);
	}
	SET_ERRNO(EINVAL);
	return(NOTOK);
}

/*
 * The following here are stolen from BSD because I get mutiply defined
 * symbols between sig.o and posix_sig.o in Sun's libc.a under Sunos 4.1.3.
 * The problem is that sigprocmask() is defined in posix_sig.o, in the same
 * module that a lot of other sigset-primitives are defined, and we have
 * our definition of sigprocmask() here, but use those other primitives.
 */

#undef sigemptyset
#undef sigfillset
#undef sigaddset
#undef sigdelset
#undef sigismember

static const sigset_t __sigemptyset = __SIGEMPTYSET;
int sigemptyset(sigset_t *set)
{
	*set = __sigemptyset;
	return (0);
}

static const sigset_t __sigfillset = __SIGFILLSET;
int sigfillset(sigset_t * set)
{
	*set = __sigfillset;
	return (0);
}

#define _MAXIMUM_SIG NSIG

int sigaddset(sigset_t *set, int signo)
{
	if (signo <= 0 || signo >= _MAXIMUM_SIG) {
		errno = EINVAL;
		return -1;
	}
	__SIGADDSET(set, signo);
	return (0);
}

int sigdelset(sigset_t *set, int signo)
{
	if (signo <= 0 || signo >= _MAXIMUM_SIG) {
		errno = EINVAL;
		return -1;
	}
	__SIGDELSET(set, signo);
	return (0);
}

int sigismember(const sigset_t *set, int signo)
{
	if (signo <= 0 || signo >= _MAXIMUM_SIG) {
		errno = EINVAL;
		return -1;
	}
	return(__SIGISMEMBER(set, signo));
}

