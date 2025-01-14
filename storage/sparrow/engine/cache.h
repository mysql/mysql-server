/*
	Cache.
*/

#ifndef _engine_cache_h_
#define _engine_cache_h_

#include "../handler/plugin.h"	// For configuration parameters.
#include "exception.h"
#include "types.h"
#include "hash.h"
#include "list.h"
#include "cond.h"
#include "io.h"

#include "mysql/psi/mysql_file.h"

#ifdef _WIN32
#pragma warning(disable:4355)
#endif

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CacheEntry
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<class ID, class V, class H> class CacheEntry : public SYSidlink<CacheEntry<ID, V, H> > {
private:

	uint32_t references_;
	uint32_t valid_:1;
	uint32_t level_:31;	// LRU level from which this entry was initially taken.
	ID id_;
	V value_;

private:

	CacheEntry<ID, V, H>(const CacheEntry<ID, V, H>& right);
	CacheEntry<ID, V, H>& operator = (const CacheEntry<ID, V, H>& right);

public:

	// Default constructor.
	CacheEntry<ID, V, H>()
		: references_(0), valid_(false), level_(0) {
	}

	// Search (key) constructor.
	CacheEntry<ID, V, H>(const ID& id)
		: references_(0), valid_(false), level_(0), id_(id) {
	}

	// Constructor with level and value.
	CacheEntry<ID, V, H>(const uint32_t level, const ID& id, const V& value)
		: references_(0), valid_(false), level_(level), id_(id), value_(value) {
	}

	bool operator == (const CacheEntry<ID, V, H>& right) const {
		if (this != &right) {
			return id_ == right.id_;
		} else {
			return true;
		}
	}

	void acquire() {
		references_++;
	}

	bool release() {
		assert(references_ > 0);
		return --references_ == 0;
	}

	bool isReferenced() const {
		return references_ > 0;
	}

	void setValid(const bool valid) {
		valid_ = valid;
	}

	bool isValid() const {
		return valid_;
	}

	const ID& getId() const {
		return id_;
	}

	void setId(const ID& id) {
		id_ = id;
	}

	const V& getValue() const {
		return value_;
	}

	V& getValue() {
		return value_;
	}

	uint32_t getLevel() const {
		return level_;
	}

	void setLevel(const uint32_t newLevel) {
		level_ = newLevel;
	}

	void initialize(SYSpVector<CacheEntry<ID, V, H>, 64>& entry_vect, const H& hint) _THROW_(SparrowException) {
		value_.initialize(id_, entry_vect, hint);
	}

	void clear() {
		value_.clear();
	}

	uint32_t hash() const {
		return id_.hash();
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CacheStat
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CacheStat {
public:

	volatile uint64_t& acquires_;	// Number of calls to acquire().
	volatile uint64_t& releases_;	// Number of calls to release().
	volatile uint64_t& misses_;	// Number of cache misses when acquire() is called. An IO operation was required.
	volatile uint64_t& hits_;		// Number of cache hits when acquire() is called.
	volatile uint64_t& slowHits_;	// Number of "slow" cache hits when acquire() is called. A "slow" hit occurs
	// when the entry is not in the cache (it must be read from disc) or, when it is in the cache, but it is still being initialized by
	// another thread. In this case, the caller thread must wait until the entry initialization is
	// completed by the other thread.

public:

	CacheStat(volatile uint64_t& acquires, volatile uint64_t& releases, volatile uint64_t& misses, volatile uint64_t& hits, volatile uint64_t& slowHits)
		: acquires_(acquires), releases_(releases), misses_(misses), hits_(hits), slowHits_(slowHits) {
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// LvlCacheStat
//////////////////////////////////////////////////////////////////////////////////////////////////////

class LvlCacheStat {
public:

	volatile uint64_t* misses_;	// Number of cache misses when acquire() is called. An IO operation was required.
	volatile uint64_t* hits_;		// Number of cache hits when acquire() is called.
	volatile uint64_t* slowHits_;	// Number of "slow" cache hits when acquire() is called. A "slow" hit occurs
	// when the entry is not in the cache (it must be read from disc) or, when it is in the cache, but it is still being initialized by
	// another thread. In this case, the caller thread must wait until the entry initialization is
	// completed by the other thread.

public:

	LvlCacheStat() : misses_(NULL), hits_(NULL), slowHits_(NULL) {
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CacheLevel
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<class ID, class V, class H> class CacheLevel {
private:

	const uint32_t level_;
	SYSidlist<CacheEntry<ID, V, H> > lru_;	// Unreferenced (i.e. can be reused) entries for this level.
	const uint32_t capacity_;					// Initial number of entries in this level.
	Cond cond_;								// Condition variable set when the level is not empty.
	volatile uint32_t*	fillRatio_;

private:

	static Str getName(const char* name, const uint32_t level);

public:

	CacheLevel<ID, V, H>(const uint32_t level, Lock& lock, SYSidlist<CacheEntry<ID, V, H> >& list);

	CacheEntry<ID, V, H>* acquire(const bool wait = true);

	void remove(CacheEntry<ID, V, H>* entry);

	void release(CacheEntry<ID, V, H>* entry);

	void getExtraEntries(SYSidlist<CacheEntry<ID, V, H> >& list);

	void balance(SYSidlist<CacheEntry<ID, V, H> >& list);
};

// STATIC
template<class ID, class V, class H> inline Str CacheLevel<ID, V, H>::getName(const char* name, const uint32_t level) {
	char tmp[128];
	snprintf(tmp, sizeof(tmp), "%s::CacheLevel%u::cond_", name, level);
	return Str(tmp);
}

template<class ID, class V, class H> inline CacheLevel<ID, V, H>::CacheLevel(const uint32_t level, Lock& lock,
	SYSidlist<CacheEntry<ID, V, H> >& list) : level_(level), capacity_(list.entries()), cond_(false, lock, CacheLevel<ID, V, H>::getName(lock.getName(), level).c_str()) {
	while (!list.isEmpty()) {
		lru_.append(list.removeFirst());
	}
	switch (level) {
	case 0: fillRatio_ = &SparrowStatus::get().blockCacheLvl0FillRatio_; break;
	case 1: fillRatio_ = &SparrowStatus::get().blockCacheLvl1FillRatio_; break;
	case 2: fillRatio_ = &SparrowStatus::get().blockCacheLvl2FillRatio_; break;
	case 3: fillRatio_ = &SparrowStatus::get().blockCacheLvl3FillRatio_; break;
	default: fillRatio_ = NULL; break;
	}
}

template<class ID, class V, class H> inline CacheEntry<ID, V, H>* CacheLevel<ID, V, H>::acquire(const bool wait /* = true */) {
	while (lru_.isEmpty()) {
		if (wait) {
			// If the LRU is empty, we have to wait for an available entry.
			cond_.wait(true);
		} else {
			return 0;
		}
	}
	Atomic::inc32(fillRatio_);
	return lru_.removeFirst();
}

template<class ID, class V, class H> inline void CacheLevel<ID, V, H>::remove(CacheEntry<ID, V, H>* entry) {
	lru_.remove(entry);
	Atomic::inc32(fillRatio_);
}

template<class ID, class V, class H> inline void CacheLevel<ID, V, H>::release(CacheEntry<ID, V, H>* entry) {
	lru_.append(entry);
	Atomic::dec32(fillRatio_);
	if (lru_.entries() == 1) {
		// The LRU was empty, unblock waiter if any.
		cond_.signal(true);
	}
}

template<class ID, class V, class H> inline void CacheLevel<ID, V, H>::getExtraEntries(SYSidlist<CacheEntry<ID, V, H> >& list) {
	uint32_t	prev_entries = lru_.entries();
	while (lru_.entries() > capacity_) {
		list.append(lru_.removeFirst());
	}
	Atomic::add32(fillRatio_, prev_entries-lru_.entries());
}

// Takes entries from the given list and adds then to this level's entries, up to this level max capacity.
template<class ID, class V, class H> inline void CacheLevel<ID, V, H>::balance(SYSidlist<CacheEntry<ID, V, H> >& list) {
	int		added = 0;
	while (!list.isEmpty() && lru_.entries() < capacity_) {
		CacheEntry<ID, V, H>* entry = list.removeFirst();
		entry->setLevel(level_);
		lru_.prepend(entry);
		++added;
		if (lru_.entries() == 1) {
			// The LRU was empty, unblock waiter if any.
			cond_.signal(true);
		}
	}
	Atomic::add32(fillRatio_, -added);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Cache
//////////////////////////////////////////////////////////////////////////////////////////////////////

// ID = key type (ex: file ID + offset)
// V = value type (ex: block of data from that file).
// N = number of levels.

template<class ID, class V, uint32_t N, class H> class Cache : public Lock {
private:

	// Cache hash.
	SYSpHash<CacheEntry<ID, V, H>, SYShPoolAllocator<CacheEntry<ID, V, H>*> > hash_;

	// Cache levels.
	CacheLevel<ID, V, H>* levels_[N];

	// Entries being initialized.
	SYSpHash<CacheEntry<ID, V, H>, SYShPoolAllocator<CacheEntry<ID, V, H>*> > initializing_;

	// Condition for initializing entries.
	Cond initCond_;

	// Cache statistics.
	CacheStat stat_;
	LvlCacheStat stat_per_lvl_[N];

private:

	static uint32_t getTotalEntries(uint32_t* entries) {
		uint32_t totalEntries = 0;
		for (uint32_t level = 0; level < N; ++level) {
			totalEntries += entries[level];
		}
		return totalEntries;
	}

	void balance();

	bool isInitializingNoLock(const ID& id);
	bool isInitializingNoLock(const ID* ids, uint32_t n); 
	bool needsInitializingNoLock(const ID& id);
	CacheEntry<ID, V, H>* acquireNoLock(bool& initialize, const uint32_t level, const ID& id,
		const bool create, const bool updateStats);
	void acquireMultipleNoLock(const uint32_t level, const ID* ids, const uint32_t n, SYSpVector<CacheEntry<ID, V, H>, 64>& entries);
	void releaseNoLock(CacheEntry<ID, V, H>* entry, const uint32_t newLevel, const bool clear, const bool updateStats);

public:

	Cache<ID, V, N, H>(const char* name, uint32_t* entries, CacheEntry<ID, V, H>** cacheEntries,
		volatile uint64_t& acquires, volatile uint64_t& releases, volatile uint64_t& misses, volatile uint64_t& hits, volatile uint64_t& slowHits);

	// Note there is no destructor; a cache is allocated upon startup and never destroyed.

	CacheEntry<ID, V, H>* acquire(const uint32_t level, const ID& id, const H& hint, const bool create, const bool updateStats) _THROW_(SparrowException);
	void acquireMultiple(const uint32_t level, const ID* ids, const uint32_t n, SYSpVector<CacheEntry<ID, V, H>, 64>& entries);
	void release(CacheEntry<ID, V, H>* entry, const uint32_t newLevel, const bool clear, const bool updateStats);
	CacheEntry<ID, V, H>* releaseAndAcquire(CacheEntry<ID, V, H>* entry, const uint32_t level, const ID& id, const H& hint) _THROW_(SparrowException);
	void initializeEntry(CacheEntry<ID, V, H>* entry, SYSpVector<CacheEntry<ID, V, H>, 64>& entries, const H& hint) _THROW_(SparrowException);
	void init_lvl_stat(const LvlCacheStat* stat_per_lvl);

	void putBack(SYSpVector<CacheEntry<ID, V, H>, 64>& entries);

	void clear();
};

// Creates a new cache for a given number of entries (one number per level).
// If cacheEntries is null, this constructor takes care of filling the level(s) and the hash pool
// with default entries so there is no memory allocation in the future.
// If cacheEntries is not null, the caller will have to provide one list of cache entries for
// each level.
template<class ID, class V, uint32_t N, class H> inline Cache<ID, V, N, H>::Cache(const char* name, uint32_t* entries,
	CacheEntry<ID, V, H>** cacheEntries,
	volatile uint64_t& acquires, volatile uint64_t& releases, volatile uint64_t& misses, volatile uint64_t& hits, volatile uint64_t& slowHits)
	: Lock(false, name), hash_(getTotalEntries(entries)), initializing_(128),
	initCond_(false, *this, (Str(name) + Str("::initCond_")).c_str()), stat_(acquires, releases, misses, hits, slowHits) {
	for (uint32_t level = 0; level < N; ++level) {

		SYSidlist<CacheEntry<ID, V, H> > list;
		if (cacheEntries == 0) {
			for (uint32_t i = 0; i < entries[level]; ++i) {
				CacheEntry<ID, V, H>* entry = new CacheEntry<ID, V, H>(level, ID(), V());
				hash_.insert(entry);						// Default entry.
				list.append(entry);
			}
		} else {
			CacheEntry<ID, V, H>* entry = cacheEntries[level];
			while (entry != 0) {
				CacheEntry<ID, V, H>* next = entry->next_;
				hash_.insert(entry);						// Entry built by the caller.
				list.append(entry);
				entry = next;
			}
		}
		levels_[level] = new CacheLevel<ID, V, H>(level, *this, list);
	}

	// The pool is filled, as well as the levels: clear the hash.
	hash_.clear();
}

template<class ID, class V, uint32_t N, class H> inline void Cache<ID, V, N, H>::init_lvl_stat(const LvlCacheStat* stat_per_lvl) {
	for (uint32_t level = 0; level < N; ++level) {
		stat_per_lvl_[level] = stat_per_lvl[level];
	}
}

template<class ID, class V, uint32_t N, class H> inline void Cache<ID, V, N, H>::initializeEntry(CacheEntry<ID, V, H>* entry,
	SYSpVector<CacheEntry<ID, V, H>, 64>& entries, const H& hint) _THROW_(SparrowException) {
	try {
		entry->clear();
		entry->initialize(entries, hint);
		putBack( entries );
		entry->setValid(true);
		Guard guard(*this);
		initializing_.remove(entry);
		hash_.insert(entry);
		initCond_.signalAll(true);
	} catch(const SparrowException&) {
		// Failure; insert it anyway to unblock waiters.
		entry->clear();
		entry->setValid(false);
		putBack( entries );
		{
			Guard guard(*this);
			initializing_.remove(entry);
			//hash_.insert(entry);		// This is weird: storing an invalid entry in the cache. If the reference is 0, there'll be no way to remove it.
			initCond_.signalAll(true);
		}

		// Release entry before throwing exception.
		release(entry, entry->getLevel(), false, true);
		throw;
	}
}


template<class ID, class V, uint32_t N, class H> inline bool Cache<ID, V, N, H>::isInitializingNoLock( const ID& id )
{
	const CacheEntry<ID, V, H> key(id);
	CacheEntry<ID, V, H>* entry = 0;
	entry = initializing_.find(&key);
	return ( entry != 0 );
}

template<class ID, class V, uint32_t N, class H> inline bool Cache<ID, V, N, H>::isInitializingNoLock( const ID* ids, uint32_t n ) 
{
	for (uint32_t i = 0; i < n; ++i) {
		const ID& id = ids[i];
		if ( isInitializingNoLock( id ) ) {
			return true;
		}
	}
	return false;
}

template<class ID, class V, uint32_t N, class H> inline bool Cache<ID, V, N, H>::needsInitializingNoLock( const ID& id )
{
	const CacheEntry<ID, V, H> key(id);
	CacheEntry<ID, V, H>* entry = 0;
	entry = hash_.find(&key);
	return ( entry == 0 );
}


template<class ID, class V, uint32_t N, class H> inline CacheEntry<ID, V, H>* Cache<ID, V, N, H>::acquireNoLock(bool& initialize, const uint32_t level,
	const ID& id, const bool create, const bool updateStats) {
	CacheEntry<ID, V, H>* entry = 0;
	{
		const CacheEntry<ID, V, H> key(id);
		if (updateStats) {
			stat_.acquires_++;
		}
		entry = hash_.find(&key);
		if (entry == 0) {
			assert( initializing_.find(&key) == 0 );

			// Return 0 if the caller does not want the entry to be created.
			if (!create) {
				return 0;
			}

			// No entry found; initialize one.
			entry = levels_[level]->acquire();

			// Make sure the entry is not accessible.
			hash_.remove(entry);
			entry->setId(id);
			entry->acquire();

			// The entry will be initialized below, outside the lock.
			initializing_.insert(entry);
			initialize = true;
		} else {
			if (!entry->isReferenced()) {
				// If the entry is not referenced, it is in its level.
				// Remove it from its level so it cannot be reused.
				levels_[entry->getLevel()]->remove(entry);
			}
			entry->acquire();
		}
	}
	return entry;
}

// Acquire multiple entries for the given array of keys.
template<class ID, class V, uint32_t N, class H> inline void Cache<ID, V, N, H>::acquireMultipleNoLock(const uint32_t level,
	const ID* ids, const uint32_t n, SYSpVector<CacheEntry<ID, V, H>, 64>& entries)
{
	if (n == 0) {
		return;
	}
	
	for (uint32_t i = 0; i < n; ++i)
	{
		const ID& id = ids[i];
		const CacheEntry<ID, V, H> key(id);
		CacheEntry<ID, V, H>* entry = 0;
		entry = hash_.find(&key);

		if (entry == 0) {
			assert( isInitializingNoLock( id ) == 0 );

			// No entry found; take one from the level and initialize it.
			entry = levels_[level]->acquire();

			// Make sure the entry is not accessible.
			hash_.remove(entry);
			entry->setId(id);
			entry->clear();
			entry->setValid(false);
			entry->acquire();

			// The entry will be initialized by the caller, outside the lock.
			initializing_.insert(entry);
		} else {
			if (!entry->isReferenced()) {
				// If the entry is not referenced, it is in its level.
				// Remove it from its level so it cannot be reused.
				levels_[entry->getLevel()]->remove(entry);
			}
			entry->acquire();
		}
		entry->prev_ = NULL;
		entry->next_ = NULL;
		entries.append(entry);
	}
}


// Gets or initializes a cache entry for the given key. An exception is thrown
// if the cache entry initialization fails (I/O error, etc).
// In this case, the entry is in the cache, but marked as not valid.
// So the caller should always check entry->isValid() before using the entry.
template<class ID, class V, uint32_t N, class H> inline CacheEntry<ID, V, H>* Cache<ID, V, N, H>::acquire(const uint32_t level,
	const ID& id, const H& hint, const bool create, const bool updateStats) _THROW_(SparrowException) {
	bool initialize = false;
	CacheEntry<ID, V, H>* entry = 0;
	bool hasWaited = false;

	// Use hint to expand list of ids to acquire
	ID*		ids = NULL;		// contains additional ids deduced from hint
	uint32_t	n = 0;
	ID::expandId( id, hint, ids, n );

	SYSpVector<CacheEntry<ID, V, H>, 64> additional_entries;

	bool	acquired = false;

	do
	{
		Guard guard(*this);

		// If block is being initialized, wait.
		const CacheEntry<ID, V, H> key(id);
		entry = initializing_.find(&key);
		if ( entry == 0 )
		{
			if ( n == 0 ) {
				entry = acquireNoLock( initialize, level, id, create, updateStats );
				acquired = true;
			} else {
				// If the value needs to be initialized, we need to reserve the optional additional blocks that will be used during initialization
				// If it's not possible to reserve all blocks, now, don't reserve anything.
				if ( needsInitializingNoLock( id ) ) {
					if ( !isInitializingNoLock( ids, n ) ) { 
						entry = acquireNoLock( initialize, level, id, create, updateStats );
						assert( initialize == true );
						acquireMultipleNoLock( level, ids, n, additional_entries );
						assert( !create || additional_entries.entries() == n );
						acquired = true;
					} else {
						// Possible deadlock, so don't do anything (wait).
					}
				} else {
					entry = acquireNoLock( initialize, level, id, create, updateStats );
					assert( initialize == false );
					acquired = true;
				}
			}
		}

		if ( !acquired ) { 
			//spw_print_error("acquire put to wait: Id is initializing.");
			initCond_.wait(true);
			hasWaited = true;
		}
	} while ( !acquired );

	if (updateStats) {
		if (initialize) {
			stat_.misses_++;
			if ( stat_per_lvl_[level].misses_ != NULL ) {
				(*stat_per_lvl_[level].misses_)++;
			}
		}		
		if (hasWaited) {
			stat_.slowHits_++;
			if ( stat_per_lvl_[level].slowHits_ != NULL ) {
				(*stat_per_lvl_[level].slowHits_)++;
			}
		} else {
			stat_.hits_++;
			if ( stat_per_lvl_[level].hits_ != NULL ) {
				(*stat_per_lvl_[level].hits_)++;
			}
		}
	}

	if ( initialize ) {
		initializeEntry( entry, additional_entries, hint );
	}
	
	return entry;
}


// Acquire multiple entries for the given array of keys.
template<class ID, class V, uint32_t N, class H> inline void Cache<ID, V, N, H>::acquireMultiple(const uint32_t level,
	const ID* ids,
	const uint32_t n,
	SYSpVector<CacheEntry<ID, V, H>, 64>& entries) {
		if (n == 0) {
			return;
		}
		Guard guard(*this);
		for (uint32_t i = 0; i < n; ++i) {
			const ID& id = ids[i];
			const CacheEntry<ID, V, H> key(id);
			CacheEntry<ID, V, H>* entry = 0;
			for ( ; ; ) {
				entry = hash_.find(&key);
				if (entry == 0) {
					// Entry not in the cache. Maybe it is being initialized?
					entry = initializing_.find(&key);
					if (entry == 0) {
						// Entry does not exist.
						break;
					} else {
						for ( ; ; ) {
							// Wait until entry is initialized and retry.
							initCond_.wait(1000, true);
							if (initializing_.find(&key) == 0) {
								break;
							}
						}
					}
				} else {
					break;
				}
			}
			if (entry == 0) {
				// No entry found; take one from the level and initialize it.
				entry = levels_[level]->acquire();

				// Make sure the entry is not accessible.
				hash_.remove(entry);
				entry->setId(id);
				entry->clear();
				entry->setValid(false);
				entry->acquire();

				// The entry will be initialized by the caller, outside the lock.
				initializing_.insert(entry);
			} else {
				if (!entry->isReferenced()) {
					// If the entry is not referenced, it is in its level.
					// Remove it from its level so it cannot be reused.
					levels_[entry->getLevel()]->remove(entry);
				}
				entry->acquire();
			}
			entry->prev_ = NULL;
			entry->next_ = NULL;
			entries.append(entry);
		}
}


// Adjust levels to their initial capacity.
template<class ID, class V, uint32_t N, class H> inline void Cache<ID, V, N, H>::balance() {
	if (N > 1) {
		SYSidlist<CacheEntry<ID, V, H> > list;
		for (uint32_t level = 0; level < N; ++level) {
			levels_[level]->getExtraEntries(list);
		}
		if (!list.isEmpty()) {
			for (uint32_t level = 0; level < N; ++level) {
				levels_[level]->balance(list);
			}
			assert(list.isEmpty());
		}
	}
}

template<class ID, class V, uint32_t N, class H> inline void Cache<ID, V, N, H>::releaseNoLock(CacheEntry<ID, V, H>* entry,
	const uint32_t newLevel, const bool clear, const bool updateStats) {
	if (updateStats) {
		stat_.releases_++;
	}
	if (entry->release()) {
		if (clear) {
			entry->clear();
			hash_.remove(entry);
		}
		const uint32_t level = entry->getLevel();
		if (level == newLevel) {
			levels_[level]->release(entry);
		} else {
			entry->setLevel(newLevel);
			levels_[newLevel]->release(entry);
		}
		balance();
	}
}

// Releases a cache entry. If it is no longer referenced, put it in the level.
// If the clear flag is true, in addition to put the entry back into the level, it is
// cleared and removed from the hash table.
// The cache entry is moved to another level by specifying a newLevel different
// from the current entry's level.
template<class ID, class V, uint32_t N, class H> inline void Cache<ID, V, N, H>::release(CacheEntry<ID, V, H>* entry,
	const uint32_t newLevel, const bool clear, const bool updateStats) {
	Guard guard(*this);
	releaseNoLock(entry, newLevel, clear, updateStats);
}

template<class ID, class V, uint32_t N, class H> inline CacheEntry<ID, V, H>* Cache<ID, V, N, H>::releaseAndAcquire(CacheEntry<ID, V, H>* entry,
	const uint32_t level, const ID& id, const H& hint) _THROW_(SparrowException) {
	{
		Guard guard(*this);
		releaseNoLock(entry, level, false, true);
	}
	return acquire(level, id, hint, true, true);
}

// Put back the given entries.
template<class ID, class V, uint32_t N, class H> inline void Cache<ID, V, N, H>::putBack(SYSpVector<CacheEntry<ID, V, H>, 64>& entries) {
	if (entries.isEmpty()) {
		return;
	}
	Guard guard(*this);

	for ( uint i=0; i<entries.entries(); ++i ) {
		CacheEntry<ID, V, H>*	entry = entries[i];
		assert(entry->prev_ == NULL && entry->next_ == NULL );
		const uint32_t level = entry->getLevel();
		if (initializing_.remove(entry) != 0) {
			initCond_.signalAll(true);
			hash_.insert(entry);
		}
		if (entry->release()) {
			levels_[level]->release(entry);
		}
	}
	entries.clear();
	balance();
}

// Clear all entries in cache.
template<class ID, class V, uint32_t N, class H> inline void Cache<ID, V, N, H>::clear() {
	Guard guard(*this);
	for (uint32_t level = 0; level < N; ++level) {
		CacheEntry<ID, V, H>* entry;
		while ((entry = levels_[level]->acquire(false)) != 0) {
			entry->clear();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CacheGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<class ID, class V, uint32_t N, class H> class CacheGuard {
private:

	Cache<ID, V, N, H>& cache_;
	CacheEntry<ID, V, H>* entry_;
	const bool clear_;

public:

	CacheGuard<ID, V, N, H>(Cache<ID, V, N, H>& cache, const uint32_t level, const ID& id, const H& hint, const bool clear) _THROW_(SparrowException)
		: cache_(cache), entry_(cache.acquire(level, id, hint, true, true)), clear_(clear) {
	}

	~CacheGuard<ID, V, N, H>() {
		cache_.release(entry_, entry_->getLevel(), clear_, true);
	}

	CacheEntry<ID, V, H>* get() {
		return entry_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileId
//////////////////////////////////////////////////////////////////////////////////////////////////////

enum FileType {
	FILE_TYPE_MISC = 0,
	FILE_TYPE_DATA = 1,
	FILE_TYPE_INDEX = 2,
	FILE_TYPE_STRING = 3
};

class FileId {
protected:

	char name_[FN_REFLEN];

	// File type.
	FileType type_;

	// File mode.
	// Note: caching handles for files being written/appended does not make sense, except that we do not
	// want to exceed the maximum number of opened files, so we rely on the file cache to achieve this.
	FileMode mode_;

public:

	FileId(const FileType type = FILE_TYPE_MISC, const FileMode mode = FILE_MODE_READ) : type_(type), mode_(mode) {
		name_[0] = 0;
	}

	FileId(const char* name, const FileType type, const FileMode mode) : type_(type), mode_(mode) {
		strcpy(name_, name);
	}

	bool operator == (const FileId& right) const {
		if (this != &right) {
			return mode_ == right.mode_ && strcmp(name_, right.name_) == 0;
		} else {
			return true;
		}
	}

	const char* getName() const {
		return name_;
	}

	char* getName() {
		return name_;
	}

	FileType getType() const {
		return type_;
	}

	FileMode getMode() const {
		return mode_;
	}

	static void expandId(const FileId& id, const int& hint, FileId*& ids, uint32_t& n) _THROW_(SparrowException) {
	}


	uint32_t hash() const {
		uint32_t result = 31 + static_cast<uint32_t>(mode_);
		int off = 0;
		for ( ; ; ) {
			const uint8_t v = static_cast<uint8_t>(name_[off++]);
			if (v == 0) {
				break;
			}
			result = 31 * result + v;
		}
		return result;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileHandle
//////////////////////////////////////////////////////////////////////////////////////////////////////

class FileHandle {
private:

	const char* name_;
	File file_;
	PSI_file_key key_;
#ifndef _WIN32
	Lock* lock_;
#endif

public:

	FileHandle() : file_(-1)
#ifndef _WIN32
		, lock_(0)
#endif
	{
	}

	FileHandle& operator = (const FileHandle& right) {
		file_ = right.file_;

		// No need to copy other attributes.
		return *this;
	}

	FileHandle(const FileHandle& right) {
#ifndef _WIN32
		lock_ = 0;
#endif
		*this = right;
	}

	~FileHandle() {
#ifndef _WIN32
		delete lock_;
#endif
	}

	void initialize(const FileId& id, SYSpVector<CacheEntry<FileId, FileHandle, int>, 64>& entries, const int hint) _THROW_(SparrowException);

	void clear();

	uint32_t read(const uint64_t offset, uint8_t* data, const uint32_t size) const _THROW_(SparrowException);

	uint32_t readMultiple(const uint64_t offset, uint8_t** data, const uint32_t size) const _THROW_(SparrowException);

	uint32_t write(const uint64_t offset, uint8_t* data, const uint32_t size) const _THROW_(SparrowException);

	uint64_t getSize() const _THROW_(SparrowException);

	const char* getName() const {
		return name_;
	}

	File getFile() const {
		return file_;
	}

	PSI_file_key getKey() const {
		return key_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileCache
//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef CacheEntry<FileId, FileHandle, int> FileCacheEntry;
typedef CacheGuard<FileId, FileHandle, 1, int> FileCacheGuard;

class FileCache : public Cache<FileId, FileHandle, 1, int> {
private:

	static FileCache* cache_;
	static PSI_file_info psiInfo_[];

public:

	static PSI_file_key dataKey_;
	static PSI_file_key indexKey_;
	static PSI_file_key stringKey_;
	static PSI_file_key miscKey_;

public:

	FileCache(uint32_t entries);
	static void initialize() _THROW_(SparrowException);
	static FileCache& get() {
		return *cache_;
	}

	static void releaseFile(const FileId& id, const bool remove);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PartitionFile
//////////////////////////////////////////////////////////////////////////////////////////////////////

class PersistentPartition;
class PartitionFile {
protected:

	uint32_t id_;			// Id of master file.
	uint32_t fileId_;		// DATA_FILE for data file, or STRING_FILE for string file, or id in array of indexes.
	uint64_t serial_;		// Partition serial number.

public:

	PartitionFile() : id_(0), fileId_(0), serial_(0) {
	}

	PartitionFile(const PersistentPartition& partition, const uint32_t index);

	PartitionFile(const PartitionFile& right) : id_(right.id_), fileId_(right.fileId_), serial_(right.serial_) {		
	}

	bool operator == (const PartitionFile& right) const {
		if (this != &right) {
			return id_ == right.id_ && fileId_ == right.fileId_ && serial_ == right.serial_;
		} else {
			return true;
		}
	}

	bool operator != (const PartitionFile& right) const {
		return !(*this == right);
	}

	uint64_t getSerial() const {
		return serial_;
	}

	uint32_t getFileId() const {
		return fileId_;
	}

	const char* getFileName(char* name) const _THROW_(SparrowException);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ReadCacheHint
//////////////////////////////////////////////////////////////////////////////////////////////////////

class ReadCacheHint {
public:

	virtual const BlockCacheHint& getBlockHint() const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// WriteCacheHint
//////////////////////////////////////////////////////////////////////////////////////////////////////

class WriteCacheHint {
public:

	virtual const PartitionFile& getPartitionFile() const = 0;

	virtual uint32_t getLevel() const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SimpleWriteCacheHint
//////////////////////////////////////////////////////////////////////////////////////////////////////

class SimpleWriteCacheHint : public WriteCacheHint {
private:

	const PartitionFile& file_;
	const uint32_t level_;

public:

	SimpleWriteCacheHint(const PartitionFile& file, const uint32_t level) : file_(file), level_(level) {
	}

	const PartitionFile& getPartitionFile() const override {
		return file_;
	}

	uint32_t getLevel() const override {
		return level_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileOffset
//////////////////////////////////////////////////////////////////////////////////////////////////////

class FileOffset : public PartitionFile {
protected:

	uint64_t offset_;

public:

	FileOffset() : PartitionFile(), offset_(0) {
	}

	FileOffset(const PartitionFile& partitionFile) : PartitionFile(partitionFile), offset_(0) {
	}

	FileOffset(const PersistentPartition& partition, const uint32_t fileId, const uint64_t offset) : PartitionFile(partition, fileId), offset_(offset) {
	}

	FileOffset(const FileOffset& right, const uint64_t offset) : PartitionFile(right), offset_(offset) {		
	}

	bool operator == (const FileOffset& right) const {
		if (this != &right) {
			return offset_ == right.offset_ && static_cast<const PartitionFile&>(*this) == right;
		} else {
			return true;
		}
	}

	uint64_t getOffset() const {
		return offset_;
	}

	void setOffset(const uint64_t offset) {
		offset_ = offset;
	}

	static void expandId(const FileOffset& id, const BlockCacheHint& hint, FileOffset*& ids, uint32_t& n) _THROW_(SparrowException);

	uint32_t hash() const {
		uint32_t result = 31 + id_;
		result = 31 * result + fileId_;
		result = 31 * result + static_cast<uint32_t>(serial_ ^ (serial_ >> 32));
		result = 31 * result + static_cast<uint32_t>(offset_ ^ (offset_ >> 32));
		return result;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileBlock
//////////////////////////////////////////////////////////////////////////////////////////////////////

class FileBlock {
private:

	uint8_t* data_;	// Allocated once and for all, points to a block with size == sparrow_cache_block_size
	uint32_t length_;	// Actual length of read block, <= sparrow_cache_block_size.

public:

	FileBlock() : data_(0), length_(0) {
	}

	FileBlock(uint8_t* data, const uint32_t length = 0) : data_(data), length_(length) {
	}

	uint8_t* getData() const {
		return data_;
	}

	uint32_t getLength() const {
		return length_;
	}

	void initialize(const FileOffset& id, SYSpVector<CacheEntry<FileOffset, FileBlock, BlockCacheHint>, 64>& entries, const BlockCacheHint& hint) _THROW_(SparrowException);
	void replace(const FileBlock& value);
	void clear() {
		length_ = 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BlockCache
//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef CacheEntry<FileOffset, FileBlock, BlockCacheHint> BlockCacheEntry;
typedef SYSpVector<BlockCacheEntry, 64> BlockCacheEntries;
typedef SYSidlistIterator<BlockCacheEntry> BlockCacheIterator;

class BlockCache : public Cache<FileOffset, FileBlock, 4, BlockCacheHint> {
private:

	static BlockCache* cache_;
	static uint32_t chunkSize_;

public:

	BlockCache(uint32_t* entries, BlockCacheEntry** cacheEntries) _THROW_(SparrowException);
	static void initialize() _THROW_(SparrowException);
	static BlockCache& get() {
		return *cache_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BlockCacheEntriesGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

class BlockCacheEntriesGuard {
private:

	BlockCacheEntries entries_;

public:

	BlockCacheEntriesGuard() {
	}
	~BlockCacheEntriesGuard() {
		BlockCache::get().putBack(entries_);
	}
	BlockCacheEntries& get() {
		return entries_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileStatGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

class FileStatGuard {
private:

	const FileHandle& handle_;
	const PSI_file_operation operation_;
	PSI_file_locker_state state_;
	struct PSI_file_locker* locker_{nullptr};
	size_t bytes_;

public:

	FileStatGuard(const FileHandle& handle, const char* file, const uint32_t line, const PSI_file_operation operation, const size_t count)
		: handle_(handle), operation_(operation), bytes_(0) {
#ifdef HAVE_PSI_FILE_INTERFACE
		if (operation_ == PSI_FILE_OPEN || operation_ == PSI_FILE_CREATE) {
			const char* name = handle.getName();
			const size_t l = strlen(name);
			name += l > PFS_MAX_INFO_NAME_LENGTH ? (l - PFS_MAX_INFO_NAME_LENGTH) : 0;
			locker_ = PSI_FILE_CALL(get_thread_file_name_locker)(&state_, handle_.getKey(), operation_, name, &locker_);
		} else {
			locker_ = PSI_FILE_CALL(get_thread_file_descriptor_locker)(&state_, handle_.getFile(), operation_);
		}
		if (locker_ != nullptr) {
			if (operation_ == PSI_FILE_OPEN || operation_ == PSI_FILE_CREATE) {
				PSI_FILE_CALL(start_file_open_wait)(locker_, file, line);
			} else {
				PSI_FILE_CALL(start_file_wait)(locker_, count, file, line);
			}
		}
#endif
	}

	void setBytes(const size_t bytes) {
		bytes_ = bytes;
	}

	~FileStatGuard() {
#ifdef HAVE_PSI_FILE_INTERFACE
		if (locker_ != nullptr) {
			if (operation_ == PSI_FILE_OPEN || operation_ == PSI_FILE_CREATE) {
				PSI_FILE_CALL(end_file_open_wait_and_bind_to_descriptor)(locker_, handle_.getFile());
			} else {
				PSI_FILE_CALL(end_file_wait)(locker_, bytes_);
			}
		}
#endif
	}
};

#define IO_STAT_CREATE(H) FileStatGuard __ioguard(*H, __FILE__, __LINE__, PSI_FILE_CREATE, 0)
#define IO_STAT_OPEN(H) FileStatGuard __ioguard(*H, __FILE__, __LINE__, PSI_FILE_OPEN, 0)
#define IO_STAT_CLOSE(H) FileStatGuard __ioguard(*H, __FILE__, __LINE__, PSI_FILE_CLOSE, 0)
#define IO_STAT_OTHER(H, O, B) FileStatGuard __ioguard(*H, __FILE__, __LINE__, O, static_cast<size_t>(B))
#define IO_STAT_BYTES(B) __ioguard.setBytes(B)

}

#endif /* #ifndef _engine_cache_h_ */

