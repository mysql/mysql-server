/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2006-03-22	Paul McCullagh
 *
 * H&G2JCtL
 *
 * This file contains windows specific code.
 */

#ifndef __win_xt_h__
#define __win_xt_h__

#ifdef XT_WIN
#include <windef.h>
#include <my_pthread.h>
#else
#include <pthread.h>
#endif

#include "locklist_xt.h"

#ifdef DEBUG
//#define DEBUG_LOCKING
#endif

#define xt_cond_struct			_opaque_pthread_cond_t
#define xt_cond_type			pthread_cond_t

#define xt_cond_wait			pthread_cond_wait
#define xt_cond_wakeall			pthread_cond_broadcast

#ifdef	__cplusplus
extern "C" {
#endif
void	xt_p_init_threading(void);
int		xt_p_set_normal_priority(pthread_t thr);
int		xt_p_set_low_priority(pthread_t thr);
int		xt_p_set_high_priority(pthread_t thr);
#ifdef	__cplusplus
}
#endif

#ifdef XT_WIN

#ifdef	__cplusplus
extern "C" {
#endif

typedef LPVOID pthread_key_t;

typedef struct xt_mutex_struct {
	CRITICAL_SECTION	mt_cs;
#ifdef XT_THREAD_LOCK_INFO
	const char		   *mt_name;
	XTThreadLockInfoRec mt_lock_info;
#endif
} xt_mutex_type;

typedef struct xt_rwlock_struct {
  xt_mutex_type			rw_ex_lock;
  xt_mutex_type			rw_sh_lock;
  pthread_cond_t		rw_sh_cond;
  int					rw_sh_count;
  int					rw_ex_count;
  int					rw_sh_complete_count;
  int					rw_magic;
#ifdef XT_THREAD_LOCK_INFO
	const char		   *rw_name;
	XTThreadLockInfoRec rw_lock_info;
#endif
} xt_rwlock_type;

#ifdef XT_THREAD_LOCK_INFO
int xt_p_mutex_init(xt_mutex_type *mutex, const pthread_mutexattr_t *attr, const char *name);
#else
int xt_p_mutex_init(xt_mutex_type *mutex, const pthread_mutexattr_t *attr);
#endif
int xt_p_mutex_destroy(xt_mutex_type *mutex);
int xt_p_mutex_lock(xt_mutex_type *mx);
int xt_p_mutex_unlock(xt_mutex_type *mx);
int xt_p_mutex_trylock(xt_mutex_type *mutex);

#ifdef XT_THREAD_LOCK_INFO
int xt_p_rwlock_init(xt_rwlock_type *rwlock, const pthread_condattr_t *attr, const char *name);
#else
int xt_p_rwlock_init(xt_rwlock_type *rwlock, const pthread_condattr_t *attr);
#endif
int		xt_p_rwlock_destroy(xt_rwlock_type *rwlock);
int		xt_p_rwlock_rdlock(xt_rwlock_type *mx);
int		xt_p_rwlock_wrlock(xt_rwlock_type *mx);
xtBool	xt_p_rwlock_try_wrlock(xt_rwlock_type *rwl);
int		xt_p_rwlock_unlock(xt_rwlock_type *mx);

int		xt_p_cond_wait(xt_cond_type *cond, xt_mutex_type *mutex);
int		xt_p_cond_timedwait(xt_cond_type *cond, xt_mutex_type *mutex, struct timespec *abstime);

int xt_p_join(pthread_t thread, void **value);

#ifdef	__cplusplus
}
#endif

#ifdef XT_THREAD_LOCK_INFO
#define xt_p_rwlock_init_with_name(a,b,c)   xt_p_rwlock_init(a,b,c)
#define xt_p_rwlock_init_with_autoname(a,b) xt_p_rwlock_init_with_name(a,b,LOCKLIST_ARG_SUFFIX(a))
#else
#define xt_p_rwlock_init_with_name(a,b,c)   xt_p_rwlock_init(a,b,c)
#define xt_p_rwlock_init_with_autoname(a,b) xt_p_rwlock_init(a,b)
#endif

#define xt_slock_rwlock_ns		xt_p_rwlock_rdlock
#define xt_xlock_rwlock_ns		xt_p_rwlock_wrlock
#define xt_xlock_try_rwlock_ns	xt_p_rwlock_try_wrlock
#define xt_unlock_rwlock_ns		xt_p_rwlock_unlock

#ifdef XT_THREAD_LOCK_INFO
#define xt_p_mutex_init_with_name(a,b,c)   xt_p_mutex_init(a,b,c)
#define xt_p_mutex_init_with_autoname(a,b) xt_p_mutex_init_with_name(a,b,LOCKLIST_ARG_SUFFIX(a))
#else
#define xt_p_mutex_init_with_name(a,b,c)   xt_p_mutex_init(a,b)
#define xt_p_mutex_init_with_autoname(a,b) xt_p_mutex_init(a,b)
#endif
#define xt_lock_mutex_ns		xt_p_mutex_lock
#define xt_unlock_mutex_ns		xt_p_mutex_unlock
#define xt_mutex_trylock		xt_p_mutex_trylock

#else // XT_WIN

/* Finger weg! */
#ifdef pthread_mutex_t
#undef pthread_mutex_t
#endif
#ifdef pthread_rwlock_t
#undef pthread_rwlock_t
#endif
#ifdef pthread_mutex_init
#undef pthread_mutex_init
#endif
#ifdef pthread_mutex_destroy
#undef pthread_mutex_destroy
#endif
#ifdef pthread_mutex_lock
#undef pthread_mutex_lock
#endif
#ifdef pthread_mutex_unlock
#undef pthread_mutex_unlock
#endif
#ifdef pthread_cond_wait
#undef pthread_cond_wait
#endif
#ifdef pthread_cond_broadcast
#undef pthread_cond_broadcast
#endif
#ifdef pthread_mutex_trylock
#undef pthread_mutex_trylock
#endif

/*
 * -----------------------------------------------------------------------
 * Reedefinition of pthread locking, for debugging
 */

struct XTThread;


#ifdef XT_THREAD_LOCK_INFO

#define xt_p_mutex_init_with_name(a,b,c)   xt_p_mutex_init(a,b,c)
#define xt_p_mutex_init_with_autoname(a,b) xt_p_mutex_init_with_name(a,b,LOCKLIST_ARG_SUFFIX(a))

#define xt_p_rwlock_init_with_name(a,b,c)   xt_p_rwlock_init(a,b,c)
#define xt_p_rwlock_init_with_autoname(a,b) xt_p_rwlock_init_with_name(a,b,LOCKLIST_ARG_SUFFIX(a))

#else

#define xt_p_mutex_init_with_name(a,b,c)   xt_p_mutex_init(a,b)
#define xt_p_mutex_init_with_autoname(a,b) xt_p_mutex_init(a,b)

#define xt_p_rwlock_init_with_name(a,b,c)   xt_p_rwlock_init(a,b)
#define xt_p_rwlock_init_with_autoname(a,b) xt_p_rwlock_init_with_name(a,b)

#endif

#ifdef DEBUG_LOCKING

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct xt_mutex_struct {
	unsigned short				mu_init;
	unsigned short				mu_trace;
	unsigned int				mu_line;
	const char					*mu_file;
	struct XTThread				*mu_locker;
	pthread_mutex_t				mu_plock;
#ifdef XT_THREAD_LOCK_INFO
	const char		   			*mu_name;
	XTThreadLockInfoRec 		mu_lock_info;
#endif
} xt_mutex_type;

typedef struct xt_rwlock_struct {
	u_int						rw_init;
	volatile u_int				rw_readers;
	struct XTThread				*rw_locker;
	pthread_rwlock_t			rw_plock;
#ifdef XT_THREAD_LOCK_INFO
	const char		   			*rw_name;
	XTThreadLockInfoRec 		rw_lock_info;
#endif
} xt_rwlock_type;

int		xt_p_rwlock_rdlock(xt_rwlock_type *mx);
int		xt_p_rwlock_wrlock(xt_rwlock_type *mx);
xtBool	xt_p_rwlock_try_wrlock(xt_rwlock_type *mx);
int		xt_p_rwlock_unlock(xt_rwlock_type *mx);

int xt_p_mutex_lock(xt_mutex_type *mx, u_int line, const char *file);
int xt_p_mutex_unlock(xt_mutex_type *mx);
int xt_p_mutex_trylock(xt_mutex_type *mutex);
int xt_p_mutex_destroy(xt_mutex_type *mutex);
#ifdef XT_THREAD_LOCK_INFO
int xt_p_mutex_init(xt_mutex_type *mutex, const pthread_mutexattr_t *attr, const char *name);
#else
int xt_p_mutex_init(xt_mutex_type *mutex, const pthread_mutexattr_t *attr);
#endif
int xt_p_rwlock_destroy(xt_rwlock_type * rwlock);
#ifdef XT_THREAD_LOCK_INFO
int xt_p_rwlock_init(xt_rwlock_type *rwlock, const pthread_rwlockattr_t *attr, const char *name);
#else
int xt_p_rwlock_init(xt_rwlock_type *rwlock, const pthread_rwlockattr_t *attr);
#endif
int xt_p_cond_wait(xt_cond_type *cond, xt_mutex_type *mutex);
int xt_p_cond_timedwait(xt_cond_type *cond, xt_mutex_type *mutex, const struct timespec *abstime);

#ifdef	__cplusplus
}
#endif

#define xt_slock_rwlock_ns			xt_p_rwlock_rdlock
#define xt_xlock_rwlock_ns			xt_p_rwlock_wrlock
#define xt_xlock_try_rwlock_ns		xt_p_rwlock_try_wrlock
#define xt_unlock_rwlock_ns			xt_p_rwlock_unlock

#define xt_lock_mutex_ns(x)			xt_p_mutex_lock(x, __LINE__, __FILE__)
#define xt_unlock_mutex_ns			xt_p_mutex_unlock
#define xt_mutex_trylock			xt_p_mutex_trylock

#else // DEBUG_LOCKING

#define xt_rwlock_struct			_opaque_pthread_rwlock_t
#define xt_mutex_struct				_opaque_pthread_mutex_t

#define xt_rwlock_type				pthread_rwlock_t
#define xt_mutex_type				pthread_mutex_t

#define xt_slock_rwlock_ns			pthread_rwlock_rdlock
#define xt_xlock_rwlock_ns			pthread_rwlock_wrlock
#define xt_xlock_try_rwlock_ns(x)	(pthread_rwlock_trywrlock(x) == 0)
#define xt_unlock_rwlock_ns			pthread_rwlock_unlock

#define xt_lock_mutex_ns			pthread_mutex_lock
#define xt_unlock_mutex_ns			pthread_mutex_unlock
#define xt_mutex_trylock			pthread_mutex_trylock

#define xt_p_mutex_trylock			pthread_mutex_trylock
#define xt_p_mutex_destroy			pthread_mutex_destroy
#define xt_p_mutex_init				pthread_mutex_init
#define xt_p_rwlock_destroy			pthread_rwlock_destroy
#define xt_p_rwlock_init			pthread_rwlock_init
#define xt_p_cond_wait				pthread_cond_wait
#define xt_p_cond_timedwait			pthread_cond_timedwait

#endif // DEBUG_LOCKING

#define xt_p_join				pthread_join

#endif // XT_WIN

#endif
