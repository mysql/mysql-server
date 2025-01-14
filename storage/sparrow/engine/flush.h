/*
	Partition flush.
*/

#ifndef _engine_flush_h_
#define _engine_flush_h_

#include "transient.h"
#include "persistent.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FlushTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class FlushTask : public TransientTask {
private:

	void process(TransientPartition& partition, const uint64_t timestamp)  override _THROW_(SparrowException) {
		partition.flush(timestamp);
	}

public:

	FlushTask(Master* master, const uint64_t serial) : TransientTask(master, serial, Flush::getQueue()) {
		Atomic::inc32(&SparrowStatus::get().tasksPendingFlushTasks_);
	}

	~FlushTask() {
		Atomic::dec32(&SparrowStatus::get().tasksPendingFlushTasks_);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FlushAllTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class FlushAllTask : public Task {
private:
	static Lock lock_;
	static FlushAllTask* task_;

public:

	FlushAllTask() : Task(Flush::getQueue()) {
		Atomic::inc32(&SparrowStatus::get().tasksPendingFlushAllTasks_);
	}

	~FlushAllTask() {
		Atomic::dec32(&SparrowStatus::get().tasksPendingFlushAllTasks_);
	}

	virtual bool operator == (const FlushAllTask& right) const {
		return true;
	}

	virtual bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override {
		return 0;
	}

	void run(const uint64_t timestamp) override _THROW_(SparrowException);

	static bool flushAll(const uint64_t timestamp = 0) {
		Guard guard(lock_);
		if (task_ != 0) {
			return false;
		}
		task_ = new FlushAllTask();
		Scheduler::addTask(task_, timestamp, true);
		return task_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TransientJob
//////////////////////////////////////////////////////////////////////////////////////////////////////

class TransientJob : public Job, public MasterDependency {
protected:

	TransientPartitionGuard partition_;
	PersistentPartitionGuard mainPartition_;

public:

	TransientJob(TransientPartition* partition, PersistentPartitionGuard mainPartition)
		: MasterDependency(partition->getMaster()), partition_(partition), mainPartition_(mainPartition) {
	}

	virtual ~TransientJob() {
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FlushJob
//////////////////////////////////////////////////////////////////////////////////////////////////////

class FlushJob : public TransientJob {
private:

	Jobs workerJobs_;
	Jobs writerJobs_;

public:

	FlushJob(TransientPartition* partition, PersistentPartitionGuard mainPartition, const uint32_t workerJobs, const uint32_t writerJobs)
		: TransientJob(partition, mainPartition), workerJobs_(workerJobs), writerJobs_(writerJobs) {
			Atomic::inc32(&SparrowStatus::get().tasksPendingFlushJobs_);
	}

	virtual ~FlushJob() {
		Atomic::dec32(&SparrowStatus::get().tasksPendingFlushJobs_);
	}

	void process() override;

	void stop() override {
	}

	Jobs& getWorkerJobs() {
		return workerJobs_;
	}

	Jobs& getWriterJobs() {
		return writerJobs_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// StringJob
//////////////////////////////////////////////////////////////////////////////////////////////////////

class StringJob : public TransientJob {
public:

	StringJob(TransientPartition* partition, PersistentPartitionGuard mainPartition)
		: TransientJob(partition, mainPartition) {
			Atomic::inc32(&SparrowStatus::get().tasksPendingStringJobs_);
	}

	virtual ~StringJob() {
		Atomic::dec32(&SparrowStatus::get().tasksPendingStringJobs_);
	}

	void process() override;

	void stop() override {
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// IndexJob
//////////////////////////////////////////////////////////////////////////////////////////////////////

class IndexJob : public TransientJob {
private:

	const uint32_t id_;

public:

	IndexJob(TransientPartition* partition, PersistentPartitionGuard mainPartition, const uint32_t id)
		: TransientJob(partition, mainPartition), id_(id) {
			Atomic::inc32(&SparrowStatus::get().tasksPendingIndexJobs_);
	}

	virtual ~IndexJob() {
		Atomic::dec32(&SparrowStatus::get().tasksPendingIndexJobs_);
	}

	void process() override;

	void stop() override {
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// WriteJob
//////////////////////////////////////////////////////////////////////////////////////////////////////

class WriteJob : public TransientJob {
private:

	const uint32_t id_;
	Indirector* indirector_;

public:

	WriteJob(TransientPartition* partition, PersistentPartitionGuard mainPartition, const uint32_t id, Indirector* indirector)
		: TransientJob(partition, mainPartition), id_(id), indirector_(indirector) {
			Atomic::inc32(&SparrowStatus::get().tasksPendingWriteJobs_);
	}

	virtual ~WriteJob() {
		Atomic::dec32(&SparrowStatus::get().tasksPendingWriteJobs_);
		delete indirector_;
	}

	void process() override;

	void stop() override {
	}
};

}

#endif /* #ifndef _engine_flush_h_ */
