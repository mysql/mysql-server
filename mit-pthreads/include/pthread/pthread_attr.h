/* ==== pthread_attr.h ========================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@mit.edu
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
 * $Id$
 *
 * Description : Basic pthread attributes header.
 *
 *  1.00 93/11/03 proven
 *      -Started coding this file.
 */

#define _POSIX_THREAD_ATTR_STACKSIZE

#define	PTHREAD_STACK_DEFAULT		65536

/* flags */
#define	PTHREAD_DETACHED			0x1
#define PTHREAD_SCOPE_SYSTEM		0x2
#define PTHREAD_INHERIT_SCHED		0x4
#define PTHREAD_NOFLOAT				0x8

#define	PTHREAD_CREATE_DETACHED		PTHREAD_DETACHED
#define	PTHREAD_CREATE_JOINABLE		0
#define PTHREAD_SCOPE_PROCESS		0
#define PTHREAD_EXPLICIT_SCHED		0

/*
 * New pthread attribute types.
 */
enum schedparam_policy {
	SCHED_RR,
	SCHED_IO,
	SCHED_FIFO,
	SCHED_OTHER
};

struct pthread_attr {
	enum schedparam_policy	schedparam_policy;
	int						sched_priority;

	int						flags;
	void *					arg_attr;
	void 					(*cleanup_attr)();
	void *					stackaddr_attr;
	size_t					stacksize_attr;
};

struct sched_param {
	int						sched_priority;
	void *					no_data;
};

/*
 * New functions
 */

__BEGIN_DECLS

#if defined(DCE_COMPAT)

typedef struct pthread_attr * pthread_attr_t;

int pthread_attr_create			__P_((pthread_attr_t *));
int pthread_attr_delete			__P_((pthread_attr_t *));

#else

typedef struct pthread_attr pthread_attr_t;

int	pthread_attr_init			__P_((pthread_attr_t *));
int	pthread_attr_destroy		__P_((pthread_attr_t *));
int	pthread_attr_setstacksize	__P_((pthread_attr_t *, size_t));
int	pthread_attr_getstacksize	__P_((pthread_attr_t *, size_t *));
int	pthread_attr_setstackaddr	__P_((pthread_attr_t *, void *));
int	pthread_attr_getstackaddr	__P_((pthread_attr_t *, void **));
int	pthread_attr_setdetachstate	__P_((pthread_attr_t *, int ));
int	pthread_attr_getdetachstate	__P_((pthread_attr_t *, int *));
int	pthread_attr_setscope		__P_((pthread_attr_t *, int ));
int	pthread_attr_getscope		__P_((pthread_attr_t *, int *));
int	pthread_attr_setinheritsched __P_((pthread_attr_t *, int ));
int	pthread_attr_getinheritsched __P_((pthread_attr_t *, int *));
int	pthread_attr_setschedpolicy __P_((pthread_attr_t *, int ));
int	pthread_attr_getschedpolicy __P_((pthread_attr_t *, int *));
int	pthread_attr_setschedparam 	__P_((pthread_attr_t *, struct sched_param *));
int	pthread_attr_getschedparam 	__P_((pthread_attr_t *, struct sched_param *));

int	pthread_attr_setfloatstate	__P_((pthread_attr_t *, int ));
int	pthread_attr_getfloatstate	__P_((pthread_attr_t *, int *));
int	pthread_attr_setcleanup		__P_((pthread_attr_t *, void (*routine)(void *),
								  void *));

#endif
		
__END_DECLS
