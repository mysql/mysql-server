/*
	Scheduler thread.
*/

#ifndef _engine_scheduler_h_
#define _engine_scheduler_h_

#include "exception.h"
#include "thread.h"
#include "hash.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Task
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Task : public SYSidlink<Task> {
private:

	Queue<Job>& queue_;
	volatile bool stopping_;

public:

	Task(Queue<Job>& queue) : queue_(queue), stopping_(false) {
	}

	virtual ~Task() {
	}

	// Task period, in milliseconds. If zero, the task is not scheduled again after its first run.
	virtual uint64_t getPeriod() const = 0;

	// Task execution method.
	virtual void run(const uint64_t timestamp) _THROW_(SparrowException) = 0;

	// To stop a task.
	void stop();

	bool isStopping() const {
		return stopping_ || queue_.isStopping();
	}

	virtual bool operator == (const Task& right) const = 0;

	Queue<Job>& getQueue() {
		return queue_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TaskJob
//////////////////////////////////////////////////////////////////////////////////////////////////////

class TaskJob : public Job {
private:

	uint64_t timestamp_;
	Task* task_;

public:

	TaskJob(const uint64_t timestamp, Task* task) : timestamp_(timestamp), task_(task) {
	}

	void process() override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Tasks
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Tasks {
private:

	uint64_t timestamp_;		// Timestamp in milliseconds.
	SYSidlist<Task> tasks_;	// List of tasks for this timestamp.

private:

	Tasks& operator = (const Tasks& right);

public:

	Tasks() : timestamp_(0) {
	}

	Tasks(const uint64_t timestamp) : timestamp_(timestamp) {
	}

	uint32_t hash() const {
		return 31 + static_cast<uint32_t>(timestamp_ ^ (timestamp_ >> 32));
	}

	uint64_t getTimestamp() const {
		return timestamp_;
	}

	void add(Task* task, const bool first) {
		if ( first ) {
			tasks_.prepend(task);
		} else {
			tasks_.append(task);
		}
	}

	bool remove(Task* task) {
		if (tasks_.contains(task)) {
			tasks_.remove(task);
			return true;
		} else {
			return false;
		}
	}

	Task* removeFirst() {
		return tasks_.removeFirst();
	}

	bool isEmpty() const {
		return tasks_.isEmpty();
	}

	bool operator == (const Tasks& right) const {
		return timestamp_ == right.timestamp_;
	}

	bool contains(const Task& task) {
		return tasks_.contains( task );
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Scheduler
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Scheduler : public Thread {
	friend class TaskJob;

private:

	static Scheduler* scheduler_;

	SYSpHash<Tasks> tasks_;
	Cond cond_;
	const uint64_t start_;

private:

	static bool removeTaskNoLock(Task* task);

	static void addTaskNoLock(Task* task, const uint64_t timestamp, const bool remove, const bool first=false);

protected:

	bool process() override;

	bool notifyStop() override;

	bool deleteAfterExit() override {
		return false;
	}

public:

	Scheduler() : Thread("Scheduler::scheduler_"), tasks_(16), cond_(false, "Scheduler::cond_"), start_(now()) {
	}

	~Scheduler() {
	}

	static void addTask(Task* task, const uint64_t timestamp, const bool remove = false);

	static void addTask(Task* task);

	static void stopTask(Task* task);

	static bool moveTask(Task* task, const uint64_t timestamp);

	static void initialize() _THROW_(SparrowException);

	static void shutdown() {
		if ( scheduler_ != NULL )
			scheduler_->stop();
	}

	// Current timestamp, in milliseconds.
	static uint64_t now() {
#if defined(_WIN32)
		FILETIME ft;
		GetSystemTimeAsFileTime(&ft);
		const uint64_t t = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) + ft.dwLowDateTime;
		return (t - 116444736000000000ULL) / 10000;
#else
		struct timeval tv;
		gettimeofday(&tv, 0);
		return (static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec) / 1000;
#endif
	}

	// Server uptime, in milliseconds.
	static uint64_t uptime() {
		if (scheduler_ == 0) {
			return 0;
		} else {
			const uint64_t s = scheduler_->start_;
			const uint64_t t = now();
			return t > s ? t - s : 0;
		}
	}
};

}

#endif /* #ifndef _engine_scheduler_h_ */
