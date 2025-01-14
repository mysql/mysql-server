#ifndef _spw_api_thread_h_
#define _spw_api_thread_h_

#include "cond.h"
#include "mysql/psi/mysql_thread.h"
//#include "include/my_pthread.h"

namespace Sparrow {
    
//////////////////////////////////////////////////////////////////////////////////////////////////////
// Thread
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Thread {
public:

	// Lock used by all start/stop condition variables.
	static Lock lock_;

private:

	char*	m_name_{nullptr};
	my_thread_handle thread_;
	volatile bool running_;
	volatile bool stop_;		// To signal a stop command to the thread
	Cond startCond_;
	Cond stopCond_;

protected:
	my_thread_t threadId_;

public:

	// TODO: understand why the version with (Str() + Str()).c_str() does not compile
	//Thread(const char* name) : running_(false), stop_(false), startCond_(false, lock_, (Str(name) + Str("::startCond_")).c_str()),
	//	stopCond_(false, lock_, (Str(name) + Str("::stopCond_")).c_str()) {
	Thread(const char* name) : running_(false), stop_(false), startCond_(false, lock_, "::startCond_"),
		stopCond_(false, lock_, "::stopCond_"), threadId_(0) {
		if (name != nullptr) {
			m_name_ = my_strdup(name, MYF(MY_FAE));
		}
	}

	virtual ~Thread() {
		if (m_name_ != nullptr) {
			my_free(const_cast<char*>(m_name_));
		}
	}

	bool start() {
		my_thread_attr_t attr;
		my_thread_attr_init(&attr);
		my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_DETACHED);
		my_thread_attr_setstacksize(&attr, 262144);
		Guard guard(lock_);
		if (my_thread_create(&thread_, &attr, reinterpret_cast<void*(*)(void*)>(handler), static_cast<void*>(this)) != 0 ) {
			return false;
		}
		startCond_.wait(true);
		return true;
	}

	void stop() {
		if ( running_ ) {
			Guard guard(lock_);
			stop_ = true;
			notifyStop();
			stopCond_.wait(2000, true);
		} 
	}

	void join() {
		my_thread_join(&thread_, nullptr);
	}

	bool isRunning() const { return running_; }

protected:

	void stopping() { running_ = false; }

	virtual bool process() = 0;

	virtual void notifyStop() = 0;

	virtual bool deleteAfterExit() = 0;

private:

	static void* handler(void *p) {
		Thread* thread = (Thread*)p;
		thread->running_ = true;
		thread->threadId_ = my_thread_self();
		thread->startCond_.signal();
		while (!thread->stop_) {
			if (!thread->process()) {
				break;
			}
		}
		thread->running_ = false;

		PRINT_DBUG("Thread stopped!");
		thread->stopCond_.signal();
		/*if (thread->stop_) {
			thread->stopCond_.signal();
		} else {*/
			if (thread->deleteAfterExit()) {
				delete thread;
			}
		//}
		return 0;
	}
};

}

#endif /* #ifndef _spw_api_thread_h_ */
