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

#if defined(HAVE_PSI_INTERFACE) && defined(UNIV_PFS_THREAD)
/* This macro register the current thread and its key
with performance schema */
#define pfs_register_thread(key)				\
do {								\
	struct PSI_thread* psi = PSI_THREAD_CALL(new_thread)(key, NULL, 0);\
	PSI_THREAD_CALL(set_thread_os_id)(psi);			\
	PSI_THREAD_CALL(set_thread)(psi);			\
} while (0)

/* This macro delist the current thread from performance schema */
#define pfs_delete_thread()					\
do {								\
	PSI_THREAD_CALL(delete_current_thread)();		\
} while (0)
#endif /* HAVE_PSI_INTERFACE  && UNIV_PFS_THREAD */

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

class Runnable {
public:
#ifdef UNIV_PFS_THREAD
	/** Constructor for the Runnable object.
	@param[in]	pfs_key		This is a conditional compile. */
	explicit Runnable(mysql_pfs_key_t pfs_key) : m_pfs_key(pfs_key) { }
#else
	Runnable() { }
#endif /* UNIV_PFS_THREAD */

public:
	template<typename F, typename ... Args>
	void run(F&& f, Args&& ... args)
	{
#ifdef UNIV_DEBUG_THREAD_CREATION
	ib::info()
		<< "Thread start, id "
		<< std::this_thread::native_handle;
#endif /* UNIV_DEBUG_THREAD_CREATION */
		my_thread_init();

#ifdef UNIV_PFS_THREAD
		PSI_thread*	psi;

		psi = PSI_THREAD_CALL(new_thread)(m_pfs_key, nullptr, 0);

		PSI_THREAD_CALL(set_thread_os_id)(psi);
		PSI_THREAD_CALL(set_thread)(psi);
#endif /* UNIV_PFS_THREAD */

		using return_type = typename std::result_of<F(Args ...)>::type;

		auto	task = std::make_shared<
			std::packaged_task<return_type()>>(
				std::bind(std::forward<F>(f),
					  std::forward<Args>(args) ...));

		std::atomic_thread_fence(std::memory_order_release);;

		int	old;

		old = os_thread_count.fetch_add(1, std::memory_order_relaxed);

		ut_a(old <= static_cast<int>(srv_max_n_threads) - 1);

		(*task)();

		std::atomic_thread_fence(std::memory_order_release);;

		old = os_thread_count.fetch_sub(1, std::memory_order_relaxed);
		ut_a(old > 0);

		my_thread_end();

#ifdef UNIV_PFS_THREAD
		pfs_delete_thread();
#endif /* UNIV_PFS_THREAD */

#ifdef UNIV_DEBUG_THREAD_CREATION
		ib::info()
			<< "Thread exit, id "
			<< static_cast<size_t>(os_thread_get_curr_id());
#endif /* UNIV_DEBUG_THREAD_CREATION */
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

