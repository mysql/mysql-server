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

#include "univ.i"

#include "os0atomic.h" /* os_compare_and_swap_ulint() */
#include "ut0new.h" /* UT_NEW*(), UT_DELETE*() */
#include "ut0rnd.h" /* ut_fold_ull() */

/* Enable this to implement a stats gathering inside ut_lock_free_hash_t.
It may cause significant performance slowdown. */
#define UT_HASH_IMPLEMENT_PRINT_STATS

/** An interface class to a basic hash table, that ut_lock_free_hash_t is. */
class ut_hash_interface_t {
public:
	/** The value that is returned when the searched for key is not
	found. */
	static const uint64_t	NOT_FOUND = UINT64_MAX;

	/** Destructor. */
	virtual
	~ut_hash_interface_t()
	{
	}

	/** Get the value mapped to a given key.
	@param[in]	key	key to look for
	@return the value that corresponds to key or NOT_FOUND. */
	virtual
	uint64_t
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
		uint64_t	val) = 0;

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
	void
	grow()
	{
		if (m_next.load() != NULL) {
			/* Somebody already appended. */
			return;
		}

		const size_t	n_next_elements = m_n_base_elements * 2;

		next_t		next = UT_NEW(
			ut_lock_free_list_node_t<T>(n_next_elements),
			mem_key_buf_stat_per_index_t);

		next_t		expected = NULL;

		/* Publish the allocated entry. If somebody did this in the
		meantime then just discard the allocated entry and do
		nothing. */
		if (!m_next.compare_exchange_strong(expected, next)) {
			/* Somebody just did that. */
			UT_DELETE(next);
		}
	}

	/** Base array. */
	T*					m_base;

	/** Number of elements in 'm_base'. */
	size_t					m_n_base_elements;

	/** Pointer to the next node if any or NULL. */
	typedef ut_lock_free_list_node_t<T>*	next_t;
	os_atomic_t<next_t>			m_next;
};

/** Lock free hash table. It stores (key, value) pairs where both the key
and the value are of type uint64_t.
Assumptions:
* Keys can only transition from UNUSED to some real value, but never
from a real value to UNUSED or from a real value to another real value.
* Values can only transition from NOT_FOUND to some real value, but never
from a real value to NOT_FOUND. */
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
	uint64_t
	get(
		uint64_t	key) const
	{
		ut_ad(key != UNUSED);

		const key_val_t*	tuple = get_tuple(key);

		if (tuple == NULL) {
			return(NOT_FOUND);
		}

		/* Here if another thread is just setting this key for the
		first time, then the tuple could be (key, NOT_FOUND)
		(remember all vals are initialized to NOT_FOUND initially)
		in which case we will return NOT_FOUND below which is fine. */

		return(tuple->m_val.load());
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
		uint64_t	val)
	{
		ut_ad(key != UNUSED);
		ut_ad(val != NOT_FOUND);

		key_val_t*	tuple = insert_or_get_position(key);

		tuple->m_val.store(val);
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

		key_val_t*	tuple = insert_or_get_position(key);

		/* Here tuple->m_val is either NOT_FOUND or some real value.
		Try to replace NOT_FOUND with 1. If that fails, then this means
		it is some real value in which case we should increment it
		with 1. We know that m_val will never move from some real value
		to NOT_FOUND. */
		uint64_t	not_found = NOT_FOUND;
		if (!tuple->m_val.compare_exchange_strong(not_found, 1)) {
			++tuple->m_val;
		}
	}

	/** Decrement the value of a given key with 1 or do nothing if a
	tuple with the given key is not found. With respect to calling this
	together with set(), inc() or dec() the same applies as with inc(),
	see its comment. The only guarantee is that the calls will execute
	in isolation, but the order in which they will execute is
	undeterministic.
	@param[in]	key	key whose value to decrement */
	void
	dec(
		uint64_t	key)
	{
		ut_ad(key != UNUSED);

		key_val_t*	tuple = get_tuple(key);

		if (tuple == NULL) {
			/* Nothing to decrement. We can either signal this
			to the caller (e.g. return bool from this method) or
			assert. For now we just return. */
			return;
		}

		/* Try to CAS "N" with "N - 1" in a busy loop. This could
		starve if lots of threads modify the same key. But we can't
		use an atomic decrement while checking for >0 and !=NOT_FOUND
		at the same time. */
		for (;;) {
			uint64_t	cur_val = tuple->m_val.load();

			ut_a(cur_val > 0);

			if (cur_val == NOT_FOUND) {
				break;
			}

			const uint64_t	new_val = cur_val - 1;

			if (tuple->m_val.compare_exchange_strong(cur_val,
								 new_val)) {
				break;
			}
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

	/** (key, val) tuple type. */
	struct key_val_t {
		key_val_t()
		:
		m_key(UNUSED),
		m_val(NOT_FOUND)
		{
		}

		/** Key. */
		os_atomic_t<uint64_t>	m_key;

		/** Value. */
		os_atomic_t<uint64_t>	m_val;
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
		[0, UINTPTR_MAX] -> [0, arr_size - 1] if this one turns
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
		/* The atomic operation gives correct results, but has
		a _huge_ performance impact. */
		os_atomic_increment_ulint(&m_n_search, 1);
		/* The unprotected operation gives a significant skew, but
		has almost no performance impact. */
		//++m_n_search;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

		const size_t	start = guess_position(key, arr_size);
		const size_t	end = start + arr_size;

		for (size_t i = start; i < end; i++) {

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
			os_atomic_increment_ulint(&m_n_search_iterations, 1);
			//++m_n_search_iterations;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

			/* arr_size is a power of 2. */
			const size_t	cur_pos = i & (arr_size - 1);
			key_val_t*	cur_tuple = &arr[cur_pos];
			const uint64_t	cur_key = cur_tuple->m_key.load();

			if (cur_key == key) {
				return(cur_tuple);
			} else if (cur_key == UNUSED) {
				return(NULL);
			}
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
		     cur_arr = cur_arr->m_next.load()) {

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
		os_atomic_increment_ulint(&m_n_search, 1);
		//++m_n_search;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

		const size_t	start = guess_position(key, arr_size);
		const size_t	end = start + arr_size;

		/* We do not have os_compare_and_swap_ptr(), thus we use
		os_compare_and_swap_ulint(). */
		ut_ad(sizeof(uint64_t) == sizeof(ulint));

		for (size_t i = start; i < end; i++) {

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
			os_atomic_increment_ulint(&m_n_search_iterations, 1);
			//++m_n_search_iterations;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

			/* arr_size is a power of 2. */
			const size_t	cur_pos = i & (arr_size - 1);
			key_val_t*	cur_tuple = &arr[cur_pos];
			const uint64_t	cur_key = cur_tuple->m_key.load();

			if (cur_key == key) {
				return(cur_tuple);
			}

			if (cur_key == UNUSED) {
				uint64_t	unused = UNUSED;
				if (cur_tuple->m_key.compare_exchange_strong(
						unused, key)) {
					/* Here cur_tuple->m_val is either
					NOT_FOUND (as it was initialized) or
					some real value. */
					return(cur_tuple);
				}

				/* CAS failed, which means that some other
				thread just changed the current key from UNUSED
				to something else. See if the new value is
				'key'. */
				if (cur_tuple->m_key.load() == key) {
					return(cur_tuple);
				}

				/* The current key, which was UNUSED, has been
				replaced with something else (!= key). Keep
				searching for a free slot. */
			}
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
		for (ut_lock_free_list_node_t<key_val_t>* cur_arr = m_data;
		     ;
		     cur_arr = cur_arr->m_next.load()) {

			key_val_t*	t;

			t = insert_or_get_position_in_array(
				cur_arr->m_base,
				cur_arr->m_n_base_elements,
				key);

			if (t != NULL) {
				return(t);
			}

			if (cur_arr->m_next.load() == NULL) {
				cur_arr->grow();
			}
		}
	}

	/** Storage for the (key, val) tuples. */
	ut_lock_free_list_node_t<key_val_t>*	m_data;

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
	/** Number of searches performed in this hash. */
	mutable ulint				m_n_search;

	/** Number of elements processed for all searches. */
	mutable ulint				m_n_search_iterations;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */
};

#endif /* ut0lock_free_hash_h */
