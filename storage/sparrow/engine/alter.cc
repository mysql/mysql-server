/*
	Online table modifications.
*/

#include "alter.h"
#include "internalapi.h"
#include "fileutil.h"
#include "purge.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// AlterWorker
//////////////////////////////////////////////////////////////////////////////////////////////////////

const JobThreadFactory AlterWorker::factory_("AlterWorker");
JobThreadPool* AlterWorker::threadPool_ = 0;
Lock AlterWorker::lock_(true, "AlterWorker::lock_");

// STATIC
void AlterWorker::initialize() _THROW_(SparrowException) {
	Guard guard(lock_);
	if (threadPool_ == 0) {
		// The queue is not bulk because we want jobs to be distributed across workers.
		threadPool_ = new JobThreadPool(AlterWorker::factory_, &sparrow_max_alter_threads,
			&SparrowStatus::get().alterThreads_, "AlterWorker::Queue", false);
	}
}

// STATIC
void AlterWorker::shutdown() {
	Guard guard(lock_);
	if (threadPool_ != 0) {
		threadPool_->stop();
	}
}

// STATIC
void AlterWorker::sendJob(Job* job) {
	threadPool_->send(job);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MainAlterTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

void MainAlterTask::run(const uint64_t timestamp) _THROW_(SparrowException) {
	SPARROW_ENTER("MainAlterTask::run");
	partition_->alter(this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// AlterTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

void AlterTask::run(const uint64_t timestamp) _THROW_(SparrowException) {
	SPARROW_ENTER("AlterTask::run");
	try {
		const AlterationType type = alteration_.getType();
		if (type == ALT_ADD_INDEX) {
			*stats_ += partition_->createIndex(alteration_.getId(), this);
		} else if (type == ALT_DROP_INDEX) {
			*stats_ += partition_->dropIndex(alteration_.getId());
		}
	} catch(const SparrowException& e) {
		e.toLog();
	}
	if (Atomic::dec32(counter_) == 0 && !isStopping()) {
		PersistentPartitionGuard next = partition_->alterationDone(*stats_, newIndexAlterSerial_);
		delete counter_;
		delete stats_;
		if (next.get() != 0) {
			Scheduler::addTask(new MainAlterTask(next.get()));
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Alteration
//////////////////////////////////////////////////////////////////////////////////////////////////////

Str Alteration::getDescription(const Master& master) const {
	PrintBuffer buffer;
	switch (type_) {
		case ALT_ADD_INDEX:	// Fall through.
		case ALT_DROP_INDEX: {
			const Indexes& indexes = master.getIndexes();
			const Index& index = indexes[id_];
			const ColumnIds& columnIds = index.getColumnIds();
			buffer << (type_ == ALT_ADD_INDEX ? "Adding " : "Dropping ") << (index.isUnique() ? "unique " : "")
				<< "index " << index.getName() << "(";
			const Columns& columns = master.getColumns();
			for (uint32_t i = 0; i < columnIds.length(); ++i) {
				if (i > 0) {
					buffer << ", ";
				}
				buffer << columns[columnIds[i]].getName();
			}
			buffer << ")";
			break;
		}
		default: break;
	}
	return Str(reinterpret_cast<const char*>(buffer.getData()), static_cast<int>(buffer.position()));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// AlterComparator
//////////////////////////////////////////////////////////////////////////////////////////////////////

int AlterComparator::compare(const uint32_t row1, const uint32_t row2, const bool sortByRow) const {
	if (task_ != 0 && (comparisons_++ % 16384) == 0 && task_->isStopping()) {
		return 0;
	}
	const uint64_t offset1 = reader1_.seekRecordData(row1);
	const uint64_t offset2 = reader2_.seekRecordData(row2);
	uint8_t bitArray1[SPARROW_MAX_BIT_SIZE];
	recordWrapper_.readBits(reader1_, bitArray1);
	uint8_t bitArray2[SPARROW_MAX_BIT_SIZE];
	recordWrapper_.readBits(reader2_, bitArray2);
	uint8_t buffer1[MAX_KEY_LENGTH];
	uint8_t buffer2[MAX_KEY_LENGTH];
	const uint64_t start1 = offset1 + recordWrapper_.getBitSize();
	const uint64_t start2 = offset2 + recordWrapper_.getBitSize();
	const uint32_t nbInfos = infos_.length();
	for (uint32_t i = 0; i < nbInfos; ++i) {
		const ColumnInfo& info = infos_[i];
		const FieldBase& field = *fields_[info.getId()];
		uint8_t bits1 = 0;
		uint8_t bits2 = 0;
		uint32_t bitOffset = info.getBitOffset();
		const uint32_t nbits = info.getNBits();
		for (uint32_t b = 0; b < nbits; ++b, ++bitOffset) {
			bits1 |= ((bitArray1[bitOffset / 8] & (1 << (bitOffset % 8))) == 0 ? 0 : 1) << b;
			bits2 |= ((bitArray2[bitOffset / 8] & (1 << (bitOffset % 8))) == 0 ? 0 : 1) << b;
		}
		const uint32_t offset = info.getOffset();
		reader1_.seek(start1 + offset);
		uint8_t* p1 = buffer1;
		field.readPersistent(reader1_, stringReader1_, bits1, p1, true);
		const bool null1 = field.isNullable() && *p1++ != 0;
		reader2_.seek(start2 + offset);
		uint8_t* p2 = buffer2;
		field.readPersistent(reader2_, stringReader2_, bits2, p2, true);
		const bool null2 = field.isNullable() && *p2++ != 0;
		if (null1) {	// NULLs are the smallest values.
			if (!null2) {
				return -1;
			}
		} else if (null2) {
			return 1;
		} else {
			const int cmp = field.compare(p1, p2);
			if (cmp != 0) {
				return cmp;
			}
		}
	}
	if (sortByRow) {
		// Sort by row: in case of identical values, we get a better locality.
		return row1 > row2 ? 1 : (row1 < row2 ? -1 : 0);
	} else {
		return 0;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// AlterWriter
//////////////////////////////////////////////////////////////////////////////////////////////////////

void AlterWriter::writeRecord(ByteBuffer& buffer, const uint32_t row) _THROW_(SparrowException) {
	const uint64_t offset = reader_.seekRecord(row);
	uint8_t bitArray[SPARROW_MAX_BIT_SIZE];
	recordWrapper_.readBits(reader_, bitArray);

	// Write bits to output buffer.
	uint8_t* bitValues = bitArray_.data();
	memset(bitValues, 0, bitArray_.length());
	uint32_t n = 0;
	for (uint32_t i = 0; i < infos_.length(); ++i) {
		const ColumnInfo& info = infos_[i];
		uint32_t bitOffset = info.getBitOffset();
		const uint32_t nbits = info.getNBits();
		for (uint32_t b = 0; b < nbits; ++b, ++bitOffset, ++n) {
			if (bitArray[bitOffset / 8] & (1 << (bitOffset % 8))) {
				bitValues[n / 8] |= (1 << (n % 8));
			}
		}
	}
	buffer << ByteBuffer(bitValues, bitArray_.length());

	// Write field values.
	const uint64_t start = offset + recordWrapper_.getBitSize();
	BinBuffer* binBuffer = partition_.getVersion() >= PersistentPartition::appendVersion_ ? 0 : &binBuffer_;
	for (uint32_t i = 0; i < infos_.length(); ++i) {
		const ColumnInfo& info = infos_[i];
		uint8_t bits = 0;
		uint32_t bitOffset = info.getBitOffset();
		const uint32_t nbits = info.getNBits();
		for (uint32_t b = 0; b < nbits; ++b, ++bitOffset) {
			bits |= ((bitArray[bitOffset / 8] & (1 << (bitOffset % 8))) == 0 ? 0 : 1) << b;
		}
		const uint32_t offset = info.getOffset();
		reader_.seek(start + offset);
		fields_[info.getId()]->copy(reader_, stringReader_, bits, buffer, binBuffer);
	}
}

}

