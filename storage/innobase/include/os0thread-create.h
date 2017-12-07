/*****************************************************************************

Copyright (c) 2017, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file include/os0thread-create.h
The interface to the threading wrapper

Created 2016-May-17 Sunny Bains
*******************************************************/

#ifndef os0thread_create_h
#define os0thread_create_h

#include "univ.i"
#include "os0thread.h"

#include <my_thread.h>
#include <atomic>
#include <functional>

/** Maximum number of threads inside InnoDB */
extern ulint	srv_max_n_threads;

/** Number of threads active. */
extern	std::atomic_int	os_thread_count;


/** Initializes OS thread management data structures. */
inline
void
os_thread_open()
{
	/* No op */
}

/** Check if there are threads active.
@return true if the thread count > 0. */
inline
bool
os_thread_any_active()
{
	return(os_thread_count.load(std::memory_order_relaxed) > 0);
}

/** Frees OS thread management data structures. */
inline
void
os_thread_close()
{
	if (os_thread_any_active()) {

		ib::warn()
			<< "Some ("
			<< os_thread_count.load(std::memory_order_relaxed)
			<< ") threads are still active";
	}
}

/** Wrapper for a callable, it will count the number of registered
Runnable instances and will register the thread executing the callable
with the PFS and the Server threading infrastructure. */
class Runnable {
public:
#ifdef UNIV_PFS_THREAD
	/** Constructor for the Runnable object.
	@param[in]	pfs_key		Performance schema key */
	explicit Runnable(mysql_pfs_key_t pfs_key) : m_pfs_key(pfs_key) { }
#else
	/** Constructor for the Runnable object.
	@param[in]	pfs_key		Performance schema key (ignored) */
	explicit Runnable(mysql_pfs_key_t) { }
#endif /* UNIV_PFS_THREAD */

public:
	/** Method to execute the callable
	@param[in]	f		Callable object
	@param[in]	args		Variable number of args to F */
	template<typename F, typename ... Args>
	void operator()(F&& f, Args&& ... args)
	{
		preamble();

		auto task = std::bind(
			std::forward<F>(f), std::forward<Args>(args) ...);

		task();

		epilogue();
	}
private:
	/** Register the thread with the server */
	void preamble()
	{
		my_thread_init();

#ifdef UNIV_PFS_THREAD
		PSI_thread*	psi;

		psi = PSI_THREAD_CALL(new_thread)(m_pfs_key.m_value, nullptr,
						  0);

		PSI_THREAD_CALL(set_thread_os_id)(psi);
		PSI_THREAD_CALL(set_thread)(psi);
#endif /* UNIV_PFS_THREAD */

		std::atomic_thread_fence(std::memory_order_release);

		int	old;

		old = os_thread_count.fetch_add(1, std::memory_order_relaxed);

		ut_a(old <= static_cast<int>(srv_max_n_threads) - 1);
	}

	/** Deregister the thread */
	void epilogue()
	{
		std::atomic_thread_fence(std::memory_order_release);

		int	old;

		old = os_thread_count.fetch_sub(1, std::memory_order_relaxed);
		ut_a(old > 0);

		my_thread_end();

#ifdef UNIV_PFS_THREAD
		PSI_THREAD_CALL(delete_current_thread)();
#endif /* UNIV_PFS_THREAD */
	}
private:
#ifdef UNIV_PFS_THREAD
	/** Performance schema key */
	const mysql_pfs_key_t		m_pfs_key;
#endif /* UNIV_PFS_THREAD */
};

/** Create a detached thread
@param[in]	pfs_key		Performance schema thread key
@param[in]	f		Callable instance
@param[in]	args		zero or more args */
template<typename F, typename ... Args>
void
create_detached_thread(mysql_pfs_key_t pfs_key, F&& f, Args&& ... args)
{
	std::thread     t(Runnable(pfs_key), f, args ...);

	t.detach();
}

#ifdef UNIV_PFS_THREAD
#define os_thread_create(...)		create_detached_thread(__VA_ARGS__)
#else
#define os_thread_create(k, ...)	create_detached_thread(0, __VA_ARGS__)
#endif /* UNIV_PFS_THREAD */

#endif /* !os0thread_create_h */

