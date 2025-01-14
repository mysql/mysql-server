/*
	Partition flush.
*/

#include "flush.h"
#include "internalapi.h"
#include "hash.h"
#include "fileutil.h"

#include "../engine/log.h"


namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FlushAllTask
//////////////////////////////////////////////////////////////////////////////////////////////////////
Lock FlushAllTask::lock_(true, "FlushAllTask::lock_");
FlushAllTask* FlushAllTask::task_ = 0;

void FlushAllTask::run(const uint64_t timestamp) _THROW_(SparrowException) {
	InternalApi::flushAll(false);
	{
		Guard guard(lock_);
		task_ = 0;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FlushJob
//////////////////////////////////////////////////////////////////////////////////////////////////////

void FlushJob::process() {
	// Check that no one else is already flushing into that main partition
	mainPartition_->getMaster().startFlush(mainPartition_->getSerial());

	const uint32_t nbWorkers = workerJobs_.length();
	const uint32_t nbWriters = writerJobs_.length();
	partition_->setJobCounter(nbWorkers + nbWriters);

	// Send Worker jobs to Worker thread pool, and Writer jobs to Writer thread pool.
	for (uint32_t i = 0; i < nbWorkers; ++i) {
		Worker::sendJob(workerJobs_[i]);
	}
	for (uint32_t i = 0; i < nbWriters; ++i) {
		Writer::sendJob(writerJobs_[i]);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// StringJob
//////////////////////////////////////////////////////////////////////////////////////////////////////

void StringJob::process() {
	try {
		partition_->flushStrings(mainPartition_);
	} catch(const SparrowException& e) {
		partition_->error();
		spw_print_error("Failed to flush strings from partition %s.%s.%llu (try %u): %s",
			partition_->getMaster()->getDatabase().c_str(), partition_->getMaster()->getTable().c_str(), static_cast<ulonglong>(partition_->getSerial()), partition_->getNbFlushTries(), e.getText());
		partition_->endFlush(mainPartition_);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// IndexJob
//////////////////////////////////////////////////////////////////////////////////////////////////////

void IndexJob::process() {
	partition_->compute(mainPartition_, id_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// WriteJob
//////////////////////////////////////////////////////////////////////////////////////////////////////

void WriteJob::process() {
	if (id_ == DATA_FILE) {
		partition_->getMaster()->blockUpdate();
	}
	try {
		partition_->write(mainPartition_, id_, indirector_);
	} catch(const SparrowException& e) {
		partition_->error();
		if (id_ == DATA_FILE) {
			spw_print_error("Failed to flush data from partition %s.%s.%llu (try %u): %s",
				partition_->getMaster()->getDatabase().c_str(), partition_->getMaster()->getTable().c_str(), static_cast<ulonglong>(partition_->getSerial()), partition_->getNbFlushTries(), e.getText());
		} else {
			spw_print_error("Failed to flush index %u from partition %s.%s.%llu (try %u): %s",
				id_, partition_->getMaster()->getDatabase().c_str(), partition_->getMaster()->getTable().c_str(), static_cast<ulonglong>(partition_->getSerial()), partition_->getNbFlushTries(), e.getText());
		}
	}
	partition_->endFlush(mainPartition_);
}

}
