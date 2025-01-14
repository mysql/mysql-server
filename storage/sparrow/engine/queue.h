/*
	Message queue.
*/

#ifndef _engine_queue_h_
#define _engine_queue_h_

#include "types.h"
#include "list.h"
#include "cond.h"
#include "vec.h"
#include "misc.h"

#ifdef _WIN32
#pragma warning(disable:4355)
#endif

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Queue
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename M> class MessageThread;
template<typename M> class Queue : public Lock {
	friend class MessageThread<M>;

private:

	SYSpSlist<M> list_;
	const bool bulk_;	// If true, all messages in the queue are returned by wait().
	Cond notFull_;
	volatile bool stopped_;
	const uint32_t capacity_;

	// LIFO stack of idle threads. This enables unecessary idle threads to time out.
	SYSpVector<MessageThread<M>, 16> idleThreads_;

protected:

	virtual void needMoreThreads(const uint32_t count) {
	}

	virtual void threadTimedOut(MessageThread<M>* thread) {
	}

public:

	Queue(const char* name, const bool bulk) : Lock(false, name), bulk_(bulk), notFull_(false, *this, (Str(name) + Str("::notFull_")).c_str()),
		stopped_(false), capacity_(0) {
	}

	Queue(const char* name, const uint32_t capacity, const bool bulk) : Lock(false, name), bulk_(bulk), notFull_(false, *this, (Str(name) + Str("::notFull_")).c_str()),
		stopped_(false), capacity_(capacity) {
	}

	virtual ~Queue() {
	}

	uint32_t getSize() {
		Guard guard(*this);
		return list_.entries();
	}

	void send(M* message) {
		if (stopped_) {
			delete message;
			return;
		}
		Guard guard(*this);
		while (capacity_ != 0 && list_.entries() == capacity_) {
			notFull_.wait(true);
		}
		list_.append(message);
		if (idleThreads_.isEmpty()) {
			needMoreThreads(bulk_ ? 1 : list_.entries());
		} else {
			idleThreads_.last()->getCond().signal(true);
		}
	}

	void send(SYSpSlist<M>& list) {
		if (list.isEmpty()) {
			return;
		}
		if (stopped_) {
			list.clearAndDestroy();
			return;
		}
		Guard guard(*this);
		if (capacity_ == 0) {
			list_.appendAll(list);
			if (bulk_) {
				if (idleThreads_.isEmpty()) {
					needMoreThreads(1);
				}
			} else if (idleThreads_.length() < list_.entries())  {
				needMoreThreads(list_.entries() - idleThreads_.length());
			}
		} else {
			SYSpSlistIterator<M> iterator(list);
			while (++iterator) {
				while (list_.entries() == capacity_) {
					notFull_.wait(true);
				}
				list_.append(iterator.key());
			}
		}
		if (!idleThreads_.isEmpty()) {
			idleThreads_.last()->getCond().signal(true);
		}
	}

	bool wait(MessageThread<M>* thread, const uint64_t milliseconds, SYSpSlist<M>& list) {
		Guard guard(*this);
		if (stopped_) {
			return false;
		}
		bool idle = false;
		while (list_.isEmpty()) {
			if (!idle) {
				idleThreads_.append(thread);
				idle = true;
			}
			if (!thread->getCond().wait(milliseconds, true) || stopped_) {
				idleThreads_.remove(thread);
				return false;
			}
		}
		if (idle) {
			idleThreads_.remove(thread);
		}
		if (bulk_) {
			list_.getAll(list);
		} else {
			list.append(list_.removeAt(0));
		}
		notFull_.signal(true);
		return true;
	}

	void signal() {
		Guard guard(*this);
		stopped_ = true;
		for (uint32_t i = 0; i < idleThreads_.length(); ++i) {
			idleThreads_[i]->getCond().signal(true);
		}
		list_.clearAndDestroy();
	}

	bool isStopping() const {
		return stopped_;
	}
};

}

#endif /* #ifndef _engine_queue_h_ */
