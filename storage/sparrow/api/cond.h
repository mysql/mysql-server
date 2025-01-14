/*
	Condition variable.
*/

#ifndef _spw_api_cond_h_
#define _spw_api_cond_h_

#include "lock.h"
#include "mysql/psi/mysql_cond.h"
#include "my_systime.h"

namespace Sparrow {

class Cond {
private:

	const char *m_name;
	native_cond_t	cond_;
	Lock* lock_;
	uint32_t volatile nbWaiters_;
	uint8_t owned_:1;
	uint8_t static_:1;

private:

	static SYSslist<Cond*>& getStatics() {
		static SYSslist<Cond*> statics;
		return statics;
	}

	void initialize() {
		native_cond_init(&cond_);
	}

	void clear() {
		if (m_name != 0) {
			native_cond_destroy(&cond_);
			free(const_cast<char*>(m_name));
			if (owned_) {
				delete lock_;
			}
			m_name = 0;
		}
	}

public:

	Cond(const bool isStatic, const char* name) : lock_(new Lock(isStatic, name)), nbWaiters_(0), owned_(true), static_(isStatic) {
		m_name = my_strdup(name, MYF(MY_FAE));
		if (static_) {
			Cond::getStatics().append(this);
		} else {
			initialize();
		}
	}

	Cond(const bool isStatic, Lock& lock, const char* name) : lock_(&lock), nbWaiters_(0), owned_(false), static_(isStatic) {
		m_name = my_strdup(name, MYF(MY_FAE));
		if (static_) {
			Cond::getStatics().append(this);
		} else {
			initialize();
		}
	}

	Cond& operator = (const Cond&) = delete;
	Cond(const Cond&) = delete;

	static void initializeStatics() {
		SYSslistIterator<Cond*> iterator(Cond::getStatics());
		while (++iterator) {
			iterator.key()->initialize();
		}
	}

	static void deinitializeStatics() {
		SYSslistIterator<Cond*> iterator(Cond::getStatics());
		while (++iterator) {
			iterator.key()->clear();
		}
	}

	~Cond() {
		clear();
	}

	Lock& getLock() {
		return *lock_;
	}

	void acquire() {
		lock_->lock();
	}

	void release() {
		lock_->unlock();
	}

	void signal(const bool acquired = false) {
		if (!acquired) {
			acquire();
		}
		if (nbWaiters_ > 0) {
			native_cond_signal(&cond_);
		}
		if (!acquired) {
			release();
		}
	}

	void signalAll(const bool acquired = false) {
		if (!acquired) {
			acquire();
		}
		if (nbWaiters_ > 0) {
			native_cond_broadcast(&cond_);
		}
		if (!acquired) {
			release();
		}
	}

	bool wait(const uint64_t milliseconds, const bool acquired = false) {
		if (!acquired) {
			acquire();
		}
		nbWaiters_++;
		int status;
		if (milliseconds == 0) {
			status = native_cond_wait(&cond_, lock_->get());	// Infinite wait.
		} else {
			struct timespec t;
			const uint64_t nanoseconds = milliseconds * 1000000;
			set_timespec_nsec(&t, nanoseconds);
			status = native_cond_timedwait(&cond_, lock_->get(), &t);
		}
		nbWaiters_--;
		if (!acquired) {
			release();
		}
		return (status == 0);
	}

	bool wait(const bool acquired = false) {
		return wait(0, acquired);
	}
};

}

#endif /* #ifndef _spw_api_cond_h_ */
