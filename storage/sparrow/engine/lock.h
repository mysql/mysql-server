/*
	Lock types.
*/

#ifndef _engine_lock_h_
#define _engine_lock_h_

#include <my_base.h>
//#include <my_pthread.h>
#include "my_sys.h"
#include "my_systime.h"
#include "mysql/service_mysql_alloc.h"  // my_free
//#include "thr_mutex.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/mysql_rwlock.h"

#include "list.h"

namespace Sparrow {

#define PFS_MAX_INFO_NAME_LENGTH 128
#define PFS_MAX_OS_NAME_LENGTH (16 - 3)


// Simple lock.
class Lock {
private:

	PSI_mutex_key key_;
	PSI_mutex_info info_;
	mysql_mutex_t lock_;
	bool static_;
	bool initialized_;
	//static pthread_mutex_t registerLock_;
	//static bool registerLockInitialized_;

private:

	static SYSslist<Lock*>& getStatics() {
		static SYSslist<Lock*> statics;
		return statics;
	}

	static native_mutex_t* initializeRegisterLock() {
		native_mutex_t* registerLock = new native_mutex_t();
		native_mutex_init(registerLock, MY_MUTEX_INIT_FAST);
		return registerLock;
	}

	static native_mutex_t* getRegisterLock() {
        static native_mutex_t* registerLock = Lock::initializeRegisterLock();
		return registerLock;
	}

public:

	// To make sure registrations into the performance schema are serialized.
	static void lockPSI() {
		native_mutex_lock(Lock::getRegisterLock());
	}

	static void unlockPSI() {
		native_mutex_unlock(Lock::getRegisterLock());
	}

private:

	void initialize() {
#ifdef HAVE_PSI_INTERFACE
		//if (PSI_server != 0) {
			Lock::lockPSI();
			//PSI_server->register_mutex("sparrow", &info_, 1);
            mysql_mutex_register("sparrow", &info_, 1);
			Lock::unlockPSI();
		//}
#endif
		mysql_mutex_init(key_, &lock_, MY_MUTEX_INIT_FAST);
		initialized_ = true;
	}

	void clear() {
		if (info_.m_name != 0) {
			if (initialized_) {
				mysql_mutex_destroy(&lock_);
			}
			my_free(const_cast<char*>(info_.m_name));
			info_.m_name = 0;
		}
	}

public:

	Lock(const bool isStatic, const char* name) : static_(isStatic), initialized_(false) {
		info_.m_key = &key_;
		//const size_t l = strlen(name);
		//name += l > PFS_MAX_INFO_NAME_LENGTH ? (l - PFS_MAX_INFO_NAME_LENGTH) : 0;
        info_.m_name = my_strdup(PSI_INSTRUMENT_ME, name, MYF(MY_WME));
		info_.m_flags = 0;
        info_.m_volatility = PSI_VOLATILITY_UNKNOWN;
        info_.m_documentation = PSI_DOCUMENT_ME;
		if (static_) {
			Lock::getStatics().append(this);
		} else {
			initialize();
		}
	}

    Lock &operator=(const Lock &) = delete;
    Lock(const Lock &) = delete;

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
		mysql_mutex_lock(&lock_);
	}

	bool tryLock() {
		if (mysql_mutex_trylock(&lock_) == 0) {
			return true;
		} else {
			return false;
		}
	}

	void unlock() {
	    mysql_mutex_unlock(&lock_);
	}

	const char* getName() const {
		return info_.m_name;
	}

	mysql_mutex_t* get() {
		return &lock_;
	}
};

// Single writer, multiple readers lock.
class RWLock {
private:

	PSI_rwlock_key key_;
	PSI_rwlock_info info_;
	mysql_rwlock_t lock_;
	bool static_;

private:

	static SYSslist<RWLock*>& getStatics() {
		static SYSslist<RWLock*> statics;
		return statics;
	}

	void initialize() {
#ifdef HAVE_PSI_INTERFACE
		//if (PSI_server != 0) {
			Lock::lockPSI();
            mysql_rwlock_register("sparrow", &info_, 1);
			Lock::unlockPSI();
		//}
#endif
		mysql_rwlock_init(key_, &lock_);
	}

	void clear() {
		if (info_.m_name != 0) {
			mysql_rwlock_destroy(&lock_);
			my_free(const_cast<char*>(info_.m_name));
			info_.m_name = 0;
		}
	}

public:

	RWLock(const bool isStatic, const char* name) : static_(isStatic) {
		info_.m_key = &key_;
		//const size_t l = strlen(name);
		//name += l > PFS_MAX_INFO_NAME_LENGTH ? (l - PFS_MAX_INFO_NAME_LENGTH) : 0;
        info_.m_name = my_strdup(PSI_INSTRUMENT_ME, name, MYF(MY_WME));
        info_.m_flags = 0;
        info_.m_volatility = PSI_VOLATILITY_UNKNOWN;
        info_.m_documentation = PSI_DOCUMENT_ME;
        if (static_) {
			RWLock::getStatics().append(this);
		} else {
			initialize();
		}
	}

    RWLock &operator=(const RWLock &) = delete;
    RWLock(const RWLock &) = delete;


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
		mysql_rwlock_rdlock(&lock_);
	}

	void writeLock() {
		mysql_rwlock_wrlock(&lock_);
	}

	bool tryReadLock() {
		if (mysql_rwlock_tryrdlock(&lock_) == 0) {
			return true;
		} else {
			return false;
		}
	}

	bool tryWriteLock() {
		if (mysql_rwlock_trywrlock(&lock_) == 0) {
			return true;
		} else {
			return false;
		}
	}

	void unlock() {
		mysql_rwlock_unlock(&lock_);
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

	ReadGuard(RWLock& lock, const bool doTry = false, const bool acquire=true) : lock_(&lock), acquired_(false) {
		if (acquire) {
			if (lock_ != 0) {
				if (doTry) {
					acquired_ = lock_->tryReadLock();
				} else {
					lock_->readLock();
					acquired_ = true;
				}
			}
		}
	}

	ReadGuard() : lock_(0), acquired_(false) {
	}

	void reset() {
		acquired_ = false;
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

	WriteGuard(RWLock& lock, const bool doTry=false, const bool favorRead=false, const bool acquire=true) : lock_(&lock), acquired_(false) {
		if (acquire) {
			if (doTry) {
				acquired_ = lock_->tryWriteLock();
			} else {
				if (favorRead) {
					while ( !lock_->tryWriteLock() ) {
						my_sleep( 1000 );
					}
					acquired_ = true;
				} else {
					lock_->writeLock();
					acquired_ = true;
				}
			}
		}
	}

	WriteGuard() : lock_(0), acquired_(false) {
	}

	void acquire() {
		if (!acquired_ && lock_ != 0) {
			lock_->writeLock();
			acquired_ = true;
		}
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

#endif /* #ifndef _engine_lock_h_ */
