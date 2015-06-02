/* Copyright (c) 2015, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* See http://code.google.com/p/googletest/wiki/Primer */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

/* Enable TEST_STD_MAP or TEST_STD_UNORDERED_MAP below to perf test std::map
or std::unordered_map instead of the InnoDB lock free hash. */

/*
#define TEST_STD_MAP
*/

/* To test this, compile with -std=c++11 */
/*
#define TEST_STD_UNORDERED_MAP
*/

#if defined(TEST_STD_MAP) && defined(TEST_STD_UNORDERED_MAP)
#error Both TEST_STD_MAP and TEST_STD_UNORDERED_MAP are defined.
#endif /* TEST_STD_MAP && TEST_STD_UNORDERED_MAP */

#ifdef TEST_STD_UNORDERED_MAP
#include <unordered_map>
#endif /* TEST_STD_UNORDERED_MAP */

#ifdef TEST_STD_MAP
#include <map>
#endif /* TEST_STD_MAP */

#define __STDC_LIMIT_MACROS

#include <gtest/gtest.h>

#include "univ.i"

#include "sync0policy.h" /* needed by ib0mutex.h, which is not self contained */
#include "ib0mutex.h" /* SysMutex */
#include "os0thread.h" /* os_thread_*() */
#include "srv0conc.h" /* srv_max_n_threads */
#include "sync0debug.h" /* sync_check_init(), sync_check_close() */
#include "sync0mutex.h" /* mutex_enter() */
#include "ut0lock_free_hash.h"

extern SysMutex	thread_mutex;

namespace innodb_lock_free_hash_unittest {

#if defined(TEST_STD_MAP) || defined(TEST_STD_UNORDERED_MAP)
class std_hash_t : public ut_hash_interface_t {
public:
#ifdef TEST_STD_MAP
	typedef std::map<uint64_t, int64_t>		map_t;
#else
	typedef std::unordered_map<uint64_t, int64_t>	map_t;
#endif

	/** Constructor. */
	std_hash_t()
	{
		m_map_latch.init("std_hash_t latch", __FILE__, __LINE__);
	}

	/** Destructor. */
	~std_hash_t()
	{
		m_map_latch.destroy();
	}

	int64_t
	get(
		uint64_t	key) const
	{
		m_map_latch.enter(0, 0, __FILE__, __LINE__);

		map_t::const_iterator	it = m_map.find(key);

		int64_t	val;

		if (it != m_map.end()) {
			val = it->second;
		} else {
			val = NOT_FOUND;
		}

		m_map_latch.exit();

		return(val);
	}

	void
	set(
		uint64_t	key,
		int64_t		val)
	{
		m_map_latch.enter(0, 0, __FILE__, __LINE__);

		m_map[key] = val;

		m_map_latch.exit();
	}

	void
	del(
		uint64_t	key)
	{
		m_map_latch.enter(0, 0, __FILE__, __LINE__);

		m_map.erase(key);

		m_map_latch.exit();
	}

	void
	inc(
		uint64_t	key)
	{
		m_map_latch.enter(0, 0, __FILE__, __LINE__);

		map_t::iterator	it = m_map.find(key);

		if (it != m_map.end()) {
			++it->second;
		} else {
			m_map.insert(map_t::value_type(key, 1));
		}

		m_map_latch.exit();
	}

	void
	dec(
		uint64_t	key)
	{
		m_map_latch.enter(0, 0, __FILE__, __LINE__);

		map_t::iterator	it = m_map.find(key);

		if (it != m_map.end()) {
			--it->second;
		} else {
			m_map.insert(map_t::value_type(key, -1));
		}

		m_map_latch.exit();
	}

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
	void
	print_stats()
	{
	}
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

private:
	map_t				m_map;
	mutable OSBasicMutex<NoPolicy>	m_map_latch;
};
#endif /* TEST_STD_MAP || TEST_STD_UNORDERED_MAP */

/** Generate a key to use in the (key, value) tuples.
@param[in]	i		some sequential number
@param[in]	extra_bits	extra bits to OR into the result
@return a key, derived from 'i' and 'extra_bits' */
inline
uint64_t
key_gen(
	size_t		i,
	uint64_t	extra_bits)
{
	return((i * 7 + 3) | extra_bits);
}

/** Generate a value to use in the (key, value) tuples.
@param[in]	i	some sequential number
@return a value derived from 'i' */
inline
int64_t
val_from_i(
	size_t	i)
{
	/* Make sure that the returned value is big enough, so that a few
	decrements don't make it negative. */
	return(i * 13 + 10000);
}

/** Insert some tuples in the hash, generating their keys and values
@param[in,out]	hash		hash into which to insert
@param[in]	n_elements	number of elements to insert
@param[in]	key_extra_bits	extra bits to use for key generation */
void
hash_insert(
	ut_hash_interface_t*	hash,
	size_t			n_elements,
	uint64_t		key_extra_bits)
{
	for (size_t i = 0; i < n_elements; i++) {
		hash->set(key_gen(i, key_extra_bits), val_from_i(i));
	}
}

/** Delete the tuples from the hash, inserted by hash_insert(), when called
with the same arguments.
@param[in,out]	hash		hash from which to delete
@param[in]	n_elements	number of elements to delete
@param[in]	key_extra_bits	extra bits to use for key generation */
void
hash_delete(
	ut_hash_interface_t*	hash,
	size_t			n_elements,
	uint64_t		key_extra_bits)
{
	for (size_t i = 0; i < n_elements; i++) {
		hash->del(key_gen(i, key_extra_bits));
	}
}

/** Check that the tuples inserted by hash_insert() are present in the hash.
@param[in]	hash		hash to check
@param[in]	n_elements	number of elements inserted by hash_insert()
@param[in]	key_extra_bits	extra bits that were given to hash_insert() */
void
hash_check_inserted(
	const ut_hash_interface_t*	hash,
	size_t				n_elements,
	uint64_t			key_extra_bits)
{
	for (size_t i = 0; i < n_elements; i++) {
		const uint64_t	key = key_gen(i, key_extra_bits);

		ASSERT_EQ(val_from_i(i), hash->get(key));
	}
}

/** Check that the tuples deleted by hash_delete() are missing from the hash.
@param[in]	hash		hash to check
@param[in]	n_elements	number of elements deleted by hash_delete()
@param[in]	key_extra_bits	extra bits that were given to hash_delete() */
void
hash_check_deleted(
	const ut_hash_interface_t*	hash,
	size_t				n_elements,
	uint64_t			key_extra_bits)
{
	for (size_t i = 0; i < n_elements; i++) {
		const uint64_t	key = key_gen(i, key_extra_bits);

		const int64_t	not_found = ut_hash_interface_t::NOT_FOUND;

		ASSERT_EQ(not_found, hash->get(key));
	}
}

TEST(ut0lock_free_hash, single_threaded)
{
#if defined(TEST_STD_MAP) || defined(TEST_STD_UNORDERED_MAP)
	ut_hash_interface_t*	hash = new std_hash_t();
#else /* TEST_STD_MAP || TEST_STD_UNORDERED_MAP */
	ut_hash_interface_t*	hash = new ut_lock_free_hash_t(1048576);
#endif /* TEST_STD_MAP || TEST_STD_UNORDERED_MAP */

	const size_t	n_elements = 16 * 1024;

	hash_insert(hash, n_elements, 0);

	hash_check_inserted(hash, n_elements, 0);

	hash_delete(hash, n_elements, 0);

	hash_check_deleted(hash, n_elements, 0);

	hash_insert(hash, n_elements, 0);

	hash_check_inserted(hash, n_elements, 0);

	const size_t	n_iter = 512;

	for (size_t it = 0; it < n_iter; it++) {
		/* Increment the values of some and decrement of others. */
		for (size_t i = 0; i < n_elements; i++) {

			const bool	should_inc = i % 2 == 0;
			const uint64_t	key = key_gen(i, 0);

			/* Inc/dec from 0 to 9 times, depending on 'i'. */
			for (size_t j = 0; j < i % 10; j++) {
				if (should_inc) {
					hash->inc(key);
				} else {
					hash->dec(key);
				}
			}
		}
	}

	/* Check that increment/decrement was done properly. */
	for (size_t i = 0; i < n_elements; i++) {

		const bool	was_inc = i % 2 == 0;
		const int64_t	delta = (i % 10) * n_iter;

		ASSERT_EQ(val_from_i(i) + (was_inc ? delta : -delta),
			  hash->get(key_gen(i, 0)));
	}

	hash_delete(hash, n_elements, 0);

	hash_check_deleted(hash, n_elements, 0);

	delete hash;
}

/** Global hash, edited from many threads concurrently. */
ut_hash_interface_t*	global_hash;

/** Number of common tuples (edited by all threads) to insert into the hash. */
static const size_t	N_COMMON = 512;

/** Number of private, per-thread tuples to insert by each thread. */
static const size_t	N_PRIV_PER_THREAD = 128;

/** Number of threads to start. Overall the hash will be filled with
N_COMMON + N_THREADS * N_PRIV_PER_THREAD tuples. */
static const size_t	N_THREADS = 32;

/** Hammer the global hash with inc(), dec() and set(). The inc()/dec()
performed on the common keys will net to 0 when this thread ends. It also
inserts some tuples with keys that are unique to this thread.
@param[in]	arg	thread id, used to generate thread-private keys */
extern "C"
os_thread_ret_t
DECLARE_THREAD(thread)(
	void*	arg)
{
	const uintptr_t	thread_id = reinterpret_cast<uintptr_t>(arg);
	const uint64_t	key_extra_bits = thread_id << 32;

	hash_insert(global_hash, N_PRIV_PER_THREAD, key_extra_bits);

	hash_check_inserted(global_hash, N_PRIV_PER_THREAD, key_extra_bits);

	const size_t	n_iter = 512;

	for (size_t i = 0; i < n_iter; i++) {
		for (size_t j = 0; j < N_COMMON; j++) {
			const uint64_t	key = key_gen(j, 0);

			global_hash->inc(key);
			global_hash->inc(key);
			global_hash->inc(key);

			global_hash->dec(key);
			global_hash->inc(key);

			global_hash->dec(key);
			global_hash->dec(key);
			global_hash->dec(key);
		}

		for (size_t j = 0; j < N_PRIV_PER_THREAD; j++) {
			const uint64_t	key = key_gen(j, key_extra_bits);

			for (size_t k = 0; k < 4; k++) {
				global_hash->inc(key);
				global_hash->dec(key);
			}
		}
	}

	hash_check_inserted(global_hash, N_PRIV_PER_THREAD, key_extra_bits);

	hash_delete(global_hash, N_PRIV_PER_THREAD, key_extra_bits);

	hash_check_deleted(global_hash, N_PRIV_PER_THREAD, key_extra_bits);

	os_thread_exit(NULL);

	OS_THREAD_DUMMY_RETURN;
}

TEST(ut0lock_free_hash, multi_threaded)
{
	srv_max_n_threads = 1024;

	sync_check_init();
	os_thread_init();

#if defined(TEST_STD_MAP) || defined(TEST_STD_UNORDERED_MAP)
	global_hash = new std_hash_t();
#else /* TEST_STD_MAP || TEST_STD_UNORDERED_MAP */
	global_hash = new ut_lock_free_hash_t(1024 * 16);
#endif /* TEST_STD_MAP || TEST_STD_UNORDERED_MAP */

	hash_insert(global_hash, N_COMMON, 0);

	for (uintptr_t i = 0; i < N_THREADS; i++) {
		/* Avoid thread_id == 0 because that will collide with the
		shared tuples, thus use 'i + 1' instead of 'i'. */
		os_thread_create(thread, reinterpret_cast<void*>(i + 1), NULL);
	}

	/* Wait for all threads to exit. */
	mutex_enter(&thread_mutex);
	while (os_thread_count > 0) {
		mutex_exit(&thread_mutex);
		os_thread_sleep(100000 /* 0.1 sec */);
		mutex_enter(&thread_mutex);
	}
	mutex_exit(&thread_mutex);

	hash_check_inserted(global_hash, N_COMMON, 0);

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
	global_hash->print_stats();
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

	delete global_hash;

	os_thread_free();
	sync_check_close();
}

}
