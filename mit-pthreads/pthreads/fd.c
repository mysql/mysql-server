/* ==== fd.c ============================================================
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
 * Description : All the syscalls dealing with fds.
 *
 *  1.00 93/08/14 proven
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>				/* For ioctl */
#endif
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <pthread/posix.h>

/*
 * These first functions really should not be called by the user.
 *
 * I really should dynamically figure out what the table size is.
 */
static pthread_mutex_t fd_table_mutex = PTHREAD_MUTEX_INITIALIZER;
static const int dtablecount = 4096/sizeof(struct fd_table_entry);
int dtablesize;

static int fd_get_pthread_fd_from_kernel_fd( int );

/* ==========================================================================
 * Allocate dtablecount entries at once and populate the fd_table.
 *
 * fd_init_entry()
 */
int fd_init_entry(int entry)
{
	struct fd_table_entry *fd_entry;
	int i, round;

	if (fd_table[entry] == NULL) {
		round = entry - entry % dtablecount;

		if ((fd_entry = (struct fd_table_entry *)malloc(
		  sizeof(struct fd_table_entry) * dtablecount)) == NULL) {
			return(NOTOK);
		}
		
		for (i = 0; i < dtablecount && round+i < dtablesize; i++) {
			fd_table[round + i] = &fd_entry[i];

			fd_table[round + i]->ops 	= NULL;
			fd_table[round + i]->type 	= FD_NT;
			fd_table[round + i]->fd.i 	= NOTOK;
			fd_table[round + i]->flags 	= 0;
			fd_table[round + i]->count 	= 0;

			pthread_mutex_init(&(fd_table[round + i]->mutex), NULL);
			pthread_queue_init(&(fd_table[round + i]->r_queue));
			pthread_queue_init(&(fd_table[round + i]->w_queue));
			fd_table[round + i]->r_owner 	= NULL;
			fd_table[round + i]->w_owner 	= NULL;
			fd_table[round + i]->r_lockcount= 0;
			fd_table[round + i]->w_lockcount= 0;

			fd_table[round + i]->next 		= NULL;
		}
	}
	return(OK);
}

/* ==========================================================================
 * fd_check_entry()
 */
int fd_check_entry(unsigned int entry) 
{
	int ret = OK;

	pthread_mutex_lock(&fd_table_mutex);

	if (entry < dtablesize) { 
		if (fd_table[entry] == NULL) {
			if (fd_init_entry(entry)) {
				SET_ERRNO(EBADF);
				ret = -EBADF;
			}
		}
	} else {
		SET_ERRNO(EBADF);
		ret = -EBADF;
	}

	pthread_mutex_unlock(&fd_table_mutex);
	return(ret);
}

/* ==========================================================================
 * fd_init()
 */
void fd_init(void)
{
	int i;

	if ((dtablesize = machdep_sys_getdtablesize()) < 0) {
		/* Can't figure out the table size. */
		PANIC();
	}

	/* select() can only handle FD_SETSIZE descriptors, so our inner loop will
	 * break if dtablesize is higher than that.  This should be removed if and
	 * when the inner loop is rewritten to use poll(). */
	if (dtablesize > FD_SETSIZE) {
		dtablesize = FD_SETSIZE;
	}

	if (fd_table = (struct fd_table_entry **)malloc(
	  sizeof(struct fd_table_entry) * dtablesize)) {
		memset(fd_table, 0, sizeof(struct fd_table_entry) * dtablesize);
		if (fd_check_entry(0) == OK) {
			return;
		}
	}

	/*
	 * There isn't enough memory to allocate a fd table at init time.
	 * This is a problem.
	 */
	PANIC();

}

/* ==========================================================================
 * fd_allocate()
 */
int fd_allocate()
{
	pthread_mutex_t * mutex;
	int i;

	for (i = 0; i < dtablesize; i++) {
		if (fd_check_entry(i) == OK) {
			mutex = &(fd_table[i]->mutex);
			if (pthread_mutex_trylock(mutex)) {
				continue;
			}
			if (fd_table[i]->count || fd_table[i]->r_owner
			  || fd_table[i]->w_owner) {
				pthread_mutex_unlock(mutex);
				continue;
			}
			if (fd_table[i]->type == FD_NT) {
				/* Test to see if the kernel version is in use */
				if ((machdep_sys_fcntl(i, F_GETFL, NULL)) >= OK) {
					/* If so continue; */
					pthread_mutex_unlock(mutex);
					continue;
				}
			}
			fd_table[i]->count++;
			pthread_mutex_unlock(mutex);
			return(i);
		}
	}
	SET_ERRNO(ENFILE);
	return(NOTOK);
}

/*----------------------------------------------------------------------
 * Function:	fd_get_pthread_fd_from_kernel_fd
 * Purpose:		get the fd_table index of a kernel fd
 * Args:		fd	= kernel fd to convert
 * Returns:		fd_table index, -1 if not found
 * Notes:
 *----------------------------------------------------------------------*/
static int
fd_get_pthread_fd_from_kernel_fd( int kfd )
{
	int j;

	/* This is *SICK*, but unless there is a faster way to
	 * turn a kernel fd into an fd_table index, this has to do.
	 */
	for( j=0; j < dtablesize; j++ ) {
		if( fd_table[j] &&
			fd_table[j]->type != FD_NT &&
			fd_table[j]->type != FD_NIU &&
			fd_table[j]->fd.i == kfd ) {
			return j;
		}
	}

	/* Not listed byfd, Check for kernel fd == pthread fd */
	if( fd_table[kfd] == NULL || fd_table[kfd]->type == FD_NT ) {
		/* Assume that the kernel fd is the same */
		return kfd;
	}

	return NOTOK;							/* Not found */
}

/* ==========================================================================
 * fd_basic_basic_unlock()
 *
 * The real work of unlock without the locking of fd_table[fd].lock.
 */
void fd_basic_basic_unlock(struct fd_table_entry * entry, int lock_type)
{
	struct pthread *pthread;

	if (entry->r_owner == pthread_run) {
		if ((entry->type == FD_HALF_DUPLEX) ||
	      (entry->type == FD_TEST_HALF_DUPLEX) ||
		  (lock_type == FD_READ) || (lock_type == FD_RDWR)) {
			if (entry->r_lockcount == 0) {
				if (pthread = pthread_queue_deq(&entry->r_queue)) {
					pthread_sched_prevent();
					entry->r_owner = pthread;
 					if ((SET_PF_DONE_EVENT(pthread)) == OK) {
						pthread_sched_other_resume(pthread);
					} else {
						pthread_sched_resume();
					}
				} else {
					entry->r_owner = NULL;
   	    		}
			} else {
				entry->r_lockcount--;
			}
		}
	}

	if (entry->w_owner == pthread_run) {
		if ((entry->type != FD_HALF_DUPLEX) &&
	      (entry->type != FD_TEST_HALF_DUPLEX) &&
		  ((lock_type == FD_WRITE) || (lock_type == FD_RDWR))) {
			if (entry->w_lockcount == 0) {
				if (pthread = pthread_queue_deq(&entry->w_queue)) {
					pthread_sched_prevent();
					entry->w_owner = pthread;
 					if ((SET_PF_DONE_EVENT(pthread)) == OK) {
						pthread_sched_other_resume(pthread);
					} else {
						pthread_sched_resume();
					}
				} else {
					entry->w_owner = NULL;
        		}
			} else {
				entry->w_lockcount--;
			}
		}
	}
}

/* ==========================================================================
 * fd_basic_unlock()
 */
void fd_basic_unlock(int fd, int lock_type)
{
	fd_basic_basic_unlock(fd_table[fd], lock_type);
}

/* ==========================================================================
 * fd_unlock()
 */
void fd_unlock(int fd, int lock_type)
{
	pthread_mutex_t *mutex;

	mutex = &(fd_table[fd]->mutex);
	pthread_mutex_lock(mutex);
	fd_basic_basic_unlock(fd_table[fd], lock_type);
	pthread_mutex_unlock(mutex);
}

/* ==========================================================================
 * fd_basic_lock()
 * 
 * The real work of lock without the locking of fd_table[fd].lock.
 * Be sure to leave the lock the same way you found it. i.e. locked.
 */
int fd_basic_lock(unsigned int fd, int lock_type, pthread_mutex_t * mutex, 
  struct timespec * timeout)
{
	semaphore *plock;

	switch (fd_table[fd]->type) {
	case FD_NIU:
		/* If not in use return EBADF error */
		SET_ERRNO(EBADF);
		return(NOTOK);
		break;
	case FD_NT:
		/*
		 * If not tested, test it and see if it is valid 
		 * If not ok return EBADF error 
		 */
		fd_kern_init(fd);
		if (fd_table[fd]->type == FD_NIU) {
			SET_ERRNO(EBADF);
			return(NOTOK);
		}
		break;
	case FD_TEST_HALF_DUPLEX:
	case FD_TEST_FULL_DUPLEX:
		/* If a parent process reset the fd to its proper state */
		if (!fork_lock) {
			/* It had better be a kernel fd */
			fd_kern_reset(fd);
		}
		break;
	default:
		break;
	}

	if ((fd_table[fd]->type == FD_HALF_DUPLEX) ||
      (fd_table[fd]->type == FD_TEST_HALF_DUPLEX) ||
	  (lock_type == FD_READ) || (lock_type == FD_RDWR)) {
		if (fd_table[fd]->r_owner) {
			if (fd_table[fd]->r_owner != pthread_run) {
				pthread_sched_prevent();
   	    		pthread_queue_enq(&fd_table[fd]->r_queue, pthread_run);
				SET_PF_WAIT_EVENT(pthread_run);
				pthread_mutex_unlock(mutex);
	
				if (timeout) {
					/* get current time */
					struct timespec current_time;
					machdep_gettimeofday(&current_time);
					sleep_schedule(&current_time, timeout);

					/* Reschedule will unlock pthread_run */
					pthread_run->data.fd.fd = fd;
					pthread_run->data.fd.branch = __LINE__;
					pthread_resched_resume(PS_FDLR_WAIT);
					pthread_mutex_lock(mutex);

					/* If we're the owner then we have to cancel the sleep */
					if (fd_table[fd]->r_owner != pthread_run) {
						CLEAR_PF_DONE_EVENT(pthread_run);
						SET_ERRNO(ETIMEDOUT);
						return(NOTOK);
					}
					sleep_cancel(pthread_run);
				} else {
					/* Reschedule will unlock pthread_run */
					pthread_run->data.fd.fd = fd;
					pthread_run->data.fd.branch = __LINE__;
					pthread_resched_resume(PS_FDLR_WAIT);
					pthread_mutex_lock(mutex);
				}
				CLEAR_PF_DONE_EVENT(pthread_run);
			} else {
				fd_table[fd]->r_lockcount++;
			}
		}
		fd_table[fd]->r_owner = pthread_run;
	}
	if ((fd_table[fd]->type != FD_HALF_DUPLEX) &&
      (fd_table[fd]->type != FD_TEST_HALF_DUPLEX) &&
	  ((lock_type == FD_WRITE) || (lock_type == FD_RDWR))) {
		if (fd_table[fd]->w_owner) {
			if (fd_table[fd]->w_owner != pthread_run) {
				pthread_sched_prevent();
   	    		pthread_queue_enq(&fd_table[fd]->w_queue, pthread_run);
				SET_PF_WAIT_EVENT(pthread_run);
				pthread_mutex_unlock(mutex);
	
				if (timeout) {
					/* get current time */
					struct timespec current_time;
					machdep_gettimeofday(&current_time);
					sleep_schedule(&current_time, timeout);

					/* Reschedule will unlock pthread_run */
					pthread_run->data.fd.fd = fd;
					pthread_run->data.fd.branch = __LINE__;
					pthread_resched_resume(PS_FDLR_WAIT);
					pthread_mutex_lock(mutex);

					/* If we're the owner then we have to cancel the sleep */
					if (fd_table[fd]->w_owner != pthread_run) {
						if (lock_type == FD_RDWR) {
							/* Unlock current thread */
							fd_basic_unlock(fd, FD_READ);
						}
						CLEAR_PF_DONE_EVENT(pthread_run);
						SET_ERRNO(ETIMEDOUT);
						return(NOTOK);
					}
					sleep_cancel(pthread_run);
				} else {
					/* Reschedule will unlock pthread_run */
					pthread_run->data.fd.fd = fd;
					pthread_run->data.fd.branch = __LINE__;
					pthread_resched_resume(PS_FDLR_WAIT);
					pthread_mutex_lock(mutex);
				}
				CLEAR_PF_DONE_EVENT(pthread_run);
			} else {
				fd_table[fd]->w_lockcount++;
			}
		}
		fd_table[fd]->w_owner = pthread_run;
	}
	if (!fd_table[fd]->count) {
		fd_basic_unlock(fd, lock_type);
        return(NOTOK);
    }
	return(OK);
}

/*----------------------------------------------------------------------
 * Function:	fd_unlock_for_cancel
 * Purpose:		Unlock all fd locks held prior to being cancelled
 * Args:		void
 * Returns:
 *		OK or NOTOK
 * Notes:
 *	Assumes the kernel is locked on entry
 *----------------------------------------------------------------------*/
int
fd_unlock_for_cancel( void )
{
	int i, fd;
	struct pthread_select_data *data;
	int rdlk, wrlk, lktype;
	int found;

	/* What we do depends on the previous state of the thread */
	switch( pthread_run->old_state ) {
	case PS_RUNNING:
	case PS_JOIN:
	case PS_SLEEP_WAIT:
	case PS_WAIT_WAIT:
	case PS_SIGWAIT:
	case PS_FDLR_WAIT:
	case PS_FDLW_WAIT:
	case PS_DEAD:
	case PS_UNALLOCED:
		break;							/* Nothing to do */

	case PS_COND_WAIT:
	    CLEAR_PF_GROUP( pthread_run, PF_EVENT_GROUP );
		/* Must reaquire the mutex according to the standard */
		if( pthread_run->data.mutex == NULL ) {
			PANIC();
		}
		pthread_mutex_lock( pthread_run->data.mutex );
	    break;

	case PS_FDR_WAIT:
	    CLEAR_PF_GROUP( pthread_run, PF_EVENT_GROUP);
		/* Free the lock on the fd being used */
		fd = fd_get_pthread_fd_from_kernel_fd( pthread_run->data.fd.fd );
		if( fd == NOTOK ) {
			PANIC();					/* Can't find fd */
		}
		fd_unlock( fd, FD_READ );
		break;

	case PS_FDW_WAIT:					/* Waiting on i/o */
	    CLEAR_PF_GROUP( pthread_run, PF_EVENT_GROUP);
		/* Free the lock on the fd being used */
		fd = fd_get_pthread_fd_from_kernel_fd( pthread_run->data.fd.fd );
		if( fd == NOTOK ) {
			PANIC();					/* Can't find fd */
		}
		fd_unlock( fd, FD_WRITE );
		break;

	case PS_SELECT_WAIT:
		data = pthread_run->data.select_data;

	    CLEAR_PF_GROUP( pthread_run, PF_EVENT_GROUP);

		for( i = 0; i < data->nfds; i++) {
			rdlk =(FD_ISSET(i,&data->readfds)
					   || FD_ISSET(i,&data->exceptfds));
			wrlk = FD_ISSET(i, &data->writefds);
			lktype = rdlk ? (wrlk ? FD_RDWR : FD_READ) : FD_WRITE;

			if( ! (rdlk || wrlk) )
				continue;				/* No locks, no unlock */

			if( (fd = fd_get_pthread_fd_from_kernel_fd( i )) == NOTOK ) {
				PANIC();				/* Can't find fd */
			}

			fd_unlock( fd, lktype );
		}
	    break;

	case PS_MUTEX_WAIT:
		PANIC();						/* Should never cancel a mutex wait */

	default:
		PANIC();						/* Unknown thread status */
	}
}

/* ==========================================================================
 * fd_lock()
 */
#define pthread_mutex_lock_timedwait(a, b) pthread_mutex_lock(a)

int fd_lock(unsigned int fd, int lock_type, struct timespec * timeout)
{
	struct timespec current_time;
	pthread_mutex_t *mutex;
	int error;

	if ((error = fd_check_entry(fd)) == OK) {
		mutex = &(fd_table[fd]->mutex);
		if (pthread_mutex_lock_timedwait(mutex, timeout)) {
			SET_ERRNO(ETIMEDOUT);
			return(-ETIMEDOUT);
		}
		error = fd_basic_lock(fd, lock_type, mutex, timeout);
		pthread_mutex_unlock(mutex);
	}
	return(error);
}

/* ==========================================================================
 * fd_free()
 *
 * Assumes fd is locked and owner by pthread_run
 * Don't clear the queues, fd_unlock will do that.
 */
struct fd_table_entry * fd_free(int fd)
{
	struct fd_table_entry *fd_valid;

    fd_valid = NULL;
	fd_table[fd]->r_lockcount = 0;
	fd_table[fd]->w_lockcount = 0;
	if (--fd_table[fd]->count) {
		fd_valid = fd_table[fd];
		fd_table[fd] = fd_table[fd]->next;
		fd_valid->next = fd_table[fd]->next;
		/* Don't touch queues of fd_valid */
	}

	fd_table[fd]->type 	= FD_NIU;
	fd_table[fd]->fd.i 	= NOTOK;
	fd_table[fd]->next 	= NULL;
	fd_table[fd]->flags = 0;
	fd_table[fd]->count = 0;
	return(fd_valid);
}


/* ==========================================================================
 * ======================================================================= */

/* ==========================================================================
 * read_timedwait()
 */
ssize_t read_timedwait(int fd, void *buf, size_t nbytes, 
  struct timespec * timeout)
{
	int ret;

	if ((ret = fd_lock(fd, FD_READ, NULL)) == OK) {
     	ret = fd_table[fd]->ops->read(fd_table[fd]->fd,
		  fd_table[fd]->flags, buf, nbytes, timeout); 
		fd_unlock(fd, FD_READ);
	} 
	return(ret);
}

/* ==========================================================================
 * read()
 */
ssize_t read(int fd, void *buf, size_t nbytes)
{
	return(read_timedwait(fd, buf, nbytes, NULL));
}

/* ==========================================================================
 * readv_timedwait()
 */
int readv_timedwait(int fd, const struct iovec *iov, int iovcnt,
  struct timespec * timeout)
{
	int ret;

	if ((ret = fd_lock(fd, FD_READ, NULL)) == OK) {
     	ret = fd_table[fd]->ops->readv(fd_table[fd]->fd,
		  fd_table[fd]->flags, iov, iovcnt, timeout); 
		fd_unlock(fd, FD_READ);
	} 
	return(ret);
}

/* ==========================================================================
 * readv()
 */
ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
	return(readv_timedwait(fd, iov, iovcnt, NULL));
}

/* ==========================================================================
 * write()
 */
ssize_t write_timedwait(int fd, const void *buf, size_t nbytes, 
			struct timespec * timeout)
{
  int ret;

  if ((ret = fd_lock(fd, FD_WRITE, NULL)) == OK)
  {
    ret = fd_table[fd]->ops->write(fd_table[fd]->fd,
				   fd_table[fd]->flags, buf, nbytes,
				   timeout); 
    fd_unlock(fd, FD_WRITE);
  }
  return(ret);
}

/* ==========================================================================
 * write()
 */
ssize_t write(int fd, const void * buf, size_t nbytes)
{
	return(write_timedwait(fd,  buf, nbytes, NULL));
}

/* ==========================================================================
 * writev_timedwait()
 */
int writev_timedwait(int fd, const struct iovec *iov, int iovcnt,
  struct timespec * timeout)
{
	int ret;

	 if ((ret = fd_lock(fd, FD_WRITE, NULL)) == OK) {
     	ret = fd_table[fd]->ops->writev(fd_table[fd]->fd,
		  fd_table[fd]->flags, iov, iovcnt, timeout); 
        fd_unlock(fd, FD_WRITE);
    }
    return(ret);
}

/* ==========================================================================
 * writev()
 */
ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
	return(writev_timedwait(fd, iov, iovcnt, NULL));
}

/* ==========================================================================
 * lseek()
 */
off_t lseek(int fd, off_t offset, int whence)
{
	off_t ret;

	 if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK) {
     	ret = fd_table[fd]->ops->seek(fd_table[fd]->fd,
		  fd_table[fd]->flags, offset, whence); 
        fd_unlock(fd, FD_RDWR);
    }
    return(ret);
}

/* ==========================================================================
 * close()
 *
 * The whole close procedure is a bit odd and needs a bit of a rethink.
 * For now close() locks the fd, calls fd_free() which checks to see if
 * there are any other fd values poinging to the same real fd. If so
 * It breaks the wait queue into two sections those that are waiting on fd
 * and those waiting on other fd's. Those that are waiting on fd are connected
 * to the fd_table[fd] queue, and the count is set to zero, (BUT THE LOCK IS NOT
 * RELEASED). close() then calls fd_unlock which give the fd to the next queued
 * element which determins that the fd is closed and then calls fd_unlock etc...
 *
 * XXX close() is even uglier now. You may assume that the kernel fd is the
 * same as fd if fd_table[fd] == NULL or if fd_table[fd]->type == FD_NT.
 * This is true because before any fd_table[fd] is allocated the corresponding
 * kernel fd must be checks to see if it's valid.
 */
int close(int fd)
{
  struct fd_table_entry * entry;
  pthread_mutex_t *mutex;
  union fd_data realfd;
  int ret, flags;

  if(fd < 0 || fd >= dtablesize)
  {
    SET_ERRNO(EBADF);
    return -1;
  }
  /* Need to lock the newfd by hand */
  pthread_mutex_lock(&fd_table_mutex);
  if (fd_table[fd]) {
    pthread_mutex_unlock(&fd_table_mutex);
    mutex = &(fd_table[fd]->mutex);
    pthread_mutex_lock(mutex);

    /*
     * XXX Gross hack ... because of fork(), any fd closed by the
     * parent should not change the fd of the child, unless it owns it.
     */
    switch(fd_table[fd]->type) {
    case FD_NIU:
      pthread_mutex_unlock(mutex);
      ret = -EBADF;
      break;
    case FD_NT:	
      /* 
       * If it's not tested then the only valid possibility is it's
       * kernel fd.
       */
      ret = machdep_sys_close(fd);
      fd_table[fd]->type = FD_NIU;
      pthread_mutex_unlock(mutex);
      break;
    case FD_TEST_FULL_DUPLEX:
    case FD_TEST_HALF_DUPLEX:
      realfd = fd_table[fd]->fd;
      flags = fd_table[fd]->flags;
      if ((entry = fd_free(fd)) == NULL) {
	ret = fd_table[fd]->ops->close(realfd, flags);
      } else {
	/* There can't be any others waiting for fd. */
	pthread_mutex_unlock(&entry->mutex);
	/* Note: entry->mutex = mutex */
	mutex = &(fd_table[fd]->mutex);
      }
      pthread_mutex_unlock(mutex);
      break;
    default:
      ret = fd_basic_lock(fd, FD_RDWR, mutex, NULL);
      if (ret == OK) {
	realfd = fd_table[fd]->fd;
	flags = fd_table[fd]->flags;
	pthread_mutex_unlock(mutex);
	if ((entry = fd_free(fd)) == NULL) {
	  ret = fd_table[fd]->ops->close(realfd, flags);
	} else {
	  fd_basic_basic_unlock(entry, FD_RDWR);
	  pthread_mutex_unlock(&entry->mutex);
						/* Note: entry->mutex = mutex */
	}
	fd_unlock(fd, FD_RDWR);
      } else {
	pthread_mutex_unlock(mutex);
      }
      break;
    }
  } else {
    /* Don't bother creating a table entry */
    pthread_mutex_unlock(&fd_table_mutex);
    ret = machdep_sys_close(fd);
  }
  if( ret < 0) {
    SET_ERRNO(-ret);
    ret = -1;
  }
  return(ret);
}

/* ==========================================================================
 * fd_basic_dup()
 *
 *
 * This is a MAJOR guess!! I don't know if the mutext unlock is valid
 * in the BIG picture.  But it seems to be needed to avoid deadlocking
 * with ourselves when we try to close the duped file descriptor.
 */
static inline void fd_basic_dup(int fd, int newfd)
{
	fd_table[newfd]->next = fd_table[fd]->next;
	fd_table[fd]->next = fd_table[newfd];
	fd_table[newfd] = fd_table[fd];
	fd_table[fd]->count++;
	pthread_mutex_unlock(&fd_table[newfd]->next->mutex);

}

/* ==========================================================================
 * dup2()
 *
 * Note: Always lock the lower number fd first to avoid deadlocks.
 * Note: Leave the newfd locked. It will be unlocked at close() time.
 * Note: newfd must be locked by hand so it can be closed if it is open,
 * 		 or it won't be opened while dup is in progress.
 */
int dup2(fd, newfd)
{
	struct fd_table_entry * entry;
	pthread_mutex_t *mutex;
	union fd_data realfd;
	int ret, flags;

	if ((ret = fd_check_entry(newfd)) != OK)
	  return ret;

	if (newfd < dtablesize) {
		if (fd < newfd) {
			if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK) {
				/* Need to lock the newfd by hand */
				mutex = &(fd_table[newfd]->mutex);
				pthread_mutex_lock(mutex);

				/* Is it inuse */
				if (fd_basic_lock(newfd, FD_RDWR, mutex, NULL) == OK) {
					realfd = fd_table[newfd]->fd;
					flags = fd_table[newfd]->flags;
					/* free it and check close status */
					if ((entry = fd_free(newfd)) == NULL) {
						entry = fd_table[newfd];
     					entry->ops->close(realfd, flags);
						if (entry->r_queue.q_next) {
							if (fd_table[fd]->next) {
						  		fd_table[fd]->r_queue.q_last->next = 
							      entry->r_queue.q_next;
							} else {
						  		fd_table[fd]->r_queue.q_next = 
							      entry->r_queue.q_next;
							}
							fd_table[fd]->r_queue.q_last = 
							  entry->r_queue.q_last;
						}
						if (entry->w_queue.q_next) {
							if (fd_table[fd]->next) {
						  		fd_table[fd]->w_queue.q_last->next = 
							      entry->w_queue.q_next;
							} else {
						  		fd_table[fd]->w_queue.q_next = 
							      entry->w_queue.q_next;
							}
							fd_table[fd]->w_queue.q_last = 
							  entry->w_queue.q_last;
						}
						entry->r_queue.q_next = NULL;
						entry->w_queue.q_next = NULL;
						entry->r_queue.q_last = NULL;
						entry->w_queue.q_last = NULL;
						entry->r_owner = NULL;
						entry->w_owner = NULL;
						ret = OK;
					} else {
						fd_basic_basic_unlock(entry, FD_RDWR);
						pthread_mutex_unlock(&entry->mutex);
						/* Note: entry->mutex = mutex */
					}
				}
				fd_basic_dup(fd, newfd);
			}
			fd_unlock(fd, FD_RDWR);
		} else {
			/* Need to lock the newfd by hand */
			mutex = &(fd_table[newfd]->mutex);
			pthread_mutex_lock(mutex);

			if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK) {
				/* Is newfd inuse */
				if ((ret = fd_basic_lock(newfd, FD_RDWR, mutex, NULL)) == OK) {
					realfd = fd_table[newfd]->fd;
					flags = fd_table[newfd]->flags;
					/* free it and check close status */
					if ((entry = fd_free(newfd)) == NULL) {
						entry = fd_table[newfd];
   		  				entry->ops->close(realfd, flags);
						if (entry->r_queue.q_next) {
							if (fd_table[fd]->next) {
						  		fd_table[fd]->r_queue.q_last->next = 
							      entry->r_queue.q_next;
							} else {
						  		fd_table[fd]->r_queue.q_next = 
							      entry->r_queue.q_next;
							}
							fd_table[fd]->r_queue.q_last = 
							  entry->r_queue.q_last;
						}
						if (entry->w_queue.q_next) {
							if (fd_table[fd]->next) {
						  		fd_table[fd]->w_queue.q_last->next = 
							      entry->w_queue.q_next;
							} else {
						  		fd_table[fd]->w_queue.q_next = 
							      entry->w_queue.q_next;
							}
							fd_table[fd]->w_queue.q_last = 
							  entry->w_queue.q_last;
						}
						entry->r_queue.q_next = NULL;
						entry->w_queue.q_next = NULL;
						entry->r_queue.q_last = NULL;
						entry->w_queue.q_last = NULL;
						entry->r_owner = NULL;
						entry->w_owner = NULL;
						ret = OK;
					} else {
						fd_basic_basic_unlock(entry, FD_RDWR);
						pthread_mutex_unlock(&entry->mutex);
						/* Note: entry->mutex = mutex */
					}
					fd_basic_dup(fd, newfd);
				}
				fd_unlock(fd, FD_RDWR);
			}
		}
	} else {
		ret = NOTOK;
	}
	return(ret);
			
}

/* ==========================================================================
 * dup()
 */
int dup(int fd)
{
	int ret;

	if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK) {
		ret = fd_allocate();
		fd_basic_dup(fd, ret);
		fd_unlock(fd, FD_RDWR);
	}
	return(ret);
}

/* ==========================================================================
 * fcntl()
 */
int fcntl(int fd, int cmd, ...)
{
	int ret, realfd, flags;
	struct flock *flock;
	semaphore *plock;
	va_list ap;

	flags = 0;
	if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK) {
		va_start(ap, cmd);
		switch(cmd) {
		case F_DUPFD:
			ret = fd_allocate();
			fd_basic_dup(va_arg(ap, int), ret);
			break;
		case F_SETFD:
			break;
		case F_GETFD:
			break;
		case F_GETFL:
			ret = fd_table[fd]->flags;
			break;
		case F_SETFL:
			flags = va_arg(ap, int);
     		if ((ret = fd_table[fd]->ops->fcntl(fd_table[fd]->fd,
			  fd_table[fd]->flags, cmd, flags | __FD_NONBLOCK)) == OK) {
				fd_table[fd]->flags = flags;
			}
			break;
/*		case F_SETLKW: */
			/*
			 * Do the same as SETLK but if it fails with EACCES or EAGAIN
			 * block the thread and try again later, not implemented yet
			 */
/*		case F_SETLK: */
/*		case F_GETLK: 
			flock = va_arg(ap, struct flock*);
     		ret = fd_table[fd]->ops->fcntl(fd_table[fd]->fd,
			  fd_table[fd]->flags, cmd, flock);
			break; */
		default:
			/* Might want to make va_arg use a union */
     		ret = fd_table[fd]->ops->fcntl(fd_table[fd]->fd,
			  fd_table[fd]->flags, cmd, va_arg(ap, void*));
			break;
		}
		va_end(ap);
		fd_unlock(fd, FD_RDWR);
	}
	return(ret);
}

/* ==========================================================================
 * getdtablesize()
 */
int getdtablesize()
{
	return dtablesize;
}

/* ==========================================================================
 * ioctl()
 *
 * Really want to do a real implementation of this that parses the args ala
 * fcntl(), above, but it will have to be a totally platform-specific,
 * nightmare-on-elm-st-style sort of thing.  Might even deserve its own file
 * ala select()... --SNL
 */
#ifndef ioctl_request_type
#define ioctl_request_type unsigned long	/* Dummy patch by Monty */
#endif

int
ioctl(int fd, ioctl_request_type request, ...)
{
    int ret;
	pthread_va_list ap;
	caddr_t arg;

	va_start( ap, request );			/* Get the arg */
	arg = va_arg(ap,caddr_t);
	va_end( ap );

	if (fd < 0 || fd >= dtablesize)
	    ret = NOTOK;
	else if (fd_table[fd]->fd.i == NOTOK)
	    ret = machdep_sys_ioctl(fd, request, arg);
	else if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK) {
	    ret = machdep_sys_ioctl(fd_table[fd]->fd.i, request, arg);
		if( ret == 0 && request == FIONBIO ) {
			/* Properly set NONBLOCK flag */
			int v = *(int *)arg;
			if( v )
				fd_table[fd]->flags |= __FD_NONBLOCK;
			else
				fd_table[fd]->flags &= ~__FD_NONBLOCK;
		}
		fd_unlock(fd, FD_RDWR);
	}
	return ret;
}

