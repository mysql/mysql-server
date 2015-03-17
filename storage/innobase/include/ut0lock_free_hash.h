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

/** Lock free hash table. It stores (key, value) pairs where both the key
and the value are of type uintptr_t.
Assumption: basic reads and writes to uintptr_t are atomic.
*/
class ut_lock_free_hash_t {
public:
	/** A constant that is returned when the searched for key is not
	found. */
	static const uintptr_t	NOT_FOUND = UINTPTR_MAX;

	/** Constructor. Not thread safe. */
	ut_lock_free_hash_t()
	{
		m_arr_size = 1024;
		m_arr = UT_NEW_ARRAY(key_val_t, m_arr_size,
				     mem_key_buf_stat_per_index_t);

		/* Confirm that the keys are aligned (which also means that
		the vals are aligned). Only then the basic read/write
		will be atomic. */
		ut_a(reinterpret_cast<uintptr_t>(&m_arr[0].key)
		     % sizeof(uintptr_t) == 0);

		for (size_t i = 0; i < m_arr_size; i++) {
			m_arr[i].key = UNUSED;
			m_arr[i].val = NOT_FOUND;
		}
	}

	/** Destructor. Not thread safe. */
	~ut_lock_free_hash_t()
	{
		UT_DELETE_ARRAY(m_arr);
	}

	/** Get the value mapped to a given key.
	@param[in]	key	key to look for
	@return the value that corresponds to key or NOT_FOUND. */
	uintptr_t
	get(
		uintptr_t	key)
	{
		const size_t	pos = get_position(key);

		if (pos == INVALID_POS) {
			return(NOT_FOUND);
		}

		/* Here if another thread is just setting this key for the
		first time, then the tuple could be (key, NOT_FOUND)
		(remember all vals are initialized to NOT_FOUND in the
		constructor) in which case we will return NOT_FOUND below
		which is fine. */

		return(m_arr[pos].val);
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
		const size_t	pos = insert_or_get_position(key);

		m_arr[pos].val = val;
	}

	/** Increment the value for a given key with 1 or insert a new tuple
	(key, 1).
	@param[in]	key	key whose value to increment or insert as 1 */
	void
	inc(
		uintptr_t	key)
	{
		const size_t	pos = insert_or_get_position(key);

		/* Here m_arr[pos].val is either NOT_FOUND or some real value.
		Try to replace NOT_FOUND with 1. If that fails, then this means
		it is some real value in which case we should increment it
		with 1. */
		if (!os_compare_and_swap_ulint(&m_arr[pos].val, NOT_FOUND, 1)) {

			os_atomic_increment_ulint(&m_arr[pos].val, 1);
		}
	}

	/** Decrement the value of a given key with 1 or do nothing if a
	tuple with the given key is not found.
	@param[in]	key	key whose value to decrement */
	void
	dec(
		uintptr_t	key)
	{
		const size_t	pos = get_position(key);

		if (pos == INVALID_POS) {
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
			const uintptr_t	cur_val = m_arr[pos].val;

			ut_a(cur_val > 0);

			const uintptr_t	new_val = cur_val - 1;

			if (os_compare_and_swap_ulint(
					&m_arr[pos].val, cur_val, new_val)) {
				break;
			}
		}
	}

private:
	/** A constant used initially for all keys. */
	static const uintptr_t	UNUSED = UINTPTR_MAX;

	/** A constant used to designate an invalid position. Returned by
	methods that are supposed to find a given key when that key is not
	present. */
	static const size_t	INVALID_POS = SIZE_MAX;

	/** (key, val) tuple type. */
	struct key_val_t {
		/** Key. */
		uintptr_t	key;

		/** Value. */
		uintptr_t	val;
	};

	/** A hash function used to map a key to its suggested position in the
	array. A linar search to the right is done after this position to find
	the tuple with the given key or find a tuple with key=UNUSED which
	means that the key is not present in the array.
	@param[in]	key	key to map into a position
	@return a position (index) in the array where the tuple is guessed
	to be */
	size_t
	guess_position(
		uintptr_t	key)
	{
		/* Implement a better hashing function to map
		[0, UINTPTR_MAX] -> [0, m_arr_size - 1] if this one turns
		out to generate too many collisions. */
		return(static_cast<size_t>(key) % m_arr_size);
	}

	/** Get the position of a key into the array.
	@param[in]	key	key to search for
	@return either the position (index) in the array or INVALID_POS if
	not found */
	size_t
	get_position(
		uintptr_t	key)
	{
		const size_t	start = guess_position(key);

		for (size_t i = start; i < m_arr_size + start; i++) {
			const size_t	cur_pos = i % m_arr_size;
			const uintptr_t	cur_key = m_arr[cur_pos].key;
			if (cur_key == key) {
				return(cur_pos);
			} else if (cur_key == UNUSED) {
				return(INVALID_POS);
			}
		}

		return(INVALID_POS);
	}

	/** Insert the given key into the array or return its position if
	already present.
	@param[in]	key	key to insert or whose position to retrieve
	@return position of the inserted or previously existent key */
	size_t
	insert_or_get_position(
		uintptr_t	key)
	{
		const size_t	start = guess_position(key);

		/* We do not have os_compare_and_swap_ptr(), thus we use
		os_compare_and_swap_ulint(). */
		ut_ad(sizeof(uintptr_t) == sizeof(ulint));

		for (size_t i = start; i < m_arr_size + start; i++) {
			const size_t	cur_pos = i % m_arr_size;
			const uintptr_t	cur_key = m_arr[cur_pos].key;

			if (cur_key == key
			    || (cur_key == UNUSED
				&& os_compare_and_swap_ulint(
					&m_arr[cur_pos].key, UNUSED, key))) {
				/* Here m_arr[cur_pos].val is either NOT_FOUND
				(as it was initialized in the constructor) or
				some real value. */
				return(cur_pos);
			}
		}

		ut_error;

		return(INVALID_POS);
	}

	/** Storage for the (key, val) tuples. */
	key_val_t*	m_arr;

	/** Number of elements in 'm_arr'. Replace all "x % m_arr_size"
	expressions with "x & (m_arr_size - 1)" and an assert that m_arr_size
	is a power of 2 if there is any visible performance difference. */
	size_t		m_arr_size;
};

#endif /* ut0lock_free_hash_h */
