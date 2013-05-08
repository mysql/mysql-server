/*****************************************************************************

Copyright (c) 2012, 2013, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/******************************************************************//**
@file include/ut0mutex.h
Policy based mutexes.

Created 2012-03-24 Sunny Bains.
***********************************************************************/

#ifndef UNIV_INNOCHECKSUM

#ifndef ut0mutex_h
#define ut0mutex_h

extern ulong	srv_spin_wait_delay;
extern ulong	srv_n_spin_wait_rounds;
extern ulong 	srv_force_recovery_crash;

#include "os0atomic.h"
#include "sync0policy.h"
#include "ib0mutex.h"

#ifndef UNIV_DEBUG

# ifdef HAVE_IB_LINUX_FUTEX
typedef PolicyMutex<TTASFutexMutex<NoPolicy> >  FutexMutex;
# endif /* HAVE_IB_LINUX_FUTEX */

typedef PolicyMutex<TTASEventMutex<TrackPolicy> > SyncArrayMutex;
typedef PolicyMutex<TTASMutex<NoPolicy> > SpinMutex;
typedef PolicyMutex<OSTrackMutex<NoPolicy> > SysMutex;
typedef PolicyMutex<OSBasicMutex<NoPolicy> > EventMutex;

#else /* !UNIV_DEBUG */

# ifdef HAVE_IB_LINUX_FUTEX
typedef PolicyMutex<TTASFutexMutex<DebugPolicy> > FutexMutex;
# endif /* HAVE_IB_LINUX_FUTEX */

typedef PolicyMutex<TTASEventMutex<DebugPolicy> > SyncArrayMutex;
typedef PolicyMutex<TTASMutex<DebugPolicy> > SpinMutex;
typedef PolicyMutex<OSTrackMutex<DebugPolicy> > SysMutex;
typedef PolicyMutex<OSBasicMutex<DebugPolicy> > EventMutex;

#endif /* !UNIV_DEBUG */

#ifdef MUTEX_FUTEX
/** The default mutex type. */
typedef FutexMutex ib_mutex_t;
#define MUTEX_TYPE	"Uses futexes"
#elif defined(MUTEX_SYS)
typedef SysMutex ib_mutex_t;
#define MUTEX_TYPE	"Uses system mutexes"
#elif defined(MUTEX_EVENT)
typedef SyncArrayMutex ib_mutex_t;
#define MUTEX_TYPE	"Uses event mutexes"
#else
#error "ib_mutex_t type is unknown"
#endif /* MUTEX_FUTEX */

#include "ut0mutex.ic"

#endif /* ut0mutex_h */

#endif /* !UNIV_INNOCHECKSUM */
