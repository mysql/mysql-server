/* ==== fd_sysv.c ============================================================
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
 * Description : Transforms BSD socket calls to SYSV streams.
 *
 *  1.00 94/11/19 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <config.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

#if defined (HAVE_SYSCALL_PUTMSG) && defined (HAVE_SYSCALL_GETMSG) && !defined(HAVE_SYSCALL_SOCKETCALL) && !defined(HAVE_SYSCALL_SOCKET)
#define HAVE_STREAMS	1

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <tiuser.h>
#include <sys/tihdr.h>
#include <netinet/in.h>
#include <sys/timod.h>

#define STREAM_BUF_SIZE	sizeof(union T_primitives) + sizeof(struct sockaddr)

extern struct pthread_queue fd_wait_read, fd_wait_write;

/* ==========================================================================
 * putmsg_timedwait_basic()
 */
static int putmsg_timedwait_basic(int fd, struct strbuf * ctlptr, 
  struct strbuf * dataptr, int flags, struct timespec * timeout)
{

	int ret;

    pthread_run->sighandled=0;		/* Added by monty */
    while ((ret = machdep_sys_putmsg(fd_table[fd]->fd.i,
      ctlptr,  dataptr, flags)) < OK) {
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
                sleep_schedule(& current_time, timeout);

                pthread_resched_resume(PS_FDW_WAIT);

                /* We're awake */
                pthread_sched_prevent();
		if (sleep_cancel(pthread_run) == NOTOK) {
		  CLEAR_PF_DONE_EVENT(pthread_run);
                    pthread_sched_resume();
                    SET_ERRNO(ETIMEDOUT);
                    ret = -ETIMEDOUT;
                    break;
                }
                pthread_sched_resume();
            } else {
                pthread_resched_resume(PS_FDW_WAIT);
            }
			CLEAR_PF_DONE_EVENT(pthread_run);
	    if (pthread_run->sighandled) /* Added by monty */
	    {				/* We where aborted */
	      SET_ERRNO(EINTR);
	      ret= -EINTR;
	      break;
	    }
        } else {
            SET_ERRNO(-ret);
            break;
        }
    }
    return(ret);
}

/* ==========================================================================
 * putmsg_timedwait()
 */
int putmsg_timedwait(int fd, struct strbuf * ctlptr, struct strbuf * dataptr,
  int flags, struct timespec * timeout)
{
	int ret;

	if ((ret = fd_lock(fd, FD_WRITE, timeout)) == OK) {
		ret = putmsg_timedwait_basic(fd, ctlptr, dataptr, flags, timeout);
    	fd_unlock(fd, FD_WRITE);
	}
	return(ret);
}

/* ==========================================================================
 * putmsg()
 */
int putmsg(int fd, struct strbuf * ctlptr, struct strbuf * dataptr,
  int flags)
{
	return(putmsg_timedwait(fd, ctlptr, dataptr, flags, NULL));
}

/* ==========================================================================
 * getmsg_timedwait_basic()
 */
int getmsg_timedwait_basic(int fd, struct strbuf * ctlptr, 
  struct strbuf * dataptr, int * flags, struct timespec * timeout)
{
	int ret;

    pthread_run->sighandled=0;		/* Added by monty */
    while ((ret = machdep_sys_getmsg(fd_table[fd]->fd.i,
      ctlptr, dataptr, flags)) < OK) {
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
                sleep_schedule(& current_time, timeout);

                pthread_resched_resume(PS_FDR_WAIT);

                /* We're awake */
                pthread_sched_prevent();
                if (sleep_cancel(pthread_run) == NOTOK) {
					CLEAR_PF_DONE_EVENT(pthread_run);
                    pthread_sched_resume();
                    SET_ERRNO(ETIMEDOUT);
                    ret = -ETIMEDOUT;
                    break;
                }
                pthread_sched_resume();
            } else {
                pthread_resched_resume(PS_FDR_WAIT);
            }
			CLEAR_PF_DONE_EVENT(pthread_run);
	    if (pthread_run->sighandled) /* Added by monty */
	    {				/* We where aborted */
	      SET_ERRNO(EINTR);
	      ret= -EINTR;
	      break;
	    }

        } else {
            SET_ERRNO(-ret);
            break;
        }
    }
	return(ret);
}

/* ==========================================================================
 * getmsg_timedwait()
 */
int getmsg_timedwait(int fd, struct strbuf * ctlptr, struct strbuf * dataptr,
  int * flags, struct timespec * timeout)
{
	int ret;

    if ((ret = fd_lock(fd, FD_READ, timeout)) == OK) {
		ret = getmsg_timedwait_basic(fd, ctlptr, dataptr, flags, timeout);
        fd_unlock(fd, FD_READ);
	}
	return (ret);
}

/* ==========================================================================
 * getmsg()
 */
int getmsg(int fd, struct strbuf * ctlptr, struct strbuf * dataptr,
  int * flags)
{
	return(getmsg_timedwait(fd, ctlptr, dataptr, flags, NULL));
}

#endif

/* ==========================================================================
 * Here are the berkeley socket functions implemented with stream calls.
 * These are not POSIX.
 * ======================================================================= */

#if (!defined (HAVE_SYSCALL_BIND)) && defined(HAVE_STREAMS)

/* ==========================================================================
 * bind()
 */
int bind(int fd, const struct sockaddr *name, int namelen)
{
  char buf[STREAM_BUF_SIZE];
  union T_primitives * res;
  struct T_bind_req * req;
  struct T_bind_ack * ack;
  struct strbuf strbuf;
  int flags, ret;

  if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK)
  {
    req = (struct T_bind_req *)buf;
    req->PRIM_type = T_BIND_REQ;
    req->ADDR_length = namelen;
    req->ADDR_offset = sizeof(struct T_bind_req);
    req->CONIND_number = 4;
    memcpy(buf + sizeof(struct T_bind_req), name, namelen);

    strbuf.len = sizeof(struct T_bind_req) + namelen;
    strbuf.maxlen = STREAM_BUF_SIZE;
    strbuf.buf = buf;

    if ((ret=putmsg_timedwait_basic(fd, &strbuf, NULL, 0, NULL)) == OK)
    {
      memset(buf, 0, STREAM_BUF_SIZE);

      strbuf.len = sizeof(struct T_bind_ack) + namelen;
      strbuf.maxlen = STREAM_BUF_SIZE;
      strbuf.buf = buf;
      flags = 0;

      if ((ret = getmsg_timedwait_basic(fd, &strbuf, NULL,
					&flags, NULL)) >= OK)
      {
	res = (union T_primitives *)buf;

	switch(res->type) {
	case T_BIND_ACK:
	  ret = OK;
	  break;
	default:
	  SET_ERRNO(EPROTO);			/* What should this be? */
	  ret = NOTOK;
	  break;
	}
      }
      else
      {
	SET_ERRNO(-ret);
	ret = NOTOK;
      }
    }
    else
    {
      SET_ERRNO(-ret);
      ret = NOTOK;
    }
    fd_unlock(fd, FD_RDWR);
  }
  return(ret);
}

#endif

#if (!defined (HAVE_SYSCALL_CONNECT)) && defined(HAVE_STREAMS)

/* ==========================================================================
 * connect()
 */
int connect(int fd, const struct sockaddr *name, int namelen)
{
  char buf[STREAM_BUF_SIZE];
  union T_primitives * res;
  struct T_conn_req * req;
  struct T_conn_con * con;
  struct T_ok_ack * ok;
  struct strbuf strbuf;
  int flags, ret;

  if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK)
  {
    req = (struct T_conn_req *)buf;
    req->PRIM_type = T_CONN_REQ;
    req->DEST_length = namelen;
    req->DEST_offset = sizeof(struct T_conn_req);
    req->OPT_length = 0;
    req->OPT_offset = 0;
    memcpy(buf + sizeof(struct T_conn_req), name, namelen);

    strbuf.len = sizeof(struct T_conn_req) + namelen;
    strbuf.maxlen = STREAM_BUF_SIZE;
    strbuf.buf = buf;

    if ((ret=putmsg_timedwait_basic(fd, &strbuf, NULL, 0, NULL)) != OK)
      goto err;
    
    memset(buf, 0, STREAM_BUF_SIZE);
    ok = (struct T_ok_ack *)buf;

    strbuf.maxlen = STREAM_BUF_SIZE;
    strbuf.len = STREAM_BUF_SIZE;
    strbuf.buf = buf;
    flags = 0;

    if ((ret=getmsg_timedwait_basic(fd, &strbuf, NULL, &flags, NULL)) < OK)
      goto err;				/* Fixed by monty */
    if (ok->PRIM_type != T_OK_ACK)
    {
      ret= -EPROTO;			/* What should this be? */
      goto err;
    }

    memset(buf, 0, STREAM_BUF_SIZE);
    strbuf.maxlen = STREAM_BUF_SIZE;
    strbuf.len = STREAM_BUF_SIZE;
    strbuf.buf = buf;
    flags = 0;

    if ((ret=getmsg_timedwait_basic(fd, &strbuf, NULL, &flags, NULL) < OK))
      goto err;

    res = (union T_primitives *) buf;
    switch(res->type) {
    case T_CONN_CON:
      ret = OK;
      break;
    case T_DISCON_IND:
      ret= -ECONNREFUSED;
      goto err;
    default:
      ret= -EPROTO;			/* What should this be? */
      goto err;
    }
    fd_unlock(fd, FD_RDWR);
  }
  return(ret);

 err:
  fd_unlock(fd, FD_RDWR);
  SET_ERRNO(-ret);				/* Proably not needed... */
  return NOTOK;
}

#endif

#if (!defined (HAVE_SYSCALL_LISTEN)) && defined(HAVE_STREAMS)

/* ==========================================================================
 * listen()
 */
int listen(int fd, int backlog)
{
	return(OK);
}

#endif

#if (!defined (HAVE_SYSCALL_SOCKET)) && defined(HAVE_STREAMS)

extern ssize_t 			__fd_kern_write();
static pthread_ssize_t 	__fd_sysv_read();
extern int				__fd_kern_close();
extern int				__fd_kern_fcntl();
extern int				__fd_kern_writev();
extern int				__fd_kern_readv();
extern off_t			__fd_kern_lseek();

/* Normal file operations */
static struct fd_ops __fd_sysv_ops = {
    __fd_kern_write, __fd_sysv_read, __fd_kern_close, __fd_kern_fcntl,
    __fd_kern_writev, __fd_kern_readv, __fd_kern_lseek, 1
};

/* ==========================================================================
 * read()   
 */         
static pthread_ssize_t __fd_sysv_read(union fd_data fd_data, int flags,
  void *buf, size_t nbytes, struct timespec * timeout)
{           
	struct strbuf dataptr;
    int fd = fd_data.i;
    int getmsg_flags;
    int ret;    

    getmsg_flags = 0;
	dataptr.len = 0;
	dataptr.buf = buf;
	dataptr.maxlen = nbytes;

    pthread_run->sighandled=0;		/* Added by monty */
    while ((ret = machdep_sys_getmsg(fd, NULL, &dataptr, &getmsg_flags)) < OK) {
        if (!(fd_table[fd]->flags & __FD_NONBLOCK) &&
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
                sleep_schedule(& current_time, timeout);
            
                pthread_resched_resume(PS_FDR_WAIT);
 
                /* We're awake */
                pthread_sched_prevent();
                if (sleep_cancel(pthread_run) == NOTOK) {
                    CLEAR_PF_DONE_EVENT(pthread_run);
                    pthread_sched_resume();
                    SET_ERRNO(ETIMEDOUT);
                    ret = -ETIMEDOUT;
                    break;
                }
                pthread_sched_resume();
            } else {
                pthread_resched_resume(PS_FDR_WAIT);
            }
            CLEAR_PF_DONE_EVENT(pthread_run);
	    if (pthread_run->sighandled) /* Added by monty */
	    {				/* We where aborted */
	      SET_ERRNO(EINTR);
	      return(NOTOK);
	    }
        } else {
            SET_ERRNO(-ret);
	    return(NOTOK);
            break;
        }
    }
    return(dataptr.len);
}   

/* ==========================================================================
 * socket_tcp()
 */
static int socket_tcp(int fd)
{
	int ret;

	if ((ret = machdep_sys_open("/dev/tcp", O_RDWR | O_NONBLOCK, 0)) >= OK) {
        /* Should fstat the file to determine what type it is */
        fd_table[fd]->ops   = & __fd_sysv_ops;
        fd_table[fd]->type  = FD_FULL_DUPLEX;
        fd_table[fd]->fd.i  = ret;
        fd_table[fd]->flags = 0;
    }
    return(ret);
}

/* ==========================================================================
 * socket()
 */
int socket(int af, int type, int protocol)
{
  int fd, fd_kern;

  if ((fd = fd_allocate()) < OK)
    return (fd);

  switch(af) {
  case AF_INET:
    switch(type) {
    case SOCK_STREAM:
      if ((fd_kern = socket_tcp(fd)) >= OK)
	return(fd);
      SET_ERRNO(-fd_kern);
      break;
    case SOCK_DGRAM:
      if ((fd_kern = machdep_sys_open("/dev/udp",
				      O_RDWR | O_NONBLOCK, 0)) >= OK) {
	/* Should fstat the file to determine what type it is */
	fd_table[fd]->ops   = & __fd_sysv_ops;
	fd_table[fd]->type  = FD_FULL_DUPLEX;
	fd_table[fd]->fd.i  = fd_kern;
	fd_table[fd]->flags = 0;
	return(fd);
      }
      SET_ERRNO(-fd_kern);
      break;
    default:
      SET_ERRNO(EPROTONOSUPPORT);
      break;
    }
    break;
  case AF_UNIX:
  case AF_ISO:
  case AF_NS:
  default:
    SET_ERRNO(EPROTONOSUPPORT);
    break;
  }
  fd_table[fd]->count = 0;
  return(NOTOK);				/* Fixed by monty */
}

#endif

#if (!defined (HAVE_SYSCALL_ACCEPT)) && defined(HAVE_STREAMS)

/* ==========================================================================
 * accept_fd()
 */
static int accept_fd(int fd, struct sockaddr *name, int *namelen, char * buf,
		     int SEQ_number)
{
  struct T_conn_res * res;
  struct strbuf strbuf;
  int fd_new, fd_kern;

  /* Get a new table entry */
  if ((fd_new = fd_allocate()) < OK)
    return(NOTOK);

  /* Get the new kernel entry */
  if (!((fd_kern = socket_tcp(fd_new)) < OK)) { 
    res = (struct T_conn_res *)buf;
    res->PRIM_type = T_CONN_RES;
    /* res->QUEUE_ptr = (queue_t *)&fd_kern; */
    res->OPT_length = 0;
    res->OPT_offset = 0;
    res->SEQ_number = SEQ_number;
		
    strbuf.maxlen = sizeof(union T_primitives) +sizeof(struct sockaddr);
    strbuf.len = sizeof(struct T_conn_ind) + (*namelen);
    strbuf.buf = buf;

    {
      struct strfdinsert insert;

      insert.ctlbuf.maxlen = (sizeof(union T_primitives) +
			      sizeof(struct sockaddr));
      insert.ctlbuf.len =  sizeof(struct T_conn_ind);
      insert.ctlbuf.buf = buf;
      insert.databuf.maxlen = 0;
      insert.databuf.len = 0;
      insert.databuf.buf = NULL;
      insert.flags = 0;
      insert.fildes = fd_kern;
      insert.offset = 4;
      /* Should the following be checked ? */
      machdep_sys_ioctl(fd_table[fd]->fd.i,  I_FDINSERT, &insert);
    }

    /*		if (putmsg_timedwait_basic(fd, &strbuf, NULL, 0, NULL) == OK) {
		/* 			return(fd_new); */
    {
      int flags = 0;
      int ret;

      /* Should the following be checked ? */
      ret = getmsg_timedwait_basic(fd, &strbuf, NULL, &flags, NULL); 
      return(fd_new);

    }
    machdep_sys_close(fd_kern);
  }
  fd_table[fd_new]->count = 0;
  return(NOTOK);
}


/* ==========================================================================
 * accept()
 */
int accept(int fd, struct sockaddr *name, int *namelen)
{
  char buf[sizeof(union T_primitives) + sizeof(struct sockaddr)];
  struct T_conn_ind * ind;
  struct strbuf strbuf;
  int flags, ret;

  if ((ret = fd_lock(fd, FD_RDWR, NULL)) == OK)
  {
    ind = (struct T_conn_ind *)buf;
    ind->PRIM_type = T_CONN_IND;
    ind->SRC_length = (*namelen);
    ind->SRC_offset = sizeof(struct T_conn_ind);
    ind->OPT_length = 0;
    ind->OPT_offset = 0;
    ind->SEQ_number = 0;

    strbuf.maxlen = sizeof(union T_primitives) + sizeof(struct sockaddr);
    strbuf.len = sizeof(struct T_conn_ind) + (*namelen);
    strbuf.buf = buf;
    flags = 0;

    if ((ret=getmsg_timedwait_basic(fd, &strbuf, NULL, &flags, NULL)) < OK)
    {
      SET_ERRNO(-ret);
      ret= NOTOK;
    }
    else
      ret = accept_fd(fd, name, namelen, buf, ind->SEQ_number);
    fd_unlock(fd, FD_RDWR);
  }
  return(ret);
}

#endif /* HAVE_SYSCALL_ACCEPT */

#if (!defined (HAVE_SYSCALL_SENDTO)) && defined (HAVE_STREAMS)

/* ==========================================================================
 * sendto_timedwait()
 */
ssize_t sendto_timedwait(int fd, const void * msg, size_t len, int flags, 
  const struct sockaddr *name, int namelen, struct timespec * timeout)
{
	char buf[STREAM_BUF_SIZE];
	struct T_unitdata_req * req;
	struct strbuf dataptr;
	struct strbuf ctlptr;
    ssize_t ret, prio;

	req = (struct T_unitdata_req *)buf;
	req->PRIM_type = T_UNITDATA_REQ;
	req->DEST_length = namelen;
	req->DEST_offset = sizeof(struct T_unitdata_req);
	req->OPT_length = 0;
	req->OPT_offset = 0;
	memcpy(buf + sizeof(struct T_unitdata_req), name, namelen);

	ctlptr.len = sizeof(struct T_unitdata_req) + namelen;
	ctlptr.maxlen = STREAM_BUF_SIZE;
	ctlptr.buf = buf;

	dataptr.len = len;
	dataptr.maxlen = len;
	dataptr.buf = (void *)msg;

	if ((ret = putmsg_timedwait(fd, &ctlptr, &dataptr, 0, timeout)) == OK) {
		ret = len;
	}
	return(ret);
}

/* ==========================================================================
 * sendto()
 */
ssize_t sendto(int fd, const void * msg, size_t len, int flags,
  const struct sockaddr *to, int to_len)
{
	return(sendto_timedwait(fd, msg, len, flags, to, to_len, NULL));
}

#endif

#if (!defined (HAVE_SYSCALL_SEND)) && defined (HAVE_STREAMS)

/* ==========================================================================
 * send_timedwait()
 */
ssize_t send_timedwait(int fd, const void * msg, size_t len, int flags, 
  struct timespec * timeout)
{
	char buf[STREAM_BUF_SIZE];
	struct T_unitdata_req * req;
	struct strbuf dataptr;
	struct strbuf ctlptr;
	ssize_t ret, prio;

	req = (struct T_unitdata_req *)buf;
	req->PRIM_type = T_UNITDATA_REQ;
	req->DEST_length = 0;
	req->DEST_offset = 0;
	req->OPT_length = 0;
	req->OPT_offset = 0;

	ctlptr.len = sizeof(struct T_unitdata_req);
	ctlptr.maxlen = STREAM_BUF_SIZE;
	ctlptr.buf = buf;

	dataptr.len = len;
	dataptr.maxlen = len;
	dataptr.buf = (void *)msg;

	if ((ret = putmsg_timedwait(fd, &ctlptr, &dataptr, 0, timeout)) == OK) {
		ret = len;
	}
	return(ret);
}

/* ==========================================================================
 * send()
 */
ssize_t send(int fd, const void * msg, size_t len, int flags)
{
	return(send_timedwait(fd, msg, len, flags, NULL));
}

#endif

#if (!defined (HAVE_SYSCALL_RECVFROM)) && defined(HAVE_STREAMS)

/* ==========================================================================
 * recvfrom_timedwait()
 */
ssize_t recvfrom_timedwait(int fd, void * msg, size_t len, int flags,
  struct sockaddr * name, int * namelen, struct timespec * timeout)
{
	char buf[STREAM_BUF_SIZE];
	struct T_unitdata_ind * ind;
	struct strbuf dataptr;
	struct strbuf ctlptr;
	int ret, prio;

	ctlptr.len = 0;
	ctlptr.maxlen = STREAM_BUF_SIZE;
	ctlptr.buf = buf;

	dataptr.maxlen = len;
	dataptr.len = 0;
	dataptr.buf = msg;
	
	prio = 0;

	ret = getmsg_timedwait(fd, &ctlptr, &dataptr, &prio, timeout);
	if (ret >= OK) {
		if (name != NULL) {
			ind = (struct T_unitdata_ind *)buf;

			if (*namelen > ind->SRC_length)
				*namelen = ind->SRC_length;
			memcpy(name, buf + ind->SRC_offset, *namelen);
		}
		ret = dataptr.len;
	}

	return(ret);
}

/* ==========================================================================
 * recvfrom()
 */
ssize_t recvfrom(int fd, void * buf, size_t len, int flags,
  struct sockaddr * from, int * from_len)
{
	return(recvfrom_timedwait(fd, buf, len, flags, from, from_len, NULL));
}

#endif

#if (!defined (HAVE_SYSCALL_RECV)) && defined(HAVE_STREAMS)

/* ==========================================================================
 * recv_timedwait()
 */
ssize_t recv_timedwait(int fd, void * msg, size_t len, int flags,
  struct timespec * timeout)
{
	char buf[STREAM_BUF_SIZE];
	struct T_unitdata_ind * ind;
	struct strbuf dataptr;
	struct strbuf ctlptr;
	int ret, prio;

	ctlptr.len = 0;
	ctlptr.maxlen = STREAM_BUF_SIZE;
	ctlptr.buf = buf;

	dataptr.maxlen = len;
	dataptr.len = 0;
	dataptr.buf = msg;
	
	prio = 0;

	ret = getmsg_timedwait(fd, &ctlptr, &dataptr, &prio, timeout);
	if (ret >= OK)
		ret = dataptr.len;

	return(ret);
}

/* ==========================================================================
 * recv()
 */
ssize_t recv(int fd, void * buf, size_t len, int flags,
  struct sockaddr * from, int * from_len)
{
	return(recv_timedwait(fd, buf, len, flags, NULL));
}

#endif

#if (!defined (HAVE_SYSCALL_SETSOCKOPT)) && defined(HAVE_STREAMS)
/* ==========================================================================
 * setsockopt()
 */
int setsockopt(int s, int level, int optname, const void *optval, int optlen)
{
	return(0);
}
#endif

struct foo {			/* Used by getsockname and getpeername */
	long	a;
	int	b;
	struct sockaddr *name;
};

#if (!defined (HAVE_SYSCALL_GETSOCKNAME)) && defined(HAVE_STREAMS)
/* ==========================================================================
 * getsockname()
 */


int getsockname(int s, struct sockaddr *name, int *namelen)
{
	struct foo foo;
	int i;
	if (*namelen < sizeof(struct sockaddr)) {
		SET_ERRNO(ENOMEM);
		return(-1);
	}
	foo.a = 0x84;
	foo.b = 0;
	foo.name = name;
	i = ioctl(s, TI_GETMYNAME, &foo);
	*namelen = foo.b;
	return(i);
}
#endif

#if (!defined (HAVE_SYSCALL_GETPEERNAME)) && defined(HAVE_STREAMS)
/* ==========================================================================
 * getpeername() ; Added by Monty
 */

int getpeername(int s, struct sockaddr *name, int *namelen)
{
	struct foo foo;
	int i;
	if (*namelen < sizeof(struct sockaddr)) {
		SET_ERRNO(ENOMEM);
		return(-1);
	}
	foo.a = 0x84;				/* Max length ? */
	foo.b = 0;				/* Return length */
	foo.name = name;			/* Return buffer */
	i = ioctl(s, TI_GETPEERNAME, &foo);
	*namelen = foo.b;
	return(i);
}
#endif


#if (!defined (HAVE_SYSCALL_SHUTDOWN)) && defined(HAVE_STREAMS)
/* ==========================================================================
 * shutdown()
 */

int shutdown(int s, int how)
{
	return(0);
}
#endif
