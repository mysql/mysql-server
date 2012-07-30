/*****************************************************************************

Copyright (c) 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/ib_mutex.h
Policy based mutexes.

Created 2012-03-24 Sunny Bains.
***********************************************************************/

#ifndef ib_mutex_h
#define ib_mutex_h

#include "univ.i"
#include "sync0sync.h"

#ifdef UNIV_PFS_MUTEX
/** InnoDB default policy */
struct CheckedPolicy {
	CheckedPolicy(ulint level, mysql_pfs_key_t key)
		:
		m_level(level),
		m_key(key) { }
	ulint			m_level;	/*!< Mutex ordering */
	mysql_pfs_key_t		m_key;		/*!< Performance schema key */
};
#else
/** InnoDB default policy */
struct CheckedPolicy {
	CheckedPolicy(ulint level)
		:
		m_level(level) { }
	ulint			m_level;	/*!< Mutex ordering */
};
#endif /* UNIV_PFS_MUTEX */

/** InnoDB default mutex. */
class ClassicMutex {
public:
	ClassicMutex(const CheckedPolicy& policy) UNIV_NOTHROW {
		mutex_create(policy.m_key, &m_mutex, policy.m_level);
	}

	~ClassicMutex() { mutex_free(&m_mutex); }

	/** Release the muytex. */
	void exit() UNIV_NOTHROW { mutex_exit(&m_mutex); }

	/** Acquire the mutex. */
	void enter() UNIV_NOTHROW { mutex_enter(&m_mutex); }

	/** Try and lock the mutex, return 0 on SUCCESS and -1 on error. */
	int try_lock() UNIV_NOTHROW { return(mutex_enter_nowait(&m_mutex)); }

#ifdef UNIV_DEBUG
	/** @return true if the thread owns the mutex. */
	bool is_owned() UNIV_NOTHROW { return(mutex_own(&m_mutex)); }
#endif /* UNIV_DEBUG */

private:
	ib_mutex_t		m_mutex;
};

/** Mutex that doesnot wait on an event and only spins. */
class SpinOnlyMutex {
public:
	SpinOnlyMutex(const CheckedPolicy& policy) UNIV_NOTHROW {
		mutex_create(policy.m_key, &m_mutex, policy.m_level);
	}

	~SpinOnlyMutex() { mutex_free(&m_mutex); }

	/** Release the muytex. */
	void exit() UNIV_NOTHROW { mutex_exit(&m_mutex); }

	/** Acquire the mutex. */
	void enter() UNIV_NOTHROW { mutex_enter_spinonly(&m_mutex); }

	/** Try and lock the mutex, return 0 on SUCCESS and -1 on error. */
	int try_lock() UNIV_NOTHROW { return(mutex_enter_nowait(&m_mutex)); }

#ifdef UNIV_DEBUG
	/** @return true if the thread owns the mutex. */
	bool is_owned() UNIV_NOTHROW { return(mutex_own(&m_mutex)); }
#endif /* UNIV_DEBUG */
private:
	/** The mutex implementation */
	ib_mutex_t		m_mutex;
};

/** Mutex interface for all policy mutexes. */
template <typename MutexImpl, typename Policy>
class PolicyMutex {
public:
	PolicyMutex(const Policy& policy) UNIV_NOTHROW : m_impl(policy) { }
	~PolicyMutex() { }

	/** Release the mutex. */
	void exit() UNIV_NOTHROW { m_impl.exit(); }

	/** Acquire the mutex. */
	void enter() UNIV_NOTHROW { m_impl.enter(); }

	/** Try and lock the mutex, return 0 on SUCCESS and -1 on error. */
	int trylock() UNIV_NOTHROW { return(m_impl.try_lock()); }

#ifdef UNIV_DEBUG
	/** @return true if the thread owns the mutex. */
	bool is_owned() UNIV_NOTHROW { return(m_impl.is_owned()); }
#endif /* UNIV_DEBUG */

private:
	/** The mutex implementation */
	MutexImpl	m_impl;
};

typedef PolicyMutex<ClassicMutex, CheckedPolicy>  Mutex;
typedef PolicyMutex<SpinOnlyMutex, CheckedPolicy>  SpinMutex;

#endif /* ib_mutex_h */
