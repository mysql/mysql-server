
/* ==== machdep.c ============================================================
 * Copyright (c) 1995 by Chris Provenzano, proven@mit.edu
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
 *      products derived from this software without specific prior written
 *      permission.
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
 * Description : Machine dependent functions for SCO3.2v5 on i386
 *
 *	1.00 96/11/21 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "engine-i386-freebsd-2.0.c,v 1.1 1995/03/01 01:21:20 proven Exp";
#endif

#include <pthread.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <stropts.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/unistd.h>
#include <sys/utsname.h>
#include <sys/sysi86.h>

void machdep_sys_abort(char*fname,int lineno)

{
	char buf[128];

	sprintf(buf,"panic: %s => %d\n", fname, lineno);
	machdep_sys_write(1, buf, strlen(buf));
	abort();
}

#if 0
int setitimer(int which, struct itimerval* value, struct itimerval* ovalue)

{
	register int ret;
	if ((ret = machdep_sys_setitimer(which,value,ovalue))<0) {
	   errno = -ret;
	   return -1;
        }
	else {
	   return 0;
	}
}
#endif

/* ==========================================================================
 * machdep_save_state()
 */
int machdep_save_state(void)
{
    return(setjmp(pthread_run->machdep_data.machdep_state));
}

/* ==========================================================================
 * machdep_save_state()
 */
int machdep_save_float_state(struct pthread * pthread)
{
	char * fdata = (char *)pthread->machdep_data.machdep_float_state;
	__asm__ ("fsave %0"::"m" (*fdata));
}

/* ==========================================================================
 * machdep_restore_state()
 */
void machdep_restore_state(void)
{
    longjmp(pthread_run->machdep_data.machdep_state, 1);
}

/* ==========================================================================
 * machdep_restore_float_state()
 */
int machdep_restore_float_state(void)
{
	char * fdata = (char *)pthread_run->machdep_data.machdep_float_state;
	__asm__ ("frstor %0"::"m" (*fdata));
}

/* ==========================================================================
 * machdep_set_thread_timer()
 */
void machdep_set_thread_timer(struct machdep_pthread *machdep_pthread)
{
    if (machdep_sys_setitimer(ITIMER_VIRTUAL, &(machdep_pthread->machdep_timer), NULL)) {
        PANIC();
    }
}

/* ==========================================================================
 * machdep_unset_thread_timer()
 */
void machdep_unset_thread_timer(struct machdep_pthread *machdep_pthread)
{
    struct itimerval zeroval = { { 0, 0 }, { 0, 0 } };
	int ret;

	if (machdep_pthread) {
    	ret = machdep_sys_setitimer(ITIMER_VIRTUAL, &zeroval, 
		  &(machdep_pthread->machdep_timer));
	} else {
    	ret = machdep_sys_setitimer(ITIMER_VIRTUAL, &zeroval, NULL); 
    }

	if (ret) {
       	PANIC();
	}
}

/* ==========================================================================
 * machdep_pthread_cleanup()
 */
void *machdep_pthread_cleanup(struct machdep_pthread *machdep_pthread)
{
    return(machdep_pthread->machdep_stack);
}

/* ==========================================================================
 * machdep_pthread_start()
 */
void machdep_pthread_start(void)
{
	context_switch_done();
	pthread_sched_resume();

    /* Run current threads start routine with argument */
    pthread_exit(pthread_run->machdep_data.start_routine
      (pthread_run->machdep_data.start_argument));

    /* should never reach here */
    PANIC();
}

/* ==========================================================================
 * __machdep_stack_free()
 */ 
void __machdep_stack_free(void * stack)
{   
    free(stack);
}

/* ==========================================================================
 * __machdep_stack_alloc()
 */
void * __machdep_stack_alloc(size_t size)
{
    void * stack;
   
    return(malloc(size));
}  

/* ==========================================================================
 * __machdep_pthread_create()
 */
void __machdep_pthread_create(struct machdep_pthread *machdep_pthread,
  void *(* start_routine)(), void *start_argument, 
  long stack_size, long nsec, long flags)
{
    machdep_pthread->start_routine = start_routine;
    machdep_pthread->start_argument = start_argument;

    machdep_pthread->machdep_timer.it_value.tv_sec = 0;
    machdep_pthread->machdep_timer.it_interval.tv_sec = 0;
    machdep_pthread->machdep_timer.it_interval.tv_usec = 0;
    machdep_pthread->machdep_timer.it_value.tv_usec = nsec / 1000;

    setjmp(machdep_pthread->machdep_state);
    /*
     * Set up new stact frame so that it looks like it
     * returned from a longjmp() to the beginning of
     * machdep_pthread_start().
     */
    machdep_pthread->machdep_state[JB_PC] = (int)machdep_pthread_start;

    /* Stack starts high and builds down. */
    machdep_pthread->machdep_state[JB_SP] =
      (int)machdep_pthread->machdep_stack + stack_size;
}

/* ==========================================================================
 * machdep_sys_wait3() 
 */
machdep_sys_wait3(int * b, int c, int * d)
{
#if 0
        return(machdep_sys_wait4(0, b, c, d));
#else
	return -ENOSYS;
#endif
}
 
/* ==========================================================================
 * machdep_sys_fstat()
 */
machdep_sys_fstat(int f, struct stat* b)
{
	return machdep_sys_fxstat(0x33, f, b);
}

/* ==========================================================================
 * machdep_sys_dup2()
 */
machdep_sys_dup2(int a, int b)
{
	machdep_sys_close(b);
	return machdep_sys_fcntl(a, F_DUPFD, b);
}

/* ==========================================================================
 * machdep_sys_getdtablesize()
 */
machdep_sys_getdtablesize()

{
	register int ret;
	if ((ret = machdep_sys_sysconf(_SC_OPEN_MAX))<0)
	   PANIC();
	return ret;
}

/* ==========================================================================
 * machdep_sys_fchown()
 */
machdep_sys_fchown(int fd,uid_t owner,gid_t group)

{
    return -ENOSYS;
}

/* ==========================================================================
 * machdep_sys_fchmod()
 */
machdep_sys_fchmod(int fd,mode_t mode)

{
    return -ENOSYS;
}

/* ==========================================================================
 * machdep_sys_getdirentries()
 */
int machdep_sys_getdirentries(int fd, char * buf, int len, int * seek)
{
	return(machdep_sys_getdents(fd, buf, len));
}

/* ==========================================================================
 * SCO Socket calls are a bit different
 * ==========================================================================
 * machdep_sys_socket()
 */
int machdep_sys_socket(int domain, int type, int protocol)
{
	register int s, fd, ret;	
	struct socksysreq req;

	if ((s = machdep_sys_open("/dev/socksys", 0))<0)
           return s;

	req.args[0] = SO_SOCKET;
	req.args[1] = (int)domain;
	req.args[2] = (int)type;
	req.args[3] = (int)protocol;
	if ((fd = machdep_sys_ioctl(s, SIOCSOCKSYS, &req))<0) {
           machdep_sys_close(s);
	   return fd;
        }

	if ((ret=machdep_sys_dup2(fd, s))<0) {
	   machdep_sys_close(fd);
	   return ret;
	}

	machdep_sys_close(fd);
	return s;

}

/* ==========================================================================
 * machdep_sys_accept()
 */
int machdep_sys_accept(int s, struct sockaddr * b, int * c)
{
	struct socksysreq req;

	req.args[0] = SO_ACCEPT;
	req.args[1] = (int)s;
	req.args[2] = (int)b;
	req.args[3] = (int)c;

	return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

/* ==========================================================================
 * machdep_sys_bind()
 */
int machdep_sys_bind(int s, const struct sockaddr * b, int c)
{
	struct socksysreq req;

	req.args[0] = SO_BIND;
	req.args[1] = (int)s;
	req.args[2] = (int)b;
	req.args[3] = (int)c;

	return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

/* ==========================================================================
 * machdep_sys_connect()
 */
int machdep_sys_connect(int s, const struct sockaddr * b, int c)
{
	struct socksysreq req;

	req.args[0] = SO_CONNECT;
	req.args[1] = (int)s;
	req.args[2] = (int)b;
	req.args[3] = (int)c;

	return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

/* ==========================================================================
 * machdep_sys_listen()
 */
int machdep_sys_listen(int s, int backlog)
{
	struct socksysreq req;

	req.args[0] = SO_LISTEN;
	req.args[1] = (int)s;
	req.args[2] = (int)backlog;

	return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

/* ==========================================================================
 * machdep_sys_shutdown()
 */
int machdep_sys_shutdown(int s, int b)
{
    struct socksysreq req;

    req.args[0] = SO_SHUTDOWN;
    req.args[1] = (int)s;
    req.args[2] = (int)b;

    return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

/* ==========================================================================
 * machdep_sys_getsockopt()
 */
int machdep_sys_getsockopt(int s, int b, int c, char *d, int *e)
{
	struct socksysreq req;

	req.args[0] = SO_GETSOCKOPT;
	req.args[1] = (int)s;
	req.args[2] = (int)b;
	req.args[3] = (int)c;
	req.args[4] = (int)d;
	req.args[5] = (int)e;

	return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

/* ==========================================================================
 * machdep_sys_setsockopt()
 */
int machdep_sys_setsockopt(int s, int b, int c, char *d, int e)
{
	struct socksysreq req;

	req.args[0] = SO_SETSOCKOPT;
	req.args[1] = (int)s;
	req.args[2] = (int)b;
	req.args[3] = (int)c;
	req.args[4] = (int)d;
	req.args[5] = (int)e;

	return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

/* ==========================================================================
 * machdep_sys_getpeername()
 */
int machdep_sys_getpeername(int s, struct sockaddr *b, int *c)
{
	struct socksysreq req;

	req.args[0] = SO_GETPEERNAME;
	req.args[1] = (int)s;
	req.args[2] = (int)b;
	req.args[3] = (int)c;

	return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

/* ==========================================================================
 * machdep_sys_send()
 */
int machdep_sys_send(int s, char *b, int c, int d)
{
	struct socksysreq req;

	req.args[0] = SO_SEND;
	req.args[1] = (int)s;
	req.args[2] = (int)b;
	req.args[3] = (int)c;
	req.args[4] = (int)d;

	return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

/* ==========================================================================
 * machdep_sys_sendto()
 */
int machdep_sys_sendto(int s, char *b, int c, int d,
  struct sockaddr *e, int f)
{
	struct socksysreq req;

	req.args[0] = SO_SENDTO;
	req.args[1] = (int)s;
	req.args[2] = (int)b;
	req.args[3] = (int)c;
	req.args[4] = (int)d;
	req.args[5] = (int)e;
	req.args[6] = (int)f;

	return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

/* ==========================================================================
 * machdep_sys_recv()
 */
int machdep_sys_recv(int s, char *b, int c, int d)
{
	struct socksysreq req;

	req.args[0] = SO_RECV;
	req.args[1] = (int)s;
	req.args[2] = (int)b;
	req.args[3] = (int)c;
	req.args[4] = (int)d;

	return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

/* ==========================================================================
 * machdep_sys_recvfrom()
 */
int machdep_sys_recvfrom(int s, char *buf, int len, int flags,
  struct sockaddr *from, int *fromlen)
{
	struct socksysreq req;

	req.args[0] = SO_RECVFROM;
	req.args[1] = (int)s;
	req.args[2] = (int)buf;
	req.args[3] = (int)len;
	req.args[4] = (int)flags;
	req.args[5] = (int)from;
	req.args[6] = (int)fromlen;

	return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

/* ==========================================================================
 * machdep_sys_socketpair()
 */
int machdep_sys_socketpair(int d, int type, int protocol, int sv[2])

{
	register int s1, s2;
	register int ret;
	struct socksysreq req;

	if (d != AF_UNIX)
	   return -EPROTONOSUPPORT;
	if ((s1=machdep_sys_socket(d,type,protocol))<0) {
	   return s1;
	}
	if ((s2=machdep_sys_socket(d,type,protocol))<0) {
	   machdep_sys_close(s1);
	   return s2;
	}
	req.args[0] = SO_SOCKPAIR;
	req.args[1] = s1;
	req.args[2] = s2;
	if ((ret=machdep_sys_ioctl(s1,SIOCSOCKSYS,&req))<0) {
	   machdep_sys_close(s1);
	   machdep_sys_close(s2);
	   return ret;
	}
	sv[0] = s1;
	sv[1] = s2;
	return 0;
}

/* ==========================================================================
 * machdep_sys_getsockname()
 */
int machdep_sys_getsockname(int s, char * b, int * c)
{
    struct socksysreq req;

    req.args[0] = SO_GETSOCKNAME;
    req.args[1] = (int)s;
    req.args[2] = (int)b;
    req.args[3] = (int)c;

    return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

int machdep_sys_sendmsg(int s, const struct msghdr *msg, int flags)

{
    struct socksysreq req;

    req.args[0] = SO_SENDMSG;
    req.args[1] = (int)s;
    req.args[2] = (int)msg;
    req.args[3] = (int)flags;

    return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

int machdep_sys_recvmsg(int s, struct msghdr *msg, int flags)

{
    struct socksysreq req;

    req.args[0] = SO_RECVMSG;
    req.args[1] = (int)s;
    req.args[2] = (int)msg;
    req.args[3] = (int)flags;

    return(machdep_sys_ioctl(s, SIOCSOCKSYS, &req));
}

u_short ntohs(u_short n)

{
    union {
       unsigned char u_nc[4];
       u_short u_ns;
    } ns;
    register unsigned char* p = &ns.u_nc[0];

    ns.u_ns = n;
    return (p[0]<<8)|p[1];
}

u_short htons(u_short h)

{
    union {
       unsigned char u_nc[2];
       u_short u_ns;
    } ns;
    register unsigned char* p = &ns.u_nc[0];
    p[0] = (h>>8)&0xFF;
    p[1] = (h&0xFF);
    return ns.u_ns;
}


u_long ntohl(u_long n)

{
    union {
       unsigned char u_nc[4];
       u_long u_nl;
    } nl;
    register unsigned char* p = &nl.u_nc[0];

    nl.u_nl = n;
    return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
}

u_long htonl(u_long h)

{
    union {
       unsigned char u_nc[4];
       u_long u_nl;
    } nl;
    register unsigned char* p = &nl.u_nc[0];
    p[0] = (h>>24)&0xFF;
    p[1] = (h>>16)&0xFF;
    p[2] = (h>>8)&0xFF;
    p[3] = (h&0xFF);
    return nl.u_nl;
}

int getdomainname(char* domain,int len)

{
	/* edi = len */
    struct socksysreq req;
    register int ret, fd;
    if (len>MAXHOSTNAMELEN)
       len = MAXHOSTNAMELEN;

    if ((fd = machdep_sys_open("/dev/socksys", 0)) < 0)
	 return fd;

    req.args[0] = SO_GETIPDOMAIN;
    req.args[1] = (int)domain;
    req.args[2] = (int)len;
    if((ret=machdep_sys_ioctl(fd, SIOCSOCKSYS, &req))<0) {
       machdep_sys_close(fd);
       return ret;
    }

    machdep_sys_close(fd);
    domain[len-1] = '\0';
    return 0;
}

int gethostname(char* name, int namelen)

{
	struct utsname uts;
	register int ret, len;
	char domain[MAXHOSTNAMELEN+1];

	if (name==NULL)
	   return -EFAULT;
	if ((ret=machdep_sys_uname(&uts))<0)
 	   return ret;
	if (namelen<(len=strlen(uts.nodename)))
	   return -EFAULT;
	strncpy(name,uts.nodename,len);
	if (namelen>len)
	   name[len] = '\0';
	if ((ret=getdomainname(domain, namelen - len))<0)
	   return ret;
	if (domain[0]=='\0')
	   return 0;
	if (len + strlen(domain) + 2 > namelen)
	   return -EFAULT;
	strcat(name, ".");
	strcat(name, domain);
	return 0;
}

int gettimeofday(struct timeval* tp, struct timezone* tz)

{
	register int ret;
	if ((ret = machdep_sys_gettimeofday(tp, NULL))<0) {
	   errno = -ret;
	   return -1;
        }
	else {
	   return 0;
	}
}

int kill(pid_t pid, int signo)

{
	register int ret;
	if ((ret = machdep_sys_kill(pid,signo))<0) {
	   errno = -ret;
	   return -1;
        }
	else {
	   return 0;
	}
}

typedef void (*signal_t(int signo, void (*func)(int)))(int);

signal_t* _libc_signal = NULL;

void (*signal(int signo, void (*func)(int)))(int)

{
	int ret;
	void (*oldfunc)(int);
	extern void (*machdep_sys_signal(int signo, void (*func)(int),int* r))(int);
	if (_libc_signal!=NULL)
	   return (*_libc_signal)(signo, func);

	oldfunc = machdep_sys_signal(signo, func, &ret);
	if (ret!=0) {
	   errno = ret;
	   return SIG_ERR;
	}
	else {
	   return oldfunc;
	}
}

int (*_libc_sigaction)(int ,const struct sigaction *, struct sigaction *) = NULL;
int sigaction(int sig,const struct sigaction *act, struct sigaction *oact)

{
	register int ret;
	if (_libc_sigaction!=NULL)
	   return (*_libc_sigaction)(sig,act,oact);
	if ((ret = machdep_sys_sigaction(sig,act,oact))<0) {
	   errno = -ret;
	   return -1;
	}
	else {
	   return 0;
	}
}

int (*_libc_sigprocmask)(int, const sigset_t *, sigset_t *) = NULL;

int sigprocmask(int how, const sigset_t *set, sigset_t * oset)

{
	register int ret;
	if (_libc_sigprocmask!=NULL)
	   return (*_libc_sigprocmask)(how,set,oset);
	if ((ret = machdep_sys_sigprocmask(how,set,oset))<0) {
	   errno = -ret;
	   return -1;
	}
	else {
	   return 0;
	}
}

int (*_libc_sigsuspend)(const sigset_t *) = NULL;

int sigsuspend(const sigset_t *set)
{
	register int ret;
	if (_libc_sigsuspend!=NULL)
	   return (*_libc_sigsuspend)(set);
	if ((ret = machdep_sys_sigsuspend(set))<0) {
	   errno = -ret;
	   return -1;
	}
	else {
	   return 0;
	}
}

int _sigrelse(sig)
int sig;

{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, sig);
    return sigprocmask(SIG_UNBLOCK,&mask,NULL);
}

int _sighold(sig)
int sig;

{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, sig);
    return sigprocmask(SIG_BLOCK,&mask,NULL);
}

void (*sigset(int sig, void (*func)(int)))(int)
{
    return signal(sig, func);
}


int (*_libc_getmsg)(int , struct strbuf *, struct strbuf *, int *) = NULL;

int getmsg(int fd, struct strbuf * ctlptr, struct strbuf * dataptr,
  int * flags)
{
	register int ret;
	if (_libc_getmsg != NULL)
	   return (*_libc_getmsg)(fd,ctlptr,dataptr,flags);
	else if ((ret=machdep_sys_getmsg(fd,ctlptr,dataptr,flags))<0) {
	   errno = -ret;
	   return -1;
	}
	else
	   return ret;
}

int (*_libc_putmsg)(int , const struct strbuf *, const struct strbuf *, int) = NULL;

int putmsg(int fd, const struct strbuf * ctlptr, const struct strbuf * dataptr,
  int flags)
{
	register int ret;
	if (_libc_putmsg != NULL)
	   return (*_libc_putmsg)(fd,ctlptr,dataptr,flags);
	else if ((ret=machdep_sys_putmsg(fd,ctlptr,dataptr,flags))<0) {
	   errno = -ret;
	   return -1;
	}
	else
	   return ret;
}

int ftime(struct timeb* tp)

{
	register int ret;
	if ((ret=machdep_sys_ftime(tp))<0) {
		errno = -ret;
		return NOTOK;
	}
	return 0;
}

int getpagesize()

{
	register int ret;
#if 0
	if ((ret = machdep_sys_sysconf(_SC_PAGE_SIZE))<0) {
	   PANIC();
	   SET_ERRNO(-ret);
	   return -1;
	}
	else {
	   return 0;
	}
#else
	return PAGESIZE;
#endif
}

static 	pthread_mutex_t machdep_mutex = 
{ MUTEX_TYPE_COUNTING_FAST, PTHREAD_QUEUE_INITIALIZER, \
	 NULL, SEMAPHORE_CLEAR, { NULL }, MUTEX_FLAGS_INITED };

static 	pthread_mutex_t malloc_mutex = 
{ MUTEX_TYPE_COUNTING_FAST, PTHREAD_QUEUE_INITIALIZER, \
	 NULL, SEMAPHORE_CLEAR, { NULL }, MUTEX_FLAGS_INITED };

struct stdlock {
    volatile long init;
    pthread_mutex_t* mutex;
};

static void machdep_stdinitlock(struct stdlock* lock)

{
	if (lock==0) PANIC();
	pthread_mutex_lock(&machdep_mutex);
	if (!lock->init) {
	   register pthread_mutex_t* mutex;
	   pthread_mutexattr_t attr;

	   lock->init = 1;
	   lock->mutex = &machdep_mutex;
	   mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
	   pthread_mutexattr_init (&attr);
	   pthread_mutexattr_settype (&attr, MUTEX_TYPE_COUNTING_FAST);
	   pthread_mutex_init(mutex, &attr);
	   lock->mutex = mutex;
	}
	pthread_mutex_unlock(&machdep_mutex);
}

void machdep_stdlock(struct stdlock* lock)

{
	if (lock==0) PANIC();
	if (!lock->init)
	   machdep_stdinitlock(lock);
	pthread_mutex_lock(lock->mutex);
}

void machdep_stdunlock(struct stdlock* lock)

{
	if (lock==0) PANIC();
	if (!lock->init)
	   machdep_stdinitlock(lock);
	pthread_mutex_unlock(lock->mutex);
}

int machdep_stdtrylock(struct stdlock* lock)

{
	if (lock==0) PANIC();
	if (!lock->init)
	   machdep_stdinitlock(lock);
	return pthread_mutex_trylock(lock->mutex);
}

int machdep_stdtryunlock(struct stdlock* lock)

{
	if (lock==0) PANIC();
	if (!lock->init)
	   machdep_stdinitlock(lock);
	if (pthread_mutex_trylock(lock->mutex))
	   return pthread_mutex_unlock(lock->mutex);
	return 0;
}

extern void (*_libc_stdlock)(struct stdlock* lock);
extern void (*_libc_stdunlock)(struct stdlock* lock);
extern int (*_libc_stdtrylock)(struct stdlock* lock);
extern int (*_libc_stdtryunlock)(struct stdlock* lock);

int machdep_sys_init()

{
	typedef void (*voidfunc_t)();
	extern voidfunc_t _libc_read;
	extern voidfunc_t _libc_write;
	extern voidfunc_t _libc_readv;
	extern voidfunc_t _libc_writev;
	extern voidfunc_t _libc_open;
	extern voidfunc_t _libc_close;
	extern voidfunc_t _libc_fork;
	extern voidfunc_t _libc_fcntl;
	extern voidfunc_t _libc_dup;
	extern voidfunc_t _libc_pipe;
	extern voidfunc_t _libc_select;
	extern voidfunc_t _libc_malloc;
	extern voidfunc_t _libc_realloc;
	extern voidfunc_t _libc_free;
	extern ssize_t pthread_read (int , char*, int );
	extern ssize_t pthread_write (int , char*, int );
	extern int pthread_close (int);
	extern int pthread_dup (int);
	extern int pthread_fork ();
	extern int pthread_pipe (int*);
	extern int pthread_fcntl(int, int, ...);
	extern int pthread_open(const char *, int, ...);
	extern ssize_t pthread_readv (int , const struct iovec *, int );
	extern ssize_t pthread_writev (int , const struct iovec *, int );
	extern int pthread_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
	extern int pthread_getmsg(int , struct strbuf *, struct strbuf *,int*);
	extern int pthread_putmsg(int , const struct strbuf *, const struct strbuf *,int);
	extern void (*pthread_signal(int , void (*)(int)))(int);
	extern int pthread_sigaction(int,const struct sigaction *, struct sigaction *);
	extern int pthread_sigprocmask(int, const sigset_t *, sigset_t *);
	extern int pthread_sigsuspend(const sigset_t *);


	static struct {
		voidfunc_t *p;
		voidfunc_t f;
	} maptable[] = {
		{(voidfunc_t*)&_libc_read, (voidfunc_t) pthread_read},
		{(voidfunc_t*)&_libc_write, (voidfunc_t) pthread_write},
		{(voidfunc_t*)&_libc_readv, (voidfunc_t) pthread_readv},
		{(voidfunc_t*)&_libc_writev, (voidfunc_t) pthread_writev},
		{(voidfunc_t*)&_libc_open, (voidfunc_t) pthread_open},
		{(voidfunc_t*)&_libc_close, (voidfunc_t) pthread_close},
		{(voidfunc_t*)&_libc_fork, (voidfunc_t) pthread_fork},
		{(voidfunc_t*)&_libc_fcntl, (voidfunc_t) pthread_fcntl},
		{(voidfunc_t*)&_libc_dup, (voidfunc_t) pthread_dup},
		{(voidfunc_t*)&_libc_pipe, (voidfunc_t) pthread_pipe},
		{(voidfunc_t*)&_libc_select, (voidfunc_t) pthread_select},
		{(voidfunc_t*)&_libc_getmsg, (voidfunc_t) pthread_getmsg},
		{(voidfunc_t*)&_libc_putmsg, (voidfunc_t) pthread_putmsg},
		{(voidfunc_t*)&_libc_signal, (voidfunc_t) pthread_signal},
		{(voidfunc_t*)&_libc_sigaction, (voidfunc_t) pthread_sigaction},
		{(voidfunc_t*)&_libc_sigprocmask, (voidfunc_t) pthread_sigprocmask},
		{(voidfunc_t*)&_libc_sigsuspend, (voidfunc_t) pthread_sigsuspend},
		{(voidfunc_t*) 0, (voidfunc_t) 0}
	}; 
	register int i;

	for (i=0; maptable[i].p; i++)
	    *maptable[i].p = maptable[i].f;

        _libc_stdlock = machdep_stdlock;
        _libc_stdunlock = machdep_stdunlock;
        _libc_stdtrylock = machdep_stdtrylock;
        _libc_stdtryunlock = machdep_stdtryunlock;
	return 0;
}

#if 0
extern end;
char* nd = (char*) &end;
char* brk(const char* endds)

{
	register int ret;

	if ((ret = machdep_sys_brk((char*)endds))<0) {
	   SET_ERRNO(-ret);
	   return (char*) -1;
	}
	else {
	   nd = (char*) endds;
	   return 0;
	}
}

char *sbrk(int incr)

{
	register char* ret;
	if (incr!=0 && (ret=brk(nd + incr))!=0)
	   return ret;
	else
	   return nd - incr;
}
#endif

sigset_t sigmask(int sig)

{
    sigset_t oset;
    sigemptyset(&oset);
    sigaddset(&oset, sig);
    return oset;
}

sigset_t sigsetmask(sigset_t set)

{
     sigset_t oset;
     sigprocmask(SIG_SETMASK,&set,&oset);
     return oset;
}

sigset_t sigblock(sigset_t set)

{
     sigset_t oset;
     sigprocmask(SIG_BLOCK,&set,&oset);
     return oset;
}
