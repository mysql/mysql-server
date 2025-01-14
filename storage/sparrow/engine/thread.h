/*
	Thread base classes.
*/

#ifndef _engine_thread_h_
#define _engine_thread_h_

#include "queue.h"
#include "exception.h"
#include "cond.h"
#include "vec.h"
#include "misc.h"
#include "mysql/psi/mysql_thread.h"

//typedef void*(*ThreadFunction)(void*);
//extern "C" int createSparrowThread(PSI_thread_key key, pthread_t* thread, pthread_attr_t* attr, ThreadFunction func, void* arg);
extern "C" int createSparrowThread(PSI_thread_key key, my_thread_handle* thread, const my_thread_attr_t* attr, my_start_routine func, void* arg);

extern uint sparrow_idle_thread_timeout;

class THD;

namespace Sparrow {
    
//////////////////////////////////////////////////////////////////////////////////////////////////////
// Thread
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Thread {
public:

	// Lock used by all start/stop condition variables.
	static Lock lock_;

private:

	PSI_thread_key key_;
	PSI_thread_info info_;
	my_thread_handle thread_;
	volatile bool stop_;
	volatile bool stopped_;
	Cond startCond_;
	Cond stopCond_;

public:

	Thread(const char* name) : stop_(false), stopped_(false), startCond_(false, lock_, (Str(name) + Str("::startCond_")).c_str()),
		stopCond_(false, lock_, (Str(name) + Str("::stopCond_")).c_str()) {
		info_.m_key = &key_;
		const size_t l = strlen(name);
		const char* os_name = name;
		os_name += (l >= PFS_MAX_OS_NAME_LENGTH ? (l - PFS_MAX_OS_NAME_LENGTH + 1) : 0);
		info_.m_os_name = my_strdup(PSI_INSTRUMENT_ME, os_name, MYF(MY_WME));

		name += l > PFS_MAX_INFO_NAME_LENGTH ? (l - PFS_MAX_INFO_NAME_LENGTH) : 0;
		info_.m_name = my_strdup(PSI_INSTRUMENT_ME, name, MYF(MY_WME));
		info_.m_flags = 0;
        info_.m_volatility = PSI_VOLATILITY_UNKNOWN;
        info_.m_documentation = PSI_DOCUMENT_ME;
#ifdef HAVE_PSI_INTERFACE
		//if (PSI_server != 0) {
			Lock::lockPSI();
			mysql_thread_register("sparrow", &info_, 1);
			Lock::unlockPSI();
		//}
#endif
	}

	virtual ~Thread() {
		if (info_.m_name != nullptr) {
			my_free(const_cast<char*>(info_.m_name));
			info_.m_name = nullptr;
		}
	}

	bool start() {
		my_thread_attr_t attr;
		my_thread_attr_init(&attr);
		my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_DETACHED);
		my_thread_attr_setstacksize(&attr, my_thread_stack_size);
		Guard guard(lock_);
		if (createSparrowThread(key_, &thread_, &attr, handler, static_cast<void*>(this)) != 0) {
			return false;
		}
		startCond_.wait(true);
		return true;
	}

	void stop(const uint64_t timeout=0) {
		{
			Guard guard(lock_);
			stop_ = true;
		}
		if ( notifyStop() ) {
			Guard guard(lock_);
			if (!stopped_)
				stopCond_.wait(true);
		}
	}

	void join() {
		my_thread_join(&thread_, nullptr);
	}

	static void initTHD(THD*& thd, void* stackStart);
	static void deleteThreadSpecific(THD*& thd);

protected:

	virtual bool process() = 0;

	virtual bool notifyStop() = 0;

	virtual bool deleteAfterExit() = 0;

private:

	static void* handler(void *p) {
		THD* thd = 0;	// Need to be first for THD thread stack.
		Thread* thread = (Thread*)p;
		initTHD(thd, &thd);
		thread->startCond_.signal();
		while (!thread->stop_) {
			if (!thread->process()) {
				break;
			}
		}
#ifdef HAVE_PSI_INTERFACE
		//if (PSI_server != 0) {
			PSI_THREAD_CALL(delete_current_thread)();
		//}
#endif
		deleteThreadSpecific(thd);
		if (thread->stop_) {
			Guard guard(Thread::lock_);
			thread->stopped_ = true;
			thread->stopCond_.signal(true);
		} else {
			if (thread->deleteAfterExit()) {
				delete thread;
			}
		}
		my_thread_exit(nullptr);
		return nullptr;

	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MessageGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename M> class MessageGuard {
private:

	SYSpSlist<M>& messages_;

public:

	MessageGuard(SYSpSlist<M>& messages) : messages_(messages) {
	}

	~MessageGuard() {
		messages_.clearAndDestroy();
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MessageThread
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename M> class MessageThread {
	friend class Queue<M>;

private:

	PSI_thread_key key_;
	PSI_thread_info info_;
	my_thread_handle thread_;
	Queue<M>* queue_;
	Cond cond_;
	const bool owned_;
	volatile bool stopped_;
	Cond startCond_;
	Cond stopCond_;
	uint* timeout_;

private:

	void initialize(const char* name) {
		info_.m_key = &key_;
		const size_t l = strlen(name);
		const char* os_name = name;
		os_name += (l >= PFS_MAX_OS_NAME_LENGTH ? (l - PFS_MAX_OS_NAME_LENGTH + 1) : 0);
		info_.m_os_name = my_strdup(PSI_INSTRUMENT_ME, os_name, MYF(MY_WME));

		name += l > PFS_MAX_INFO_NAME_LENGTH ? (l - PFS_MAX_INFO_NAME_LENGTH) : 0;
		info_.m_name = my_strdup(PSI_INSTRUMENT_ME, name, MYF(MY_WME));
		info_.m_flags = 0;
        info_.m_volatility = PSI_VOLATILITY_UNKNOWN;
        info_.m_documentation = PSI_DOCUMENT_ME;
#ifdef HAVE_PSI_INTERFACE
		//if (PSI_server != 0) {
			Lock::lockPSI();
			mysql_thread_register("sparrow", &info_, 1);
			Lock::unlockPSI();
		//}
#endif
	}

	Cond& getCond() {
		return cond_;
	}

public:

	MessageThread(const char* name, const bool bulk)
		: queue_(new Queue<M>(name, bulk)), cond_(false, *queue_, (Str(name) + Str("::cond_")).c_str()), owned_(true), stopped_(false), 
		startCond_(false, Thread::lock_, (Str(name) + Str("::startCond_")).c_str()),
		stopCond_(false, Thread::lock_, (Str(name) + Str("::stopCond_")).c_str()), timeout_(0) {
		initialize(name);
	}

	MessageThread(const char* name, Queue<M>& queue, uint* timeout)
		: queue_(&queue), cond_(false, *queue_, (Str(name) + Str("::cond_")).c_str()), owned_(false), stopped_(false),
		startCond_(false, Thread::lock_, (Str(name) + Str("::startCond_")).c_str()),
		stopCond_(false, Thread::lock_, (Str(name) + Str("::stopCond_")).c_str()), timeout_(timeout) {
		initialize(name);
	}

	void send(M* message) {
		queue_->send(message);
	}

	bool start() {
		my_thread_attr_t attr;
		my_thread_attr_init(&attr);
		my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_DETACHED);
		my_thread_attr_setstacksize(&attr, my_thread_stack_size);
		Guard guard(Thread::lock_);
		if (createSparrowThread(key_, &thread_, &attr, handler, static_cast<void*>(this)) != 0) {
			return false;
		}
		startCond_.wait(true);
		return true;
	}

	void stop() {
		Guard guard(Thread::lock_);
		if (owned_) {
			queue_->signal();
		}
		while (!stopped_) {
			stopCond_.wait(true);
		}
	}

	virtual ~MessageThread() {
		my_free(const_cast<char*>(info_.m_name));
		if (owned_) {
			delete queue_;
		}
	}

	bool operator == (const MessageThread<M>& right) const {
		return this == &right;
	}

protected:

	// If message is 0, this thread timed out.
	// Return true to continue, or false to stop thread.
	virtual bool process(SYSpSlist<M>* messages) = 0;

private:

	static void* handler(void *p) {
		THD* thd = 0;	// Need to be first for THD thread stack.
		MessageThread<M>* thread = (MessageThread<M>*)p;
		Thread::initTHD(thd, &thd);
		thread->startCond_.signal();
		SYSpSlist<M> messages;
		bool timedOut = false;
		while (true) {
			MessageGuard<M> guard(messages);
			volatile uint* timeout = thread->timeout_;
			const bool ok = thread->queue_->wait(thread, timeout == 0 ? 0 : *timeout, messages);
			if (thread->queue_->stopped_) {
				break;
			}
			if (ok) {
				if (!thread->process(&messages)) {
					break;
				}
			} else if (*thread->timeout_ > 0) {
				timedOut = true;
				break;
			}
		}
#ifdef HAVE_PSI_INTERFACE
		//if (PSI_server != 0) {
			PSI_THREAD_CALL(delete_current_thread)();
		//}
#endif
		Thread::deleteThreadSpecific(thd);
		if (timedOut) {
			thread->queue_->threadTimedOut(thread);
		} else {
			Guard guard(Thread::lock_);
			thread->stopped_ = true;
			thread->stopCond_.signal(true);
		}
		my_thread_exit(nullptr);
		return nullptr;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MessageThreadFactory
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename M> class MessageThreadFactory {
public:

	virtual ~MessageThreadFactory() {
	}

	virtual MessageThread<M>* createThread(Queue<M>& queue) const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ThreadPool
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename M> class ThreadPool : public Queue<M> {
private:

	const MessageThreadFactory<M>& factory_;
	volatile uint32_t* maxThreads_;		// 0 for unlimited.
	volatile uint32_t* threadCount_;
	SYSpVector<MessageThread<M>, 16> threads_;

protected:

	void needMoreThreads(const uint32_t count) override {
		for (uint32_t i = 0; i < count; ++i) {
			const uint32_t length = threads_.length();
			if (*maxThreads_ != 0 && length >= *maxThreads_) {
				break;
			} else {
				MessageThread<M>* thread = factory_.createThread(*this);
				if (thread->start()) {
					threads_.append(thread);
					(*threadCount_)++;
				} else {
					delete thread;
					break;
				}
			}
		}
	}

	void threadTimedOut(MessageThread<M>* thread) override {
		{
			Guard guard(*this);
			threads_.remove(thread);
			(*threadCount_)--;
		}
		delete thread;
	}

public:

	ThreadPool(const MessageThreadFactory<M>& factory, volatile uint32_t* maxThreads, volatile uint32_t* threadCount, const char* name, const bool bulk)
		: Queue<M>(name, bulk), factory_(factory), maxThreads_(maxThreads), threadCount_(threadCount) {
	}

	void stop() {
		Queue<M>::signal();
		while (true) {
			MessageThread<M>* thread;
			{
				Guard guard(*this);
				if (threads_.isEmpty()) {
					break;
				}
				thread = threads_.first();
				threads_.removeFirst();
			}
			thread->stop();
			delete thread;
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Job
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Job {
public:

	virtual ~Job() {
	}

	virtual void process() = 0;
};

typedef SYSpVector<Job, 16> Jobs;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// JobThread
//////////////////////////////////////////////////////////////////////////////////////////////////////

class JobThread : public MessageThread<Job> {
protected:

	bool process(SYSpSlist<Job>* jobs) override {
		SYSpSlistIterator<Job> iterator(*jobs);
		while (++iterator) {
			try {
				iterator.key()->process();
			} catch(const SparrowException& e) {
				e.toLog();
			}
		}
		return true;
	}

public:

	JobThread(const char* name, Queue<Job>& queue) : MessageThread<Job>(name, queue, &sparrow_idle_thread_timeout) {
	}
	
	virtual ~JobThread() {
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ThreadNameGenerator
//////////////////////////////////////////////////////////////////////////////////////////////////////

class ThreadNameGenerator {
private:

	const char* prefix_;

public:

	ThreadNameGenerator(const char* prefix) : prefix_(prefix) {
	}

	Str getName() const {
		static volatile uint32_t counter = 0;
		char tmp[1024];
		snprintf(tmp, sizeof(tmp), "%s(%u)", prefix_, Atomic::inc32(&counter));
		return Str(tmp);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// JobThreadFactory
//////////////////////////////////////////////////////////////////////////////////////////////////////

class JobThreadFactory : public MessageThreadFactory<Job>, private ThreadNameGenerator {
public:

	JobThreadFactory(const char* prefix) : ThreadNameGenerator(prefix) {
	}

	MessageThread<Job>* createThread(Queue<Job>& queue) const override {
		return new JobThread(getName().c_str(), queue);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Worker thread pool
//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef ThreadPool<Job> JobThreadPool;

class Worker {
private:

	static const JobThreadFactory factory_;
	static JobThreadPool* threadPool_;

public:

	static void initialize() _THROW_(SparrowException);
	static void shutdown();
	static void sendJob(Job* job);
	static Queue<Job>& getQueue() {
		return *threadPool_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Flush
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Flush {
private:

	static const JobThreadFactory factory_;
	static JobThreadPool* threadPool_;
	static volatile uint32_t maxThreads_;

public:

	static void initialize() _THROW_(SparrowException);
	static void shutdown();
	static void sendJob(Job* job);
	static Queue<Job>& getQueue() {
		return *threadPool_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Writer thread pool
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Writer {
private:

	static const JobThreadFactory factory_;
	static JobThreadPool* threadPool_;

public:

	static void initialize() _THROW_(SparrowException);
	static void shutdown();
	static void sendJob(Job* job);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ApiWorker
//////////////////////////////////////////////////////////////////////////////////////////////////////

class ApiWorker {
private:

	static const JobThreadFactory factory_;
	static JobThreadPool* threadPool_;

public:

	static void initialize() _THROW_(SparrowException);
	static void shutdown();
	static void sendJob(Job* job);
};

}

#endif /* #ifndef _engine_thread_h_ */
