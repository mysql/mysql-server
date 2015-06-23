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

	/** Decrement the value of a given key with 1 or insert a new tuple
	(key, -1).
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

	/** Get the next array in the chain, creating it if it does not exist.
	@return the next array. */
	next_t
	get_next_grow_if_necessary()
	{
		next_t	next = m_next.load(boost::memory_order_relaxed);

		if (next == NULL) {
			next = grow();
		}

		return(next);
	}

	/** Base array. */
	T*			m_base;

	/** Number of elements in 'm_base'. */
	size_t			m_n_base_elements;

	/** Pointer to the next node if any or NULL. */
	boost::atomic<next_t>	m_next;

private:

	/** Create and append a new array to this one and store a pointer
	to it in 'm_next'. This is done in a way that multiple threads can
	attempt this at the same time and only one will succeed. When this
	method returns, the caller can be sure that the job is done (either
	by this or another thread).
	@return the next array, created and appended by this or another
	thread */
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

			/* 'expected' has the current value which
			must be != NULL because the CAS failed. */
			ut_ad(expected != NULL);

			return(expected);
		}

		return(new_arr);
	}
};

/** Lock free hash table. It stores (key, value) pairs where both the key
and the value are of integer type.
* Transitions for keys (a real key is anything other than UNUSED):
  * UNUSED -> real key -- allowed
  anything else is not allowed:
  * real key -> UNUSED -- not allowed
  * real key -> another real key -- not allowed
* Transitions for values (a real value is anything other than NOT_FOUND,
  DELETED and GOTO_NEXT_ARRAY):
  * NOT_FOUND -> real value -- allowed
  * NOT_FOUND -> DELETED -- allowed
  * real value -> another real value -- allowed
  * real value -> DELETED -- allowed
  * real value -> GOTO_NEXT_ARRAY -- allowed
  * DELETED -> real value -- allowed
  * DELETED -> GOTO_NEXT_ARRAY -- allowed
  anything else is not allowed:
  * NOT_FOUND -> GOTO_NEXT_ARRAY -- not allowed
  * real value -> NOT_FOUND -- not allowed
  * DELETED -> NOT_FOUND -- not allowed
  * GOTO_NEXT_ARRAY -> real value -- not allowed
  * GOTO_NEXT_ARRAY -> NOT_FOUND -- not allowed
  * GOTO_NEXT_ARRAY -> DELETED -- not allowed
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

		m_data.store(
			UT_NEW(arr_node_t(initial_size),
			       mem_key_buf_stat_per_index_t),
			boost::memory_order_relaxed);
	}

	/** Destructor. Not thread safe. */
	~ut_lock_free_hash_t()
	{
		UT_DELETE(m_data.load(boost::memory_order_relaxed));
	}

	/** Get the value mapped to a given key.
	@param[in]	key	key to look for
	@return the value that corresponds to key or NOT_FOUND. */
	int64_t
	get(
		uint64_t	key) const
	{
		ut_ad(key != UNUSED);

		arr_node_t*	arr = m_data.load(boost::memory_order_relaxed);

		for (;;) {
			const key_val_t*	tuple = get_tuple(key, &arr);

			if (tuple == NULL) {
				return(NOT_FOUND);
			}

			/* Here if another thread is just setting this key
			for the first time, then the tuple could be
			(key, NOT_FOUND) (remember all vals are initialized
			to NOT_FOUND initially) in which case we will return
			NOT_FOUND below which is fine. */

			int64_t	v = tuple->m_val.load(
				boost::memory_order_relaxed);

			if (v == DELETED) {
				return(NOT_FOUND);
			} else if (v != GOTO_NEXT_ARRAY) {
				return(v);
			}

			arr = arr->m_next.load(boost::memory_order_relaxed);

			if (arr == NULL) {
				return(NOT_FOUND);
			}
		}
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
		ut_ad(val != NOT_FOUND);
		ut_ad(val != DELETED);
		ut_ad(val != GOTO_NEXT_ARRAY);

		insert_or_update(key, val, false,
				 m_data.load(boost::memory_order_relaxed));
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
	(key == 5, val == DELETED), which get()s for key == 5 will return as
	NOT_FOUND.
	[2] If del() executes first then the tuple will become
	(key == 5, val == DELETED) and then inc() will change it to
	(key == 5, value == 1).
	It is undefined which one of [1] or [2] will happen. It is up to the
	caller to accept this behavior or prevent it at a higher level.
	@param[in]	key	key whose pair to delete */
	void
	del(
		uint64_t	key)
	{
		ut_ad(key != UNUSED);

		arr_node_t*	arr = m_data.load(boost::memory_order_relaxed);

		for (;;) {
			key_val_t*	tuple = get_tuple(key, &arr);

			if (tuple == NULL) {
				/* Nothing to delete. */
				return;
			}

			int64_t	v = tuple->m_val.load(
				boost::memory_order_relaxed);

			for (;;) {
				if (v == GOTO_NEXT_ARRAY) {
					break;
				}

				if (tuple->m_val.compare_exchange_strong(
						v,
						DELETED,
						boost::memory_order_relaxed)) {
					return;
				}

				/* CAS stored the most recent value of 'm_val'
				into 'v'. */
			}

			arr = arr->m_next.load(boost::memory_order_relaxed);

			if (arr == NULL) {
				return;
			}
		}
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

		insert_or_update(key, 1, true,
				 m_data.load(boost::memory_order_relaxed));
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

		insert_or_update(key, -1, true,
				 m_data.load(boost::memory_order_relaxed));
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

	/** A val == DELETED designates that this cell in the array has been
	used in the past, but it was deleted later. Searches should return
	NOT_FOUND when they encounter it. */
	static const int64_t	DELETED = NOT_FOUND - 1;

	/** A val == GOTO_NEXT_ARRAY designates that this tuple (key, whatever)
	has been moved to the next array. The search for it should continue
	there. */
	static const int64_t	GOTO_NEXT_ARRAY = DELETED - 1;

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

	/** An array node in the hash. The hash table consists of a linked
	list of such nodes. */
	typedef ut_lock_free_list_node_t<key_val_t>	arr_node_t;

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
				return(cur_tuple);
			} else if (cur_key == UNUSED) {
				return(NULL);
			}
		}

		return(NULL);
	}

	/** Get the array cell of a key.
	@param[in]	key	key to search for
	@param[in,out]	arr	start the search from this array; when this
	method ends, *arr will point to the array in which the search
	ended (in which the returned key_val resides)
	@return pointer to the array cell or NULL if not found */
	key_val_t*
	get_tuple(
		uint64_t	key,
		arr_node_t**	arr) const
	{
		for (;;) {
			key_val_t*	t = get_tuple_from_array(
				(*arr)->m_base,
				(*arr)->m_n_base_elements,
				key);

			if (t != NULL) {
				return(t);
			}

			*arr = (*arr)->m_next.load(boost::memory_order_relaxed);

			if (*arr == NULL) {
				return(NULL);
			}
		}
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
		}

		return(NULL);
	}

	/** Update the value of a given tuple.
	@param[in,out]	t		tuple whose value to update
	@param[in]	val		value to set or delta to apply
	@param[in]	is_delta	if true then set the new value to
	old + val, otherwise just set to val
	@retval		true		update succeeded
	@retval		false		update failed due to GOTO_NEXT_ARRAY
	@return whether the update succeeded or not */
	bool
	update_tuple(
		key_val_t*	t,
		int64_t		val,
		bool		is_delta)
	{
		int64_t	v = t->m_val.load(boost::memory_order_relaxed);

		for (;;) {
			if (v == GOTO_NEXT_ARRAY) {
				return(false);
			}

			const int64_t	new_val
				= is_delta && v != NOT_FOUND && v != DELETED
				? v + val
				: val;

			if (t->m_val.compare_exchange_strong(
					v,
					new_val,
					boost::memory_order_relaxed)) {
				return(true);
			}

			/* When CAS fails it sets the value of m_val into v. */
		}
	}

	/** Insert a new tuple or update an existent one. If a tuple with this
	key does not exist then a new one is inserted (key, val) and is_delta
	is ignored. If a tuple with this key exists and is_delta is true, then
	the current value is changed to be current value + val, otherwise it
	is overwritten to be val.
	@param[in]	key		key to insert or whose value to update
	@param[in]	val		value to set, if the tuple does not
	exist or if is_delta is false, then the new value is set to val,
	otherwise it is set to old + val
	@param[in]	is_delta	if true then set the new value to
	old + val, otherwise just set to val.
	@param[in]	arr		array to start the search from */
	void
	insert_or_update(
		uint64_t	key,
		int64_t		val,
		bool		is_delta,
		arr_node_t*	arr)
	{
		/* Loop through the arrays until we find a free slot to insert
		or until we find a tuple with the specified key and manage to
		update it. */
		for (;;) {
			key_val_t*	t = insert_or_get_position_in_array(
				arr->m_base,
				arr->m_n_base_elements,
				key);

			/* t == NULL means that the array is full, must expand
			and go to the next array. */

			/* update_tuple() returning false means that the value
			of the tuple is GOTO_NEXT_ARRAY, so we must go to the
			next array. */

			if (t != NULL && update_tuple(t, val, is_delta)) {
				return;
			}

			arr = arr->get_next_grow_if_necessary();
		}
	}

	/** Storage for the (key, val) tuples. */
	boost::atomic<arr_node_t*>	m_data;

#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
	/* The atomic type gives correct results, but has a _huge_
	performance impact. The unprotected operation gives a significant
	skew, but has almost no performance impact. */

	/** Number of searches performed in this hash. */
#if 0
	mutable uint64_t		m_n_search;
#else
	mutable boost::atomic_uint64_t	m_n_search;
#endif

	/** Number of elements processed for all searches. */
#if 0
	mutable uint64_t		m_n_search_iterations;
#else
	mutable boost::atomic_uint64_t	m_n_search_iterations;
#endif
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */
};

#endif /* ut0lock_free_hash_h */
