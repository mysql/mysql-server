/* ==== fd_kern.c ============================================================
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
 * Description : Deals with the valid kernel fds.
 *
 *  1.00 93/09/27 proven
 *      -Started coding this file.
 *
 *	1.01 93/11/13 proven
 *		-The functions readv() and writev() added.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include "config.h"
#include <pthread.h>
#include <unistd.h>
#include <sys/compat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread/posix.h>
#include <string.h>

#if defined (HAVE_SYSCALL_SENDTO) && !defined (HAVE_SYSCALL_SEND)

pthread_ssize_t machdep_sys_send (int fd, const void *msg, size_t len,
 int flags)
{
  return machdep_sys_sendto (fd, msg, len, flags,
			     (const struct sockaddr *) 0, 0);
}

#endif

#if defined (HAVE_SYSCALL_RECVFROM) && !defined (HAVE_SYSCALL_RECV)

pthread_ssize_t machdep_sys_recv (int fd, void *buf, size_t len, int flags)
{
  return machdep_sys_recvfrom (fd, buf, len, flags,
			       (struct sockaddr *) 0, (int *) 0);
}

#endif

/* ==========================================================================
 * Check if there is any signal with must be handled.  Added by Monty
 * This could be somewhat system dependent but it should work.
 */

static int fd_check_if_pending_signal(struct pthread *pthread)
{
  int i;
  unsigned long *pending,*mask;
  if (!pthread->sigcount)
    return 0;
  pending= (unsigned long*) &pthread->sigpending;
  mask=    (unsigned long*) &pthread->sigmask;

  for (i=0 ; i < sizeof(pthread->sigpending)/sizeof(unsigned long); i++)
  {
    if (*pending && (*mask ^ (unsigned) ~0L))
      return 1;
    pending++;
    mask++;
  }
  return 0;
}

/* ==========================================================================
 * Variables used by both fd_kern_poll and fd_kern_wait
 */
struct pthread_queue fd_wait_read = PTHREAD_QUEUE_INITIALIZER;
struct pthread_queue fd_wait_write = PTHREAD_QUEUE_INITIALIZER;
struct pthread_queue fd_wait_select = PTHREAD_QUEUE_INITIALIZER;

static struct timeval __fd_kern_poll_timeout = { 0, 0 }; /* Moved by monty */
extern struct timeval __fd_kern_wait_timeout;
extern volatile sig_atomic_t sig_to_process;

/*
 * ==========================================================================
 * Do a select if there is someting to wait for.
 * This is to a combination of the old fd_kern_poll() and fd_kern_wait()
 * Return 1 if nothing to do.
 */

static int fd_kern_select(struct timeval *timeout)
{
  fd_set fd_set_read, fd_set_write, fd_set_except;
  struct pthread *pthread, *deq;
  int count, i;

  if (!fd_wait_read.q_next && !fd_wait_write.q_next && !fd_wait_select.q_next)
    return 1;					/* Nothing to do */

  FD_ZERO(&fd_set_read);
  FD_ZERO(&fd_set_write);
  FD_ZERO(&fd_set_except);
  for (pthread = fd_wait_read.q_next; pthread; pthread = pthread->next)
    FD_SET(pthread->data.fd.fd, &fd_set_read);
  for (pthread = fd_wait_write.q_next; pthread; pthread = pthread->next)
    FD_SET(pthread->data.fd.fd, &fd_set_write);
  for (pthread = fd_wait_select.q_next; pthread; pthread = pthread->next)
  {
    for (i = 0; i < pthread->data.select_data->nfds; i++) {
      if (FD_ISSET(i, &pthread->data.select_data->exceptfds))
	FD_SET(i, &fd_set_except);
      if (FD_ISSET(i, &pthread->data.select_data->writefds))
	FD_SET(i, &fd_set_write);
      if (FD_ISSET(i, &pthread->data.select_data->readfds))
	FD_SET(i, &fd_set_read);
    }
  }

  /* Turn off interrupts for real while we set the timer.  */

  if (timeout == &__fd_kern_wait_timeout)
  {						/* from fd_kern_wait() */
    sigset_t sig_to_block, oset;
    sigfillset(&sig_to_block);
    machdep_sys_sigprocmask(SIG_BLOCK, &sig_to_block, &oset);

    machdep_unset_thread_timer(NULL); 
    __fd_kern_wait_timeout.tv_usec = 0;
    __fd_kern_wait_timeout.tv_sec = (sig_to_process) ? 0 : 3600;

    machdep_sys_sigprocmask(SIG_UNBLOCK, &sig_to_block, &oset);
  }
    /*
     * There is a small but finite chance that an interrupt will
     * occure between the unblock and the select. Because of this
     * sig_handler_real() sets the value of __fd_kern_wait_timeout
     * to zero causing the select to do a poll instead of a wait.
     */

  while ((count = machdep_sys_select(dtablesize, &fd_set_read,
				     &fd_set_write, &fd_set_except,
				     timeout)) < OK)
  {
    if (count == -EINTR)
      return 0;
    PANIC();
  }
	
  for (pthread = fd_wait_read.q_next; pthread; ) {
    if (count && FD_ISSET(pthread->data.fd.fd, &fd_set_read) ||
	fd_check_if_pending_signal(pthread))
    {
      if (FD_ISSET(pthread->data.fd.fd, &fd_set_read))
	count--;
      deq = pthread;
      pthread = pthread->next;
      pthread_queue_remove(&fd_wait_read, deq);
      if (SET_PF_DONE_EVENT(deq) == OK) {
	pthread_prio_queue_enq(pthread_current_prio_queue, deq);
	deq->state = PS_RUNNING;
      }
      continue;
    } 
    pthread = pthread->next;
  }
					
  for (pthread = fd_wait_write.q_next; pthread; ) {
    if (count && FD_ISSET(pthread->data.fd.fd, &fd_set_write) ||
	fd_check_if_pending_signal(pthread))
    {
      if (FD_ISSET(pthread->data.fd.fd, &fd_set_read))
	count--;
      deq = pthread;
      pthread = pthread->next;
      pthread_queue_remove(&fd_wait_write, deq);
      if (SET_PF_DONE_EVENT(deq) == OK) {
	pthread_prio_queue_enq(pthread_current_prio_queue, deq);
	deq->state = PS_RUNNING;
      }
      continue;
    } 
    pthread = pthread->next;
  }

  for (pthread = fd_wait_select.q_next; pthread; )
  {
    int found_one=0;	/* Loop fixed by monty */
    if (count)
    {
      fd_set tmp_readfds, tmp_writefds, tmp_exceptfds;
      memcpy(&tmp_readfds, &pthread->data.select_data->readfds,
	     sizeof(fd_set));
      memcpy(&tmp_writefds, &pthread->data.select_data->writefds,
	     sizeof(fd_set));
      memcpy(&tmp_exceptfds, &pthread->data.select_data->exceptfds,
	     sizeof(fd_set));

      for (i = 0; i < pthread->data.select_data->nfds; i++) {
	if (FD_ISSET(i, &tmp_exceptfds))
	{
	  if (! FD_ISSET(i, &fd_set_except))
	    FD_CLR(i, &tmp_exceptfds);
	  else
	    found_one=1;
	}
	if (FD_ISSET(i, &tmp_writefds))
	{
	  if (! FD_ISSET(i, &fd_set_write))
	    FD_CLR(i, &tmp_writefds);
	  else 
	    found_one=1;
	}
	if (FD_ISSET(i, &tmp_readfds))
	{
	  if (! FD_ISSET(i, &fd_set_read))
	    FD_CLR(i, &tmp_readfds);
	  else
	    found_one=1;
	}
      }
      if (found_one)
      {
	memcpy(&pthread->data.select_data->readfds, &tmp_readfds,
	       sizeof(fd_set));
	memcpy(&pthread->data.select_data->writefds, &tmp_writefds,
	       sizeof(fd_set));
	memcpy(&pthread->data.select_data->exceptfds, &tmp_exceptfds,
	       sizeof(fd_set));
      }
    }
    if (found_one || fd_check_if_pending_signal(pthread))
    {
      deq = pthread;
      pthread = pthread->next;
      pthread_queue_remove(&fd_wait_select, deq);
      if (SET_PF_DONE_EVENT(deq) == OK) {
	pthread_prio_queue_enq(pthread_current_prio_queue, deq);
	deq->state = PS_RUNNING;
      }
    } else {
      pthread = pthread->next;
    }
  }
  return 0;
}


/* ==========================================================================
 * fd_kern_poll()
 *
 * Called only from context_switch(). The kernel must be locked.
 *
 * This function uses a linked list of waiting pthreads, NOT a queue.
 */ 

void fd_kern_poll()
{
  fd_kern_select(&__fd_kern_poll_timeout);
}


/* ==========================================================================
 * fd_kern_wait()
 *
 * Called when there is no active thread to run.
 */

void fd_kern_wait()
{
  if (fd_kern_select(&__fd_kern_wait_timeout))
    /* No threads, waiting on I/O, do a sigsuspend */
    sig_handler_pause();
}


/* ==========================================================================
 * Special Note: All operations return the errno as a negative of the errno
 * listed in errno.h
 * ======================================================================= */

/* ==========================================================================
 * read()
 */
pthread_ssize_t __fd_kern_read(union fd_data fd_data, int flags, void *buf,
  size_t nbytes, struct timespec * timeout)
{
  int fd = fd_data.i;
  int ret;

  pthread_run->sighandled=0;		/* Added by monty */
  while ((ret = machdep_sys_read(fd, buf, nbytes)) < OK) { 
    if (!(flags & __FD_NONBLOCK) &&
	((ret == -EWOULDBLOCK) || (ret == -EAGAIN))) {
      pthread_sched_prevent();

      /* queue pthread for a FDR_WAIT */
      SET_PF_WAIT_EVENT(pthread_run);
      pthread_run->data.fd.fd = fd;
      pthread_queue_enq(&fd_wait_read, pthread_run);

      if (timeout) {
	/* get current time */
	struct timespec current_time;
	machdep_gettimeofday(&current_time);
	sleep_schedule(&current_time, timeout);

	SET_PF_AT_CANCEL_POINT(pthread_run);
	pthread_resched_resume(PS_FDR_WAIT);
	CLEAR_PF_AT_CANCEL_POINT(pthread_run);

	/* We're awake */
	pthread_sched_prevent();
	if (sleep_cancel(pthread_run) == NOTOK) {
	  CLEAR_PF_DONE_EVENT(pthread_run);
	  pthread_sched_resume();
	  SET_ERRNO(ETIMEDOUT);
	  ret= NOTOK;
	  break;
	}
	pthread_sched_resume();
      } else {
	SET_PF_AT_CANCEL_POINT(pthread_run);
	pthread_resched_resume(PS_FDR_WAIT);
	CLEAR_PF_AT_CANCEL_POINT(pthread_run);
      }
      CLEAR_PF_DONE_EVENT(pthread_run);
      if (pthread_run->sighandled)	/* Added by monty */
      {					/* We where aborted */
	SET_ERRNO(EINTR);
	ret= NOTOK;
	break;
      }
    } else {
      SET_ERRNO(-ret); 
      ret = NOTOK; 
      break;
    }
  }
  return(ret);
}

/* ==========================================================================
 * readv()
 */
int __fd_kern_readv(union fd_data fd_data, int flags, const struct iovec *iov,
		    int iovcnt, struct timespec * timeout)
{
  int fd = fd_data.i;
  int ret;

  pthread_run->sighandled=0;		/* Added by monty */
  while ((ret = machdep_sys_readv(fd, iov, iovcnt)) < OK) { 
    if (!(flags & __FD_NONBLOCK) &&
	((ret == -EWOULDBLOCK) || (ret == -EAGAIN))) {
      pthread_sched_prevent();

      /* queue pthread for a FDR_WAIT */
      pthread_run->data.fd.fd = fd;
      SET_PF_WAIT_EVENT(pthread_run);
      pthread_queue_enq(&fd_wait_read, pthread_run);

      if (timeout) {
	/* get current time */
	struct timespec current_time;
	machdep_gettimeofday(&current_time);
	sleep_schedule(&current_time, timeout);

	SET_PF_AT_CANCEL_POINT(pthread_run);
	pthread_resched_resume(PS_FDW_WAIT);
	CLEAR_PF_AT_CANCEL_POINT(pthread_run);

	/* We're awake */
	pthread_sched_prevent();
	if (sleep_cancel(pthread_run) == NOTOK) {
	  CLEAR_PF_DONE_EVENT(pthread_run);
	  pthread_sched_resume();
	  SET_ERRNO(ETIMEDOUT);
	  ret = NOTOK;
	  break;
	}
	pthread_sched_resume();
      } else {
	SET_PF_AT_CANCEL_POINT(pthread_run);
	pthread_resched_resume(PS_FDW_WAIT);
	CLEAR_PF_AT_CANCEL_POINT(pthread_run);
      }
      CLEAR_PF_DONE_EVENT(pthread_run);
      if (pthread_run->sighandled) /* Added by monty */
      {			/* We where aborted */
	SET_ERRNO(EINTR);
	ret= NOTOK;
	break;
      }
    } else {
      SET_ERRNO(-ret);
      ret = NOTOK;
      break;
    }
  }
  return(ret);
}

/* ==========================================================================
 * write()
 */
pthread_ssize_t __fd_kern_write(union fd_data fd_data, int flags,
 const void *buf, size_t nbytes, struct timespec * timeout)
{
  int fd = fd_data.i;
  int ret;

  pthread_run->sighandled=0;		/* Added by monty */
  while ((ret = machdep_sys_write(fd, buf, nbytes)) < OK) { 
    if (!(flags & __FD_NONBLOCK) &&
	((ret == -EWOULDBLOCK) || (ret == -EAGAIN))) {
      pthread_sched_prevent();

      /* queue pthread for a FDW_WAIT */
      pthread_run->data.fd.fd = fd;
      SET_PF_WAIT_EVENT(pthread_run);
      pthread_queue_enq(&fd_wait_write, pthread_run);

      if (timeout) {
	/* get current time */
	struct timespec current_time;
	machdep_gettimeofday(&current_time);
	sleep_schedule(&current_time, timeout);

	pthread_resched_resume(PS_FDW_WAIT);

	/* We're awake */
	pthread_sched_prevent();
	if (sleep_cancel(pthread_run) == NOTOK) {
	  CLEAR_PF_DONE_EVENT(pthread_run);
	  pthread_sched_resume();
	  SET_ERRNO(ETIMEDOUT);
	  ret = NOTOK;
	  break;
	}
	pthread_sched_resume();
      } else {
	pthread_resched_resume(PS_FDW_WAIT);
      }
      CLEAR_PF_DONE_EVENT(pthread_run);
      if (pthread_run->sighandled)		/* Added by monty */
      {						/* We where aborted */
	SET_ERRNO(EINTR);
	ret= NOTOK;
	break;
      }
    } else {
      SET_ERRNO(-ret);
      ret = NOTOK;
      break;
    }
  }
  return(ret);
}

/* ==========================================================================
 * writev()
 */
int __fd_kern_writev(union fd_data fd_data, int flags, const struct iovec *iov,
  int iovcnt, struct timespec * timeout)
{
	int fd = fd_data.i;
	int ret;

    pthread_run->sighandled=0;		/* Added by monty */
    while ((ret = machdep_sys_writev(fd, iov, iovcnt)) < OK) { 
		if (!(flags & __FD_NONBLOCK) &&
          ((ret == -EWOULDBLOCK) || (ret == -EAGAIN))) {
			pthread_sched_prevent();

			/* queue pthread for a FDW_WAIT */
			pthread_run->data.fd.fd = fd;
			SET_PF_WAIT_EVENT(pthread_run);
			pthread_queue_enq(&fd_wait_write, pthread_run);

			if (timeout) {
				/* get current time */
				struct timespec current_time;
				machdep_gettimeofday(&current_time);
				sleep_schedule(&current_time, timeout);

				pthread_resched_resume(PS_FDW_WAIT);

				/* We're awake */
				pthread_sched_prevent();
				if (sleep_cancel(pthread_run) == NOTOK) {
					CLEAR_PF_DONE_EVENT(pthread_run);
					pthread_sched_resume();
					SET_ERRNO(ETIMEDOUT);
					ret = NOTOK;
					break;
				}
				pthread_sched_resume();
			} else {
				pthread_resched_resume(PS_FDW_WAIT);
			}
			CLEAR_PF_DONE_EVENT(pthread_run);
			if (pthread_run->sighandled) /* Added by monty */
			{			/* We where aborted */
			  SET_ERRNO(EINTR);
			  ret= NOTOK;
			  break;
			}
        } else {
            break;
        }
    }
    return(ret);
}

/* ==========================================================================
 * For blocking version we really should set an interrupt
 * fcntl()
 */
int __fd_kern_fcntl(union fd_data fd_data, int flags, int cmd, int arg)
{
	int fd = fd_data.i;

	return(machdep_sys_fcntl(fd, cmd, arg));
}

/* ==========================================================================
 * close()
 */
int __fd_kern_close(union fd_data fd_data, int flags)
{
	int fd = fd_data.i;

	return(machdep_sys_close(fd));
}

/* ==========================================================================
 * lseek()
 * Assume that error number is in the range 0- 255 to get bigger
 * range of seek. ; Monty
 */
off_t __fd_kern_lseek(union fd_data fd_data, int f, off_t offset, int whence)
{
	int fd = fd_data.i;
	extern off_t machdep_sys_lseek(int, off_t, int);
	off_t ret=machdep_sys_lseek(fd, offset, whence);
	if ((long) ret < 0L && (long) ret >=  -255L)
	{
	  SET_ERRNO(ret);
	  ret= NOTOK;
	}
	return ret;
}

/*
 * File descriptor operations
 */
extern machdep_sys_close();

/* Normal file operations */
static struct fd_ops __fd_kern_ops = {
	__fd_kern_write, __fd_kern_read, __fd_kern_close, __fd_kern_fcntl,
	__fd_kern_writev, __fd_kern_readv, __fd_kern_lseek, 1
};

/* NFS file opperations */

/* FIFO file opperations */

/* Device operations */

/* ==========================================================================
 * open()
 *
 * Because open could potentially block opening a file from a remote
 * system, we want to make sure the call will timeout. We then try and open
 * the file, and stat the file to determine what operations we should
 * associate with the fd.
 *
 * This is not done yet
 *
 * A regular file on the local system needs no special treatment.
 */
int open(const char *path, int flags, ...)
{
	int fd, mode, fd_kern;
	struct stat stat_buf;
	va_list ap;

	/* If pthread scheduling == FIFO set a virtual timer */
	if (flags & O_CREAT) {
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	} else {
		mode = 0;
	}

	if (!((fd = fd_allocate()) < OK)) {
		fd_table[fd]->flags = flags;
		flags |= __FD_NONBLOCK;

		if (!((fd_kern = machdep_sys_open(path, flags, mode)) < OK)) {

			/* fstat the file to determine what type it is */
			if (machdep_sys_fstat(fd_kern, &stat_buf)) {
				PANIC();
			}
			if (S_ISREG(stat_buf.st_mode)) {
				fd_table[fd]->ops = &(__fd_kern_ops);
				fd_table[fd]->type = FD_HALF_DUPLEX;
			} else {
				fd_table[fd]->ops = &(__fd_kern_ops);
				fd_table[fd]->type = FD_FULL_DUPLEX;
			}
			fd_table[fd]->fd.i = fd_kern; 
			return(fd);
		}

		fd_table[fd]->count = 0;
		SET_ERRNO(-fd_kern);
	}
	return(NOTOK);
}

/* ==========================================================================
 * create()
 */
int create(const char *path, mode_t mode)
{
	return creat (path, mode);
}

/* ==========================================================================
 * creat()
 */
#undef creat

int creat(const char *path, mode_t mode)
{
	return open (path, O_CREAT | O_TRUNC | O_WRONLY, mode);
}

/* ==========================================================================
 * fchown()
 */
int fchown(int fd, uid_t owner, gid_t group)
{
	int ret;

	if ((ret = fd_lock(fd, FD_WRITE, NULL)) == OK) {
		if ((ret = machdep_sys_fchown(fd_table[fd]->fd.i, owner, group)) < OK) {
			SET_ERRNO(-ret);
			ret = NOTOK;
		}
		fd_unlock(fd, FD_WRITE);
	}
	return(ret);
}

/* ==========================================================================
 * fchmod()
 */
int fchmod(int fd, mode_t mode)
{
	int ret;

	if ((ret = fd_lock(fd, FD_WRITE, NULL)) == OK) {
		if ((ret = machdep_sys_fchmod(fd_table[fd]->fd.i, mode)) < OK) {
			SET_ERRNO(-ret);
			ret = NOTOK;
		}
		fd_unlock(fd, FD_WRITE);
	}
	return(ret);
}

/* ==========================================================================
 * ftruncate()
 */
int ftruncate(int fd, off_t length)
{
	int ret;

	if ((ret = fd_lock(fd, FD_WRITE, NULL)) == OK) {
		if ((ret = machdep_sys_ftruncate(fd_table[fd]->fd.i, length)) < OK) {
			SET_ERRNO(-ret);
			ret = NOTOK;
		}
		fd_unlock(fd, FD_WRITE);
	}
	return(ret);
}

#if defined (HAVE_SYSCALL_FLOCK)
/* ==========================================================================
 * flock()
 *
 *  Added (mevans)
 */
int flock(int fd, int operation)
{
  int ret;

  if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK) {
    if ((ret = machdep_sys_flock(fd_table[fd]->fd.i,
				 operation)) < OK) {
      SET_ERRNO(-ret);
      ret = NOTOK;
    }
    fd_unlock(fd, FD_RDWR);
  }
  return(ret);
}
#endif

/* ==========================================================================
 * pipe()
 */
int pipe(int fds[2])
{
	int kfds[2];
	int ret;

	if ((fds[0] = fd_allocate()) >= OK) {
		if ((fds[1] = fd_allocate()) >= OK) {
			if ((ret = machdep_sys_pipe(kfds)) >= OK) {
				fd_table[fds[0]]->flags = machdep_sys_fcntl(kfds[0], F_GETFL, NULL);
				machdep_sys_fcntl(kfds[0], F_SETFL, fd_table[fds[0]]->flags | __FD_NONBLOCK);
				fd_table[fds[1]]->flags = machdep_sys_fcntl(kfds[1], F_GETFL, NULL);
				machdep_sys_fcntl(kfds[1], F_SETFL, fd_table[fds[1]]->flags | __FD_NONBLOCK);

				fd_table[fds[0]]->ops = &(__fd_kern_ops);
				fd_table[fds[1]]->ops = &(__fd_kern_ops);

				/* Not really full duplex but ... */
				fd_table[fds[0]]->type = FD_FULL_DUPLEX;
				fd_table[fds[1]]->type = FD_FULL_DUPLEX;

				fd_table[fds[0]]->fd.i = kfds[0];
				fd_table[fds[1]]->fd.i = kfds[1];

				return(OK);
			} else {
				SET_ERRNO(-ret);
			}
			fd_table[fds[1]]->count = 0;
		}
		fd_table[fds[0]]->count = 0;
	}
	return(NOTOK);
}

/* ==========================================================================
 * fd_kern_reset()
 * Change the fcntl blocking flag back to NONBLOCKING. This should only
 * be called after a fork.
 */
void fd_kern_reset(int fd)
{
	switch (fd_table[fd]->type) {
	case FD_TEST_HALF_DUPLEX:
		machdep_sys_fcntl(fd_table[fd]->fd.i, F_SETFL,
          fd_table[fd]->flags | __FD_NONBLOCK);
		fd_table[fd]->type = FD_HALF_DUPLEX;
		break;
	case FD_TEST_FULL_DUPLEX:
		machdep_sys_fcntl(fd_table[fd]->fd.i, F_SETFL,
		  fd_table[fd]->flags | __FD_NONBLOCK);
		fd_table[fd]->type = FD_FULL_DUPLEX;
		break;
	default:
		break;
	}
}

/* ==========================================================================
 * fd_kern_init()
 *
 * Assume the entry is locked before routine is invoked
 *
 * This may change. The problem is setting the fd to nonblocking changes
 * the parents fd too, which may not be the desired result.
 *
 * New added feature: If the fd in question is a tty then we open it again
 * and close the original, this way we don't have to worry about the
 * fd being NONBLOCKING to the outside world.
 */
void fd_kern_init(int fd)
{
	if ((fd_table[fd]->flags = machdep_sys_fcntl(fd, F_GETFL, NULL)) >= OK) {
		if (isatty_basic(fd)) {
			int new_fd;

			if ((new_fd = machdep_sys_open(__ttyname_basic(fd), O_RDWR)) >= OK){
				if (machdep_sys_dup2(new_fd, fd) == OK) {
					/* Should print a warning */

					/* Should also set the flags to that of opened outside of
					process */
				}
				machdep_sys_close(new_fd);
			}
		}
		/* We do these things regaurdless of the above results */
		machdep_sys_fcntl(fd, F_SETFL, fd_table[fd]->flags | __FD_NONBLOCK);
		fd_table[fd]->ops 	= &(__fd_kern_ops);
		fd_table[fd]->type 	= FD_HALF_DUPLEX;
		fd_table[fd]->fd.i 	= fd;
		fd_table[fd]->count = 1;

	}
}

/* ==========================================================================
 * fd_kern_gettableentry()
 *
 * Remember only return a a file descriptor that I will modify later.
 * Don't return file descriptors that aren't owned by the child, or don't
 * have kernel operations.
 */
static int fd_kern_gettableentry(const int child, int fd)
{
	int i;

	for (i = 0; i < dtablesize; i++) {
		if (fd_table[i]) {
			if (fd_table[i]->fd.i == fd) {
				if (child) {
					if ((fd_table[i]->type != FD_TEST_HALF_DUPLEX) &&
		   		 	  (fd_table[i]->type != FD_TEST_FULL_DUPLEX)) {
						continue;
					}
				} else {
					if ((fd_table[i]->type == FD_NT) ||
           		 	  (fd_table[i]->type == FD_NIU)) {
						continue;
					}
				}
				/* Is it a kernel fd ? */
				if ((!fd_table[i]->ops) || 
				  (fd_table[i]->ops->use_kfds != 1)) {
					continue;
				}
				return(i);
			}
		}
	}
	return(NOTOK);
}

/* ==========================================================================
 * fd_kern_exec()
 *
 * Fixup the fd_table such that (fd == fd_table[fd]->fd.i) this way
 * the new immage will be OK.
 *
 * Only touch those that won't be used by the parent if we're in a child
 * otherwise fixup all.
 *
 * Returns:
 * 0 no fixup necessary
 * 1 fixup without problems
 * 2 failed fixup on some descriptors, and clobbered them.
 */
int fd_kern_exec(const int child)
{
	int ret = 0;
	int fd, i;

	for (fd = 0; fd < dtablesize; fd++) {
		if (fd_table[fd] == NULL) {
			continue;
		}
		/* Is the fd already in use ? */
		if (child) {
			if ((fd_table[fd]->type != FD_TEST_HALF_DUPLEX) &&
		      (fd_table[fd]->type != FD_TEST_FULL_DUPLEX)) {
				continue;
			}
		} else {
			if ((fd_table[fd]->type == FD_NT) ||
              (fd_table[fd]->type == FD_NIU)) {
				continue;
			}
		}
		/* Is it a kernel fd ? */
		if ((!fd_table[fd]->ops) || 
		  (fd_table[fd]->ops->use_kfds != 1)) {
			continue;
		}
		/* Does it match ? */
		if (fd_table[fd]->fd.i == fd) {
			continue;
		}
		/* OK, fixup entry: Read comments before changing. This isn't obvious */ 

		/* i is the real file descriptor fd currently represents */
		if (((i = fd_table[fd]->fd.i) >= dtablesize) || (i < 0)) {
			/* This should never happen */
			PANIC();
		}

		/*
		 * if the real file descriptor with the same number as the fake file
		 * descriptor number fd is actually in use by the program, we have
         * to move it out of the way
		 */
		if ((machdep_sys_fcntl(fd, F_GETFL, NULL)) >= OK) {
			/* fd is busy */
			int j;

			/*
			 * j is the fake file descriptor that represents the real file
			 * descriptor that we want to move. This way the fake file
			 * descriptor fd can move its real file descriptor i such that
			 * fd == i.
			 */
			if ((j = fd_kern_gettableentry(child, fd)) >= OK) {

				/*
				 * Since j represents a fake file descriptor and fd represents
				 * a fake file descriptor. If j < fd then a previous pass
				 * should have set fd_table[j]->fd.i == j.
				 */
				if (fd < j) {
					if ((fd_table[j]->fd.i = machdep_sys_dup(fd)) < OK) {
						/* Close j, there is nothing else we can do */
  						fd_table[j]->type = FD_NIU;
						ret = 2;
					}
				} else {
					/* This implies fd_table[j]->fd.i != j */
					PANIC();
				}
			}
		}

		/*
		 * Here the real file descriptor i is set to equel the fake file
		 * descriptor fd
		 */
		machdep_sys_dup2(i, fd);

		/*
		 * Now comes the really complicated part: UNDERSTAND before changing
		 *
		 * Here are the things this routine wants to do ...
		 *
		 * Case 1. The real file descriptor has only one fake file descriptor
		 * representing it. 
		 * fd -> i, fd != i ===>  fd -> fd, close(i)
		 * Example fd = 4, i = 2: then close(2), set fd -> i = 4
		 * 
		 * Case 2. The real file descriptor has more than one fake file
		 * descriptor representing it, and this is the first fake file
		 * descriptor representing the real file descriptor
		 * fd -> i, fd' -> i, fd != i ===> fd -> fd, fd' -> fd, close(i)
		 *
		 * The problem is achiving the above is very messy and difficult,
		 * but I should be able to take a short cut. If fd > i then there
		 * will be no need to ever move i, this is because the fake file
		 * descriptor foo that we would have wanted to represent the real
		 * file descriptor i has already been processed. If fd < i then by
		 * moving i to fd all subsequent fake file descriptors fd' should fall
		 * into the previous case and won't need aditional adjusting.
		 *
		 * Does this break the above fd < j check .... It shouldn't because j
		 * is a fake file descriptor and if j < fd then j has already moved 
		 * its real file descriptor foo such that foo <= j therefore foo < fd
		 * and not foo == fd therefor j cannot represent the real 
		 * filedescriptor that fd want to move to and be less than fd
		 */
		if (fd < i) {
			fd_table[fd]->fd.i = fd;
			machdep_sys_close(i);
		}
		if (ret < 1) {
			 ret = 1;
		}
	}
}

/* ==========================================================================
 * fd_kern_fork()
 */
void fd_kern_fork()
{
	pthread_mutex_t *mutex;
	int fd;

	for (fd = 0; fd < dtablesize; fd++) {
		if (fd_table[fd] == NULL) {
			continue;
		}
		mutex = & (fd_table[fd]->mutex);
		if (pthread_mutex_trylock(mutex)) {
			continue;
		}
		if ((fd_table[fd]->r_owner) || (fd_table[fd]->w_owner)) {
			pthread_mutex_unlock(mutex);
			continue;
		}
		/* Is it a kernel fd ? */
		if ((!fd_table[fd]->ops) || (fd_table[fd]->ops->use_kfds != 1)) {
			pthread_mutex_unlock(mutex);
			continue;
		}
		switch (fd_table[fd]->type) {
		case FD_HALF_DUPLEX:
			machdep_sys_fcntl(fd_table[fd]->fd.i, F_SETFL, fd_table[fd]->flags);
			fd_table[fd]->type = FD_TEST_HALF_DUPLEX;
			break;
		case FD_FULL_DUPLEX:
			machdep_sys_fcntl(fd_table[fd]->fd.i, F_SETFL, fd_table[fd]->flags);
			fd_table[fd]->type = FD_TEST_FULL_DUPLEX;
			break;
		default:
			break;
		}
		pthread_mutex_unlock(mutex);
	}
}

/* ==========================================================================
 * Here are the berkeley socket functions. These are not POSIX.
 * ======================================================================= */

#if defined (HAVE_SYSCALL_SOCKET) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * socket()
 */
int socket(int af, int type, int protocol)
{
	int fd, fd_kern;

	 if (!((fd = fd_allocate()) < OK)) {

        if (!((fd_kern = machdep_sys_socket(af, type, protocol)) < OK)) {
		    int tmp_flags;

			tmp_flags = machdep_sys_fcntl(fd_kern, F_GETFL, 0);
			machdep_sys_fcntl(fd_kern, F_SETFL, tmp_flags | __FD_NONBLOCK);

            /* Should fstat the file to determine what type it is */
            fd_table[fd]->ops 	= & __fd_kern_ops;
            fd_table[fd]->type 	= FD_FULL_DUPLEX;
			fd_table[fd]->fd.i	= fd_kern;
        	fd_table[fd]->flags = tmp_flags;
            return(fd);
        }

        fd_table[fd]->count = 0;
		SET_ERRNO(-fd_kern);
    }
    return(NOTOK);
}

#endif

#if defined (HAVE_SYSCALL_BIND) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * bind()
 */
#ifdef _OS_HAS_SOCKLEN_T
int bind(int fd, const struct sockaddr *name, socklen_t namelen)
#else
int bind(int fd, const struct sockaddr *name, int namelen)
#endif
{
	/* Not much to do in bind */
	int ret;

	if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK) {
        if ((ret = machdep_sys_bind(fd_table[fd]->fd.i, name, namelen)) < OK) { 
			SET_ERRNO(-ret);
			ret = NOTOK;
		}
		fd_unlock(fd, FD_RDWR);
	}
	return(ret);
}

#endif

#if defined (HAVE_SYSCALL_CONNECT) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * connect()
 */
#ifdef _OS_HAS_SOCKLEN_T
int connect(int fd, const struct sockaddr *name, socklen_t namelen)
#else
int connect(int fd, const struct sockaddr *name, int namelen)
#endif
{
  struct sockaddr tmpname;
  int ret, tmpnamelen;

  if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK) {
    if ((ret = machdep_sys_connect(fd_table[fd]->fd.i, name, namelen)) < OK) {
      if (!(fd_table[fd]->flags & __FD_NONBLOCK) &&
	  ((ret == -EWOULDBLOCK) || (ret == -EINPROGRESS) ||
	   (ret == -EALREADY) || (ret == -EAGAIN))) {
	pthread_sched_prevent();

	/* queue pthread for a FDW_WAIT */
	SET_PF_WAIT_EVENT(pthread_run);
	pthread_run->data.fd.fd = fd_table[fd]->fd.i;
	pthread_queue_enq(&fd_wait_write, pthread_run);

	pthread_resched_resume(PS_FDW_WAIT);
	CLEAR_PF_DONE_EVENT(pthread_run);

	tmpnamelen = sizeof(tmpname);
	/* OK now lets see if it really worked */
	if (((ret = machdep_sys_getpeername(fd_table[fd]->fd.i,
					    &tmpname, &tmpnamelen)) < OK) &&
	    (ret == -ENOTCONN))
	{
	  /* Get the error, this function should not fail */
	  machdep_sys_getsockopt(fd_table[fd]->fd.i, SOL_SOCKET,
				 SO_ERROR, &ret, &tmpnamelen); 
	  SET_ERRNO(ret);		/* ret is already positive (mevans) */
	  ret = NOTOK;
	}
      } else {
	if (ret < 0)
	{
	  SET_ERRNO(-ret);
	  ret = NOTOK;
	}
      }
    }
    fd_unlock(fd, FD_RDWR);
  }
  return(ret);
}

#endif

#if defined (HAVE_SYSCALL_ACCEPT) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * accept()
 */
#ifdef _OS_HAS_SOCKLEN_T
int accept(int fd, struct sockaddr *name, socklen_t *namelen)
#else
int accept(int fd, struct sockaddr *name, int *namelen)
#endif
{
  int ret, fd_kern;

  if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK) {
    while ((fd_kern = machdep_sys_accept(fd_table[fd]->fd.i, name, namelen)) < OK) {
      if (!(fd_table[fd]->flags & __FD_NONBLOCK) &&
	  ((fd_kern == -EWOULDBLOCK) || (fd_kern == -EAGAIN))) {
	pthread_sched_prevent();

	/* queue pthread for a FDR_WAIT */
	SET_PF_WAIT_EVENT(pthread_run);
	pthread_run->data.fd.fd = fd_table[fd]->fd.i;
	pthread_queue_enq(&fd_wait_read, pthread_run);
				
	pthread_resched_resume(PS_FDR_WAIT);
	CLEAR_PF_DONE_EVENT(pthread_run);
      } else {
	fd_unlock(fd, FD_RDWR);
	SET_ERRNO(-fd_kern);
	return(NOTOK);
      }
    }
    fd_unlock(fd, FD_RDWR);

    if (!((ret = fd_allocate()) < OK)) {

      /* This may be unnecessary */
      machdep_sys_fcntl(fd_kern, F_SETFL, __FD_NONBLOCK);

      /* Should fstat the file to determine what type it is */
      fd_table[ret]->ops 		= & __fd_kern_ops;
      fd_table[ret]->type 	= FD_FULL_DUPLEX;
      fd_table[ret]->fd.i		= fd_kern;

      /* XXX Flags should be the same as those on the listening fd */
      fd_table[ret]->flags 	= fd_table[fd]->flags;
    }
  }
  return(ret);
}

#endif

#if defined (HAVE_SYSCALL_LISTEN) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * listen()
 */
int listen(int fd, int backlog) 
{
  int ret;

  if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK) {
    if ((ret = machdep_sys_listen(fd_table[fd]->fd.i, backlog)) < OK) {
      SET_ERRNO(-ret);
      ret = NOTOK;
    }
    fd_unlock(fd, FD_RDWR);
  }
  return(ret);
}

#endif

#if defined (HAVE_SYSCALL_SEND) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * send_timedwait()
 */
ssize_t send_timedwait(int fd, const void * msg, size_t len, int flags,
		       struct timespec * timeout)
{
  int ret;

  pthread_run->sighandled=0;		/* Added by monty */
  if ((ret = fd_lock(fd, FD_WRITE, timeout)) == OK) {
    while ((ret = machdep_sys_send(fd_table[fd]->fd.i,
				   msg,  len, flags)) < OK)
    {
      if (!(fd_table[fd]->flags & __FD_NONBLOCK) &&
	  ((ret == -EWOULDBLOCK) || (ret == -EAGAIN)))
      {
	pthread_sched_prevent();

	/* queue pthread for a FDW_WAIT */
	SET_PF_WAIT_EVENT(pthread_run);
	pthread_run->data.fd.fd = fd_table[fd]->fd.i;
	pthread_queue_enq(&fd_wait_write, pthread_run);

	if (timeout) {
	  /* get current time */
	  struct timespec current_time;
	  machdep_gettimeofday(&current_time);
	  sleep_schedule(&current_time, timeout);

	  pthread_resched_resume(PS_FDW_WAIT);

	  /* We're awake */
	  pthread_sched_prevent();
	  if (sleep_cancel(pthread_run) == NOTOK) {
	    CLEAR_PF_DONE_EVENT(pthread_run);
	    pthread_sched_resume();
	    ret = -ETIMEDOUT;
	    break;
	  }
	  pthread_sched_resume();
	} else {
	  pthread_resched_resume(PS_FDW_WAIT);
	}
	CLEAR_PF_DONE_EVENT(pthread_run);
	if (pthread_run->sighandled)	/* Added by monty */
	{				/* We where aborted */
	  ret= -EINTR;
	  break;
	}
      } else {
	break;
      }
    }
    fd_unlock(fd, FD_WRITE);
  }
  if (ret < 0)
  {
    SET_ERRNO(-ret);
    return(NOTOK);
  }
  return ret;
}

/* ==========================================================================
 * send()
 */
ssize_t send(int fd, const void * msg, size_t len, int flags)
{
	return(send_timedwait(fd, msg, len, flags, NULL));
}

#endif

#if defined (HAVE_SYSCALL_SENDTO) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * sendto_timedwait()
 */
ssize_t sendto_timedwait(int fd, const void * msg, size_t len,
			 int flags, const struct sockaddr *to, int to_len,
			 struct timespec * timeout)
{
  int ret;

  pthread_run->sighandled=0;		/* Added by monty */
  if ((ret = fd_lock(fd, FD_WRITE, timeout)) == OK) {
    while ((ret = machdep_sys_sendto(fd_table[fd]->fd.i,
				     msg, len, flags, to, to_len)) < OK) {
      if (!(fd_table[fd]->flags & __FD_NONBLOCK) &&
	  ((ret == -EWOULDBLOCK) || (ret == -EAGAIN))) {
	pthread_sched_prevent();

	/* queue pthread for a FDW_WAIT */
	SET_PF_WAIT_EVENT(pthread_run);
	pthread_run->data.fd.fd = fd_table[fd]->fd.i;
	pthread_queue_enq(&fd_wait_write, pthread_run);

	if (timeout) {
	  /* get current time */
	  struct timespec current_time;
	  machdep_gettimeofday(&current_time);
	  sleep_schedule(&current_time, timeout);

	  pthread_resched_resume(PS_FDW_WAIT);

	  /* We're awake */
	  pthread_sched_prevent();
	  if (sleep_cancel(pthread_run) == NOTOK) {
	    CLEAR_PF_DONE_EVENT(pthread_run);
	    pthread_sched_resume();
	    ret= -ETIMEDOUT;
	    break;
	  }
	  pthread_sched_resume();
	} else {
	  pthread_resched_resume(PS_FDW_WAIT);
	}
	CLEAR_PF_DONE_EVENT(pthread_run);
	if (pthread_run->sighandled)		/* Added by monty */
	{					/* We where aborted */
	  ret= -EINTR;
	  break;
	}
      }
      else
	break;					/* ret contains the errorcode */
    }
    fd_unlock(fd, FD_WRITE);
  }
  if (ret < 0)
  {
    SET_ERRNO(-ret);
    return(NOTOK);
  }
  return(ret);
}

/* ==========================================================================
 * sendto()
 */
#ifdef _OS_HAS_SOCKLEN_T
ssize_t sendto(int fd, const void * msg, size_t len, int flags,
	       const struct sockaddr *to, socklen_t to_len)
#else
ssize_t sendto(int fd, const void * msg, size_t len, int flags,
	       const struct sockaddr *to, int to_len)
#endif
{
	return(sendto_timedwait(fd, msg, len, flags, to, to_len, NULL));
}

#endif

#if defined (HAVE_SYSCALL_SENDMSG) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * sendmsg_timedwait()
 */
ssize_t sendmsg_timedwait(int fd, const struct msghdr *msg, int flags,
			  struct timespec * timeout)
{
  int passed_fd, ret, i;

  /* Handle getting the real file descriptor */
  for(i = 0; i < (((struct omsghdr *)msg)->msg_accrightslen/sizeof(i)); i++) {
    passed_fd = *(((int *)((struct omsghdr *)msg)->msg_accrights) + i);
    if ((ret = fd_lock(passed_fd, FD_RDWR, NULL)) == OK) {
      *(((int *)((struct omsghdr *)msg)->msg_accrights) + i)
	= fd_table[passed_fd]->fd.i;
      machdep_sys_fcntl(fd_table[passed_fd]->fd.i, F_SETFL, 
			fd_table[passed_fd]->flags);
      switch(fd_table[passed_fd]->type) {
      case FD_TEST_FULL_DUPLEX:
      case FD_TEST_HALF_DUPLEX:
	break;
      case FD_FULL_DUPLEX:
	fd_table[passed_fd]->type =  FD_TEST_FULL_DUPLEX;
	break;
      case FD_HALF_DUPLEX:
	fd_table[passed_fd]->type =  FD_TEST_HALF_DUPLEX;
	break;
      default:
	PANIC();
      }
    } else {
      fd_unlock(fd, FD_RDWR);
      SET_ERRNO(EBADF);
      return(NOTOK);
    }
    fd_unlock(fd, FD_RDWR);
  }

  pthread_run->sighandled=0;		/* Added by monty */
  if ((ret = fd_lock(fd, FD_WRITE, timeout)) == OK) {
    while((ret = machdep_sys_sendmsg(fd_table[fd]->fd.i, msg, flags)) < OK){
      if (!(fd_table[fd]->flags & __FD_NONBLOCK) &&
	  ((ret == -EWOULDBLOCK) || (ret == -EAGAIN))) {
	pthread_sched_prevent();

	/* queue pthread for a FDW_WAIT */
	SET_PF_WAIT_EVENT(pthread_run);
	pthread_run->data.fd.fd = fd_table[fd]->fd.i;
	pthread_queue_enq(&fd_wait_write, pthread_run);
				
	if (timeout) {
	  /* get current time */
	  struct timespec current_time;
	  machdep_gettimeofday(&current_time);
	  sleep_schedule(&current_time, timeout);

	  pthread_resched_resume(PS_FDW_WAIT);

	  /* We're awake */
	  pthread_sched_prevent();
	  if (sleep_cancel(pthread_run) == NOTOK) {
	    CLEAR_PF_DONE_EVENT(pthread_run);
	    pthread_sched_resume();
	    SET_ERRNO(ETIMEDOUT);
	    ret = NOTOK;
	    break;
	  }
	  pthread_sched_resume();

	} else {
	  pthread_resched_resume(PS_FDW_WAIT);
	}
	CLEAR_PF_DONE_EVENT(pthread_run);
	if (pthread_run->sighandled) /* Added by monty */
	{			/* We where aborted */
	  SET_ERRNO(EINTR);
	  ret= NOTOK;
	  break;
	}

      } else {
	SET_ERRNO(-ret);
	ret = NOTOK;
	break;
      }
    }
    fd_unlock(fd, FD_WRITE);
  }
  return(ret);
}

/* ==========================================================================
 * sendmsg()
 */
ssize_t sendmsg(int fd, const struct msghdr *msg, int flags)
{
	return(sendmsg_timedwait(fd, msg, flags, NULL));
}

#endif

#if defined (HAVE_SYSCALL_RECV) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * recv_timedwait()
 */
ssize_t recv_timedwait(int fd, void * buf, size_t len, int flags,
		       struct timespec * timeout)
{
  int ret;

  pthread_run->sighandled=0;		/* Added by monty */
  if ((ret = fd_lock(fd, FD_READ, timeout)) == OK) {
    while ((ret = machdep_sys_recv(fd_table[fd]->fd.i,
				   buf, len, flags)) < OK) {
      if (!(fd_table[fd]->flags & __FD_NONBLOCK) &&
	  ((ret == -EWOULDBLOCK) || (ret == -EAGAIN))) {
	pthread_sched_prevent();

	/* queue pthread for a FDR_WAIT */
	SET_PF_WAIT_EVENT(pthread_run);
	pthread_run->data.fd.fd = fd_table[fd]->fd.i;
	pthread_queue_enq(&fd_wait_read, pthread_run);

	if (timeout) {
	  /* get current time */
	  struct timespec current_time;
	  machdep_gettimeofday(&current_time);
	  sleep_schedule(&current_time, timeout);

	  pthread_resched_resume(PS_FDR_WAIT);

	  /* We're awake */
	  pthread_sched_prevent();
	  if (sleep_cancel(pthread_run) == NOTOK) {
	    CLEAR_PF_DONE_EVENT(pthread_run);
	    pthread_sched_resume();
	    ret = -ETIMEDOUT;
	    break;
	  }
	  pthread_sched_resume();
	} else {
	  pthread_resched_resume(PS_FDR_WAIT);
	}
	CLEAR_PF_DONE_EVENT(pthread_run);
	if (pthread_run->sighandled) /* Added by monty */
	{			/* We where aborted */
	  ret= -EINTR;
	  break;
	}

      } else {
	break;
      }
    }
    fd_unlock(fd, FD_READ);
  }
  if (ret < 0)
  {
    SET_ERRNO(-ret);
    return(NOTOK);
  }
  return(ret);
}

/* ==========================================================================
 * recv()
 */
ssize_t recv(int fd, void * buf, size_t len, int flags)
{
	return(recv_timedwait(fd, buf, len, flags, NULL));
}

#endif

#if defined (HAVE_SYSCALL_RECVFROM) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * recvfrom_timedwait()
 */
ssize_t recvfrom_timedwait(int fd, void * buf, size_t len, int flags,
			   struct sockaddr * from, int * from_len,
			   struct timespec * timeout)
{
  int ret;

  pthread_run->sighandled=0;		/* Added by monty */
  if ((ret = fd_lock(fd, FD_READ, timeout)) == OK) {
    while ((ret = machdep_sys_recvfrom(fd_table[fd]->fd.i,
				       buf, len, flags, from, from_len)) < OK) {
      if (!(fd_table[fd]->flags & __FD_NONBLOCK) &&
	  ((ret == -EWOULDBLOCK) || (ret == -EAGAIN))) {
	pthread_sched_prevent();

	/* queue pthread for a FDR_WAIT */
	SET_PF_WAIT_EVENT(pthread_run);
	pthread_run->data.fd.fd = fd_table[fd]->fd.i;
	pthread_queue_enq(&fd_wait_read, pthread_run);

	if (timeout) {
	  /* get current time */
	  struct timespec current_time;
	  machdep_gettimeofday(&current_time);
	  sleep_schedule(&current_time, timeout);

	  pthread_resched_resume(PS_FDR_WAIT);

	  /* We're awake */
	  pthread_sched_prevent();
	  if (sleep_cancel(pthread_run) == NOTOK) {
	    CLEAR_PF_DONE_EVENT(pthread_run);
	    pthread_sched_resume();
	    ret= -ETIMEDOUT;
	    break;
	  }
	  pthread_sched_resume();

	} else {
	  pthread_resched_resume(PS_FDR_WAIT);
	}
	CLEAR_PF_DONE_EVENT(pthread_run);
	if (pthread_run->sighandled)		/* Added by monty */
	{					/* We where aborted */
	  ret= -EINTR;
	  break;
	}
      } else {
	break;
      }
    }
    fd_unlock(fd, FD_READ);
  }
  if (ret < 0)
  {
    SET_ERRNO(-ret);
    return(NOTOK);
  }
  return(ret);
}

/* ==========================================================================
 * recvfrom()
 */
#ifdef _OS_HAS_SOCKLEN_T
ssize_t recvfrom(int fd, void * buf, size_t len, int flags,
  struct sockaddr * from, socklen_t * from_len)
#else
ssize_t recvfrom(int fd, void * buf, size_t len, int flags,
  struct sockaddr * from, int * from_len)
#endif
{
	return(recvfrom_timedwait(fd, buf, len, flags, from, from_len, NULL));
}

#endif

#if defined (HAVE_SYSCALL_RECVMSG) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * recvmsg_timedwait()
 */
ssize_t recvmsg_timedwait(int fd, struct msghdr *msg, int flags,
			  struct timespec * timeout) 
{
  struct stat stat_buf;
  int passed_fd, ret, i;

  pthread_run->sighandled=0;		/* Added by monty */
  if ((ret = fd_lock(fd, FD_READ, timeout)) == OK) {
    while ((ret = machdep_sys_recvmsg(fd_table[fd]->fd.i, msg, flags)) < OK) {
      if (!(fd_table[fd]->flags & __FD_NONBLOCK) &&
	  ((ret == -EWOULDBLOCK) || (ret == -EAGAIN))) {
	pthread_sched_prevent();

	/* queue pthread for a FDR_WAIT */
	SET_PF_WAIT_EVENT(pthread_run);
	pthread_run->data.fd.fd = fd_table[fd]->fd.i;
	pthread_queue_enq(&fd_wait_read, pthread_run);
		
	if (timeout) {
	  /* get current time */
	  struct timespec current_time;
	  machdep_gettimeofday(&current_time);
	  sleep_schedule(&current_time, timeout);

	  pthread_resched_resume(PS_FDR_WAIT);

	  /* We're awake */
	  pthread_sched_prevent();
	  if (sleep_cancel(pthread_run) == NOTOK) {
	    CLEAR_PF_DONE_EVENT(pthread_run);
	    pthread_sched_resume();
	    SET_ERRNO(ETIMEDOUT);
	    ret = NOTOK;
	    break;
	  }
	  pthread_sched_resume();

	} else {
	  pthread_resched_resume(PS_FDR_WAIT);
	}
	CLEAR_PF_DONE_EVENT(pthread_run);
	if (pthread_run->sighandled) /* Added by monty */
	{			/* We where aborted */
	  SET_ERRNO(EINTR);
	  ret= NOTOK;
	  break;
	}
      } else {
	SET_ERRNO(-ret);
	ret = NOTOK;
	break;
      }
    }
    fd_unlock(fd, FD_READ);

    /* Handle getting the real file descriptor */
    for (i = 0; i < (((struct omsghdr *)msg)->msg_accrightslen / sizeof(i));
	 i++) {
      passed_fd = *(((int *)((struct omsghdr *)msg)->msg_accrights) + i);
      if (!((fd = fd_allocate()) < OK)) {
	fd_table[fd]->flags = machdep_sys_fcntl(passed_fd, F_GETFL);

	if (!( fd_table[fd]->flags & __FD_NONBLOCK)) {
	  machdep_sys_fcntl(passed_fd, F_SETFL,  
			    fd_table[fd]->flags | __FD_NONBLOCK);
	}

	/* fstat the file to determine what type it is */
	machdep_sys_fstat(passed_fd, &stat_buf);
	if (S_ISREG(stat_buf.st_mode)) {
	  fd_table[fd]->type = FD_HALF_DUPLEX;
	} else {
	  fd_table[fd]->type = FD_FULL_DUPLEX;
	}
	*(((int *)((struct omsghdr *)msg)->msg_accrights) + i) = fd;
	fd_table[fd]->ops = &(__fd_kern_ops);
	fd_table[fd]->fd.i = passed_fd;
      } else {
	SET_ERRNO(EBADF);
	return(NOTOK);
	break;
      }
    }
  }
  return(ret);
}

/* ==========================================================================
 * recvmsg()
 */
ssize_t recvmsg(int fd, struct msghdr *msg, int flags) 
{
	return(recvmsg_timedwait(fd, msg, flags, NULL));
}

#endif

#if defined (HAVE_SYSCALL_SHUTDOWN) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * shutdown()
 */
int shutdown(int fd, int how)
{
	int ret;

	switch(how) {
	case 0: /* Read */
		if ((ret = fd_lock(fd, FD_READ, NULL)) == OK) {
			if ((ret = machdep_sys_shutdown(fd_table[fd]->fd.i, how)) < OK) {
				SET_ERRNO(-ret);
				ret = NOTOK;
			}
			fd_unlock(fd, FD_READ);
		}
	case 1: /* Write */
		if ((ret = fd_lock(fd, FD_WRITE, NULL)) == OK) {
			if ((ret = machdep_sys_shutdown(fd_table[fd]->fd.i, how)) < OK) {
				SET_ERRNO(-ret);
				ret = NOTOK;
			}
			fd_unlock(fd, FD_WRITE);
		}
	case 2: /* Read-Write */
		if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK) {
			if ((ret = machdep_sys_shutdown(fd_table[fd]->fd.i, how)) < OK) {
				SET_ERRNO(-ret);
				ret = NOTOK;
			}
			fd_unlock(fd, FD_RDWR);
		}
	default:
		SET_ERRNO(EBADF);
		ret = NOTOK;
		break;
	}
	return(ret);
}

#endif

#if defined (HAVE_SYSCALL_SETSOCKOPT) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * setsockopt()
 */
#ifdef _OS_HAS_SOCKLEN_T
int setsockopt(int fd, int level, int optname, const void * optval, socklen_t optlen)
#else
int setsockopt(int fd, int level, int optname, const void * optval, int optlen)
#endif
{
   int ret;

   if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK) {
     	if ((ret = machdep_sys_setsockopt(fd_table[fd]->fd.i, level,
		  optname, optval, optlen)) < OK) {
			SET_ERRNO(-ret);
			ret = NOTOK;
     	}
    	fd_unlock(fd, FD_RDWR);
   	}
	return ret;
}

#endif

#if defined (HAVE_SYSCALL_GETSOCKOPT) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * getsockopt()
 */
#ifdef _OS_HAS_SOCKLEN_T
int getsockopt(int fd, int level, int optname, void * optval, socklen_t * optlen)
#else
int getsockopt(int fd, int level, int optname, void * optval, int * optlen)
#endif
{
	int ret;

	if ((ret = fd_lock(fd, FD_READ, NULL)) == OK) {
     	if ((ret = machdep_sys_getsockopt(fd_table[fd]->fd.i, level,
										  optname, optval, optlen)) < OK) {
			SET_ERRNO(-ret);
			ret = NOTOK;
     	}
    	fd_unlock(fd, FD_RDWR);
   	}
	return ret;
}

#endif

#if defined (HAVE_SYSCALL_GETSOCKOPT) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * getsockname()
 */
#ifdef _OS_HAS_SOCKLEN_T
int getsockname(int fd, struct sockaddr * name, socklen_t * naddrlen)
#else
int getsockname(int fd, struct sockaddr * name, int * naddrlen)
#endif
{
	int ret;

	if ((ret = fd_lock(fd, FD_READ, NULL)) == OK) {
		if ((ret = machdep_sys_getsockname(fd_table[fd]->fd.i,
										   name, naddrlen)) < OK) {
			SET_ERRNO(-ret);
			ret = NOTOK;
		}
		fd_unlock(fd, FD_RDWR);
	}
	return ret;
}

#endif

#if defined (HAVE_SYSCALL_GETPEERNAME) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * getpeername()
 */
#ifdef _OS_HAS_SOCKLEN_T
int getpeername(int fd, struct sockaddr * peer, socklen_t * paddrlen)
#else
int getpeername(int fd, struct sockaddr * peer, int * paddrlen)
#endif
{
	int ret;

	if ((ret = fd_lock(fd, FD_READ, NULL)) == OK) {
		if ((ret = machdep_sys_getpeername(fd_table[fd]->fd.i, 
										   peer, paddrlen)) < OK) {
			SET_ERRNO(-ret);
			ret = NOTOK;
		}
		fd_unlock(fd, FD_READ);
	}
	return ret;
}

#endif

#if defined (HAVE_SYSCALL_SOCKETPAIR) || defined (HAVE_SYSCALL_SOCKETCALL)

/* ==========================================================================
 * socketpair()
 */
int socketpair(int af, int type, int protocol, int pair[2])
{
    int ret, fd[2];

    if (!((pair[0] = fd_allocate()) < OK)) {
		if (!((pair[1] = fd_allocate()) < OK)) {
        	if (!((ret = machdep_sys_socketpair(af, type, protocol, fd)) < OK)){
	    		int tmp_flags;

	    		tmp_flags = machdep_sys_fcntl(fd[0], F_GETFL, 0);
	    		machdep_sys_fcntl(fd[0], F_SETFL, tmp_flags | __FD_NONBLOCK);
            	fd_table[pair[0]]->ops 		= & __fd_kern_ops;
            	fd_table[pair[0]]->type 	= FD_FULL_DUPLEX;
	    		fd_table[pair[0]]->flags 	= tmp_flags;
	    		fd_table[pair[0]]->fd.i		= fd[0];

	    		tmp_flags = machdep_sys_fcntl(fd[1], F_GETFL, 0);
	    		machdep_sys_fcntl(fd[1], F_SETFL, tmp_flags | __FD_NONBLOCK);
            	fd_table[pair[1]]->ops 		= & __fd_kern_ops;
            	fd_table[pair[1]]->type 	= FD_FULL_DUPLEX;
	    		fd_table[pair[1]]->flags 	= tmp_flags;
	    		fd_table[pair[1]]->fd.i		= fd[1];

            	return(ret);
        	}
        	fd_table[pair[1]]->count = 0;
		}
        fd_table[pair[0]]->count = 0;
		SET_ERRNO(-ret);
    }
    return(NOTOK);
}

#endif
