/*****************************************************************************

Copyright (c) 2015, 2015, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/ut0lock_free_hash.h
Lock free hash implementation

Created Mar 16, 2015 Vasil Dimov
*******************************************************/

#ifndef ut0lock_free_hash_h
#define ut0lock_free_hash_h

#define BOOST_ATOMIC_NO_LIB

#include "univ.i"

#include <boost/atomic.hpp>

/* http://www.boost.org/doc/libs/1_58_0/doc/html/atomic/interface.html#atomic.interface.feature_macros */
#if BOOST_ATOMIC_INT64_LOCK_FREE != 2
#error BOOST_ATOMIC_INT64_LOCK_FREE is not 2
#endif
#if BOOST_ATOMIC_ADDRESS_LOCK_FREE != 2
#error BOOST_ATOMIC_ADDRESS_LOCK_FREE is not 2
#endif

#include "ut0new.h" /* UT_NEW*(), UT_DELETE*() */
#include "ut0rnd.h" /* ut_fold_ull() */

/* Enable this to implement a stats gathering inside ut_lock_free_hash_t.
It causes significant performance slowdown. */
#if 0
#define UT_HASH_IMPLEMENT_PRINT_STATS
#endif

/** An interface class to a basic hash table, that ut_lock_free_hash_t is. */
class ut_hash_interface_t {
public:
	/** The value that is returned when the searched for key is not
	found. */
	static const int64_t	NOT_FOUND = INT64_MAX;

	/** Destructor. */
	virtual
	~ut_hash_interface_t()
	{
	}

	/** Get the value mapped to a given key.
	@param[in]	key	key to look for
	@return the value that corresponds to key or NOT_FOUND. */
	virtual
	int64_t
	get(
		uint64_t	key) const = 0;

	/** Set the value for a given key, either inserting a new (key, val)
	tuple or overwriting an existent value.
	@param[in]	key	key whose value to set
	@param[in]	val	value to be set */
	virtual
	void
	set(
		uint64_t	key,
		int64_t		val) = 0;

	/** Delete a (key, val) pair from the hash.
	@param[in]	key	key whose pair to delete */
	virtual
	void
	del(
		uint64_t	key) = 0;

	/** Increment the value for a given key with 1 or insert a new tuple
	(key, 1).
	@param[in]	key	key whose value to increment or insert as 1 */
	virtual
	void
	inc(
		uint64_t	key) = 0;

	/** Decrement the value of a given key with 1 or do nothing if a
	tuple with the given key is not found.
	@param[in]	key	key whose value to decrement */
	virtual
	void
	dec(
		uint64_t	key) = 0;

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
	/** Print statistics about how much searches have been done on the hash
	and how many collisions. */
	virtual
	void
	print_stats() = 0;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */
};

/** A node in a linked list of arrays. The pointer to the next node is
atomically set (CAS) when a next element is allocated. */
template <typename T>
class ut_lock_free_list_node_t {
public:
	typedef ut_lock_free_list_node_t<T>*	next_t;

	/** Constructor.
	@param[in]	n_elements	number of elements to create */
	explicit
	ut_lock_free_list_node_t(
		size_t		n_elements)
		:
		m_n_base_elements(n_elements),
		m_next(NULL)
	{
		m_base = UT_NEW_ARRAY(T, m_n_base_elements,
				      mem_key_buf_stat_per_index_t);
	}

	/** Destructor. */
	~ut_lock_free_list_node_t()
	{
		UT_DELETE(m_base);
	}

	/** Create and append a new array to this one and store a pointer
	to it in 'm_next'. This is done in a way that multiple threads can
	attempt this at the same time and only one will succeed. When this
	method returns, the caller can be sure that the job is done (either
	by this or another thread). */
	next_t
	grow()
	{
		next_t	new_arr = UT_NEW(
			ut_lock_free_list_node_t<T>(m_n_base_elements * 2),
			mem_key_buf_stat_per_index_t);

		/* Publish the allocated entry. If somebody did this in the
		meantime then just discard the allocated entry and do
		nothing. */
		next_t	expected = NULL;
		if (!m_next.compare_exchange_strong(
				expected,
				new_arr,
				boost::memory_order_relaxed)) {
			/* Somebody just did that. */
			UT_DELETE(new_arr);

			/* 'expected' has the previous value which
			must be != NULL because the CAS failed. */
			ut_ad(expected != NULL);

			return(expected);
		}

		return(new_arr);
	}

	/** Base array. */
	T*					m_base;

	/** Number of elements in 'm_base'. */
	size_t					m_n_base_elements;

	/** Pointer to the next node if any or NULL. */
	boost::atomic<next_t>			m_next;
};

/** Lock free hash table. It stores (key, value) pairs where both the key
and the value are of integer type.
* Transitions for keys (a real key is anything other than UNUSED and DELETED):
  * UNUSED -> real key -- allowed
  * real key -> DELETED -- allowed
  anything else is not allowed, for example:
  * real key -> UNUSED -- not allowed
  * real key -> another real key -- not allowed
  * UNUSED -> DELETED -- not allowed
  * DELETED -> real key -- not allowed
  * DELETED -> UNUSED -- not allowed
* Transitions for values (a real value is anything other than NOT_FOUND):
  * NOT_FOUND -> real value -- allowed
  * real value -> another real value -- allowed
  * real value -> NOT_FOUND -- not allowed
*/
class ut_lock_free_hash_t : public ut_hash_interface_t {
public:
	/** Constructor. Not thread safe.
	@param[in]	initial_size	number of elements to allocate
	initially. Must be a power of 2. */
	explicit
	ut_lock_free_hash_t(
		size_t	initial_size)
#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
	:
	m_n_search(0),
	m_n_search_iterations(0)
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */
	{
		ut_a(ut_is_2pow(initial_size));

		m_data = UT_NEW(
			ut_lock_free_list_node_t<key_val_t>(initial_size),
			mem_key_buf_stat_per_index_t);
	}

	/** Destructor. Not thread safe. */
	~ut_lock_free_hash_t()
	{
		UT_DELETE(m_data);
	}

	/** Get the value mapped to a given key.
	@param[in]	key	key to look for
	@return the value that corresponds to key or NOT_FOUND. */
	int64_t
	get(
		uint64_t	key) const
	{
		ut_ad(key != UNUSED);
		ut_ad(key != DELETED);

		const key_val_t*	tuple = get_tuple(key);

		if (tuple == NULL) {
			return(NOT_FOUND);
		}

		/* Here if another thread is just setting this key for the
		first time, then the tuple could be (key, NOT_FOUND)
		(remember all vals are initialized to NOT_FOUND initially)
		in which case we will return NOT_FOUND below which is fine. */

		/* Another acceptable thing that can happen here is if another
		thread deletes this (key, val) tuple from the hash after
		get_tuple() above has found it and returned != NULL. Then this
		late get() will succeed and return the value even though at
		the time of return the tuple may already be
		(key == DELETED, val == some real value). It is up to the
		caller to handle such a situation or prevent it from happening
		altogether at a higher level in the code. It is acceptable
		because it is the same as if get() completed before del()
		started. */

		return(tuple->m_val.load(boost::memory_order_relaxed));
	}

	/** Set the value for a given key, either inserting a new (key, val)
	tuple or overwriting an existent value. If two threads call this
	method at the same time with the key, but different val, then when
	both methods have finished executing the value will be one of the
	two ones, but undeterministic which one. E.g.
	Thread 1: set(key, val_a)
	Thread 2: set(key, val_b)
	when both have finished, then a tuple with the given key will be
	present with value either val_a or val_b.
	@param[in]	key	key whose value to set
	@param[in]	val	value to be set */
	void
	set(
		uint64_t	key,
		int64_t		val)
	{
		ut_ad(key != UNUSED);
		ut_ad(key != DELETED);
		ut_ad(val != NOT_FOUND);

		key_val_t*	tuple = insert_or_get_position(key);

		tuple->m_val.store(val, boost::memory_order_relaxed);
	}

	/** Delete a (key, val) pair from the hash.
	If this gets called concurrently with get(), inc(), dec() or set(),
	then to the caller it will look like the calls executed in isolation,
	the hash structure itself will not be damaged, but it is undefined in
	what order the calls will be executed. For example:
	Let this tuple exist in the hash: (key == 5, val == 10)
	Thread 1: inc(key == 5)
	Thread 2: del(key == 5)
	[1] If inc() executes first then the tuple will become
	(key == 5, val == 11) and then del() will make it
	(key == DELETED, val == 11). At the end the hash will not contain a
	tuple for key == 5.
	[2] If del() executes first then the tuple will become
	(key == DELETED, val == 10) and then inc() will insert a new tuple
	(key == 5, value == 1).
	It is undefined which one of [1] or [2] will happen. It is up to the
	caller to accept this behavior or prevent it at a higher level.
	@param[in]	key	key whose pair to delete */
	void
	del(
		uint64_t	key)
	{
		ut_ad(key != UNUSED);
		ut_ad(key != DELETED);

		key_val_t*	tuple = get_tuple(key);

		if (tuple == NULL) {
			/* Nothing to delete. */
			return;
		}

		/* There is no need for CAS(key -> DELETED) here because
		key is not allowed to change to another key. It is only
		allowed to change into DELETED. A concurrent execution of
		to del(same key) is fine. */
		tuple->m_key.store(DELETED, boost::memory_order_relaxed);
	}

	/** Increment the value for a given key with 1 or insert a new tuple
	(key, 1).
	If two threads call this method at the same time with the same key,
	then it is guaranteed that when both calls have finished, the value
	will be incremented with 2.
	If two threads call this method and set() at the same time with the
	same key it is undeterministic whether the value will be what was
	given to set() or what was given to set() + 1. E.g.
	Thread 1: set(key, val)
	Thread 2: inc(key)
	or
	Thread 1: inc(key)
	Thread 2: set(key, val)
	when both have finished the value will be either val or val + 1.
	@param[in]	key	key whose value to increment or insert as 1 */
	void
	inc(
		uint64_t	key)
	{
		ut_ad(key != UNUSED);
		ut_ad(key != DELETED);

		key_val_t*	tuple = insert_or_get_position(key);

		/* Here tuple->m_val is either NOT_FOUND or some real value.
		Try to replace NOT_FOUND with 1. If that fails, then this means
		it is some real value in which case we should increment it
		with 1. We know that m_val will never move from some real value
		to NOT_FOUND. */
		int64_t	expected = NOT_FOUND;

		if (!tuple->m_val.compare_exchange_strong(
				expected, 1, boost::memory_order_relaxed)) {

			const int64_t	prev_val = tuple->m_val.fetch_add(
				1, boost::memory_order_relaxed);

			ut_a(prev_val + 1 != NOT_FOUND);
		}
	}

	/** Decrement the value of a given key with 1 or insert a new tuple
	(key, -1).
	With respect to calling this together with set(), inc() or dec() the
	same applies as with inc(), see its comment. The only guarantee is
	that the calls will execute in isolation, but the order in which they
	will execute is undeterministic.
	@param[in]	key	key whose value to decrement */
	void
	dec(
		uint64_t	key)
	{
		ut_ad(key != UNUSED);
		ut_ad(key != DELETED);

		key_val_t*	tuple = insert_or_get_position(key);

		/* Here tuple->m_val is either NOT_FOUND or some real value.
		Try to replace NOT_FOUND with -1. If that fails, then this means
		it is some real value in which case we should decrement it
		with 1. We know that m_val will never move from some real value
		to NOT_FOUND. */
		int64_t	expected = NOT_FOUND;

		if (!tuple->m_val.compare_exchange_strong(
				expected, -1, boost::memory_order_relaxed)) {

			const int64_t	prev_val = tuple->m_val.fetch_sub(
				1, boost::memory_order_relaxed);

			ut_a(prev_val - 1 != NOT_FOUND);
		}
	}

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
	/** Print statistics about how much searches have been done on the hash
	and how many collisions. */
	void
	print_stats()
	{
		const ulint	n_search = m_n_search;
		const ulint	n_search_iterations = m_n_search_iterations;

		ib::info() << "Lock free hash usage stats:";
		ib::info() << "number of searches: " << n_search;
		ib::info() << "number of search iterations: "
			<< n_search_iterations;
		if (n_search != 0) {
			ib::info() << "average iterations per search: "
				<< (double) n_search_iterations / n_search;
		}
	}
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

private:
	/** A key == UNUSED designates that this cell in the array is empty. */
	static const uint64_t	UNUSED = UINT64_MAX;

	/** A key == DELETED designates that this cell in the array has been
	used in the past, but it was deleted later. Searches should skip
	through it and continue as if it was a real value != UNUSED. It is
	important that such a cell is never reused for another tuple because
	late get(), inc() or dec() operations from the time before the key was
	set to DELETED could still be lurking to execute or be executing right
	now. */
	static const uint64_t	DELETED = UINT64_MAX - 1;

	/** (key, val) tuple type. */
	struct key_val_t {
		key_val_t()
		:
		m_key(UNUSED),
		m_val(NOT_FOUND)
		{
		}

		/** Key. */
		boost::atomic_uint64_t	m_key;

		/** Value. */
		boost::atomic_int64_t	m_val;
	};

	/** A hash function used to map a key to its suggested position in the
	array. A linear search to the right is done after this position to find
	the tuple with the given key or find a tuple with key == UNUSED which
	means that the key is not present in the array.
	@param[in]	key	key to map into a position
	@return a position (index) in the array where the tuple is guessed
	to be */
	size_t
	guess_position(
		uint64_t	key,
		size_t		arr_size) const
	{
		/* Implement a better hashing function to map
		[0, UINT64_MAX] -> [0, arr_size - 1] if this one turns
		out to generate too many collisions. */

		/* arr_size is a power of 2. */
		return(static_cast<size_t>(
				ut_fold_ull(key) & (arr_size - 1)
		));
	}

	/** Get the array cell of a key from a given array.
	@param[in]	arr		array to search into
	@param[in]	arr_size	number of elements in the array
	@param[in]	key		search for a tuple with this key
	@return pointer to the array cell or NULL if not found */
	key_val_t*
	get_tuple_from_array(
		key_val_t*	arr,
		size_t		arr_size,
		uint64_t	key) const
	{
#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
		++m_n_search;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

		const size_t	start = guess_position(key, arr_size);
		const size_t	end = start + arr_size;

		for (size_t i = start; i < end; i++) {

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
			++m_n_search_iterations;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

			/* arr_size is a power of 2. */
			const size_t	cur_pos = i & (arr_size - 1);

			key_val_t*	cur_tuple = &arr[cur_pos];

			const uint64_t	cur_key = cur_tuple->m_key.load(
				boost::memory_order_relaxed);

			if (cur_key == key) {
				/* cur_tuple->m_key could be changed to
				DELETED just after we have read it above, this
				is fine - let the current operation complete
				like it could have completed before the key
				was set to DELETED. */
				return(cur_tuple);
			} else if (cur_key == UNUSED) {
				return(NULL);
			}

			/* cur_key could be == DELETED here, skip through it
			and continue the search. */
		}

		return(NULL);
	}

	/** Get the array cell of a key.
	@param[in]	key	key to search for
	@return pointer to the array cell or NULL if not found */
	key_val_t*
	get_tuple(
		uint64_t	key) const
	{
		for (ut_lock_free_list_node_t<key_val_t>* cur_arr = m_data;
		     cur_arr != NULL;
		     cur_arr = cur_arr->m_next.load(
			     boost::memory_order_relaxed)) {

			key_val_t*	t;

			t = get_tuple_from_array(cur_arr->m_base,
						 cur_arr->m_n_base_elements,
						 key);

			if (t != NULL) {
				return(t);
			}
		}

		return(NULL);
	}

	/** Insert the given key into a given array or return its cell if
	already present.
	@param[in]	arr		array into which to search and insert
	@param[in]	arr_size	number of elements in the array
	@param[in]	key		key to insert or whose cell to retrieve
	@return a pointer to the inserted or previously existent tuple or NULL
	if a tuple with this key is not present in the array and the array is
	full, without any unused cells and thus insertion cannot be done into
	it. */
	key_val_t*
	insert_or_get_position_in_array(
		key_val_t*	arr,
		size_t		arr_size,
		uint64_t	key)
	{
#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
		++m_n_search;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

		const size_t	start = guess_position(key, arr_size);
		const size_t	end = start + arr_size;

		for (size_t i = start; i < end; i++) {

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
			++m_n_search_iterations;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

			/* arr_size is a power of 2. */
			const size_t	cur_pos = i & (arr_size - 1);

			key_val_t*	cur_tuple = &arr[cur_pos];

			const uint64_t	cur_key = cur_tuple->m_key.load(
				boost::memory_order_relaxed);

			if (cur_key == key) {
				return(cur_tuple);
			}

			if (cur_key == UNUSED) {
				uint64_t	expected = UNUSED;
				if (cur_tuple->m_key.compare_exchange_strong(
						expected,
						key,
						boost::memory_order_relaxed)) {
					/* Here cur_tuple->m_val is either
					NOT_FOUND (as it was initialized) or
					some real value. */
					return(cur_tuple);
				}

				ut_ad(expected != UNUSED);

				/* CAS failed, which means that some other
				thread just changed the current key from UNUSED
				to something else (which is stored in
				'expected'). See if the new value is 'key'. */
				if (expected == key) {
					return(cur_tuple);
				}

				/* The current key, which was UNUSED, has been
				replaced with something else (!= key) by a
				concurrently executing insert. Keep searching
				for a free slot. */
			}

			/* cur_key could be DELETED here, just skip through it
			like it was a normal key, other than UNUSED and the
			searched for key. */
		}

		return(NULL);
	}

	/** Insert the given key into the storage or return its cell if
	already present. This method will try expanding the storage (appending
	new arrays) as long as there is no free slot to insert.
	@param[in]	key	key to insert or whose cell to retrieve
	@return a pointer to the inserted or previously existent tuple */
	key_val_t*
	insert_or_get_position(
		uint64_t	key)
	{
		ut_lock_free_list_node_t<key_val_t>*	cur_arr = m_data;

		for (;;) {
			key_val_t*	t = insert_or_get_position_in_array(
				cur_arr->m_base,
				cur_arr->m_n_base_elements,
				key);

			if (t != NULL) {
				return(t);
			}

			ut_lock_free_list_node_t<key_val_t>*	next;

			next = cur_arr->m_next.load(boost::memory_order_relaxed);

			if (next == NULL) {
				cur_arr = cur_arr->grow();
			} else {
				cur_arr = next;
			}
		}
	}

	/** Storage for the (key, val) tuples. */
	ut_lock_free_list_node_t<key_val_t>*	m_data;

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
	/* The atomic type gives correct results, but has a _huge_
	performance impact. The unprotected operation gives a significant
	skew, but has almost no performance impact. */

	/** Number of searches performed in this hash. */
#if 0
	mutable uint64_t			m_n_search;
#else
	mutable boost::atomic_uint64_t		m_n_search;
#endif

	/** Number of elements processed for all searches. */
#if 0
	mutable uint64_t			m_n_search_iterations;
#else
	mutable boost::atomic_uint64_t		m_n_search_iterations;
#endif
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */
};

#endif /* ut0lock_free_hash_h */
