/*
	Lock types.
*/

#ifndef _spw_api_lock_h_
#define _spw_api_lock_h_

#include "my_sys.h"
#include "include/global.h"
#include "list.h"
#include "include/thr_mutex.h"
#include "include/thr_rwlock.h"

namespace Sparrow {

// Simple lock.
class Lock {
private:

	const char *m_name{nullptr};
	native_mutex_t	lock_;
	bool static_;

private:

	static SYSslist<Lock*>& getStatics() {
		static SYSslist<Lock*> statics;
		return statics;
	}

private:

	void initialize() {
		native_mutex_init(&lock_, nullptr);
	}

	void clear() {
		if (m_name != nullptr) {
			native_mutex_destroy(&lock_);
			my_free(const_cast<char*>(m_name));
			m_name = nullptr;
		}
	}

public:

	Lock(const bool isStatic, const char* name) : static_(isStatic) {
		m_name = my_strdup(name, MYF(MY_FAE));
		if (static_) {
			Lock::getStatics().append(this);
		} else {
			initialize();
		}
	}
	Lock& operator = (const Lock&) = delete;
	Lock(const Lock&) = delete;

	static void initializeStatics() {
		SYSslistIterator<Lock*> iterator(Lock::getStatics());
		while (++iterator) {
			iterator.key()->initialize();
		}
	}

	static void deinitializeStatics() {
		SYSslistIterator<Lock*> iterator(Lock::getStatics());
		while (++iterator) {
			iterator.key()->clear();
		}
	}

	~Lock() {
		clear();
	}

	void lock() {
		native_mutex_lock(&lock_);
	}

	bool tryLock() {
		if (native_mutex_trylock(&lock_) == 0) {
			return true;
		} else {
			return false;
		}
	}

	void unlock() {
	    native_mutex_unlock(&lock_);
	}

	const char* getName() const {
		return m_name;
	}

	native_mutex_t* get() {
		return &lock_;
	}
};


// Single writer, multiple readers lock.
class RWLock {
private:

	const char *m_name{nullptr};
	native_rw_lock_t	lock_;
	bool static_;

private:

	static SYSslist<RWLock*>& getStatics() {
		static SYSslist<RWLock*> statics;
		return statics;
	}

	void initialize() {
		native_rw_init(&lock_);
	}

	void clear() {
		if (m_name != nullptr) {
			native_rw_destroy(&lock_);
			my_free(const_cast<char*>(m_name));
			m_name = nullptr;
		}
	}

public:

	RWLock(const bool isStatic, const char* name) : static_(isStatic) {
		m_name = my_strdup(name, MYF(MY_FAE));
		if (static_) {
			RWLock::getStatics().append(this);
		} else {
			initialize();
		}
	}

	RWLock& operator = (const RWLock&) = delete;
	RWLock(const RWLock&) = delete;

	static void initializeStatics() {
		SYSslistIterator<RWLock*> iterator(RWLock::getStatics());
		while (++iterator) {
			iterator.key()->initialize();
		}
	}

	static void deinitializeStatics() {
		SYSslistIterator<RWLock*> iterator(RWLock::getStatics());
		while (++iterator) {
			iterator.key()->clear();
		}
	}

	~RWLock() {
		clear();
	}

	void readLock() {
		native_rw_rdlock(&lock_);
	}

	void writeLock() {
		native_rw_wrlock(&lock_);
	}

	bool tryReadLock() {
		if (native_rw_tryrdlock(&lock_) == 0) {
			return true;
		} else {
			return false;
		}
	}

	bool tryWriteLock() {
		if (native_rw_trywrlock(&lock_) == 0) {
			return true;
		} else {
			return false;
		}
	}

	void unlock() {
		native_rw_unlock(&lock_);
	}
};




// Simple lock guard.
class Guard {
private:

	Lock* lock_;
	bool acquired_;

public:

	Guard(Lock& lock, const bool doTry = false) : lock_(&lock) {
		if (doTry) {
			acquired_ = lock_->tryLock();
		} else {
			lock_->lock();
			acquired_ = true;
		}
	}

	Guard() : lock_(0), acquired_(false) {
	}

	~Guard() {
		if (acquired_ && lock_ != 0) {
			lock_->unlock();
		}
	}

	bool isAcquired() const {
		return acquired_;
	}

private:

	Guard& operator = (const Guard&);
	Guard(const Guard&);
};


// Read lock guard.
class ReadGuard {
private:

	RWLock* lock_;
	bool acquired_;

public:

	ReadGuard(RWLock& lock, const bool doTry = false) : lock_(&lock) {
		if (doTry) {
			acquired_ = lock_->tryReadLock();
		} else {
			lock_->readLock();
			acquired_ = true;
		}
	}

	ReadGuard() : lock_(0), acquired_(false) {
	}

	~ReadGuard() {
		if (acquired_ && lock_ != 0) {
			lock_->unlock();
		}
	}

	bool isAcquired() const {
		return acquired_;
	}

private:

	ReadGuard& operator = (const ReadGuard&);
	ReadGuard(const ReadGuard&);
};

// Write lock guard.
class WriteGuard {
private:

	RWLock* lock_;
	bool acquired_;

public:

	WriteGuard(RWLock& lock, const bool doTry = false) : lock_(&lock) {
		if (doTry) {
			acquired_ = lock_->tryWriteLock();
		} else {
			lock_->writeLock();
			acquired_ = true;
		}
	}

	WriteGuard() : lock_(0), acquired_(false) {
	}

	void release() {
		if (acquired_ && lock_ != 0) {
			lock_->unlock();
			acquired_ = false;
		}
	}

	~WriteGuard() {
		release();
	}

	bool isAcquired() const {
		return acquired_;
	}

private:

	WriteGuard& operator = (const WriteGuard&);
	WriteGuard(const WriteGuard&);
};


}

#endif /* #ifndef _spw_api_lock_h_ */
