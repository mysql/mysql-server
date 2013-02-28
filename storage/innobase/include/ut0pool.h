/*****************************************************************************

Copyright (c) 2013, Oracle and/or its affiliates. All Rights Reserved.

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

/******************************************************************//**
@file include/ut0pool.h
Object pool.

Created 2012-Feb-26 Sunny Bains
***********************************************************************/

#ifndef ut0pool_h
#define ut0pool_h

#include <vector>
#include <queue>

/** Allocate the memory for the object in blocks. We keep the objects sorted
on pointer so that they are closer together in case they have to be iterated
over in a list. */
template <typename Type, typename Factory>
struct Pool {

	typedef Type value_type;

	// FIXME: Add an assertion to check alignment and offset is
	// as we expect it. Also, sizeof(void*) is big, can we impove on this.
	struct Element {
		Pool*		m_pool;
		value_type	m_type;
	};

	/** Constructor
	@param size	size of the memory block */
	Pool(size_t size)
		:
		m_ptr()
	{
		ut_a(size >= sizeof(Element));

		create(size);
	}

	/** Destructor */
	~Pool()
	{
		destroy();
	}

	/** Get an object from the pool.
	@retrun a free transaction or NULL if exhausted. */
	Type*	get()
	{
		Element*	elem = NULL;

		lock();

		if (!m_pqueue.empty()) {
			elem = m_pqueue.top();
			m_pqueue.pop();
		}

		unlock();

		return(elem != NULL ? &elem->m_type : 0);
	}

	/** Add the object to the pool.
	@param ptr	object to free */
	static void free(value_type* ptr)
	{
		Element*	elem;

		byte*	p = reinterpret_cast<byte*>(ptr);
		elem = reinterpret_cast<Element*>(p - sizeof(Pool*));

		elem->m_pool->put(elem);
	}

protected:
	// Disable copying
	Pool(const Pool&);
	Pool& operator=(const Pool&);

private:

	/** Acquire the mutex */
	void lock()
	{
		mutex_enter(&m_mutex);
	}

	/** Release the mutex */
	void unlock()
	{
		mutex_exit(&m_mutex);
	}

	/* We only need to compare on pointer address. */
	typedef std::priority_queue<
		Element*, std::vector<Element*>, std::greater<Element*> >
		pqueue_t;

	/** Release the object to the free pool
	@param elem	element to free */
	void put(Element* elem)
	{
		lock();

		ut_ad((void*) elem >= m_ptr && elem < m_end);

		m_pqueue.push(elem);

		unlock();
	}

	/**
	Create the pool.
	@param size	Size of the the memory block */
	void create(size_t size)
	{
		mutex_create(pool_mutex_key, &m_mutex, SYNC_NO_ORDER_CHECK);

		m_ptr = mem_zalloc(size);
		ut_d(m_end = reinterpret_cast<byte*>(m_ptr) + size);

		Element*	elem = reinterpret_cast<Element*>(m_ptr);

		for (size_t i = 0; i < size / sizeof(*elem); ++i, ++elem) {

			elem->m_pool = this;
			Factory::init(&elem->m_type);
			m_pqueue.push(elem);
		}

		ut_ad(elem < m_end);
	}

	/** Destroy the queue */
	void destroy()
	{
		mutex_free(&m_mutex);

		Element*	prev = 0;

		while (!m_pqueue.empty()) {

			Element*	elem = m_pqueue.top();

			Factory::destroy(&elem->m_type);
			m_pqueue.pop();

			ut_a(prev != elem);
			prev = elem;
		}

		mem_free(m_ptr);
		m_ptr = 0;
	}

private:
#ifdef UNIV_DEBUG
	/** Pointer to the end of the block */
	void*			m_end;
#endif /* UNIV_DEBUG */

	/** Pointer to the base of the block */
	void*			m_ptr;

	/** Mutex to control concurrent acces to the queue. */
	ib_mutex_t		m_mutex;

	/** Priority queue ordered on the pointer addresse. */
	pqueue_t		m_pqueue;
};

template <typename Pool> 
struct PoolManager {

	typedef Pool PoolType;
	typedef typename PoolType::value_type value_type;

	PoolManager(size_t size)
		:
		m_size(size)
	{
		create();
	}

	~PoolManager()
	{
		destroy();

		ut_a(m_pools.empty());
	}

	/** Add a new pool */
	void add_pool()
	{
		PoolType*	pool = new (std::nothrow) PoolType(m_size);

		// FIXME: Add proper OOM handling
		ut_a(pool != 0);

		mutex_enter(&m_mutex);

		m_pools.push_back(pool);

		ib_logf(IB_LOG_LEVEL_INFO,
			"Number of pools: %lu", m_pools.size());

		mutex_exit(&m_mutex);
	}

	/** Get an element from one of the pools.
	@return instance or NULL if pool is empty. */
	value_type* get()
	{
		size_t	index = 0;
		value_type*	ptr = NULL;

		do {
			mutex_enter(&m_mutex);

			ut_ad(!m_pools.empty());

			size_t	n_pools = m_pools.size();

			PoolType*	pool = m_pools[index % n_pools];

			mutex_exit(&m_mutex);

			ptr = pool->get();

			if (ptr == 0 && (index / n_pools) > 2) {
				add_pool();
			}

			++index;

		} while (ptr == NULL);

		return(ptr);
	}

	static void free(value_type* ptr)
	{
		PoolType::free(ptr);	
	}

private:
	/** Create the pool manager. */
	void create()
	{
		ut_a(m_size > sizeof(value_type));
		mutex_create(pools_mutex_t, &m_mutex, SYNC_NO_ORDER_CHECK);
	}

	/** Release the resources. */
	void destroy()
	{
		typename Pools::iterator it;
		typename Pools::iterator end = m_pools.end();

		for (it = m_pools.begin(); it != end; ++it) {
			delete *it;
		}

		m_pools.clear();

		mutex_free(&m_mutex);
	}
private:
	// Disable copying
	PoolManager(const PoolManager&);
	PoolManager& operator=(const PoolManager&);

	typedef std::vector<PoolType*> Pools;

	/** Size of each block */
	size_t		m_size;

	/** Pools managed this manager */
	Pools		m_pools;

	/** Mutex protecting this pool manager */
	ib_mutex_t	m_mutex;
};

#endif /* ut0pool_h */
