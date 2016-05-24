/*****************************************************************************

Copyright (c) 1995, 2016, Oracle and/or its affiliates. All Rights Reserved.

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

#include <my_thread.h>
#include "univ.i"
#include "os0thread.h"
#include <future>

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

/** Frees OS thread management data structures. */
inline
void
os_thread_close()
{
	if (os_thread_count.load(std::memory_order_relaxed) != 0) {

		ib::warn()
			<< "Some ("
			<< os_thread_count.load(std::memory_order_relaxed)
			<< ") "
			<< " threads are still active";
	}
}

/** Check if there are threads active.
@return true if the thread count > 0. */
inline
bool
os_thread_any_active()
{
	return(os_thread_count.load(std::memory_order_relaxed) > 0);
}

/** Wrapper for a callable, it will count the number of registered
Runnable instances and will register the thread executing the callable
with the PFS and the Server threading infrastructure. */
class Runnable{
public:
#ifdef UNIV_PFS_THREAD
	/** Constructor for the Runnable object.
	@param[in]	pfs_key		Performance schema key */
	explicit Runnable(mysql_pfs_key_t pfs_key) : m_pfs_key(pfs_key) { }
#else
	Runnable() { }
#endif /* UNIV_PFS_THREAD */

public:
	/** Method to execute the callable */
	template<typename F, typename ... Args>
	void run(F&& f, Args&& ... args)
	{
		preamble();

		using return_type = typename std::result_of<F(Args ...)>::type;

		auto	task = std::make_shared<
			std::packaged_task<return_type()>>(
				std::bind(std::forward<F>(f),
					  std::forward<Args>(args) ...));

		(*task)();

		epilogue();
	}
private:
	/** Register the thread with the server */
	void preamble()
	{
		my_thread_init();

#ifdef UNIV_PFS_THREAD
		PSI_thread*	psi;

		psi = PSI_THREAD_CALL(new_thread)(m_pfs_key, nullptr, 0);

		PSI_THREAD_CALL(set_thread_os_id)(psi);
		PSI_THREAD_CALL(set_thread)(psi);
#endif /* UNIV_PFS_THREAD */

		std::atomic_thread_fence(std::memory_order_release);;

		int	old;

		old = os_thread_count.fetch_add(1, std::memory_order_relaxed);

		ut_a(old <= static_cast<int>(srv_max_n_threads) - 1);
	}

	/** Deregister the thread */
	void epilogue()
	{
		std::atomic_thread_fence(std::memory_order_release);;

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
	mysql_pfs_key_t		m_pfs_key;
#endif /* UNIV_PFS_THREAD */
};

#ifdef UNIV_PFS_THREAD
/** Creae a detached thread, without args
Note: Captures the local arguments by value [=].
@para[in]	f		Callable
@param[in]	k		PFS thread key */
#define CREATE_THREAD0(f,k)						\
do {									\
	std::packaged_task<void()> task([=]()				\
	{								\
		Runnable	runnable{k};				\
		runnable.run(f);					\
	});								\
	/* Note: This throws an exception on failure */			\
	std::thread	t(std::move(task));				\
	t.detach();							\
} while (0);

/** Creae a detached thread.
Note: Captures the local arguments by value [=].
@para[in]	f		Callable
@param[in]	k		PFS thread key */
#define CREATE_THREAD(f,k,...)						\
do {									\
	std::packaged_task<void()> task([=]()				\
	{								\
		Runnable	runnable{k};				\
		runnable.run(f, __VA_ARGS__);				\
	});								\
	/* Note: This throws an exception on failure */			\
	std::thread	t(std::move(task));				\
	t.detach();							\
} while (0);
#else /* UNIV_PFS_THREAD */
/** Creae a detached thread, without args
Note: Captures the local arguments by value [=].
@para[in]	f		Callable */
#define CREATE_THREAD0(f,k)						\
do {									\
	std::packaged_task<void()> task([=]()				\
	{								\
		Runnable	runnable{};				\
		runnable.run(f);					\
	});								\
	/* Note: This throws an exception on failure */			\
	std::thread	t(std::move(task));				\
	t.detach();							\
} while (0);

/** Creae a detached thread.
Note: Captures the local arguments by value [=].
@para[in]	f		Callable */
#define CREATE_THREAD(f,k,...)						\
do {									\
	std::packaged_task<void()> task([=]()				\
	{								\
		Runnable	runnable{};				\
		runnable.run(f, __VA_ARGS__);				\
	});								\
	/* Note: This throws an exception on failure */			\
	std::thread	t(std::move(task));				\
	t.detach();							\
} while (0);
#endif /* UNIV_PFS_THREAD */

#endif /* !os0thread_create_h */

