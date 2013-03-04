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
template <typename Type, typename Factory, typename LockStrategy>
struct Pool {

	typedef Type value_type;

	// FIXME: Add an assertion to check alignment and offset is
	// as we expect it. Also, sizeof(void*) can be 8, can we impove on this.
	struct Element {
		Pool*		m_pool;
		value_type	m_type;
	};

	/** Constructor
	@param size	size of the memory block */
	Pool(size_t size)
		:
		m_ptr(),
		m_size(size)
	{
		ut_a(size >= sizeof(Element));

		create();

		ut_ad(m_pqueue.size()
		      == (reinterpret_cast<byte*>(m_end)
			  - reinterpret_cast<byte*>(m_ptr))
		         / sizeof(Element));
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

		m_lock_strategy.enter();

		if (!m_pqueue.empty()) {
			elem = m_pqueue.top();
			m_pqueue.pop();
		}

		m_lock_strategy.exit();

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

	/* We only need to compare on pointer address. */
	typedef std::priority_queue<
		Element*, std::vector<Element*>, std::greater<Element*> >
		pqueue_t;

	/** Release the object to the free pool
	@param elem	element to free */
	void put(Element* elem)
	{
		m_lock_strategy.enter();

		ut_ad((void*) elem >= m_ptr && elem < m_end);

		m_pqueue.push(elem);

		m_lock_strategy.exit();
	}

	/**
	Create the pool.
	@param size	Size of the the memory block */
	void create()
	{
		m_lock_strategy.create();

		ut_a(m_ptr == 0);
		m_ptr = mem_zalloc(m_size);

		ut_d(m_end = reinterpret_cast<byte*>(m_ptr) + m_size);

		Element*	elem = reinterpret_cast<Element*>(m_ptr);

		for (size_t i = 0; i < m_size / sizeof(*elem); ++i, ++elem) {

			elem->m_pool = this;
			Factory::init(&elem->m_type);
			m_pqueue.push(elem);
		}

		ut_ad(elem < m_end);
	}

	/** Destroy the queue */
	void destroy()
	{
		m_lock_strategy.destroy();
#if 0
		Element*	prev = 0;

		/** FIXME: This should be the correct version, but there is a
		dangling transaction somewhere that we need to find out. Only
		in the following tests: innodb.innodb-multiple-tablespaces and
		i_innodb.innodb_bug14669848 */
		ut_ad(m_pqueue.size()
		      == (reinterpret_cast<byte*>(m_end)
			  - reinterpret_cast<byte*>(m_ptr)) / sizeof(*prev));

		while (!m_pqueue.empty()) {

			Element*	elem = m_pqueue.top();

			Factory::destroy(&elem->m_type);
			m_pqueue.pop();

			ut_a(prev != elem);
			prev = elem;
		}
#else
		Element*	elem = reinterpret_cast<Element*>(m_ptr);

		for (size_t i = 0; i < m_size / sizeof(*elem); ++i, ++elem) {

			ut_ad(elem->m_pool == this);
			Factory::destroy(&elem->m_type);
		}
#endif

		mem_free(m_ptr);
		m_ptr = 0;
		m_size = 0;
	}

private:
#ifdef UNIV_DEBUG
	/** Pointer to the end of the block */
	void*			m_end;
#endif /* UNIV_DEBUG */

	/** Pointer to the base of the block */
	void*			m_ptr;

	/** Size of the block in bytes */
	size_t			m_size;

	/** Priority queue ordered on the pointer addresse. */
	pqueue_t		m_pqueue;

	/** Lock strategy to use */
	LockStrategy		m_lock_strategy;
};

template <typename Pool, typename LockStrategy> 
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

		m_lock_strategy.enter();

		m_pools.push_back(pool);

		ib_logf(IB_LOG_LEVEL_INFO,
			"Number of pools: %lu", m_pools.size());

		m_lock_strategy.exit();
	}

	/** Get an element from one of the pools.
	@return instance or NULL if pool is empty. */
	value_type* get()
	{
		size_t	index = 0;
		value_type*	ptr = NULL;

		do {
			m_lock_strategy.enter();

			ut_ad(!m_pools.empty());

			size_t	n_pools = m_pools.size();

			PoolType*	pool = m_pools[index % n_pools];

			m_lock_strategy.exit();

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
		m_lock_strategy.create();
	}

	/** Release the resources. */
	void destroy()
	{
		typename Pools::iterator it;
		typename Pools::iterator end = m_pools.end();

		for (it = m_pools.begin(); it != end; ++it) {
			PoolType*	pool = *it;

			delete pool;
		}

		m_pools.clear();

		m_lock_strategy.destroy();
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

	/** Lock strategy to use */
	LockStrategy		m_lock_strategy;
};

#endif /* ut0pool_h */
