/*
	Condition variable.
*/

#ifndef _engine_cond_h_
#define _engine_cond_h_

#include "lock.h"
#include "mysql/psi/mysql_cond.h"

namespace Sparrow {

class Cond {
private:

	PSI_cond_key key_;
	PSI_cond_info info_;
	mysql_cond_t cond_;
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
#ifdef HAVE_PSI_INTERFACE
		//if (PSI_server != 0) {
			Lock::lockPSI();
			mysql_cond_register("sparrow", &info_, 1);
			Lock::unlockPSI();
		//}
#endif
		mysql_cond_init(key_, &cond_);
	}

	void clear() {
		if (info_.m_name != 0) {
			mysql_cond_destroy(&cond_);
			my_free(const_cast<char*>(info_.m_name));
			if (owned_) {
				delete lock_;
			}
			info_.m_name = 0;
		}
	}

	Cond& operator = (const Cond&);
	Cond(const Cond&);

public:

	Cond(const bool isStatic, const char* name) : lock_(new Lock(isStatic, name)), nbWaiters_(0), owned_(true), static_(isStatic) {
		info_.m_key = &key_;
		//const size_t l = strlen(name);
		//name += l > PFS_MAX_INFO_NAME_LENGTH ? (l - PFS_MAX_INFO_NAME_LENGTH) : 0;
		info_.m_name = my_strdup(PSI_INSTRUMENT_ME, name, MYF(MY_WME));
		info_.m_flags = 0;
        info_.m_volatility = PSI_VOLATILITY_UNKNOWN;
        info_.m_documentation = PSI_DOCUMENT_ME;
		if (static_) {
			Cond::getStatics().append(this);
		} else {
			initialize();
		}
	}

	Cond(const bool isStatic, Lock& lock, const char* name) : lock_(&lock), nbWaiters_(0), owned_(false), static_(isStatic) {
		info_.m_key = &key_;
		//const size_t l = strlen(name);
		//name += l > PFS_MAX_INFO_NAME_LENGTH ? (l - PFS_MAX_INFO_NAME_LENGTH) : 0;
		info_.m_name = my_strdup(PSI_INSTRUMENT_ME, name, MYF(MY_WME));
		info_.m_flags = 0;
        info_.m_volatility = PSI_VOLATILITY_UNKNOWN;
        info_.m_documentation = PSI_DOCUMENT_ME;
		if (static_) {
			Cond::getStatics().append(this);
		} else {
			initialize();
		}
	}

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
			mysql_cond_signal(&cond_);
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
			mysql_cond_broadcast(&cond_);
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
			status = mysql_cond_wait(&cond_, lock_->get());	// Infinite wait.
		} else {
			struct timespec t;
			const uint64_t nanoseconds = milliseconds * 1000000;
			set_timespec_nsec(&t, nanoseconds);
			status = mysql_cond_timedwait(&cond_, lock_->get(), &t);
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

#endif /* #ifndef _engine_cond_h_ */
