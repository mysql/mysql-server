/*
	Partition coalescing.
*/

#ifndef _engine_coalescing_h_
#define _engine_coalescing_h_

#include "persistent.h"
#include "binbuffer.h"
#include "search.h"
#include "sort.h"
#include "sema.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescablePartitions
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CoalescablePartitions {
private:

	CoalescingInfo info_;
	PersistentPartitions partitions_;

public:

	CoalescablePartitions(const CoalescingInfo& info) : info_(info) {
	}

	const CoalescingInfo& getInfo() const {
		return info_;
	}

	PersistentPartitions& getPartitions() {
		return partitions_;
	}

	const PersistentPartitions& getPartitions() const {
		return partitions_;
	}

	bool operator == (const CoalescablePartitions& right) const {
		return info_ == right.info_;
	}

	uint32_t hash() const {
		uint32_t result = 31 + info_.getFirst().getFirst();
		result = 31 * result + info_.getFirst().getSecond();
		const uint64_t serial = info_.getSecond();
		return 31 + static_cast<uint32_t>(serial ^ (serial >> 32));
		return result;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingWorker
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CoalescingWorker {
private:

	static const JobThreadFactory factory_;
	static JobThreadPool* threadPool_;
	static Lock lock_;

public:

	static void initialize() _THROW_(SparrowException);
	static void shutdown();
	static void sendJob(Job* job);
	static Queue<Job>& getQueue() {
		return *threadPool_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingFlags
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CoalescingFlags {
public:
	CoalescingFlags(const char* name) : lock_(false, name), counter_(0), aborted_(false) {;}

	void acquire() {
		Guard	guard(lock_);
		counter_++;
	}

	bool release() {
		Guard	guard(lock_);
		assert(counter_ > 0);
		return (--counter_ == 0);
	}

	void setAborted() {
		aborted_ = true;
	}

	bool isAborted() const { return aborted_; }

public:
	Lock	lock_;
	uint32_t	counter_;
	bool	aborted_;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingMainTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CoalescingMainTask : public MasterTask {
private:

	PersistentPartitions partitions_;
	IndexIds indexIds_;

public:

	CoalescingMainTask(Master* master, const PersistentPartitions& partitions, const IndexIds& indexIds)
		: MasterTask(CoalescingWorker::getQueue(), master), partitions_(partitions), indexIds_(indexIds) {
			get()->registerCoalescingTask(this);
			Atomic::inc32(&SparrowStatus::get().tasksPendingCoalescingMainTasks_);
	}

	~CoalescingMainTask() {
		WriteGuard guard(get()->getLock());
		get()->unregisterCoalescingTask(this);
		Atomic::dec32(&SparrowStatus::get().tasksPendingCoalescingMainTasks_);
	}

	virtual bool operator == (const CoalescingMainTask& right) const {
		return this == &right;
	}

	virtual bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override {
		return 0;
	}

	void run(const uint64_t timestamp)  override _THROW_(SparrowException);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingIndexTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CoalescingIndexTask : public MasterTask {
private:

	PersistentPartitions allPartitions_;
	PersistentPartitions partitions_;
	PersistentPartitionGuard coalescedPartition_;
	const uint32_t index_;
	CoalescingFlags*	flags_;
	bool	run_;

	void finished() {
		if (flags_->release()) {
			if (flags_->isAborted()) {
				get()->coalescingFailed(allPartitions_);
			} else {
				get()->coalescingDone(coalescedPartition_, allPartitions_);
			}
			delete flags_; 
			flags_ = NULL;
		}
	}

public:

	CoalescingIndexTask(Master* master, const PersistentPartitions& allPartitions,
		const PersistentPartitions& partitions, PersistentPartition* coalescedPartition, const uint32_t index, CoalescingFlags* flags)
		: MasterTask(CoalescingWorker::getQueue(), master), allPartitions_(allPartitions), 
		partitions_(partitions), coalescedPartition_(coalescedPartition), index_(index), flags_(flags), run_(false) {
			flags_->acquire();
			get()->registerCoalescingTask(this);		// Assumes Master.lock_ is already acquired
			Atomic::inc32(&SparrowStatus::get().tasksPendingCoalescingIndexTasks_);
	}

	~CoalescingIndexTask() {
		if (!run_) {
			assert(isStopping() == true);
			flags_->setAborted();
			finished();
		}
		WriteGuard guard(get()->getLock());
		get()->unregisterCoalescingTask(this);
		Atomic::dec32(&SparrowStatus::get().tasksPendingCoalescingIndexTasks_);
	}

	virtual bool operator == (const CoalescingIndexTask& right) const {
		return this == &right;
	}

	virtual bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override {
		return 0;
	}

	void run(const uint64_t timestamp)  override _THROW_(SparrowException);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingReaders
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CoalescingReaders : private PartitionReadersBase {
private:

	const PersistentPartitions& partitions_;
	const uint32_t index_;
	const BlockCacheHint& hint_;

private:

	CoalescingReaders(const CoalescingReaders& right);
	CoalescingReaders& operator = (const CoalescingReaders& right);

public:

	CoalescingReaders(const PersistentPartitions& partitions, const uint32_t index)
		: PartitionReadersBase(partitions.length() * 3), partitions_(partitions), index_(index), hint_(BlockCacheHint::largeAround0_) {
		for (uint32_t i = 0; i < PartitionReadersBase::capacity(); ++i) {
			PartitionReadersBase::append(0);
		}
	}

	~CoalescingReaders() {
		clearAndDestroy();
	}

	PartitionReader* get(const uint32_t partition, const uint32_t forString) _THROW_(SparrowException) {
		assert(forString <= 2);
		const uint32_t i = partition * 3 + forString;
		PartitionReader* reader = PartitionReadersBase::operator[](i);
		if (reader == 0) {
			reader = partitions_[partition]->createReader(index_, forString != 0, hint_);
			PartitionReadersBase::operator[](i) = reader;
		}
		return reader;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingKeyComparator
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CoalescingKeyComparator {
private:

	CoalescingReaders& readers_;
	const RecordWrapper& recordWrapper_;
	const uint32_t size_;
	const uint8_t* data_;
	const KeyIndirector& indirector_;
	BinBuffer* binBuffer_;
	const uint8_t* reference_;

public:

	CoalescingKeyComparator(CoalescingReaders& readers, const RecordWrapper& recordWrapper, const KeyValues& keyValues,
		const KeyIndirector& indirector, BinBuffer* binBuffer)
		: readers_(readers), recordWrapper_(recordWrapper), size_(recordWrapper.getSize() - 4), // TODO adjust row size
		data_(keyValues.data()), indirector_(indirector), binBuffer_(binBuffer), reference_(keyValues.data() + indirector[0] * size_) {
	}

	int compareTo(const uint32_t row) _THROW_(SparrowException) {
		const uint32_t row1 = indirector_[row + 1];
		ByteBuffer buffer1(data_ + row1 * size_, size_);
		ByteBuffer buffer2(reference_, size_);
		PartitionReaderGuard stringReaderGuard1(readers_.get(row1, 1));
		PartitionReaderGuard stringReaderGuard2(readers_.get(indirector_[0], 2));
		return recordWrapper_.compare(buffer1, stringReaderGuard1.get(), buffer2, stringReaderGuard2.get(), binBuffer_);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingComparator
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CoalescingComparator {
private:

	CoalescingReaders& readers_;
	const RecordWrapper& recordWrapper_;
	BinBuffer* binBuffer_;
	const uint32_t size_;
	const uint8_t* data_;

public:

	CoalescingComparator(CoalescingReaders& readers, const RecordWrapper& recordWrapper, const KeyValues& keyValues, BinBuffer* binBuffer)
		: readers_(readers), recordWrapper_(recordWrapper), binBuffer_(binBuffer), size_(recordWrapper.getSize() - 4), // TODO adjust row size
		data_(keyValues.data()) {
	}

	int compare(const uint32_t row1, const uint32_t row2) const {
		ByteBuffer buffer1(data_ + row1 * size_, size_);
		ByteBuffer buffer2(data_ + row2 * size_, size_);
		PartitionReaderGuard stringReaderGuard1(readers_.get(row1, 1));
		PartitionReaderGuard stringReaderGuard2(readers_.get(row2, 2));
		const int cmp = recordWrapper_.compare(buffer1, stringReaderGuard1.get(), buffer2, stringReaderGuard2.get(), binBuffer_);
		if (cmp == 0) {
			return row1 > row2 ? 1 : (row1 < row2 ? -1 : 0);
		} else {
			return cmp;
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Coalescing
//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef SYSvector<uint32_t> RowOffsets;	// Partition row offsets.

class Coalescing {
public:

	static void initialize() _THROW_(SparrowException);

	static void shutdown();

	static void triggerIndexCoalescing(Master* master, const PersistentPartitions& allPartitions,
		const PersistentPartitions& partitions, PersistentPartition* coalescedPartition, const IndexIds& indexIds) _THROW_(SparrowException);

	static PersistentPartition* generateDataFile(PersistentPartitions& partitions,
		const Task& task) _THROW_(SparrowException);

	static uint64_t generateIndexFile(const PersistentPartitions& partitions, const uint32_t index, PersistentPartition* coalescedPartition,
		const Task* task) _THROW_(SparrowException);
};

}

#endif /* #ifndef _engine_coalescing_h_ */
