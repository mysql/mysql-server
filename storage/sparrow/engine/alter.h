/*
	Online table modifications.
*/

#ifndef _engine_alter_h_
#define _engine_alter_h_

#include "transient.h"
#include "persistent.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// AlterWorker
//////////////////////////////////////////////////////////////////////////////////////////////////////

class AlterWorker {
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
// MainAlterTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MainAlterTask : public MasterTask {
private:

	PersistentPartitionGuard partition_;

public:

	MainAlterTask(PersistentPartition* partition)
		: MasterTask(AlterWorker::getQueue(), &partition->getMaster()), partition_(partition) {
	}

	virtual bool operator == (const MainAlterTask& right) const {
		return this == &right;
	}

	virtual bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override {
		return 0;
	}

	void run(const uint64_t timestamp) override _THROW_(SparrowException);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// AlterTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class AlterTask : public MasterTask {
private:

	uint32_t* counter_;
	AlterationStats* stats_;
	const uint32_t newIndexAlterSerial_;
	PersistentPartitionGuard partition_;
	const Alteration alteration_;

public:

	AlterTask(uint32_t* counter, AlterationStats* stats, const uint32_t newIndexAlterSerial, PersistentPartition* partition, const Alteration& alteration)
		: MasterTask(AlterWorker::getQueue(), &partition->getMaster()), counter_(counter), stats_(stats), newIndexAlterSerial_(newIndexAlterSerial),
		partition_(partition), alteration_(alteration) {
	}

	virtual bool operator == (const AlterTask& right) const {
		return this == &right;
	}

	virtual bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override {
		return 0;
	}

	void run(const uint64_t timestamp) override _THROW_(SparrowException);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// AlterComparator
//////////////////////////////////////////////////////////////////////////////////////////////////////

class AlterComparator : private DataFileReader {
private:
	
	const Task* task_;						// Interruptible alter task.
	const uint64_t recordSize_;				// Size of data records.
	PartitionReader& reader1_;				// First reader on data file.
	PartitionReader& stringReader1_;		// First reader on string file.
	PartitionReader& reader2_;				// Second reader on data file.
	PartitionReader& stringReader2_;		// Second reader on string file.
	mutable uint32_t comparisons_;			// To check if task is interrupted.

public:

	AlterComparator(const Task* task, const TableFields& fields, const ColumnIds& columnIds, const ColumnIds& skippedColumns,
		PartitionReader& reader1, PartitionReader& stringReader1, PartitionReader& reader2, PartitionReader& stringReader2)
		: DataFileReader(fields, columnIds, skippedColumns), task_(task), recordSize_(reader1.getHeader().getRecordSize()),
		reader1_(reader1), stringReader1_(stringReader1), reader2_(reader2), stringReader2_(stringReader2), comparisons_(0) {
	}

	int compare(const uint32_t row1, const uint32_t row2, const bool sortByRow) const;

	int compare(const uint32_t row1, const uint32_t row2) const {
		return compare(row1, row2, true);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// AlterWriter
//////////////////////////////////////////////////////////////////////////////////////////////////////

class AlterWriter : private DataFileReader {
private:

	const PersistentPartition& partition_;
	const uint64_t recordSize_;		// Size of data (input) records.
	PartitionReader& reader_;		// Reader on data file.
	PartitionReader& stringReader_;	// Reader on strings.
	uint32_t size_;					// Size of written records.
	uint32_t bits_;					// Number of bits in written records.
	BinBuffer binBuffer_;
	BitArray bitArray_;

public:

	AlterWriter(const PersistentPartition& partition, const TableFields& fields, const ColumnIds& columnIds, const ColumnIds& skippedColumns,
		PartitionReader& reader, PartitionReader& stringReader)
		: DataFileReader(fields, columnIds, skippedColumns), partition_(partition), recordSize_(reader.getHeader().getRecordSize()),
		reader_(reader), stringReader_(stringReader) {
		bits_ = 0;
		size_ = 0;
		for (uint32_t i = 0; i < infos_.length(); ++i) {
			bits_ += infos_[i].getNBits();
			size_ += infos_[i].getSize();
		}
		const uint32_t bitSize = (bits_ + 7) / 8;
		bitArray_ = BitArray(bitSize);
		size_ += bitSize;
	}

	uint32_t getSize() const {
		return size_;
	}

	const BinBuffer& getBinBuffer() const {
		return binBuffer_;
	}

	void writeRecord(ByteBuffer& buffer, const uint32_t row) _THROW_(SparrowException);
};

}

#endif /* #ifndef _engine_alter_h_ */
