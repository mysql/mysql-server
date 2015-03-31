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
	static const uintptr_t	NOT_FOUND = UINTPTR_MAX;

	/** Destructor. */
	virtual
	~ut_hash_interface_t()
	{
	}

	/** Get the value mapped to a given key.
	@param[in]	key	key to look for
	@return the value that corresponds to key or NOT_FOUND. */
	virtual
	uintptr_t
	get(
		uintptr_t	key) const = 0;

	/** Set the value for a given key, either inserting a new (key, val)
	tuple or overwriting an existent value.
	@param[in]	key	key whose value to set
	@param[in]	val	value to be set */
	virtual
	void
	set(
		uintptr_t	key,
		uintptr_t	val) = 0;

	/** Increment the value for a given key with 1 or insert a new tuple
	(key, 1).
	@param[in]	key	key whose value to increment or insert as 1 */
	virtual
	void
	inc(
		uintptr_t	key) = 0;

	/** Decrement the value of a given key with 1 or do nothing if a
	tuple with the given key is not found.
	@param[in]	key	key whose value to decrement */
	virtual
	void
	dec(
		uintptr_t	key) = 0;

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
	@param[in]	n_elements	number of elements to create
	@param[in]	initializer	initialize all elements to this value */
	explicit
	ut_lock_free_list_node_t(
		size_t		n_elements,
		const T&	initializer)
		:
		m_base_elements(n_elements)
	{
		m_base = UT_NEW_ARRAY(T, m_base_elements,
				      mem_key_buf_stat_per_index_t);

		for (size_t i = 0; i < m_base_elements; i++) {
			m_base[i] = initializer;
		}

		m_initializer = initializer;
		m_next = NULL;
	}

	/** Destructor. */
	~ut_lock_free_list_node_t()
	{
		UT_DELETE(m_base);
	}

	/** Append a new array to this one and store a pointer to it
	in 'm_next'. This is done in a way that multiple threads can attempt
	this at the same time and only one will succeed. When this method
	returns, the caller can be sure that the job is done (either by this
	or another thread). */
	void
	grow()
	{
		if (m_next != NULL) {
			/* Somebody already appended. */
			return;
		}

		const size_t	n_next_elements = m_base_elements * 2;

		ut_lock_free_list_node_t<T>*	next = UT_NEW(
			ut_lock_free_list_node_t<T>(n_next_elements,
						    m_initializer),
			mem_key_buf_stat_per_index_t);

		for (size_t i = 0; i < n_next_elements; i++) {
			next->m_base[i] = m_initializer;
		}

		/* Publish the allocated entry. If somebody did this in the
		meantime then just discard the allocated entry and do
		nothing. */
		if (!os_compare_and_swap_ulint(
				reinterpret_cast<ulint*>(&m_next),
				/* C++11 does not allow
				static_cast<ulint>(NULL)
				while pre-C++11 does not allow
				reinterpret_cast<ulint>(NULL). The typecast
				below satisfies both. */
				(ulint) NULL,
				reinterpret_cast<ulint>(next))) {
			/* Somebody just did that. */
			UT_DELETE(next);
		}
	}

	/** Base array. */
	T*				m_base;

	/** Number of elements in 'm_base'. */
	size_t				m_base_elements;

	/** Pointer to the next node if any or NULL. */
	ut_lock_free_list_node_t<T>*	m_next;

private:
	T				m_initializer;
};

/** Lock free hash table. It stores (key, value) pairs where both the key
and the value are of type uintptr_t.
Assumption: basic reads and writes to uintptr_t are atomic.
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
		const key_val_t	initializer = {
			UNUSED /* key */,
			NOT_FOUND /* val */
		};

		ut_a(ut_is_2pow(initial_size));

		m_data = UT_NEW(
			ut_lock_free_list_node_t<key_val_t>(initial_size,
							    initializer),
			mem_key_buf_stat_per_index_t);

		/* Confirm that the keys are aligned (which also means that
		the vals are aligned). Only then the basic read/write
		will be atomic. */
		ut_a(reinterpret_cast<uintptr_t>(&m_data->m_base[0].m_key)
		     % sizeof(uintptr_t) == 0);
	}

	/** Destructor. Not thread safe. */
	~ut_lock_free_hash_t()
	{
		UT_DELETE(m_data);
	}

	/** Get the value mapped to a given key.
	@param[in]	key	key to look for
	@return the value that corresponds to key or NOT_FOUND. */
	uintptr_t
	get(
		uintptr_t	key) const
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

		return(tuple->m_val);
	}

	/** Set the value for a given key, either inserting a new (key, val)
	tuple or overwriting an existent value.
	@param[in]	key	key whose value to set
	@param[in]	val	value to be set */
	void
	set(
		uintptr_t	key,
		uintptr_t	val)
	{
		ut_ad(key != UNUSED);
		ut_ad(val != NOT_FOUND);

		key_val_t*	tuple = insert_or_get_position(key);

		tuple->m_val = val;
	}

	/** Increment the value for a given key with 1 or insert a new tuple
	(key, 1).
	@param[in]	key	key whose value to increment or insert as 1 */
	void
	inc(
		uintptr_t	key)
	{
		ut_ad(key != UNUSED);

		key_val_t*	tuple = insert_or_get_position(key);

		/* Here tuple->m_val is either NOT_FOUND or some real value.
		Try to replace NOT_FOUND with 1. If that fails, then this means
		it is some real value in which case we should increment it
		with 1. */
		if (!os_compare_and_swap_ulint(&tuple->m_val, NOT_FOUND, 1)) {

			os_atomic_increment_ulint(&tuple->m_val, 1);
		}
	}

	/** Decrement the value of a given key with 1 or do nothing if a
	tuple with the given key is not found.
	@param[in]	key	key whose value to decrement */
	void
	dec(
		uintptr_t	key)
	{
		ut_ad(key != UNUSED);

		const key_val_t*	tuple = get_tuple(key);

		if (tuple == NULL) {
			/* Nothing to decrement. We can either signal this
			to the caller (e.g. return bool from this method) or
			assert. For now we just return. */
			return;
		}

		/* Try to CAS "N" with "N - 1" in a busy loop. This could
		starve if lots of threads modify the same key. But we can't
		use an atomic decrement while checking for >0 at the same
		time. */
		for (;;) {
			const uintptr_t	cur_val = tuple->m_val;

			ut_a(cur_val > 0);

			const uintptr_t	new_val = cur_val - 1;

			if (os_compare_and_swap_ulint(
					&tuple->m_val,
					cur_val,
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
		ib::info() << "Lock free hash usage stats:";
		ib::info() << "number of searches: " << m_n_search;
		ib::info() << "number of search iterations: "
			<< m_n_search_iterations;
		if (m_n_search != 0) {
			ib::info() << "average iterations per search: "
				<< (double) m_n_search_iterations / m_n_search;
		}
	}
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

private:
	/** A key==UNUSED designates that this cell in the array is empty. */
	static const uintptr_t	UNUSED = UINTPTR_MAX;

	/** (key, val) tuple type. */
	struct key_val_t {
		/** Key. */
		uintptr_t	m_key;

		/** Value. */
		uintptr_t	m_val;
	};

	/** A hash function used to map a key to its suggested position in the
	array. A linar search to the right is done after this position to find
	the tuple with the given key or find a tuple with key==UNUSED which
	means that the key is not present in the array.
	@param[in]	key	key to map into a position
	@return a position (index) in the array where the tuple is guessed
	to be */
	size_t
	guess_position(
		uintptr_t	key,
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
		uintptr_t	key) const
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
			const uintptr_t	cur_key = cur_tuple->m_key;

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
		uintptr_t	key) const
	{
		for (ut_lock_free_list_node_t<key_val_t>* cur_arr = m_data;
		     cur_arr != NULL;
		     cur_arr = cur_arr->m_next) {

			key_val_t*	t;

			t = get_tuple_from_array(cur_arr->m_base,
						 cur_arr->m_base_elements,
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
	@return a pointer to the inserted or previously existent tuple */
	key_val_t*
	insert_or_get_position_in_array(
		key_val_t*	arr,
		size_t		arr_size,
		uintptr_t	key)
	{
#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
		os_atomic_increment_ulint(&m_n_search, 1);
		//++m_n_search;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

		const size_t	start = guess_position(key, arr_size);
		const size_t	end = start + arr_size;

		/* We do not have os_compare_and_swap_ptr(), thus we use
		os_compare_and_swap_ulint(). */
		ut_ad(sizeof(uintptr_t) == sizeof(ulint));

		for (size_t i = start; i < end; i++) {

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
			os_atomic_increment_ulint(&m_n_search_iterations, 1);
			//++m_n_search_iterations;
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */

			/* arr_size is a power of 2. */
			const size_t	cur_pos = i & (arr_size - 1);
			key_val_t*	cur_tuple = &arr[cur_pos];
			const uintptr_t	cur_key = cur_tuple->m_key;

			if (cur_key == key) {
				return(cur_tuple);
			}

			if (cur_key == UNUSED) {
				if (os_compare_and_swap_ulint(
						&cur_tuple->m_key,
						UNUSED,
						key)) {
					/* Here cur_tuple->m_val is either
					NOT_FOUND (as it was initialized) or
					some real value. */
					return(cur_tuple);
				}

				/* CAS failed, which means that some other
				thread just changed the current key from UNUSED
				to something else. See if the new value is
				'key'. */
				if (cur_tuple->m_key == key) {
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
		uintptr_t	key)
	{
		for (ut_lock_free_list_node_t<key_val_t>* cur_arr = m_data;
		     ;
		     cur_arr = cur_arr->m_next) {

			key_val_t*	t;

			t = insert_or_get_position_in_array(
				cur_arr->m_base,
				cur_arr->m_base_elements,
				key);

			if (t != NULL) {
				return(t);
			}

			if (cur_arr->m_next == NULL) {
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
