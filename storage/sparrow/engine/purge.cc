/*
	Database automatic purge.
*/

#include "purge.h"
#include "internalapi.h"
#include "persistent.h"
#include "listener.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Purge
//////////////////////////////////////////////////////////////////////////////////////////////////////

Purge* Purge::purge_ = 0;

const uint64_t Purge::securityMargin_ = static_cast<uint64_t>(1024) * 1024 * 1024;

// STATIC
void Purge::initialize() _THROW_(SparrowException) {
	purge_ = new Purge();
	if (!purge_->start()) {
		throw SparrowException::create(false, "Cannot start purge thread");
	}
}

bool Purge::process() {
	SPARROW_ENTER("Purge::process");
	if (!sema_.wait(1000, true)) {
		const uint64_t now = Scheduler::now();
		if (now < last_ + 60000) {
			return true;
		}
	}

	if (sparrow_disable_purge) {
		last_ = Scheduler::now();
		return true;
	}

	// Loop while purge is necessary.
	DBUG_PRINT("sparrow_purge", ("Starting purge..."));
	const Masters masters = InternalApi::getAll();
	const uint32_t nbMasters = masters.length();
	const Filesystems& filesystems = FileUtil::getFilesystems(false);
	uint64_t	initialFreeDiskSpace = ULLONG_MAX;
	try {
		uint64_t limit = 0;
		uint64_t total = 0;
		uint64_t totalNormalized = 0;
		uint32_t i = 0;
		uint64_t freeDiskSpace = 0;
		bool purged = false;
		for (;;) {
			PersistentPartitions purgedPartitions;	// To delete partitions outside lock.
			for (uint32_t j = 0; j < nbMasters; ++j) {
				if (i == 0 || (i % nbMasters) == 0) {
					freeDiskSpace = FileUtil::getFreeDiskSpace();
					if (initialFreeDiskSpace == ULLONG_MAX) {
						initialFreeDiskSpace = freeDiskSpace;
					}
					total = 0;
					totalNormalized = 0;
					for (uint32_t i = 0; i < nbMasters; ++i) {
						const Master& master = *masters[i];
						ReadGuard guard(master.getLock());
						total += master.getDataSize() + master.getIndexSize();
						totalNormalized += master.getNormalizedSize();
					}
					limit = getLimit(freeDiskSpace, total);
					DBUG_PRINT("sparrow_purge", ("Free disk space %llu, total used %llu, total normalized %llu, limit %llu", static_cast<ulonglong>(freeDiskSpace), 
						static_cast<ulonglong>(total), static_cast<ulonglong>(totalNormalized), static_cast<ulonglong>(limit)));
					Atomic::set64(&SparrowStatus::get().totalSize_, total);
				}
				Master& master = *masters[i % nbMasters];
				i++;
				bool	force = false;
				PurgeMode	mode = PurgeModeControlTaskPerDB::getMode(master.getDatabase());
				if (master.needToPurge(limit, total, totalNormalized, force, mode)) {
					// Get the oldest partition of the table.
					// It will be deleted when the partition guard goes out of scope.
					DBUG_PRINT("sparrow_purge", ("Purged table %s.%s", master.getDatabase().c_str(), master.getTable().c_str()));
					PersistentPartitions	purgedPartitionsTable;
					bool	forced = master.purge(purgedPartitionsTable, force, mode);
					if (!purgedPartitionsTable.isEmpty()) {
						
						// Logging if this purge was triggered because an IO operation failed because of the device is full.
						if (logResult_) {
							uint64_t	totalSize = 0;
							for (uint k=0; k< purgedPartitionsTable.entries(); ++k) {
								totalSize += purgedPartitionsTable[k]->getDataSize() + purgedPartitionsTable[k]->getIndexSize();
							}
							totalSize /= 1024*1024;
							spw_print_information("Low free disk space, %lluMB. Purging %u partitions, forced %u, for a total size of %lluMB from table %s.%s", 
								static_cast<ulonglong>(freeDiskSpace/(1024*1024)), purgedPartitions.entries(), force, static_cast<ulonglong>(totalSize), master.getDatabase().c_str(), master.getTable().c_str());
						}

						// Logging if the partitions still contain valid data (forced purge because disk space is low)
						if (forced && sparrow_log_purge_activity) {
							uint64_t		low = UINT64_MAX;
							uint64_t		high = 0;
							uint64_t		totalSize = 0;
							for (uint k = 0; k < purgedPartitionsTable.entries(); ++k) {
								const PersistentPartition* partition = purgedPartitionsTable[k];
								if (partition->getMin() < low) low = partition->getMin();
								if (partition->getMax() > high) high = partition->getMax();
								totalSize += partition->getDataSize() + partition->getIndexSize();
							}
							totalSize /= 1024 * 1024;
							const Str low_ts = Str::fromTimestamp(low);
							const Str high_ts = Str::fromTimestamp(high);
							spw_print_information("Low free disk space, %lluMB (total used is %lluMB while the limit is %lluMB). Forced to purge %u partitions from table %s.%s, for a total size of %lluMB and containing data from %s to %s.",
								static_cast<ulonglong>(freeDiskSpace/(1024*1024)), static_cast<ulonglong>(total/(1024*1024)), static_cast<ulonglong>(limit/(1024*1024)),
								purgedPartitionsTable.entries(), master.getDatabase().c_str(), master.getTable().c_str(), static_cast<ulonglong>(totalSize), low_ts.c_str(), high_ts.c_str());
						}

						purged = true;
						break;
					}
				}
			}
			if (purgedPartitions.isEmpty()) {
				// Nothing to purge.
				break;
			}
		}

		// Check if we need to purge a specific file system.
		for (;;) {
			if (purged) {
				freeDiskSpace = FileUtil::getFreeDiskSpace();
				total = 0;
				totalNormalized = 0;
				for (uint32_t i = 0; i < nbMasters; ++i) {
					const Master& master = *masters[i];
					ReadGuard guard(master.getLock());
					total += master.getDataSize() + master.getIndexSize();
					totalNormalized += master.getNormalizedSize();
				}
				limit = getLimit(freeDiskSpace, total);
				Atomic::set64(&SparrowStatus::get().totalSize_, total);
			}
			const uint64_t fsMargin = Purge::getSecurityMargin();
			bool ok = true;
			for (uint32_t i = 0; i < filesystems.length(); ++i) {
				const uint64_t fsFree = filesystems[i]->getFree();
				if (nbMasters > 0 && fsFree <= fsMargin) {
					// A file system is getting full.
					// For each master file, get a list of partitions to delete to purge the given file system.
					// As we cannot leave "holes" in partitions, we may have to delete partitions from other file systems.
					// We choose the master file the closest to its normalized size with the lowest impact on the other
					// file systems.
					uint64_t minDelta = ULLONG_MAX;
					uint32_t minOther = UINT_MAX;
					Master* chosenMaster = 0;
					PersistentPartitions partitions;
					for (uint32_t j = 0; j < nbMasters; ++j) {
						Master& master = *masters[j];
						WriteGuard guard(master.getLock());
						PersistentPartitions tmp;
						const uint64_t delta = master.listPartitionsForFilesystem(i, tmp, limit, totalNormalized);
						const uint32_t n = tmp.length();
						if (n != 0 && n <= minOther) {
							if (n < minOther || delta < minDelta) {
								chosenMaster = &master;
								partitions = tmp;
								minDelta = delta;
								minOther = n;
							}
						}
					}
					if (chosenMaster != 0) {
						WriteGuard guard(chosenMaster->getLock());
						if (chosenMaster->purgePartitionsForFilesystem(partitions)) {
							if (logResult_) {
								uint64_t	totalSize = 0;
								for (uint k=0; k<partitions.entries(); ++k) {
									totalSize += partitions[k]->getDataSize() + partitions[k]->getIndexSize();
								}
								totalSize /= 1024*1024;
								spw_print_information("Low free disk space on file system %s: %llu. Purging %u partitions for a total size of %lluMB from table %s.%s", 
									filesystems[i]->getPath().c_str(), static_cast<ulonglong>(fsFree/(1024*1024)), 
									partitions.entries(), static_cast<ulonglong>(totalSize), chosenMaster->getDatabase().c_str(), chosenMaster->getTable().c_str());
							}
							purged = true;
						}
					}
					ok = false;
					break;
				}
			}
			if (ok) {
				break;
			}
		}

		if (logResult_) {
			spw_print_information("Purge result: free disk space changed from %lluMB to %lluMB.", static_cast<ulonglong>(initialFreeDiskSpace/(1024*1024)),
				static_cast<ulonglong>(freeDiskSpace/(1024*1024)));
		}
	} catch(const SparrowException& e) {
		e.toLog();
	}
	DBUG_PRINT("sparrow_purge", ("Purge completed"));
	last_ = Scheduler::now();
	logResult_ = false;
	return true;
}

// STATIC
uint64_t Purge::getLimit(const uint64_t freeDiskSpace, const uint64_t total) {
	// Keep a security margin for each file system.
	const uint64_t margin = Purge::getSecurityMargin() * FileUtil::getFilesystems(false).length();
	uint64_t limit = total + freeDiskSpace;
	limit = limit > margin ? limit - margin : 0;
	if (sparrow_max_disk_size != 0 && limit > sparrow_max_disk_size) {
		limit = sparrow_max_disk_size;
	}
	return limit;
}

bool Purge::notifyStop() {
	Purge::wakeUp();
	return true;
}

// STATIC
void Purge::wakeUp(bool logResult) {
	purge_->logResult_ = logResult;
	purge_->sema_.post();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PurgeTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

// To purge zombie directories, every day.
void PurgeTask::run(const uint64_t timestamp) _THROW_(SparrowException) {
	SPARROW_ENTER("PurgeTask::run");
	DBUG_PRINT("sparrow_purge", ("Cleaning up zombie data directories"));
	const Masters masters = InternalApi::getAll();
	const uint32_t nbMasters = masters.length();
	const Filesystems& filesystems = FileUtil::getFilesystems(true);
	for (uint32_t i = 0; i < nbMasters; ++i) {
		Master& master = *masters[i];
		uint64_t oldest;
		{
			ReadGuard guard(master.getLock());
			oldest = master.getOldest(true);
		}
		struct tm t;
		if (oldest != 0) {
			oldest -= oldest % 86400000;
			const time_t tt = static_cast<time_t>(oldest / 1000);
			if (gmtime_r(&tt, &t) == 0) {
				continue;
			}
		}
		const uint32_t limit = oldest == 0 ? UINT_MAX : ((t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday);
		for (uint32_t j = 0; j < filesystems.length(); ++j) {
			try {
				// Scan data directory and remove days older than oldest.
				char path[FN_REFLEN];
				Files files;
				FileUtil::scanDirectory(master.getDataDirectory(j, path), "", 1, files, false);
				SYSslistIterator<Str> iterator(files);
				while (++iterator) {
					const Str& file = iterator.key();
					const char* dirname = file.c_str();
					const size_t l = strlen(dirname);
					if (l < 8) {
						continue;
					}
					dirname += l - 8;
					uint32_t d;
					if (sscanf(dirname, "%u", &d) == 1 && d < limit) {
#ifndef NDEBUG
						const Str soldest = oldest == 0 ? Str("N/A") : Str::fromTimestamp(oldest);
						DBUG_PRINT("sparrow_purge", ("Table %s.%s: removing directory %s because it is older than %s",
							master.getDatabase().c_str(), master.getTable().c_str(), file.c_str(), soldest.c_str()));
#endif
						FileUtil::deleteDirectory(file.c_str());
					}
				}
			} catch(const SparrowException& e) {
				// Ignore error.
			}
		}
	}
}

}
