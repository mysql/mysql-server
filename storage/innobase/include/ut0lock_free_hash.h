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
#include <boost/lockfree/queue.hpp>

/* http://www.boost.org/doc/libs/1_58_0/doc/html/atomic/interface.html#atomic.interface.feature_macros */
#if BOOST_ATOMIC_INT64_LOCK_FREE != 2
#error BOOST_ATOMIC_INT64_LOCK_FREE is not 2
#endif
#if BOOST_ATOMIC_ADDRESS_LOCK_FREE != 2
#error BOOST_ATOMIC_ADDRESS_LOCK_FREE is not 2
#endif

#include "os0numa.h" /* os_numa_*() */
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

/** Multi counter. A counter class that uses a few counter variables
internally to improve performance on machines with lots of CPUs. The get()
method sums all the internal counters without taking any locks, so due to
concurrent modification of the counter, get() may return a number which
never was the sum of all the internal counters. */
class ut_lock_free_cnt_t {
public:
	/** Constructor. */
	ut_lock_free_cnt_t()
	{
		m_numa_available = os_numa_available() != -1;

		if (m_numa_available) {
			m_cnt_size = os_numa_num_configured_cpus();
		} else {
			m_cnt_size = 256;
		}

		m_cnt = UT_NEW_ARRAY(boost::atomic_int64_t*, m_cnt_size,
				     mem_key_ut_lock_free_hash_t);

		for (size_t i = 0; i < m_cnt_size; i++) {

			const size_t	s = sizeof(boost::atomic_int64_t);
			void*		mem;

			if (m_numa_available) {
				const int	node = os_numa_node_of_cpu(i);

				mem = os_numa_alloc_onnode(s, node);
			} else {
				mem = ut_malloc(s,
						mem_key_ut_lock_free_hash_t);
			}

			ut_a(mem != NULL);

			m_cnt[i] = new (mem) boost::atomic_int64_t;

			m_cnt[i]->store(0, boost::memory_order_relaxed);
		}
	}

	/** Destructor. */
	~ut_lock_free_cnt_t()
	{
		/* Needed in order to be able to explicitly call the
		destructor of boost::atomic_int64_t below. */
		using namespace boost;

		for (size_t i = 0; i < m_cnt_size; i++) {
			m_cnt[i]->~atomic_int64_t();

			if (m_numa_available) {
				os_numa_free(m_cnt[i],
					     sizeof(boost::atomic_int64_t));
			} else {
				ut_free(m_cnt[i]);
			}
		}

		UT_DELETE_ARRAY(m_cnt);
	}

	/** Increment the counter. */
	void
	inc()
	{
		const size_t	i = n_cnt_index();

		m_cnt[i]->fetch_add(1, boost::memory_order_relaxed);
	}

	/** Decrement the counter. */
	void
	dec()
	{
		const size_t	i = n_cnt_index();

		m_cnt[i]->fetch_sub(1, boost::memory_order_relaxed);
	}

	/** Get the value of the counter.
	@return counter's value */
	int64_t
	get() const
	{
		int64_t	ret = 0;

		for (size_t i = 0; i < m_cnt_size; i++) {
			ret += m_cnt[i]->load(boost::memory_order_relaxed);
		}

		return(ret);
	}

private:
	/** Derive an appropriate index in m_cnt[] for the current thread.
	@return index in m_cnt[] for this thread to use */
	size_t
	n_cnt_index() const
	{
		size_t	cpu;

#ifdef HAVE_OS_GETCPU
		cpu = static_cast<size_t>(os_getcpu());

		if (cpu >= m_cnt_size) {
			/* Could happen (rarely) if more CPUs get
			enabled after m_cnt_size is initialized. */
			cpu %= m_cnt_size;
		}
#else /* HAVE_OS_GETCPU */
		cpu = static_cast<size_t>(ut_rnd_gen_ulint() % m_cnt_size);
#endif /* HAVE_OS_GETCPU */

		return(cpu);
	}

	/** Designate whether os_numa_*() functions can be used. */
	bool			m_numa_available;

	/** The sum of all the counters in m_cnt[] designates the overall
	count. */
	boost::atomic_int64_t**	m_cnt;
	size_t			m_cnt_size;
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
		m_garbaged(false),
		M_NEXT_RETRY_FROM_START(reinterpret_cast<next_t>(0x1)),
		m_next(NULL)
	{
		m_base = UT_NEW_ARRAY(T, m_n_base_elements,
				      mem_key_ut_lock_free_hash_t);

		ut_ad(n_elements > 0);
	}

	/** Destructor. */
	~ut_lock_free_list_node_t()
	{
		UT_DELETE_ARRAY(m_base);
	}

	/** Create and append a new array to this one and store a pointer
	to it in 'm_next'. This is done in a way that multiple threads can
	attempt this at the same time and only one will succeed. When this
	method returns, the caller can be sure that the job is done (either
	by this or another thread).
	@param[out]	grown_by_this_thread	set to true if the next
	array was created and appended by this thread; set to false if
	created and appended by another thread. */
	void
	grow(
		bool*	grown_by_this_thread)
	{
		/* XXX do not *2 if the current array has too many deleted
		entries */
		next_t	new_arr = UT_NEW(
			ut_lock_free_list_node_t<T>(m_n_base_elements * 2),
			mem_key_ut_lock_free_hash_t);

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

			*grown_by_this_thread = false;
		} else {
			*grown_by_this_thread = true;
		}
	}

	/** Get a pointer to the next array in the list.
	@param[in]	list_start	the beginning of the list, which may
	be returned if the current array has been garbage collected and its
	next pointer demolished.
	@return the next array or NULL if no next */
	next_t
	get_next(
		next_t	list_start) const
	{
		next_t	next = m_next.load(boost::memory_order_relaxed);

		return(next != M_NEXT_RETRY_FROM_START ? next : list_start);
	}

	/** Change the next pointer to the specified one.
	@param[in]	new_next	new next pointer to set */
	void
	change_next(
		next_t	new_next)
	{
		m_next.store(new_next, boost::memory_order_relaxed);
	}

	/** Disable the next pointer. Attempts to go to the next pointer will
	be redirected to the start of the list. */
	void
	disable_next()
	{
		m_next.store(M_NEXT_RETRY_FROM_START,
			     boost::memory_order_relaxed);
	}

	/* This object is only ever destroyed after it is removed from the
	list of arrays in the hash table (which means that new threads cannot
	start using it) and the number of threads that use it has decreased
	to zero. */

	/** Mark the beginning of an access to this object. Used to prevent a
	destruction of this object while some threads may be accessing it. */
	bool
	begin_access()
	{
		m_n_ref.inc();

		boost::atomic_thread_fence(boost::memory_order_acq_rel);

		if (m_garbaged.load(boost::memory_order_relaxed)) {
			m_n_ref.dec();
			return(false);
		}

		return(true);
	}

	/** Mark the ending of an access to this object. */
	void
	end_access()
	{
		boost::atomic_thread_fence(boost::memory_order_release);

		m_n_ref.dec();
	}

	/** Get the number of threads that are accessing this object now.
	@return number of users (threads) of this object */
	int64_t
	n_ref()
	{
		return(m_n_ref.get());
	}

	/** Base array. */
	T*			m_base;

	/** Number of elements in 'm_base'. */
	size_t			m_n_base_elements;

	/** Designate whether the current object is in the garbage bin. */
	boost::atomic_bool	m_garbaged;

private:
	/** A constant that is set in the m_next member and designates that
	the search should be retried from the start of the list. This is used
	for arrays that are garbage collected - their m_next members are set
	to this so that readers can never traverse from one garbage collected
	array into another garbage collected array (if the m_next pointer
	of the first points to the second). When destroying those objects
	we assume that once an object is garbage collected then new readers
	can never arrive to it. */
	next_t			M_NEXT_RETRY_FROM_START;

	/** Pointer to the next node if any or NULL. */
	boost::atomic<next_t>	m_next;

	/** Counter for the current number of readers and writers to this
	object. This object is destroyed only after it is removed from the
	list, so that no new readers or writers may arrive, and after this
	counter has dropped to zero. */
	ut_lock_free_cnt_t	m_n_ref;
};

/** Lock free hash table. It stores (key, value) pairs where both the key
and the value are of integer type.
* Transitions for keys (a real key is anything other than UNUSED and AVOID):
  * UNUSED -> real key -- allowed
  * UNUSED -> AVOID -- allowed
  anything else is not allowed:
  * real key -> UNUSED -- not allowed
  * real key -> AVOID -- not allowed
  * real key -> another real key -- not allowed
  * AVOID -> UNUSED -- not allowed
  * AVOID -> real key -- not allowed
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
	initially. Must be a power of 2, greater than 0.
	@param[in]	del_when_zero	if true then automatically delete a
	tuple from the hash if due to increment or decrement its value becomes
	zero. */
	explicit
	ut_lock_free_hash_t(
		size_t	initial_size,
		bool	del_when_zero)
	:
	m_del_when_zero(del_when_zero)
#ifdef UT_HASH_IMPLEMENT_PRINT_STATS
	, m_n_search(0)
	, m_n_search_iterations(0)
#endif /* UT_HASH_IMPLEMENT_PRINT_STATS */
	{
		ut_a(initial_size > 0);
		ut_a(ut_is_2pow(initial_size));

		m_data.store(
			UT_NEW(arr_node_t(initial_size),
			       mem_key_ut_lock_free_hash_t),
			boost::memory_order_relaxed);

		m_sentinel.store(M_SENTINEL_UNLOCKED,
				 boost::memory_order_relaxed);
	}

	/** Destructor. Not thread safe. */
	~ut_lock_free_hash_t()
	{
		arr_node_t*	d = m_data.load(boost::memory_order_relaxed);
		arr_node_t*	cur = d;

		do {
			arr_node_t*	next = cur->get_next(d);

			/* Only garbage collected arrays have their next set
			to the start of the list (m_data). */
			ut_a(next != d);

			UT_DELETE(cur);

			cur = next;
		} while (cur != NULL);
	}

	/** Get the value mapped to a given key.
	@param[in]	key	key to look for
	@return the value that corresponds to key or NOT_FOUND. */
	int64_t
	get(
		uint64_t	key) const
	{
		ut_ad(key != UNUSED);
		ut_ad(key != AVOID);

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
				arr->end_access();
				return(NOT_FOUND);
			} else if (v != GOTO_NEXT_ARRAY) {
				arr->end_access();
				return(v);
			}

			/* Prevent reorder of the below m_next.load() (called
			from get_next()) with the above m_val.load().
			We want to be sure that if m_val is GOTO_NEXT_ARRAY,
			then the next array exists. It would be the same to
			m_val.load(memory_order_acquire)
			but that would impose the more expensive
			memory_order_acquire in all cases, whereas in the most
			common execution path m_val is not GOTO_NEXT_ARRAY and
			we return earlier, only using the cheaper
			memory_order_relaxed. */
			boost::atomic_thread_fence(boost::memory_order_acquire);

			arr_node_t*	next = arr->get_next(
				m_data.load(boost::memory_order_relaxed));

			ut_a(next != NULL);

			arr->end_access();

			arr = next;
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
		ut_ad(key != AVOID);
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
		ut_ad(key != AVOID);

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

					arr->end_access();
					return;
				}

				/* CAS stored the most recent value of 'm_val'
				into 'v'. */
			}

			/* Prevent reorder of the below m_next.load() (called
			from get_next()) with the above m_val.load() or the
			load from m_val.compare_exchange_strong().
			We want to be sure that if m_val is GOTO_NEXT_ARRAY,
			then the next array exists. It would be the same to
			m_val.load(memory_order_acquire) or
			m_val.compare_exchange_strong(memory_order_acquire)
			but that would impose the more expensive
			memory_order_acquire in all cases, whereas in the most
			common execution path m_val is not GOTO_NEXT_ARRAY and
			we return earlier, only using the cheaper
			memory_order_relaxed. */
			boost::atomic_thread_fence(boost::memory_order_acquire);

			arr_node_t*	next = arr->get_next(
				m_data.load(boost::memory_order_relaxed));

			ut_a(next != NULL);

			arr->end_access();

			arr = next;
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
		ut_ad(key != AVOID);

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
		ut_ad(key != AVOID);

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

	/** A key == AVOID designates an unusable cell. This cell of the array
	has been empty (key == UNUSED), but was then marked as AVOID in order
	to prevent new inserts into it. Searches should treat this like
	UNUSED (ie if they encounter it before the key they are searching for
	then stop the search and declare 'not found'). */
	static const uint64_t	AVOID = UNUSED - 1;

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
	the tuple with the given key or find a tuple with key == UNUSED or AVOID
	which means that the key is not present in the array.
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
			} else if (cur_key == UNUSED || cur_key == AVOID) {
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
			while (!(*arr)->begin_access()) {
				/* The array has been garbaged, restart
				the search from the beginning. */
				*arr = m_data.load(boost::memory_order_relaxed);
			}

			key_val_t*	t = get_tuple_from_array(
				(*arr)->m_base,
				(*arr)->m_n_base_elements,
				key);

			if (t != NULL) {
				/* end_access() will be invoked by the
				caller. */
				return(t);
			}

			arr_node_t*	next = (*arr)->get_next(
				m_data.load(boost::memory_order_relaxed));

			(*arr)->end_access();

			if (next == NULL) {
				return(NULL);
			}

			*arr = next;
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
					return(cur_tuple);
				}

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

			/* Skip through tuples with key == AVOID. */
		}

		return(NULL);
	}

	/** Copy all used elements from one array to another. Flag the ones
	in the old array as 'go to the next array'.
	@param[in,out]	src_arr	array to copy from
	@param[in,out]	dst_arr	array to copy to */
	void
	copy_to_another_array(
		arr_node_t*	src_arr,
		arr_node_t*	dst_arr)
	{
		for (size_t i = 0; i < src_arr->m_n_base_elements; i++) {
			key_val_t*	t = &src_arr->m_base[i];

			uint64_t	k = t->m_key.load(
				boost::memory_order_relaxed);

			/* Prevent further inserts into empty cells. */
			if (k == UNUSED
			    && t->m_key.compare_exchange_strong(
				    k,
				    AVOID,
				    boost::memory_order_relaxed)) {
				continue;
			}

			int64_t	v = t->m_val.load(boost::memory_order_relaxed);

			/* Insert (k, v) into the destination array. We know
			that nobody else will try this concurrently with this
			thread because:
			* this code is being executed by just one thread (the
			  thread that managed to grow the list of arrays) and
			* other normal inserts/updates with (key == k) will
			  pick the entry in src_arr. */

			for (;;) {
				if (v != DELETED) {
					insert_or_update(k, v, false, dst_arr);
				}

				/* Prevent any preceding memory operations (the
				stores from insert_or_update() in particular)
				to be reordered past the store from
				m_val.compare_exchange_strong() below. We want
				to be sure that if m_val is GOTO_NEXT_ARRAY,
				then the entry is indeed present in some of the
				next arrays (ie that insert_or_update() has
				completed and that its effects are visible to
				other threads). */
				boost::atomic_thread_fence(
					boost::memory_order_release);

				/* Now that we know (k, v) is present in some
				of the next arrays, try to CAS the tuple
				(k, v) to (k, GOTO_NEXT_ARRAY) in the current
				array. */

				if (t->m_val.compare_exchange_strong(
						v,
						GOTO_NEXT_ARRAY,
						boost::memory_order_relaxed)) {
					break;
				}

				/* If CAS fails, this means that m_val has been
				changed in the meantime and the CAS will store
				m_val's most recent value in 'v'. Retry both
				operations (this time the insert_or_update()
				call will be an update, rather than an
				insert). */
			}
		}
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

			int64_t	new_val;

			if (is_delta && v != NOT_FOUND && v != DELETED) {
				if (m_del_when_zero && v + val == 0) {
					new_val = DELETED;
				} else {
					new_val = v + val;
				}
			} else {
				new_val = val;
			}

			if (t->m_val.compare_exchange_strong(
					v,
					new_val,
					boost::memory_order_relaxed)) {
				return(true);
			}

			/* When CAS fails it sets the most recent value of
			m_val into v. */
		}
	}

	/** Find the previous array to a given array.
	@param[in]	arr	array whose previous to find, must be in the
	list of arrays that starts at m_data.
	@return the previous array or NULL if the passed in arr is the first
	entry in the list (m_data). */
	arr_node_t*
	find_prev_arr(
		const arr_node_t*	arr) const
	{
		arr_node_t*	a = m_data.load(boost::memory_order_relaxed);

		/* No previous array because arr is m_data. */
		if (a == arr) {
			return(NULL);
		}

		for (;;) {
			arr_node_t*	next = a->get_next(
				m_data.load(boost::memory_order_relaxed));

			/* Reached the end of the list without finding arr.
			Should not happen because arr is supposed to be in
			the list. */
			if (next == NULL) {
				ut_error;
			}

			if (next == arr) {
				return(a);
			}

			a = next;
		}
	}

	/** Insert a new tuple or update an existent one. If a tuple with this
	key does not exist then a new one is inserted (key, val) and is_delta
	is ignored. If a tuple with this key exists and is_delta is true, then
	the current value is changed to be current value + val, otherwise it
	is overwritten to be val.
	@param[in]	key		key to insert or whose value to update
	@param[in]	val		value to set; if the tuple does not
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
			while (!arr->begin_access()) {
				/* The array has been garbaged, restart
				the search from the beginning. */
				arr = m_data.load(boost::memory_order_relaxed);
			}

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
				arr->end_access();
				return;
			}

			arr_node_t*	next_arr = arr->get_next(
				m_data.load(boost::memory_order_relaxed));

			if (next_arr != NULL) {
				arr->end_access();
				arr = next_arr;
				/* Prevent any subsequent memory operations
				(the reads from the next array in particular)
				to be reordered before the m_next.load()
				above (called from get_next()). */
				boost::atomic_thread_fence(
					boost::memory_order_acquire);
				continue;
			}

			bool		grown_by_this_thread;

			arr->grow(&grown_by_this_thread);

			next_arr = arr->get_next(
				m_data.load(boost::memory_order_relaxed));

			if (!grown_by_this_thread) {
				arr->end_access();
				arr = next_arr;
				continue;
			}

			/* The code below is only executed by a single thread
			(for a given arr) - the winner thread that did the
			grow and got grown_by_this_thread == true.
			The garbage collecting of a given array is only done
			by a single thread (us). So we know that 'arr' will
			not be garbage collected here by another thread. So
			there is no need to protect it with begin_access() /
			end_access() below. */

			arr->end_access();

			while (!next_arr->begin_access()) {
				next_arr = arr->get_next(arr /* pass dummy */);

				/* Make sure the dummy is not returned. If the
				argument to get_next() is returned, this means
				that its m_next pointer is invalidated, but
				this cannot happen because the only place where
				this is done for a given array is below. */
				ut_a(next_arr != arr);
			}

			copy_to_another_array(arr, next_arr);

			next_arr->end_access();

			boost::atomic_thread_fence(
				boost::memory_order_acq_rel);

			/* Now that we know that all the tuples from arr have
			been migrated to the next array - remove 'arr' from the
			list of arrays, so that new readers will never
			encounter it. By now arr is filled with tuples like:
			(k, GOTO_NEXT_ARRAY) or
			(AVOID, NOT_FOUND). */

			bool	expected = M_SENTINEL_UNLOCKED;
			while (!m_sentinel.compare_exchange_strong(
					expected,
					M_SENTINEL_LOCKED,
					boost::memory_order_acquire)) {
				/* busy loop */
				expected = M_SENTINEL_UNLOCKED;
			}

			/* Now we are the only thread that executes the code
			below. */

			/* Re-read arr->m_next in case it got changed after
			grow(). Ie if the array that grow() appended was
			garbage collected and now arr->m_next points to some
			further entry in the list. */
			arr_node_t*	d = m_data.load(
				boost::memory_order_relaxed);

			next_arr = arr->get_next(d);

			/* arr->m_next cannot be disabled here (get_next()
			returns 'd') because the only code that sets it
			to M_NEXT_RETRY_FROM_START is below and we are
			guaranteed to be the only thread that ever executes
			it for arr. */
			ut_a(next_arr != d);

			if (arr == m_data.load(boost::memory_order_relaxed)) {

				m_data.store(next_arr,
					     boost::memory_order_relaxed);
			} else {
				arr_node_t*	prev_arr = find_prev_arr(arr);

				ut_a(prev_arr != NULL);

				prev_arr->change_next(next_arr);
			}

			arr->disable_next();

			m_sentinel.store(M_SENTINEL_UNLOCKED,
					 boost::memory_order_release);

			arr->m_garbaged.store(
				true, boost::memory_order_relaxed);

			m_garbage_bin.add(arr);

			m_garbage_bin.collect_some();

			arr = next_arr;
		}
	}

	/** Storage for the (key, val) tuples. */
	boost::atomic<arr_node_t*>	m_data;

	/** A sentinel to synchronize changes of all arr_node_t::m_next
	pointers in the list that begins at m_data. Let the list be:
	...
	A = (..., next = B)
	B = (..., next = C)
	C = (..., next = D)
	...
	When removing B from the list for garbage collection we do this:
	1. Read B.next, it is C
	2. Change A.next from B to what we read in 1., ie C
	the problem is that between 1. and 2. C may be garbage collected and
	removed from the list and B may be changed to B = (..., next = D). In
	this case in step 2. we will set A.next to C which is already scheduled
	for garbage collection and C will then be present in both lists - the
	normal hash table list and the garbage bin list. To avoid this we
	synchronize all modifications of arr_node_t::m_next pointers.
	Multiple resizes still happen in parallel in the bulky part that copies
	elements from the old to the new entry, but just the pointers adjusting
	is synchronized. */
	boost::atomic_bool		m_sentinel;

	/** A value of m_sentinel designating that nobody is fiddling with
	m_next pointers now. */
	static const bool		M_SENTINEL_UNLOCKED = false;

	/** A value of m_sentinel designating that the current thread is
	fiddling with m_next pointers now. */
	static const bool		M_SENTINEL_LOCKED = true;

	/** A place where entries in the list are put awaiting readers to
	go away, so that their m_data can be freed. */
	class garbage_bin_t {
	public:
		/** Constructor. */
		garbage_bin_t()
		{
			m_bin = UT_NEW(
				queue_t(32),
				mem_key_ut_lock_free_hash_t);

			m_bin_hollow = UT_NEW(
				queue_t(32),
				mem_key_ut_lock_free_hash_t);
		}

		/** Destructor. */
		~garbage_bin_t()
		{
			collect_some();

			arr_node_t*	arr;

			while (m_bin_hollow->pop(arr)) {

				ut_a(arr->n_ref() == 0);

				ut_a(arr->m_base == NULL);

				UT_DELETE(arr);
			}

			UT_DELETE(m_bin_hollow);

			UT_DELETE(m_bin);
		}

		/** Add a given array for garbage collection.
		@param[in]	arr	array to garbage collect */
		void
		add(
			arr_node_t*	arr)
		{
			ut_a(m_bin->push(arr));
		}

		/** Free the memory occupied by (some) arrays stored in the
		garbage bin. */
		void
		collect_some()
		{
			arr_node_t*	arr;
			arr_node_t*	first = NULL;

			while (m_bin->pop(arr)) {
				if (arr == first) {
					break;
				}

				if (arr->n_ref() == 0) {
					memset(arr->m_base, 0xAF,
					       sizeof(arr->m_base[0]) * arr->m_n_base_elements);
					UT_DELETE_ARRAY(arr->m_base);
					arr->m_base = NULL;

					/* Add to the bin of hollow objects
					because 'arr' itself needs to be
					deleted too, done in the destructor
					~garbage_bin_t(). */
					ut_a(m_bin_hollow->push(arr));
				} else {
					/* It is being used, return it back to
					the queue for deletion later. */
					add(arr);

					/* Remember the first element from the
					queue which we push back and terminate
					the loop when we encounter it again.
					This is to prevent an infinite loop
					endlessly pop()ing and push()ing
					elements. */
					if (first == NULL) {
						first = arr;
					}
				}
			}
		}

	private:
		typedef boost::lockfree::queue<arr_node_t*>	queue_t;

		/** Arrays that are not used anymore. Their m_data members are
		to be freed when the number of their readers and writes drop
		down to zero. */
		queue_t*					m_bin;

		/** Arrays whose m_data members have been deleted. These hollow
		objects will be freed themselves in the destructor. */
		queue_t*					m_bin_hollow;
	};

	garbage_bin_t			m_garbage_bin;

	/** True if a tuple should be automatically deleted from the hash
	if its value becomes 0 after an increment or decrement. */
	bool				m_del_when_zero;

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
