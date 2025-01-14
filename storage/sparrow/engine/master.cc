/*
	Master file.
*/

#include "master.h"
#include "transient.h"
#include "persistent.h"
#include "internalapi.h"
#include "alter.h"
#include "coalescing.h"
#include "../handler/hasparrow.h"
#include "purge.h"
#include "hash.h"
#include "fileutil.h"
#include "listener.h"

//#include "../api/api_assert.h"

#include "../engine/log.h"
#include "sql/mysqld.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Master
//////////////////////////////////////////////////////////////////////////////////////////////////////

// File version history:
// 1	Initial version.
// 2	No longer store column names
//		(added class ColumnWithName to serialize name - necessary in Sparrow API).
// 3	Replace max table size by max lifetime.
// 4	Add aggregation period.
// 5	Add auto-incremental column.
// 6	Add index and column names. Ability to alter indexes.
// 7	No longer store column size in master file.
// 8	Multiple filesystems: store filesystem index with each partition.
// 9	Ability to alter indexes.
// 10	Ability to alter indexes, continued.
// 11	Remove useless Index::id_.
// 12	Add flag Index::dropped_.
// 13	Add PersistentPartition::version_.
// 14	Add coalescing period.
// 15	Persist alter duration.
// 16	Table-specific default WHERE time period.
// 17	Ability to add/remove columns.
// 18	Textual default value on columns.
// 19	Replace drop flag by drop serial number on columns.
// 20	Add data serial number.
// 21	Table-specific string optimization size.
// 22	Add skipped columns ids to each partition
// 23	Add data compression
// 24	Store timestamp precision in the column's attribute 'info'
const uint32_t Master::currentVersion_ = 24;

//pthread_key(TABLE_SHARE*, Master::threadKey_);
thread_local TABLE_SHARE*	Master::threadKey_{nullptr};

// STATIC
void Master::initialize() {
	Master::threadKey_ = nullptr;
}

Master::Master(const char* database, const char* table, const bool key /* = false */)
	: id_(0), lock_(0), updateLock_(0), canUpdate_(0), updateOnGoing_(0), updating_(false), updateBlockers_(0), flushLock_(0), canFlush_(0),
	database_(database, !key), table_(table, !key), dnsConfiguration_(0), indexAlterSerial_(0), indexAlterStarted_(0), indexAlterElapsed_(0),
	coalescingPeriod_(3600000), defaultWhere_(0), stringOptimization_(0), coalescingTimestamp_(0),
	version_(Master::currentVersion_), depLock_(0), depCond_(0) {
	if (!isKey()) {
		setup(true);
	}
}

void Master::setup(const bool full) {
	database_ = Str(database_.c_str());
	table_ = Str(table_.c_str());
	id_ = MasterId::newId();
	MasterId::insert(this);
	database_.toLower();
	table_.toLower();
	char tmp[1024];
	snprintf(tmp, sizeof(tmp), "Master(%s.%s)::lock_", database_.c_str(), table_.c_str());
	lock_ = new RWLock(false, tmp);
	snprintf(tmp, sizeof(tmp), "Master(%s.%s)::updateLock_", database_.c_str(), table_.c_str());
	updateLock_ = new Lock(false, tmp);
	snprintf(tmp, sizeof(tmp), "Master(%s.%s)::canUpdate_", database_.c_str(), table_.c_str());
	canUpdate_ = new Cond(false, *updateLock_, tmp);
	snprintf(tmp, sizeof(tmp), "Master(%s.%s)::updateOnGoing_", database_.c_str(), table_.c_str());
	updateOnGoing_ = new Cond(false, *updateLock_, tmp);
	snprintf(tmp, sizeof(tmp), "Master(%s.%s)::flushLock_", database_.c_str(), table_.c_str());
	flushLock_ = new Lock(false, tmp);
	snprintf(tmp, sizeof(tmp), "Master(%s.%s)::canFlush_", database_.c_str(), table_.c_str());
	canFlush_ = new Cond(false, *flushLock_, tmp);
	snprintf(tmp, sizeof(tmp), "Master(%s.%s)::depLock_", database_.c_str(), table_.c_str());
	depLock_ = new Lock(false, tmp);
	snprintf(tmp, sizeof(tmp), "Master(%s.%s)::depCond_", database_.c_str(), table_.c_str());
	depCond_ = new Cond(false, *depLock_, tmp);
	if (full) {
		serial_ = 0ULL;
		timeCreated_ = std::time(nullptr);
		dataSize_ = 0ULL;
		indexSize_ = 0ULL;
		records_ = 0ULL;
		maxLifetime_ = 0;
		aggregationPeriod_ = 0;
		autoInc_ = 1;
	}
	acquireRef();
}

void Master::prepareForDeletion() {
	SPARROW_ENTER("Master::prepareForDeletion");

	// Stop dependencies and wait for them to complete.
	{
		Guard guard(getDepLock());
		SYSidlistIterator<MasterDependency> iterator(dependencies_);
		while (++iterator) {
			iterator.key()->stop();
		}
	}
	while (true) {
		Guard guard(getDepLock());
		if (dependencies_.isEmpty()) {
			break;
		}
		depCond_->wait(true);
	}
	MasterId::remove(this);

	// Close all opened files.
	closeFiles();

	// Detach partitions.
	{
		WriteGuard guard(getLock());
		for (uint32_t i = 0; i < partitions_.length(); ++i) {
			partitions_[i]->detach();
			[[maybe_unused]] bool	last_ref = partitions_[i]->releaseRef();
			assert( last_ref == true );
		}
		partitions_.clearAndDestroy();
		intervals_.clear();
		transientPartitions_.clear();
	}
	releaseRef();
}

Master::~Master() {
	SPARROW_ENTER("Master::~Master");
	if (!isKey()) {
		DBUG_PRINT("sparrow_master", ("Destroying files for table %s.%s", database_.c_str(), table_.c_str()));

		// Make sure this master is no longer seen from the API.
		assert(!InternalApi::hashContains(this));
		if (dnsConfiguration_ != 0 && dnsConfiguration_->isStarted()) {
			DnsConfiguration::release(dnsConfiguration_.get());
		}

		delete lock_;
		delete canUpdate_;
		delete updateOnGoing_;
		delete updateLock_;
		delete depLock_;

		// Delete all subdirectories, data and index files.
		char buffer[FN_REFLEN];
		const char* masterFile = getMasterFileName(buffer);
		my_delete(masterFile, MYF(0));
		const Filesystems& filesystems = FileUtil::getFilesystems(true);
		for (uint32_t i = 0; i < filesystems.length(); ++i) {
			const Filesystem& filesystem = *filesystems[i];
			snprintf(buffer, sizeof(buffer), "%s/%s/%s", filesystem.getPath().c_str(), database_.c_str(), table_.c_str());
			char dir[FN_REFLEN];
			if (formatFileName(dir, "", buffer, "")) {
				FileUtil::deleteDirectory(dir);
			}
		}
	}
}

// Close files currently opened in this table.
void Master::closeFiles() {
	const uint32_t length = partitions_.length();
	for (uint32_t i = 0; i < length; ++i) {
		const Partition& partition = *partitions_[i];
		if (partition.isTransient()) {
			continue;
		}
		const PersistentPartition& persistentPartition = static_cast<const PersistentPartition&>(partition);
		char name[FN_REFLEN];
		FileCache::releaseFile(FileId(persistentPartition.getFileName(DATA_FILE, name), FILE_TYPE_DATA, FILE_MODE_READ), false);
		FileCache::releaseFile(FileId(persistentPartition.getFileName(STRING_FILE, name), FILE_TYPE_STRING, FILE_MODE_READ), false);
		const uint32_t nbIndexes = getIndexes().length();
		for (uint32_t i = 0; i < nbIndexes; ++i) {
			FileCache::releaseFile(FileId(persistentPartition.getFileName(i, name), FILE_TYPE_INDEX, FILE_MODE_READ), false);
		}
	}
}

// Rename this table.
void Master::rename(const char* newDatabase, const char* newTable) _THROW_(SparrowException) {
	SPARROW_ENTER("Master::rename");
	DBUG_PRINT("sparrow_master", ("Renaming master file %s.%s to %s.%s", database_.c_str(), table_.c_str(), newDatabase, newTable));

	// Close all opened files.
	closeFiles();

	// Rename master file.
	char from[FN_REFLEN];
	char to[FN_REFLEN];
	const char* database = database_.c_str();
	const char* table = table_.c_str();
	Master::getMasterFileName(from, database, table, true);
	if (FileUtil::doesFileExist(from)) {
		Master::getMasterFileName(to, newDatabase, newTable, true);
		FileUtil::rename(from, to);
	}

	// Rename data directories.
	const Filesystems& filesystems = FileUtil::getFilesystems(true);
	for (uint32_t i = 0; i < filesystems.length(); ++i) {
		const Filesystem& filesystem = *filesystems[i];
		snprintf(from, sizeof(from), "%s/%s/%s", filesystem.getPath().c_str(), database, table);
		char dirFrom[FN_REFLEN];
		snprintf(to, sizeof(to), "%s/%s/%s", filesystem.getPath().c_str(), newDatabase, newTable);
		char dirTo[FN_REFLEN];
		if (formatFileName(dirFrom, "", from, "") && formatFileName(dirTo, "", to, "")) {
			dirFrom[strlen(dirFrom) - 1] = 0;
			dirTo[strlen(dirTo) - 1] = 0;
			if (FileUtil::doesFileExist(dirFrom)) {
				FileUtil::createDirectories( dirTo );
				FileUtil::rename(dirFrom, dirTo);
			}
		}
	}

	// Change internal database and table names.
	database_ = Str(newDatabase);
	table_ = Str(newTable);
}

PersistentPartitionGuard Master::findMainPartition( uint64_t serial, TimePeriod period, uint32_t columnAlterSerial, uint32_t indexAlterSerial, const ColumnIds& emptyColumnIds ) {
	SPARROW_ENTER("Master::findMainPartition");
	const uint64_t coalescingPeriod = getCoalescingPeriod();
	const TimePeriod tperiod = period;
	if (coalescingPeriod != 0) {
		const uint64_t tmin = tperiod.getMin();
		const uint64_t cmin = tmin - tmin % coalescingPeriod;
		const uint64_t cmax = cmin + coalescingPeriod;
		const TimePeriod cperiod(&cmin, &cmax, true, false);
#ifndef NDEBUG
		const Str coales = Str::fromDuration( coalescingPeriod );
		const Str trs_period = Str::fromTimePeriod(tperiod);
		const Str prs_period = Str::fromTimePeriod(cperiod);
		DBUG_PRINT("sparrow_transient", ("Transient partition %llu: coalescing p %s, %s --> %s", 
			static_cast<ulonglong>(serial), coales.c_str(), trs_period.c_str(), prs_period.c_str()));
#endif
		
		PersistentPartition*	pp = NULL;
		uint32_t					best_value = 0xFFFFFFFFUL;
		Intervals intervals;
		intervals_.findOverlaps(cperiod, intervals);
		for (uint32_t i = 0; i < intervals.length(); ++i) {
			PersistentPartition* p = static_cast<PersistentPartition*>(intervals[i]);
			if (p->isMain() && p->getVersion() >= PersistentPartition::appendVersion_
				&& p->getColumnAlterSerial() == columnAlterSerial && p->getSkippedColumns().contains(emptyColumnIds)) {
				if (best_value > emptyColumnIds.entries() - p->getSkippedColumns().entries()) {
					best_value = emptyColumnIds.entries() - p->getSkippedColumns().entries();
					pp = p;
					if (best_value == 0)
						break;
				}
			}
		}
		if (best_value <= sparrow_column_optimisation_lvl) {
			assert(pp != NULL);
			const TimePeriod pperiod = pp->getPeriod();
#ifndef NDEBUG
			TimePeriod	period_union = pperiod.makeUnion(cperiod);
			const Str pperiod_str = Str::fromTimePeriod(pperiod);
			const Str period_union_str = Str::fromTimePeriod(period_union);
			DBUG_PRINT("sparrow_transient", ("Main persisted partition %s.%s.%lu: %s =?= %s", getDatabase().c_str(),
				getTable().c_str(), pp->getSerial(), pperiod_str.c_str(), period_union_str.c_str()));
#endif
			if (pperiod.makeUnion(cperiod) == cperiod) {
#ifndef NDEBUG
				const Str speriod = Str::fromTimePeriod(tperiod);
				const Str mperiod = Str::fromTimePeriod(pperiod);
				DBUG_PRINT("sparrow_transient", ("Found main partition for %s.%s.%llu %s: %llu %s", getDatabase().c_str(),
					getTable().c_str(), static_cast<ulonglong>(serial), speriod.c_str(), static_cast<ulonglong>(pp->getSerial()), mperiod.c_str()));
#endif
				return PersistentPartitionGuard(pp);
			}
		}
	}

	// No main partition found; create new one.
	PersistentPartition* mainPartition = newPersistentPartition(PersistentPartition::currentVersion_, SAME_AS_SERIAL,
		FileUtil::chooseFilesystem(false), tperiod, 0, indexAlterSerial, columnAlterSerial, 0, 0, emptyColumnIds);
	mainPartition->acquireRef();
#ifndef NDEBUG
	const Str speriod = Str::fromTimePeriod(tperiod);
	DBUG_PRINT("sparrow_transient", ("Did not find main partition for %s.%s.%llu %s: created new with serial %llu", getDatabase().c_str(),
		getTable().c_str(), static_cast<ulonglong>(serial), speriod.c_str(), static_cast<ulonglong>(mainPartition->getSerial())));
#endif
	return PersistentPartitionGuard(mainPartition);
}

// Add to a vector all partitions in a given time period.
void Master::getPartitionsForTimePeriod(const TimePeriod& period, ReferencedPartitions& entries, QueryInfo& queryInfo) const {
	SPARROW_ENTER("Master::getPartitionsForTimePeriod");

	// Find overlaps in persistent partitions.
	if (period.isVoid()) {
		return;
	}
	Intervals intervals;
	intervals_.findOverlaps(period, intervals);
	for (uint32_t i = 0; i < intervals.length(); ++i) {
		PersistentPartition* partition = static_cast<PersistentPartition*>(intervals[i]);
		PartitionGuard partitionGuard(partition);
		if (!entries.contains(partitionGuard) && partition->getPeriod().intersects(period)
			&& (!partition->isMain() || partition->getVersion() < PersistentPartition::appendVersion_)) {
#ifndef NDEBUG
			const Str speriod = Str::fromTimePeriod(partitionGuard->getPeriod());
			DBUG_PRINT("sparrow_context", ("Load partition %llu: %s", static_cast<ulonglong>(partitionGuard->getSerial()), speriod.c_str()));
#endif
			entries.insert(partitionGuard);
		}
	}

	// Find overlaps in transient partitions.
	for (uint32_t i = 0; i < transientPartitions_.length(); ++i) {
		TransientPartition* partition = transientPartitions_[i];
		if (period.intersects(partition->getPeriod())) {
			PartitionGuard partitionGuard(partition);
			if (!entries.contains(partitionGuard)) {
				// Snapshot the current number of transient rows.
				if (queryInfo.snapshotTransientPartition(partition)) {
					// Insert only partitions with data.
					entries.insert(partitionGuard);
				}
			}
		}
	}
}

// STATIC
const char* Master::getMasterFileName(char* buffer, const char* database, const char* table,
	const bool appendExtension) _THROW_(SparrowException) {
	SPARROW_ENTER("Master::getMasterFileName");
	if (!formatFileName(buffer, table, database, appendExtension ? ".spm" : "")) {
		throw SparrowException::create(false, "Cannot build master file name for table %s.%s",
			database, table);
	}
	return buffer;
}

// STATIC
bool Master::formatFileName(char* to, const char* name, const char* dir, const char *extension) {
	char tmp[FN_REFLEN];
	if (!test_if_hard_path(dir)) {
		strxnmov(tmp, sizeof(tmp) - 1, mysql_real_data_home, dir, NullS);
		dir = tmp;
	}
	return fn_format(to, name, dir, extension, MY_APPEND_EXT | MY_UNPACK_FILENAME | MY_SAFE_PATH) != 0;
}

const char* Master::getMasterFileName(char* buffer) const _THROW_(SparrowException) {
	SPARROW_ENTER("Master::getMasterFileName");
	return getMasterFileName(buffer, database_.c_str(), table_.c_str(), true);
}

const char* Master::getDataDirectory(const uint32_t filesystem, char* buffer) const _THROW_(SparrowException) {
	SPARROW_ENTER("Master::getDataDirectory");
	char subdir[FN_REFLEN];
	snprintf(subdir, sizeof(subdir), "%s/%s/%s/", FileUtil::getFilesystemPath(filesystem), database_.c_str(), table_.c_str());
	char tmp[FN_REFLEN];
	const char* dir = subdir;
	if (!test_if_hard_path(dir)) {
		strxnmov(tmp, sizeof(tmp) - 1, mysql_real_data_home, dir, NullS);
		dir = tmp;
	}
	if (fn_format(buffer, "", dir, "", MY_SAFE_PATH) == 0) {
		throw SparrowException::create(false, "Cannot build directory name for table %s.%s",
			getDatabase().c_str(), getTable().c_str());
	}
	return buffer;
}

// STATIC
bool Master::checkForCorruption(struct tm& t) {
	if (t.tm_year < 100 || t.tm_year > 150
		|| t.tm_mday < 1 || t.tm_mday > 31
		|| t.tm_mon < 0 || t.tm_mon > 11 ) {
			char	t_str[255];
			strftime(t_str, sizeof(t_str), "%Y-%m-%d %H:%M:%S", &t);
			spw_print_error("bad partition timestamp: %s", t_str);
			return false;
	}
	return true;
}

// Build data/index file name for a given time period and file id.
const char* Master::getFileName(const uint32_t version, const uint32_t filesystem, const TimePeriod& period,
	const uint32_t fileId, const uint64_t serial, const uint64_t dataSerial, char* buffer) const _THROW_(SparrowException) {
	SPARROW_ENTER("Master::getFileName");

	// Initialize all locals here to avoid gcc error because of goto.
	char subdir[FN_REFLEN];
	time_t start;
	struct tm t;
	char filename[FN_REFLEN];
	const char* extension;
	uint32_t lowMinutes;
	uint32_t lowSeconds;
	uint32_t lowMilliseconds;
	uint32_t upMinutes;
	uint32_t upSeconds;
	uint32_t upMilliseconds;
	int l1 = snprintf(subdir, sizeof(subdir), "%s/%s/%s/", FileUtil::getFilesystemPath(filesystem), database_.c_str(), table_.c_str());
	if (l1 <= 0) {
		goto internalError;
	}
	start = static_cast<time_t>(period.getMin() / 1000);
	if (gmtime_r(&start, &t) == 0) {
		goto internalError;
	}
	if (checkForCorruption(t) == false) {
		goto internalError;
	}
	if (strftime(subdir + l1, sizeof(subdir) - l1, "%Y%m%d/%H", &t) == 0) {
		goto internalError;
	}
	lowMinutes = static_cast<uint32_t>((period.getMin() % 3600000) / 60000);
	lowSeconds = static_cast<uint32_t>((period.getMin() % 60000) / 1000);
	lowMilliseconds = static_cast<uint32_t>(period.getMin() % 1000);
	upMinutes = static_cast<uint32_t>((period.getMax() % 3600000) / 60000);
	upSeconds = static_cast<uint32_t>((period.getMax() % 60000) / 1000);
	upMilliseconds = static_cast<uint32_t>(period.getMax() % 1000);
	if (fileId == DATA_FILE) {
		if (version >= PersistentPartition::appendVersion_) {
			if (snprintf(filename, sizeof(filename), "%010llu", static_cast<ulonglong>(dataSerial)) <= 0) {
				goto internalError;
			}
		} else {
			if (snprintf(filename, sizeof(filename), "%010llu_%02u%02u%03u_%02u%02u%03u",
				static_cast<ulonglong>(serial), lowMinutes, lowSeconds, lowMilliseconds, upMinutes, upSeconds, upMilliseconds) <= 0) {
				goto internalError;
			}
		}
		extension = ".spd";
	} else if (fileId == STRING_FILE) {
		if (snprintf(filename, sizeof(filename), "%010llu",	static_cast<ulonglong>(dataSerial)) <= 0) {
			goto internalError;
		}
		extension = ".sps";
	} else {
		if (version >= PersistentPartition::appendVersion_) {
			if (snprintf(filename, sizeof(filename), "%010llu_%010llu_%02d", static_cast<ulonglong>(dataSerial), static_cast<ulonglong>(serial), fileId) <= 0) {
				goto internalError;
			}
		} else {
			if (snprintf(filename, sizeof(filename), "%010llu_%02u%02u%03u_%02u%02u%03u_%02d",
				static_cast<ulonglong>(serial), lowMinutes, lowSeconds, lowMilliseconds, upMinutes, upSeconds, upMilliseconds, fileId) <= 0) {
				goto internalError;
			}
		}
		extension = ".spi";
	}
	if (!formatFileName(buffer, filename, subdir, extension)) {
		goto internalError;
	}
	return buffer;
internalError:
	const char* type = "index";
	if (fileId == DATA_FILE) {
		type = "data";
	} else if (fileId == STRING_FILE) {
		type = "string";
	}
	throw SparrowException::create(false, "Cannot build %s file name for table %s.%s",
		type, getDatabase().c_str(), getTable().c_str());
}

// STATIC
Master* Master::fromDisk(const char* database, const char* table, TABLE_SHARE* s) _THROW_(SparrowException) {
	SPARROW_ENTER("Master::fromDisk");
	Master::threadKey_ = s;
	char filename[FN_REFLEN];
	Master* master = new Master(database, table, true);
	AutoPtr<Master> guard(master);
	master->getMasterFileName(filename);
	DBUG_PRINT("sparrow_master", ("Reading master %s.%s from file %s", database, table, filename));
	if (!FileUtil::doesFileExist(filename)) {
		return 0;
	}
	FileReader reader(filename);
	reader >> *master;
	guard.release();
	master->setup(false);
	return master;
}

// Set update time and write master file to disk.
void Master::toDisk() _THROW_(SparrowException) {
	SPARROW_ENTER("Master::toDisk");
	timeUpdated_ = std::time(nullptr);
	char filename[FN_REFLEN];
	getMasterFileName(filename);
	DBUG_PRINT("sparrow_master", ("Writing master %s.%s to file %s", database_.c_str(), table_.c_str(), filename));
	FileWriter writer(filename, FILE_TYPE_MISC, FILE_MODE_CREATE);
	writer << *this;
	writer.write();
}

TransientPartitionGuard Master::getTransientPartition( const uint64_t& timestamp ) {
	SPARROW_ENTER("Master::getTransientPartition");
	WriteGuard guard(getLock());
	TransientPartition*	partition = NULL;
	const Str ts_str = Str::fromTimestamp(timestamp);
	if ( coalescingPeriod_ == 0 || timestamp == 0 ) {
		TransientPartition*		part = transientPartitions_.isEmpty() ? 0 : transientPartitions_.last();
		if ( !part ) {
			DBUG_PRINT("sparrow_transient", ("Get transient partition for %s.%s, for %s. No transient part.", database_.c_str(), table_.c_str(), ts_str.c_str()));
		}
		else if ( part->isDone() ) {
			DBUG_PRINT("sparrow_transient", ("Get transient partition for %s.%s, for %s. But last is done.", database_.c_str(), table_.c_str(), ts_str.c_str()));
		} else {
			partition = part;
			DBUG_PRINT("sparrow_transient", ("Get transient partition for %s.%s, for %s. Using last %llu.", 
				database_.c_str(), table_.c_str(), ts_str.c_str(), static_cast<ulonglong>(partition->getSerial())));
		}
	} else {
		for ( int i=transientPartitions_.entries()-1; i>=0; --i ) {
			TransientPartition*		part = transientPartitions_[i];
			if ( part->isDone() ) 
				continue;
			TimePeriod	period = part->getPeriod();
			if ( period.getMin() == 0 || period.getMin() == ULLONG_MAX ) {
				DBUG_PRINT("sparrow_transient", ("Get transient partition for %s.%s, for %s. Part %llu has no timestamp. Ignoring.", 
					database_.c_str(), table_.c_str(), ts_str.c_str(), static_cast<ulonglong>(part->getSerial())));
			} else {
				uint64_t	low = period.getMin();
				low -= low%coalescingPeriod_;
				uint64_t	up = low + coalescingPeriod_;
				TimePeriod	period_adj( &low, &up, true, false );
				if ( period_adj.contains( timestamp ) ) {
					partition = part;
					DBUG_PRINT("sparrow_transient", ("Get transient partition for %s.%s, for %s. Using existing one, %llu.", 
						database_.c_str(), table_.c_str(), ts_str.c_str(), static_cast<ulonglong>(partition->getSerial())));
					break;
				}
			}
		}
	}
	if ( partition == 0 ) {
		partition = new TransientPartition(this, serial_++);
		partition->acquireRef();
		assert(!partitions_.contains(partition));
		partitions_.insert(partition);
		transientPartitions_.insert(partition);
		DBUG_PRINT("sparrow_transient", ("Get transient partition for %s.%s, for %s. Created new one, %llu.", 
			database_.c_str(), table_.c_str(), ts_str.c_str(), static_cast<ulonglong>(partition->getSerial())));
	}
	return TransientPartitionGuard(partition);
}

void Master::getTransientPartitions( TransientPartitions& partitions ) const {
	SPARROW_ENTER("Master::getTransientPartitions");
	uint32_t nb = transientPartitions_.length();
	partitions.resize( nb );
	for ( uint i=0; i<nb; ++i ) {
		partitions.append( TransientPartitionGuard(transientPartitions_[i]) );
	}
}

bool Master::removePartitions(const PersistentPartitions& partitions) {
	SPARROW_ENTER("Master::removePartitions");
	uint64_t dataSize = 0;
	uint64_t indexSize = 0;
	uint64_t records = 0;
	bool temporary = false;
	for (uint32_t i = 0; i < partitions.length(); ++i) {
		PersistentPartition* partition = partitions[i].get();
		partitions_.remove(partition);
		intervals_.remove(*partition);
		if (partition->isTemporary()) {
			temporary = true;
		} else {
			dataSize += partition->getDataSize();
			indexSize += partition->getIndexSize();
			records += partition->getRecords();
			if (!partition->isMain()) {
				partition->getMainPartition()->removeChildPartition(partition);
			}
		}
		DBUG_PRINT("sparrow_master", ("Removing partition %s.%s.%llu",
			getDatabase().c_str(), getTable().c_str(), static_cast<ulonglong>(partition->getSerial())));
		partition->releaseRef();
	}
	setDataSize(getDataSize() - dataSize);
	setIndexSize(getIndexSize() - indexSize);
	setRecords(getRecords() - records);
	return temporary;
}

void Master::listPartitionsForMain(const PersistentPartition& mainPartition, PersistentPartitions& partitions) const _THROW_(SparrowException) {
	SPARROW_ENTER("Master::listPartitionsForMain");
	const uint64_t dataSerial = mainPartition.getSerial();
	if (mainPartition.getVersion() >= PersistentPartition::appendVersion_) {
		for (uint32_t i = 0; i < partitions_.length(); ++i) {
			Partition* p = partitions_[i];
			if (!p->isMain() && p->getDataSerial() == dataSerial && !p->isTemporary()) {
				if (p->isTransient() || coalescedSerials_.contains(p->getSerial())) {
					// Cannot purge if main partition is referenced by a transient partition
					// or if the partition is being coalesced.
					partitions.clear();
					if (p->isTransient()) {
						throw SparrowException::create(false, "Cannot remove partition %s.%s.%llu because it is still used for data insertion",
							getDatabase().c_str(), getTable().c_str(), static_cast<ulonglong>(p->getSerial()));
					} else {
						throw SparrowException::create(false, "Cannot remove partition %s.%s.%llu because it is being coalesced",
							getDatabase().c_str(), getTable().c_str(), static_cast<ulonglong>(p->getSerial()));
					}
				}
				partitions.insert(PersistentPartitionGuard(static_cast<PersistentPartition*>(p)));
			}
		}
	} else if (coalescedSerials_.contains(dataSerial)) {
		throw SparrowException::create(false, "Cannot remove partition %s.%s.%llu because it is being coalesced",
			getDatabase().c_str(), getTable().c_str(), static_cast<ulonglong>(dataSerial));
	}
	partitions.insert(PersistentPartitionGuard(const_cast<PersistentPartition*>(&mainPartition)));
}

void Master::removePartitions(const TimePeriod& period) _THROW_(SparrowException) {
	SPARROW_ENTER("Master::removePartitions");
	PersistentPartitions partitions;
	{
		WriteGuard guard(getLock());
		Intervals intervals;
		intervals_.findOverlaps(period, intervals);
		for (uint32_t i = 0; i < intervals.length(); ++i) {
			PersistentPartition* partition = static_cast<PersistentPartition*>(intervals[i]);
			if (partition->isMain() && partition->getPeriod().intersects(period)) {
				PersistentPartitionGuard partitionGuard(partition);
				if (!partitions.contains(partitionGuard)) {
					listPartitionsForMain(*partition, partitions);
				}
			}
		}
		removePartitions(partitions);
		toDisk();
	}
}

bool Master::forceFlush() {
	SPARROW_ENTER("Master::forceFlush");
	//WriteGuard guard(getLock());
	TransientPartitions	transientPartitions;
	{
		ReadGuard guard(getLock());
		getTransientPartitions( transientPartitions );
	}
	return forceFlushNoLock( transientPartitions );
}

bool Master::forceFlushNoLock( const TransientPartitions& transientPartitions, bool master_lock_taken ) {
	SPARROW_ENTER("Master::forceFlushNoLock");
	if (transientPartitions.isEmpty()) {
		return false;
	}
	bool result = false;
	uint32_t i = transientPartitions.length();
	do {
		TransientPartition* partition = transientPartitions[--i];
		if (partition->forceFlush( master_lock_taken )) {
			result = true;
		}
	} while (i > 0);
	return result;
}

void Master::waitForFlush() {
	SPARROW_ENTER("Master::waitForFlush");
	for (;;) {
		{
			ReadGuard guard(getLock());
			if (transientPartitions_.isEmpty()) {
				return;
			}
		}
		my_sleep(100000);
	}
}

void Master::waitForFlush(const PartitionIds& flushedPartitions) {
	SPARROW_ENTER("Master::waitForFlush");
	for (;;) {
		{
			ReadGuard guard(getLock());
			uint32_t	i, j, nb = transientPartitions_.length();
			uint32_t	nbFlushed = flushedPartitions.length();
			for (i=0; i<nb; ++i) {
				uint64_t	id = transientPartitions_[i]->getSerial();
				for (j=0; j<nbFlushed; ++j) {
					if (flushedPartitions[j] == id)
						break;
				}
				if (j < nbFlushed)
					break;
			}
			if (i == nb)
				return;
		}
		my_sleep(100000);
	}
}

void Master::getColumnIds( const Names& colNames, ColumnIds& colIds ) const _THROW_(SparrowException)
{
	SPARROW_ENTER("Master::checkColumns");
	if ( colNames.isEmpty() ) {
		throw SparrowException::create(false, "Cannot insert data in %s.%s because client app sent an empty column list",
			getDatabase().c_str(), getTable().c_str());
	}

	ReadGuard guard(getLock());

	/*// Use only valid columns for checks
	Columns	validColumns(columns_.length());
	for (uint32_t i = 0; i < columns_.length(); ++i) {
		const Column& column = columns_[i];
		if (!column.isDropped()) {
			validColumns.append(column);
		}
	}

	if ( colNames.entries() > validColumns.length() ) {
		throw SparrowException::create(false, "Cannot insert data in %s.%s because client app sent too many columns: %u vs %u",
			getDatabase().c_str(), getTable().c_str(), colNames.entries(), validColumns.length());
	}
	
	// Check that the same column is not specified twice.
	for (uint32_t i=0; i<colNames.length()-1; ++i) {
		for (uint32_t j=i+1; j<colNames.length(); ++j) {
			if ( colNames[i] == colNames[j] ) {
				throw SparrowException::create(false, "Cannot insert data in %s.%s because client app sent duplicate column %s, at position %u and %u",
					getDatabase().c_str(), getTable().c_str(), colNames[i].c_str(), i, j);
			}
		}
	}*/

	// Check that the referenced columns are valid and build the indirection table between each referenced column
	//	and the corresponding column index in the table.
	colIds.resize(columns_.length());
	for (uint32_t i=0; i<colNames.length(); ++i) {
		const Str&	colName = colNames[i];
		uint32_t	j = 0;
		bool	dropped = false;
		for (; j<columns_.length(); ++j ) {
			const Column&	column = columns_[j];
			if ( colName == column.getName() ) {
				if ( column.isDropped() ) {
					dropped = true;
				} else if ( column.isFlagSet(COL_IP_LOOKUP) ) {
					throw SparrowException::create(false, "Cannot insert data in %s.%s because client app tries to set a value "
						"for an IP lookup column, %s.", getDatabase().c_str(), getTable().c_str(), colName.c_str());
				} else if ( column.isFlagSet(COL_AUTO_INC) ) {
					throw SparrowException::create(false, "Cannot insert data in %s.%s because client app tries to set a value "
						"for an auto-incremental column, %s.", getDatabase().c_str(), getTable().c_str(), colName.c_str());
				}
				break;
			}
		}
		if ( j == columns_.length() ) {
			throw SparrowException::create(false, "Cannot insert data in %s.%s because client app tries to set a value "
				"for a %s column, %s.", getDatabase().c_str(), getTable().c_str(), (dropped ? "dropped" : "non existing"), colName.c_str());
		}
		colIds.append( j );
	}
	assert(colIds.length() == colNames.length());

	/*// Build the list of columns that are not referenced.
	colMissing.resize(columns_.length() - colIds.length());
	for (uint i=0; i<columns_.length(); ++i) {
		const Column&	column = columns_[i];
		if (column.isDropped())
			continue;
		if (!colIds.contains(i)) {
			colMissing.append(i);
		}
	}*/
}

uint64_t Master::getTransientRecords() const {
	SPARROW_ENTER("Master::getTransientRecords");
	uint64_t records = 0;
	for (uint32_t i = 0; i < transientPartitions_.length(); ++i) {
		records += transientPartitions_[i]->getRecordsSafe();
	}
	return records;
}

uint64_t Master::getNormalizedSize() const {
	const uint64_t age = getAge();
	const double weight = age == 0 ? 0 : static_cast<double>(getMaxLifetime()) / age;
	return static_cast<uint64_t>(weight * (getDataSize() + getIndexSize()));
}

bool Master::needToPurge(const uint64_t limit, const uint64_t total, const uint64_t totalNormalized, bool& force, const bool mode) const {
	SPARROW_ENTER("Master::needToPurge");

	ReadGuard guard(getLock());

	force = false;
	if (partitions_.length() <= 1) {
		return false;
	}
	const uint64_t normalizedSize = getNormalizedSize();
	if (normalizedSize == 0) {
		return false;
	}

	uint64_t	age = 0;
	if (mode == PURGE_MODE_CONSTANTLY) {
		const uint64_t	t = Scheduler::now();
		age = getAge(t);
	} else {
		age = getAge();
	}
	if (age > getMaxLifetime()) {
		DBUG_PRINT("sparrow_purge", ("Need to purge table %s.%s: max lifetime exceeded (mode %u): %llu > %llu",
			getDatabase().c_str(), getTable().c_str(), mode, static_cast<ulonglong>(getAge()), static_cast<ulonglong>(getMaxLifetime())));
		return true;
	}
	if (total < limit) {
		return false;
	}
	const double weight = static_cast<double>(normalizedSize) / totalNormalized;
	const uint64_t size = getDataSize() + getIndexSize();
	const bool result = size > static_cast<uint64_t>(weight * limit);
	if ( result ) {
		force = true;
	} 
#ifndef NDEBUG
	if (result) {
		DBUG_PRINT("sparrow_purge", ("Need to purge table %s.%s: threshold crossed (mode %u) (%llu > %llu, total=%llu, limit=%llu, totalNormalized=%llu, normalizedSize=%llu, weight=%f)",
			getDatabase().c_str(), getTable().c_str(), mode, static_cast<ulonglong>(size), static_cast<ulonglong>(weight * limit), static_cast<ulonglong>(total),
			static_cast<ulonglong>(limit), static_cast<ulonglong>(totalNormalized), static_cast<ulonglong>(normalizedSize), weight));
	}
#endif
	return result;
}

void Master::logPartitions() const {

	DBUG_PRINT("sparrow_purge", ("Partition list for table %s.%s, ", database_.c_str(), table_.c_str()));
	IntervalTreeNode<uint64_t>* node = intervals_.getMin();
	while ( node != 0 ) {
		PersistentPartition* partition = static_cast<PersistentPartition*>(node->getInterval());

		const uint64_t low = partition->getMin();
		const uint64_t high = partition->getMax();
		const Str low_ts = Str::fromTimestamp(low);
		const Str high_ts = Str::fromTimestamp(high);
		DBUG_PRINT("sparrow_purge", ("Partition: serial %llu, data serial %llu, [low %s, high %s]", 
			static_cast<ulonglong>(partition->getSerial()), static_cast<ulonglong>(partition->getDataSerial()), low_ts.c_str(), high_ts.c_str()));

		node = intervals_.getNext( node );
	}
}

// Purges the oldest persistent main partition. The caller must acquire the write lock of this master file.
// Partitions to purge are returned in a vector of guards so they can be actually deleted outside the lock.
// Partitions currently being coalesced cannot be purged.
// Returns true if the returned partitions still contain valid data (only occurs if force is true, meaning if disk space is too low).
// False if they're beyond their max lifetime.
bool Master::purge(PersistentPartitions& partitions, const bool force, const bool mode) {
	SPARROW_ENTER("Master::purge");

	WriteGuard guard(getLock());

	IntervalTreeNode<uint64_t>* node = intervals_.getMin();
	PersistentPartition* partition = node == 0 ? 0 : static_cast<PersistentPartition*>(node->getInterval());
	if (partition == 0) {
		return false;
	}

	const PersistentPartition& mainPartition = *partition->getMainPartition();

	// If a transient partition is being flushed to that main partition, do not delete it.
	if ( isFlushing(mainPartition.getSerial()) ) {
		return false;
	}

	bool	forced = false;
	PersistentPartitions tmp;
	try {
		listPartitionsForMain(mainPartition, tmp);

		DBUG_PRINT("sparrow_purge", ("Oldest Main partition (i.e. purge candidate) includes: "));
		uint64_t		newestToDel = 0;
		for (uint32_t i = 0; i < tmp.length(); ++i) {
			const PersistentPartition*	partition = tmp[i];
			const uint64_t low = partition->getMin();
			const uint64_t high = partition->getMax();
			const Str low_ts = Str::fromTimestamp(low);
			const Str high_ts = Str::fromTimestamp(high);
			DBUG_PRINT("sparrow_purge", ("Partition: serial %llu, data serial %llu, [low %s, high %s]", 
				static_cast<ulonglong>(partition->getSerial()), static_cast<ulonglong>(partition->getDataSerial()), low_ts.c_str(), high_ts.c_str()));
			newestToDel = std::max(newestToDel, high);
		}

		// Check that destroying oldest partition will not destroy still valid data samples.
		const uint64_t high = (mode == PURGE_MODE_CONSTANTLY ? Scheduler::now() : getNewest());
		const uint64_t lifetime = getMaxLifetime();
		uint64_t	obsolescence = high - lifetime;
		if ( newestToDel >= obsolescence ) {
			if (!force) {
				const Str obsol_ts = Str::fromTimestamp(obsolescence);
				const Str high_ts = Str::fromTimestamp(high);
				const Str newestToDel_ts = Str::fromTimestamp(newestToDel);
				DBUG_PRINT("sparrow_purge", ("Should not delete oldest partition because it contains data upto %s which is still valid "
					"(data lifetime reaches down to %s, newest %s)", newestToDel_ts.c_str(), obsol_ts.c_str(), high_ts.c_str()));
				return false;
			}
			forced = true;
		}

		partitions.append(tmp);
		removePartitions(tmp);
		toDisk();
	} catch(const SparrowException& e) {
		// Ignore error.
		DBUG_PRINT("sparrow_purge", ("Exception : %s", e.getText()));
	}
	return forced;
}

uint64_t Master::listPartitionsForFilesystem(const uint32_t filesystem, PersistentPartitions& partitions,
	const uint64_t limit, const uint64_t totalNormalized) const {
	SPARROW_ENTER("Master::listPartitionsForFilesystem");
	IntervalTreeNode<uint64_t>* node = intervals_.getMin();
	while (node != 0) {
		PersistentPartition* partition = static_cast<PersistentPartition*>(node->getInterval());
		if (coalescedSerials_.contains(partition->getSerial())) {
			break;
		}
		if (partition->isMain() ) {
			try {
				listPartitionsForMain(*partition, partitions);
			} catch(const SparrowException& e) {
				break;
			}
			if ( partition->getFilesystem() == filesystem ) {
				const uint64_t normalizedSize = getNormalizedSize();
				uint64_t delta = 0;
				if (normalizedSize != 0 && totalNormalized != 0) {
					const uint64_t size = getDataSize() + getIndexSize();
					const double weight = static_cast<double>(normalizedSize) / totalNormalized;
					const uint64_t weightedSize = static_cast<uint64_t>(weight * limit);
					delta = weightedSize > size ? weightedSize - size : 0;
				}
	#ifndef NDEBUG
				const Str ssize(Str::fromSize(partition->getDataSize() + partition->getIndexSize()));
				DBUG_PRINT("sparrow_purge", ("Table %s.%s: got %u partitions for file system %u, table size=%s, delta=%llu", database_.c_str(), table_.c_str(),
					partitions.length(), filesystem, ssize.c_str(), static_cast<ulonglong>(delta)));
	#endif
				return delta;
			}
		}
		node = intervals_.getNext(node);
	}
	partitions.clear();
	return 0;
}

bool Master::purgePartitionsForFilesystem(const PersistentPartitions& partitions) {
	SPARROW_ENTER("Master::purgePartitionsForFilesystem");
	for (uint32_t i = 0; i < partitions.length(); ++i) {
		PersistentPartition* partition = partitions[i].get();
		if (coalescedSerials_.contains(partition->getSerial())) {
			return false;
		}
	}
	removePartitions(partitions);
	toDisk();
	return true;
}

TransientPartitions Master::setDnsConfiguration(const DnsConfiguration& configuration) {
	SPARROW_ENTER("Master::setDnsConfiguration");
	if (dnsConfiguration_ != 0 && dnsConfiguration_->isStarted()) {
		DnsConfiguration::release(dnsConfiguration_.get());
	}
	dnsConfiguration_ = 0;
	TransientPartitions partitions;
	if (!configuration.isEmpty()) {
		dnsConfiguration_ = DnsConfiguration::acquire(configuration);
#ifndef NDEBUG
		const Str s(dnsConfiguration_->print());
		DBUG_PRINT("sparrow_dns", ("Set DNS configuration on table %s.%s: %s", getDatabase().c_str(), getTable().c_str(), s.c_str()));
#endif

		// Return transient partitions to update.
		uint32_t i = transientPartitions_.length();
		if (i > 0) {
			do {
				TransientPartition* partition = transientPartitions_[--i];
				partitions.append(TransientPartitionGuard(partition));
			} while (i > 0);
		}
	}
	return partitions;
}

void Master::repair() {
	// Check that persisted partitions are not corrupted.
	//	Currently, we only check that the .spd is present. If not, the reference to the partition is removed from the master file. 
	PersistentPartitions	toremove;
	{
		WriteGuard guard(getLock());
		spw_print_information("Starting repair on %s.%s", database_.c_str(), table_.c_str());
		for (uint32_t i = 0; i < partitions_.length(); ++i) {
			Partition* p = partitions_[i];
			if ( p->isTransient() || p->isTemporary() ) {
				continue;
			}
			PersistentPartitionGuard	pp(static_cast<PersistentPartition*>(p));
			char filename[FN_REFLEN];
			pp->getFileName(DATA_FILE, filename);
			if ( !FileUtil::doesFileExist( filename ) ) {
				toremove.append(pp);
			} 
		}
		if ( toremove.isEmpty() ) {
			spw_print_information("Finished repair on %s.%s. Everything Ok.", database_.c_str(), table_.c_str());
		} else {
			for ( uint i=0; i<toremove.length(); ++i ) {
				char filename[FN_REFLEN];
				toremove[i]->getFileName(DATA_FILE, filename);
				spw_print_information("Partition %s does not exists. Removing.", filename);
			}
			removePartitions( toremove );
			toDisk();
			spw_print_information("Finished repair on %s.%s: removed %d referenced to non-existing partitions.", database_.c_str(), table_.c_str(), toremove.length());
		}
	}
}

// Deserialization.
ByteBuffer& operator >> (ByteBuffer& buffer, Master& master) {
	uint32_t version;
	buffer >> version;
	buffer.setVersion(version);
	DnsConfiguration dnsConfiguration;
	buffer >> master.columns_ >> master.indexes_ >> master.indexMappings_
		>> master.foreignKeys_ >> dnsConfiguration >> master.maxLifetime_;
	if (version < 3) {
		master.maxLifetime_ = 0;
	}
	if (version < 4) {
		master.aggregationPeriod_ = 0;
	} else {
		buffer >> master.aggregationPeriod_;
	}
	if (version >= 16) {
		buffer >> master.defaultWhere_;
	} else {
		master.defaultWhere_ = 86400000L;
	}
	if (version >= 21) {
		buffer >> master.stringOptimization_;
	} else {
		master.stringOptimization_ = 0;
	}
	if (version < 5) {
		master.autoInc_ = 1;
	} else {
		buffer >> master.autoInc_;
	}
	if (version < 6) {
		// Set column names and charsets from table definition in thread-local storage.
		TABLE_SHARE* s = Master::threadKey_;
		if (s != 0) {
			Columns& columns = master.columns_;
			for (uint32_t i = 0; i < columns.length(); ++i) {
				const Column& column = columns[i];
				Column	col(s->field[i]->field_name, column.getType(),
					column.getFlags(), column.getInfo(), s->field[i]->charset()->csname, Str());
				columns[i] = col;
				//columns[i] = Column(s->field[i]->field_name, column.getType(),
				//	column.getFlags(), column.getInfo(), s->field[i]->charset()->csname, Str());
			}
		}
	}
	buffer >> (uint64_t&)master.serial_ >> master.timeCreated_
		>> master.timeUpdated_ >> master.dataSize_ >> master.indexSize_ >> master.records_;
	if (version >= 6 && version < 9) {
		SYSvector<uint32_t> droppedIndexes;
		SYSvector<uint32_t> addedIndexes;
		buffer >> droppedIndexes >> addedIndexes;
		uint32_t serial = 1;
		for (uint32_t i = 0; i < droppedIndexes.length(); ++i) {
			master.indexAlterations_.append(Alteration(ALT_DROP_INDEX, serial++, droppedIndexes[i]));
		}
		for (uint32_t i = 0; i < addedIndexes.length(); ++i) {
			master.indexAlterations_.append(Alteration(ALT_ADD_INDEX, serial++, addedIndexes[i]));
		}
		master.indexAlterSerial_ = serial - 1;
	}
	if (version >= 9) {
		buffer >> master.indexAlterSerial_;
		if (version >= 15) {
			buffer >> master.indexAlterElapsed_;
		}
		buffer >> master.indexAlterations_;
	}
	if (version >= 14) {
		buffer >> master.coalescingPeriod_;
	}
	master.computeMappedColumnIds();
	uint32_t length;
	buffer >> length;
	for (uint32_t i = 0; i < length; ++i) {
		PersistentPartition* partition = new PersistentPartition(&master);
		buffer >> *partition;

		if (partition->makeChecks() == false) {
			spw_print_error("Ignoring invalid partition %s.%s.%llu.", master.getDatabase().c_str(), master.getTable().c_str(), static_cast<ulonglong>(partition->getSerial()));
			continue;
		}

		// Discard empty and duplicate partitions, just in case.
		if ((partition->getRecords() > 0 || partition->getDataRecords() > 0) && !master.partitions_.contains(partition)) {
			if (partition->getSerial() >= master.serial_) {
				// Adjust master serial in case of inconsistency.
				master.serial_ = partition->getSerial() + 1;
			}
			master.partitions_.append(partition);
			master.intervals_.insert(partition);
		} else if (!master.partitions_.isEmpty()) {
			// Delete partition only if the master file references at least one partition,
			// otherwise the master file is deleted! Better have a small memory leak.
			partition->releaseRef();
			delete partition;
		}
	}

	// Setup pointers to main partitions and compute stats.
	uint64_t dataSize = 0;
	uint64_t indexSize = 0;
	uint64_t records = 0;
	Partitions& partitions = master.partitions_;
	uint32_t minSerial = UINT_MAX;
	for (uint32_t i = 0; i < partitions.length(); ++i) {
		PersistentPartition* partition = static_cast<PersistentPartition*>(partitions[i]);
		PersistentPartition* mainPartition = partition;
		if (!partition->isMain()) {
			mainPartition = static_cast<PersistentPartition*>(master.getPartitionNoLock(partition->getDataSerial()).get());
		}
		if (mainPartition == 0) {
			// Just in case of bug.
			partitions.removeAt(i--);	// Wrapping.
			master.intervals_.remove(*partition);

			// Delete partition only if the master file references at least one partition,
			// otherwise the master file is deleted! Better have a small memory leak.
			if (!partitions.isEmpty()) {
				partition->releaseRef();
				delete partition;
			}
		} else {
			minSerial = std::min(minSerial, partition->getIndexAlterSerial());
			dataSize += partition->getDataSize();
			indexSize += partition->getIndexSize();
			records += partition->getRecords();
			partition->setMainPartition(mainPartition);
			if (!partition->isMain()) {
				partition->getMainPartition()->addChildPartition(partition);
			}

		}
	}

	assert(master.partitions_.isSorted());
	master.dataSize_ = dataSize;
	master.indexSize_ = indexSize;
	master.records_ = records;

	// Remove obsolete alterations.
	for (uint32_t i = 0; i < master.indexAlterations_.length(); ++i) {
		const Alteration& alteration = master.indexAlterations_[i];
		if (alteration.getSerial() <= minSerial) {
			master.indexAlterations_.removeAt(i--);	// Wrapping.
		}
	}

	// Start DNS configuration.
	master.setDnsConfiguration(dnsConfiguration);
	return buffer;
}

// Serialization
ByteBuffer& operator << (ByteBuffer& buffer, const Master& master) {
	buffer << master.version_;
	buffer << master.columns_ << master.indexes_ << master.indexMappings_
		<< master.foreignKeys_;
	if (master.dnsConfiguration_ == 0) {
		buffer << DnsConfiguration();
	} else {
		buffer << *master.dnsConfiguration_.get();
	}
	buffer << master.maxLifetime_ << master.aggregationPeriod_ << master.defaultWhere_
		<< master.stringOptimization_ << master.autoInc_ << master.serial_ << master.timeCreated_
		<< master.timeUpdated_ << master.dataSize_ << master.indexSize_ << master.records_
		<< master.indexAlterSerial_ << master.indexAlterElapsed_ << master.indexAlterations_
		<< master.coalescingPeriod_;

	// Do not write transient and temporary partitions.
	const Partitions& partitions = master.partitions_;
	const uint32_t length = partitions.length();
	uint32_t count = 0;
	uint32_t i;
	for (i = 0; i < length; ++i) {
		const Partition& partition = *partitions[i];
		if (!partition.isTransient() && !partition.isTemporary()) {
			count++;
		}
	}
	buffer << count;
	for (i = 0; i < length; ++i) {
		const Partition& partition = *partitions[i];
		if (!partition.isTransient() && !partition.isTemporary()) {
			assert(master.serial_ > partition.getSerial());
			buffer << static_cast<const PersistentPartition&>(partition);
		}
	}
	return buffer;
}

void Master::retrieve(ByteBuffer& buffer) const {
	SPARROW_ENTER("Master::retrieve");
	FileId key;
	getMasterFileName(key.getName());
	ReadGuard guard(getLock());

	// Send the master file size so it can be recreated identically by the caller process
	// (remember written files are padded with extra bytes to have a length multiple
	// of sector size).
	FileCacheGuard fileGuard(FileCache::get(), 0, key, 0, true);
	const FileHandle& handle = fileGuard.get()->getValue();
	buffer << handle.getSize();
	buffer << *this;
}

bool Master::startIndexAlter(const bool check) _THROW_(SparrowException) {
	SPARROW_ENTER("Master::startIndexAlter");
	WriteGuard guard(getLock());
	DBUG_PRINT("sparrow_alter", ("startIndexAlter on table %s.%s: check %d, %d, %llu", 
		getDatabase().c_str(), getTable().c_str(), check, indexAlterSerial_, static_cast<ulonglong>(indexAlterStarted_)));
	if (!check && (indexAlterSerial_ == 0 || indexAlterStarted_ != 0)) {
		// Nothing to do or alterations already started.
		return false;
	}
	if (partitions_.isEmpty()) {
		// No partition.
		return false;
	}
	DBUG_PRINT("sparrow_alter", ("Check if index alteration must be started for %s.%s", getDatabase().c_str(), getTable().c_str()));
	uint32_t i = partitions_.length() - 1;
	for (;;) {
		Partition* partition = partitions_[i];

		// Alter persistent partitions with pending alterations and which are not being coalesced.
		if (!partition->isTemporary() && partition->isIndexAlterable() && !partition->isReady() && !coalescedSerials_.contains(partition->getSerial())) {
			PersistentPartition* persistentPartition = static_cast<PersistentPartition*>(partition);
			if (indexAlterStarted_ == 0) {
				indexAlterStarted_ = Scheduler::now();
				AlterWorker::initialize();
#ifndef NDEBUG
				const Str stimestamp(Str::fromTimestamp(indexAlterStarted_));
				DBUG_PRINT("sparrow_alter", ("Index alteration started at %s for %s.%s", stimestamp.c_str(), getDatabase().c_str(), getTable().c_str()));
#endif
			}
			DBUG_PRINT("sparrow_alter", ("Send index alteration task for partition %s.%s.%llu (most recent)", getDatabase().c_str(), getTable().c_str(), 
				static_cast<ulonglong>(persistentPartition->getSerial())));
			Scheduler::addTask(new MainAlterTask(persistentPartition));
			return true;
		}
		if (i-- == 0) {
			// Scanned all partitions.
			if (check) {
				// This was a check after an alteration sequence: reset alter status.
				indexAlterStarted_ = 0;
				indexAlterElapsed_ = 0;
			}
			break;
		}
	}
	DBUG_PRINT("sparrow_alter", ("No index alteration necessary for %s.%s", getDatabase().c_str(), getTable().c_str()));
	return false;
}

void Master::indexAlterationDone() {
	const uint64_t now = Scheduler::now();
	if (now > indexAlterStarted_) {
		indexAlterElapsed_ += now - indexAlterStarted_;
	}
	indexAlterStarted_ = now;
}

bool Master::getIndexAlterStatus(uint64_t& elapsed, uint64_t& left, double& percentage) const {
	SPARROW_ENTER("Master::reportAlterStatus");
	if (indexAlterStarted_ == 0) {
		return false;
	}
	uint32_t total = 0;
	uint32_t ready = 0;
	for (uint32_t i = 0; i < partitions_.length(); ++i) {
		const Partition* partition = partitions_[i];
		if (!partition->isTemporary() && partition->isIndexAlterable()) {
			total++;
			const PersistentPartition* persistentPartition = static_cast<const PersistentPartition*>(partition);
			if (persistentPartition->getIndexAlterSerial() == getIndexAlterSerial()) {
				ready++;
			}
		}
	}
	if (ready == total || getIndexAlterations().isEmpty()) {
		return false;
	}
	const double achieved = static_cast<double>(ready) / total;
	elapsed = indexAlterElapsed_;
	if (achieved > 0) {
		left = static_cast<uint64_t>(elapsed / achieved * (1.0 - achieved));
		if (left > 15 * 86400000) {	// Remove meaningless value (> 15 days).
			left = 0;
		}
	} else {
		left = 0;
	}
	percentage = achieved * 100;
	return true;
}

bool Master::getIndexAlterStatus(SYSslist<Str>& strings) const {
	SPARROW_ENTER("Master::reportAlterStatus");
	uint64_t elapsed;
	uint64_t left;
	double percentage;
	if (getIndexAlterStatus(elapsed, left, percentage)) {
		char tmp[1024];
		snprintf(tmp, sizeof(tmp), "%s.%s", getDatabase().c_str(), getTable().c_str());
		strings.append(Str(tmp));
		strings.append(indexAlterations_.first().getDescription(*this));
		strings.append(Str::fromDuration(elapsed));
		if (left > 0) {
			strings.append(Str::fromDuration(left));
		} else {
			strings.append(Str("N/A"));
		}
		snprintf(tmp, sizeof(tmp), "%.1f%%", percentage);
		strings.append(Str(tmp));
		for (uint32_t i = 1; i < indexAlterations_.length(); ++i) {
			strings.append(Str());
			strings.append(indexAlterations_[i].getDescription(*this));
			strings.append(Str());
			strings.append(Str());
			strings.append(Str());
		}
		return true;
	} else {
		return false;
	}
}

void Master::dropColumn(const char* name) _THROW_(SparrowException) {
	SPARROW_ENTER("Master::dropColumn");
	const Str n(name);
	const uint32_t pos = getColumn(n);
	assert(pos != 0 && pos != SYS_NPOS);
	columns_[pos].drop(getColumnAlterSerial() + 1);
	DBUG_PRINT("sparrow_alter", ("Dropping column %s from table %s.%s", name, getDatabase().c_str(), getTable().c_str()));
	computeMappedColumnIds();
}

void Master::addColumn(const char* after, Column& newColumn) _THROW_(SparrowException) {
	SPARROW_ENTER("Master::addColumn");
	assert(after != first_keyword);
	const Str name(after);
	uint32_t pos = getColumn(name);
	assert(pos != SYS_NPOS);
	newColumn.setSerial(getColumnAlterSerial() + 1);
	pos++;
	const bool last = pos == columns_.length();
	columns_.insertAt(pos, newColumn);
	DBUG_PRINT("sparrow_alter", ("Adding column %s to table %s.%s at position %u, after column %s", newColumn.getName().c_str(),
		getDatabase().c_str(), getTable().c_str(), pos, after));
	if (!last) {
		// Update column ids in existing indexes.
		DBUG_PRINT("sparrow_alter", ("Update column ids in existing indexes"));
		Indexes newIndexes(indexes_.length());
		for (uint32_t i = 0; i < indexes_.length(); ++i) {
			const Index& index = indexes_[i];
			const ColumnIds& ids = index.getColumnIds();
			ColumnIds newIds(ids.length());
			for (uint32_t j = 0; j < ids.length(); ++j) {
				const uint32_t id = ids[j];
				newIds.append(id >= pos ? id + 1 : id);
			}
			Index newIndex(index.getName().c_str(), newIds, index.isUnique());
			if (index.isDropped()) {
				newIndex.drop();
			}
			newIndexes.append(newIndex);
		}
		indexes_ = newIndexes;
	}
	computeMappedColumnIds();
}

void Master::renameColumn(const char* from, const char* to) _THROW_(SparrowException) {
	SPARROW_ENTER("Master::renameColumn");
	const Str name(from);
	const uint32_t pos = getColumn(name);
	assert(pos != SYS_NPOS);
	Column& column = columns_[pos];
	column = Column(to, column.getType(), column.getFlags(), column.getInfo(),
		column.getCharset().c_str(), column.getDefaultValue());
}

PartitionGuard Master::getPartitionNoLock(const uint64_t serial) const {
	SPARROW_ENTER("Master::getPartitionNoLock");
	const PartitionKey key(serial);
	return PartitionGuard(partitions_.find(&key));
}

PartitionGuard Master::getPartition(const uint64_t serial) const {
	SPARROW_ENTER("Master::getPartition");
	ReadGuard guard(getLock());
	return getPartitionNoLock(serial);
}

uint64_t Master::getOldest(const bool persistentOnly /* = false */) const {
	uint64_t oldest = 0;
	intervals_.getMin(oldest);
	if (!persistentOnly) {
		for (uint32_t i = 0; i < transientPartitions_.length(); ++i) {
			TransientPartition* partition = transientPartitions_[i];
			TimePeriod period{partition->getPeriod()};
			const uint64_t* plow = period.getLow();
			if (plow != 0) {
				oldest = oldest == 0 ? *plow : std::min(oldest, *plow);
			}
		}
	}
	return oldest;
}

uint64_t Master::getNewest(const bool persistentOnly /* = false */) const {
	uint64_t newest = 0;
	intervals_.getMax(newest);
	if (!persistentOnly) {
		for (uint32_t i = 0; i < transientPartitions_.length(); ++i) {
			TransientPartition* partition = transientPartitions_[i];
			TimePeriod period{partition->getPeriod()};
			const uint64_t* phigh = period.getUp();
			if (phigh != 0) {
				newest = newest == 0 ? *phigh : std::max(newest, *phigh);
			}
		}
	}
	return newest;
}

PersistentPartition* Master::newPersistentPartition(const uint32_t version, const uint64_t dataSerial, const uint32_t filesystem,
	const TimePeriod& period, const uint32_t records, const uint32_t indexAlterSerial, const uint32_t columnAlterSerial,
	const uint64_t dataRecords, const uint64_t recordOffset, const ColumnIds& emptyColumnIds) {
	SPARROW_ENTER("Master::newPersistentPartition");
	const uint64_t serial = serial_++;
	PersistentPartition* mainPartition = 0;
	if (dataSerial != SAME_AS_SERIAL) {
		mainPartition = static_cast<PersistentPartition*>(getPartitionNoLock(dataSerial).get());
	}
	return new PersistentPartition(version, this, serial, mainPartition, filesystem,
		indexAlterSerial, columnAlterSerial, period, records, 0, 0, dataRecords, recordOffset, emptyColumnIds);
}

PersistentPartition* Master::newTemporaryPersistentPartition(const PersistentPartition& partition, const uint32_t records, const uint64_t recordOffset) {
	SPARROW_ENTER("Master::newTemporaryPersistentPartition");
	const uint64_t serial = serial_++;
	PersistentPartition* mainPartition = partition.getMainPartition();
	const TimePeriod temporaryPeriod(partition.getPeriod().getMin(), 0);
	PersistentPartition* temporary = new PersistentPartition(partition.getVersion(), this, serial, mainPartition, FileUtil::chooseFilesystem(true),
		partition.getIndexAlterSerial(), partition.getColumnAlterSerial(), temporaryPeriod, records, 0, 0, 0, recordOffset, partition.getSkippedColumns());
	temporary->acquireRef();
	WriteGuard guard(getLock());
	assert(!partitions_.contains(temporary));
	partitions_.insert(temporary);
	intervals_.insert(temporary);
	return temporary;
}

// Removes the transient partition from the list of partitions and add the new persistent partition, newPartition.
//	Also add the main partition if it's just been created and update the time intervals.
void Master::mutatePartition(TransientPartition* transientPartition, PersistentPartition* mainPartition, PersistentPartition* newPartition) {
	partitions_.remove(transientPartition);
	transientPartitions_.remove(transientPartition);
	transientPartition->releaseRef();
	if (mainPartition != 0) {
		assert(newPartition != 0);
		if (partitions_.contains(mainPartition)) {
			// Extend period of main partition.
			intervals_.remove(*mainPartition);
			mainPartition->extendPeriod(newPartition->getPeriod());
			intervals_.insert(mainPartition);
		} else {
			assert(mainPartition->getRecords() == 0 && mainPartition->getIndexSize() == 0);
			partitions_.insert(mainPartition);
			intervals_.insert(mainPartition);
		}
		assert(newPartition->getDataSize() == 0);
		newPartition->acquireRef();
		assert(!partitions_.contains(newPartition));
		partitions_.insert(newPartition);
		intervals_.insert(newPartition);
		mainPartition->addChildPartition(newPartition);
		setDataSize(getDataSize() + transientPartition->getDataSize());
		setIndexSize(getIndexSize() + transientPartition->getIndexSize());
		setRecords(getRecords() + transientPartition->getRecords());
		const uint64_t coalescingPeriod = getCoalescingPeriod();
		if (coalescingPeriod != 0) {
			uint64_t limit = transientPartition->getPeriodNoLock().getMin();
			limit -= limit % coalescingPeriod;
			coalescingTimestamp_ = std::min(limit, coalescingTimestamp_);
		}
	}
}

bool Master::hasBuiltInTimestampIndex() const {
	if (!indexes_.isEmpty()) {
		const Index& index = indexes_[0];
		if (!index.isDropped() && index.getColumnIds().length() == 1 && index.getColumnIds()[0] == 0) {
			for (uint32_t i = 0; i < partitions_.length(); ++i) {
				const Partition& partition = *partitions_[i];
				if (!partition.isTransient() && !partition.isTemporary() && static_cast<const PersistentPartition&>(partition).getVersion() == 0) {
					return true;
				}
			}
		}
	}
	return false;
}

// Find persistent partitions within the given period. The upper bound of the given period is smaller than the newest timestamp.
void Master::getCoalescingCandidates(const TimePeriod& period, CoalescingCandidates& candidates, const bool fake) const {
	SPARROW_ENTER("Master::getCoalescingCandidates");
#ifndef NDEBUG
	const Str speriod(Str::fromTimePeriod(period));
	DBUG_PRINT("sparrow_coalescing", ("Search coalescing candidates on %s.%s for period %s", getDatabase().c_str(),
		getTable().c_str(), speriod.c_str()));
#endif
	candidates.clear();
	Intervals intervals;
	intervals_.findOverlaps(period, intervals);
	const uint64_t length = period.getLength();
	for (uint32_t i = 0; i < intervals.length(); ++i) {
		PersistentPartition* partition = static_cast<PersistentPartition*>(intervals[i]);
		if (partition->isTemporary() || partition->isTransient()) {
			continue;
		}
		if (!fake && !partition->isReady()) {
			// We are interested only in persistent partitions with no alteration on going.
			continue;
		}
		if (!partition->getPeriod().intersects(period)) {
			// Reject false positives.
			continue;
		}
		const TimePeriod pperiod = partition->getPeriod();
		if (fake || !coalescedSerials_.contains(partition->getSerial())) {
			const bool isAppend = partition->getVersion() >= PersistentPartition::appendVersion_;
			PersistentPartitions partitions;
			if (isAppend && partition->isMain()) {
				// Get all child partitions.
				const ChildPartitions&	childPartitions = partition->getChildPartitions();
				for (uint32_t j = 0; j < childPartitions.length(); ++j) {
					PersistentPartition* pp = static_cast<PersistentPartition*>(childPartitions[j]);
					if (fake || !coalescedSerials_.contains(pp->getSerial())) {
						partitions.append(PersistentPartitionGuard(pp));
					}
				}
			} else if (!isAppend && pperiod.getLength() <= length) {
				partitions.append(PersistentPartitionGuard(partition));
			}
			for (uint32_t j = 0; j < partitions.length(); ++j) {
				const PersistentPartitionGuard& p = partitions[j];
				const CoalescingInfo info = p->getCoalescingInfo();
				CoalescablePartitions key(info);
				CoalescablePartitions* coalescablePartitions = candidates.find(key);
				if (coalescablePartitions == 0) {
					coalescablePartitions = candidates.insertAndReturn(key);
				}
				PersistentPartitions& ppartitions = coalescablePartitions->getPartitions();
				ppartitions.insert(p);
			}
		}
	}

	// Post-processing: coalesce a single partition only if it resides on the coalescing file system.
	CoalescingCandidatesIterator iterator(candidates);
	while (++iterator) {
		const CoalescablePartitions& cp = iterator.key();
		const PersistentPartitions& partitions = cp.getPartitions();
		if (partitions.length() == 1 && partitions.first()->getFilesystem() < COALESCING_FILESYSTEM) {
			candidates.remove(cp);
			iterator.reset();
		}
	}
}

// Triggers partition coalescing, if there are candidate partitions.
void Master::coalesce() {
	SPARROW_ENTER("Master::coalesce");
	if (!sparrow_coalescing || CoalescingControlTaskPerDB::isDisabled(database_)) {
		return;
	}
	WriteGuard guard(getLock());
	if (hasBuiltInTimestampIndex()) {
		return;
	}
	const uint64_t coalescingPeriod = getCoalescingPeriod();
	if (coalescingPeriod == 0) {
		return;
	}
	const uint64_t newest = getNewest(true);
	if (newest == 0) {
		return;
	}
	const uint64_t oldest = getOldest(true);
	const uint64_t limit = std::max(coalescingTimestamp_, oldest - (oldest % coalescingPeriod));
	if (limit == 0) {
		return;
	}
#ifndef NDEBUG
	const Str soldest(Str::fromTimestamp(oldest));
	const Str snewest(Str::fromTimestamp(newest));
	const Str stimestamp = coalescingTimestamp_ == 0 ? Str("N/A") : Str::fromTimestamp(coalescingTimestamp_);
	const Str slimit(Str::fromTimestamp(limit));
	DBUG_PRINT("sparrow_coalescing", ("Coalescing for %s.%s: oldest = %s, newest = %s, coalescing timestamp = %s, limit = %s, coalescedSerials_ contains %u", getDatabase().c_str(), getTable().c_str(),
		soldest.c_str(), snewest.c_str(), stimestamp.c_str(), slimit.c_str(), coalescedSerials_.entries()));
#endif
	uint64_t t = newest - (newest % coalescingPeriod);
	uint64_t coalescingTimestamp = 0;
	while (t >= limit) {
		const uint64_t low = t - coalescingPeriod;
		const TimePeriod period(&low, &t, true, false);
		CoalescingCandidates candidates(16);
		getCoalescingCandidates(period, candidates, false);
		if (!candidates.isEmpty()) {
#ifndef NDEBUG
			const Str speriod(Str::fromTimePeriod(period));
			DBUG_PRINT("sparrow_coalescing", ("Coalescing %u partition sets from %s.%s for period %s", candidates.entries(),
				getDatabase().c_str(), getTable().c_str(), speriod.c_str()));
#endif
			const Indexes& indexes = getIndexes();
			IndexIds indexIds(indexes.length());
			for (uint32_t i = 0; i < indexes.length(); ++i) {
				if (!indexes[i].isDropped()) {
					indexIds.append(i);
				}
			}
			CoalescingCandidatesIterator iterator(candidates);
			bool sent = false;
			while (++iterator) {
				CoalescablePartitions& cp = iterator.key();
				PersistentPartitions& partitions = cp.getPartitions();
				const uint32_t n = partitions.length();
				for (uint32_t i = 0; i < n; ++i) {
					coalescedSerials_.insert(partitions[i]->getSerial());
				}
				const CoalescingInfo& info = cp.getInfo();
				const uint32_t version = info.getFirst().getFirst();
				if (version < PersistentPartition::appendVersion_) {
					// Version < PersistentPartition::appendVersion_:
					// Coalesce data and index files.
					DBUG_PRINT("sparrow_coalescing", ("Coalescing %u partitions from %s.%s (version=%u, column alter serial=%u)", partitions.entries(),
						getDatabase().c_str(), getTable().c_str(), version, info.getFirst().getSecond()));
#ifndef NDEBUG
					for (uint32_t i = 0; i < n; ++i) {
						const PersistentPartitionGuard& partition = partitions[i];
						const Str speriod(Str::fromTimePeriod(partition->getPeriod()));
						DBUG_PRINT("sparrow_coalescing", ("Partition %s.%s.%llu: %s (filesystem %u)", getDatabase().c_str(), getTable().c_str(), 
							static_cast<ulonglong>(partition->getSerial()), speriod.c_str(), partition->getFilesystem()));
					}
#endif
					Scheduler::addTask(new CoalescingMainTask(this, partitions, indexIds));
					sent = true;
				} else {
					// Version >= PersistentPartition::appendVersion_:
					// The data file is already coalesced (data are appended to it).
					// Coalesce only index files.
					DBUG_PRINT("sparrow_coalescing", ("Coalescing %u partitions from %s.%s (version=%u, column alter serial=%u, data serial=%llu)",
						partitions.entries(), getDatabase().c_str(), getTable().c_str(), version, info.getFirst().getSecond(), static_cast<ulonglong>(info.getSecond())));
					PersistentPartition& firstPartition = *partitions[0];
					assert(firstPartition.getVersion() >= PersistentPartition::appendVersion_);
					uint64_t minTimestamp = ULLONG_MAX;
					uint64_t maxTimestamp = 0;
					uint32_t records = 0;
					for (uint32_t i = 0; i < n; ++i) {
						const PersistentPartitionGuard& partition = partitions[i];
#ifndef NDEBUG
						const Str speriod(Str::fromTimePeriod(partition->getPeriod()));
						DBUG_PRINT("sparrow_coalescing", ("Partition %s.%s.%llu: %s (filesystem %u)", getDatabase().c_str(), getTable().c_str(), static_cast<ulonglong>(partition->getSerial()),
							speriod.c_str(), partition->getFilesystem()));
#endif
						assert(!partition->isMain());
						records += partition->getRecords();
						const TimePeriod& period = partition->getPeriod();
						minTimestamp = std::min(minTimestamp, period.getMin());
						maxTimestamp = std::max(maxTimestamp, period.getMax());
					}
					PersistentPartition* coalescedPartition = newPersistentPartition(firstPartition.getVersion(), firstPartition.getDataSerial(),
							FileUtil::chooseFilesystem(false), TimePeriod(minTimestamp, maxTimestamp), records, firstPartition.getIndexAlterSerial(),
							firstPartition.getColumnAlterSerial(), 0, 0, firstPartition.getSkippedColumns());
					Coalescing::triggerIndexCoalescing(this, partitions, partitions, coalescedPartition, indexIds);
					sent = true;
				}
			}
			if (sent) {
				coalescingTimestamp = 0;
				break;
			}
		}
		t -= coalescingPeriod;
		coalescingTimestamp = std::max(t, coalescingTimestamp);
	}
	if (coalescingTimestamp != 0) {
		coalescingTimestamp_ = coalescingTimestamp;
	}
}

void Master::registerCoalescingTask(CoalescingIndexTask* task) {
	indexCoalescingTask_.append(task);
}

void Master::registerCoalescingTask(CoalescingMainTask* task) {
	mainCoalescingTask_.append(task);
}

void Master::unregisterCoalescingTask(CoalescingIndexTask* task) {
	[[maybe_unused]] bool	found = indexCoalescingTask_.remove(task);
	assert(found == true);
}

void Master::unregisterCoalescingTask(CoalescingMainTask* task) {
	[[maybe_unused]] bool	found = mainCoalescingTask_.remove(task);
	assert(found == true);
}

void Master::stopCoalescingTasks() {
	ReadGuard guard(getLock());
	for (uint i=0; i<indexCoalescingTask_.entries(); ++i) {
		CoalescingIndexTask*	task = indexCoalescingTask_[i];
		task->stop();
	}
	for (uint i=0; i<mainCoalescingTask_.entries(); ++i) {
		CoalescingMainTask*	task = mainCoalescingTask_[i];
		task->stop();
	}
}


void Master::coalescingFailed(const PersistentPartitions& partitions) _THROW_(SparrowException)
{
	SPARROW_ENTER("Master::coalescingFailed");
	{
		WriteGuard guard(getLock());
		for (uint32_t i = 0; i < partitions.length(); ++i) {
			coalescedSerials_.remove( partitions[i]->getSerial() );
		}
	}
}

void Master::coalescingDone(PersistentPartition* coalescedPartition, const PersistentPartitions& partitions) _THROW_(SparrowException) {
	SPARROW_ENTER("Master::coalescingDone");
	bool temporary = false;
	bool alter = false;
	do 
	{
		{
			WriteGuard guard(getLock());

			// First check there are no pending reference to the partitions we are going to remove. 
			//	The Coalescing task holds 2 references to each partition. There is another one in Master::partitions_. 
			//	So if a partition has a reference count greater than 3 some other module is still using that partition 
			//	and we have to wait.
			bool	can_remove = true;
			for (uint32_t i = 0; i < partitions.length(); ++i) {
				if ( partitions[i]->refs() > 3 ) {
					can_remove = false;
					break;
				}
			}

			if ( can_remove )
			{
				for (uint32_t i = 0; i < partitions.length(); ++i) {
					coalescedSerials_.remove( partitions[i]->getSerial() );
				}
				temporary = removePartitions(partitions);
				if (!temporary) {
					if (coalescedPartition->getRecords() > 0) {
						if (getIndexAlterSerial() > coalescedPartition->getIndexAlterSerial()) {
							alter = true;
						}
						setDataSize(getDataSize() + coalescedPartition->getDataSize());
						setIndexSize(getIndexSize() + coalescedPartition->getIndexSize());
						setRecords(getRecords() + coalescedPartition->getRecords());
						coalescedPartition->acquireRef();
						assert(!partitions_.contains(coalescedPartition));
						partitions_.insert(coalescedPartition);
						intervals_.insert(coalescedPartition);
						if (!coalescedPartition->isMain()) {
							coalescedPartition->getMainPartition()->addChildPartition(coalescedPartition);
						}
					}
				}
				toDisk();
				break;
			}
		}
		my_sleep(100000);		// 100ms
	} while (true);
	if (alter) {
		DBUG_PRINT("sparrow_alter", ("Master::coalescingDone( %s.%s )", getDatabase().c_str(), getTable().c_str()));
		startIndexAlter(false);
	}
	if (!temporary) {
		// Try to coalesce older partitions, if any.
		coalesce();
	}
}

// Counts partitions smaller than the coalescing period, and returns the related percentage.
// Ignore partitions in the current (not completed) coalescing period.
// The return value is negative if there is nothing to coalesce and there is no coalesced partition.
double Master::getCoalescingPercentage() const {
	SPARROW_ENTER("Master::getCoalescingPercentage");
	const uint64_t coalescingPeriod = getCoalescingPeriod();
	if (!sparrow_coalescing || coalescingPeriod == 0 || CoalescingControlTaskPerDB::isDisabled(database_)) {
		return -1;
	}
	const uint64_t newest = getNewest(true);
	if (newest == 0) {
		return -1;
	}
	const uint64_t oldest = getOldest(true);
	const uint64_t limit = oldest - (oldest % coalescingPeriod);
	if ( limit == 0 ) {
		return -1;
	}
	const uint64_t start = newest - (newest % coalescingPeriod);
	uint32_t periods = 0;
	uint32_t alreadyCoalescedPeriods = 0;
	uint64_t t = start;
	while (t >= limit) {
		const uint64_t low = t - coalescingPeriod;
		const TimePeriod period(&low, &t, true, false);
		++periods;
		CoalescingCandidates candidates(16);
		getCoalescingCandidates(period, candidates, true);
		if (candidates.isEmpty()) {
			++alreadyCoalescedPeriods;
		}
		t -= coalescingPeriod;
	}
#ifndef NDEBUG
	const Str speriod(Str::fromTimePeriod(TimePeriod(limit - coalescingPeriod, start)));
	DBUG_PRINT("sparrow_coalescing", ("Table %s.%s: %u coalesced periods vs %u total for %s", getDatabase().c_str(), getTable().c_str(), alreadyCoalescedPeriods, periods, speriod.c_str()));
#endif
	return periods == 0 ? -1 : (100.0 * alreadyCoalescedPeriods) / periods;
}

uint32_t Master::getTreeNodeSize(const uint32_t index) const {
	uint32_t bits = 0;
	uint32_t size = 0;
	{
		ReadGuard guard(getLock());
		const ColumnIds& columnIds = indexes_[index].getColumnIds();
		for (uint32_t i = 0; i < columnIds.length(); ++i) {
			const Column& column = columns_[columnIds[i]];
			size += column.getDataSize();
			bits += column.getBits();
		}
	}
	size += (bits + 7) / 8;
	size += 8;		// TODO row size
	return size;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MasterRepairTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

void MasterRepairTask::run(const uint64_t timestamp) _THROW_(SparrowException) {
	Master* master = get();
	if (master != 0) {
		master->repair();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MasterId
//////////////////////////////////////////////////////////////////////////////////////////////////////

// To find a master file using its unique id.
volatile uint32_t MasterId::counter_;
SYShash<MasterId> MasterId::idHash_(16);
RWLock MasterId::idLock_(true, "MasterId::idLock_");

//////////////////////////////////////////////////////////////////////////////////////////////////////
// RecordWrapper
//////////////////////////////////////////////////////////////////////////////////////////////////////

RecordWrapper::RecordWrapper(const TableFields& fields, const ColumnIds* columnIds, const bool tree) _THROW_(SparrowException)
	: fields_(columnIds == 0 ? 0 : columnIds->length()) {
	initialize(fields, columnIds, tree);
}

RecordWrapper::RecordWrapper(TableFields& fields, const ColumnIds* columnIds, const bool tree, const bool removeFromFields) _THROW_(SparrowException)
	: fields_(columnIds == 0 ? 0 : columnIds->length()) {
	initialize(fields, columnIds, tree);
	if (removeFromFields) {
		if (columnIds == 0) {
			// Data file: all columns.
			fields.clear();
		} else {
			// Index file.
			for (uint32_t i = 0; i < columnIds->length(); ++i) {
				fields[(*columnIds)[i]] = NULL;
			}
			for (int i = columnIds->length()-1; i >= 0; --i) {
				if (fields[i] == NULL) {
					fields.removeAt(i);
				}
			}
		}
	}
}

void RecordWrapper::initialize(const TableFields& fields, const ColumnIds* columnIds, const bool tree) _THROW_(SparrowException) {
	if (columnIds == 0) {
		// Data file: all columns.
		fields_ = fields;
	} else {
		// Index file.
		for (uint32_t i = 0; i < columnIds->length(); ++i) {
			fields_.append(fields[(*columnIds)[i]]);
		}
	}
	bits_ = 0;
	size_ = 0;
	for (uint32_t i = 0; i < fields_.length(); ++i) {
		const FieldBase* field = fields_[i];
		if (field != 0) {
			size_ += field->getSize();
			bits_ += field->getBits();
		}
	}
	bitSize_ = (bits_ + 7) / 8;
	if (bitSize_ > SPARROW_MAX_BIT_SIZE) {
		throw SparrowException::create(false, "Too many bits for record wrapper");
	}
	size_ += bitSize_;
	if (tree) {
		size_ += 8;		// TODO row size
	} else if (columnIds != 0) {
		size_ += 4;		// TODO row size
	}
}

RecordWrapper::RecordWrapper(const TableFields& fields, const ColumnIds& skippedColumnIds) _THROW_(SparrowException) {

	for (uint32_t i = 0; i < fields.length(); ++i) {
		uint32_t j = 0;
		for (; j<skippedColumnIds.length(); ++j) {
			if (skippedColumnIds[j] == i) 
				break;
		}
		if (j == skippedColumnIds.length()) {
			fields_.append(fields[i]);
		}
	}
	bits_ = 0;
	size_ = 0;
	for (uint32_t i = 0; i < fields_.length(); ++i) {
		const FieldBase* field = fields_[i];
		if (field != 0) {
			size_ += field->getSize();
			bits_ += field->getBits();
		}
	}
	bitSize_ = (bits_ + 7) / 8;
	if (bitSize_ > SPARROW_MAX_BIT_SIZE) {
		throw SparrowException::create(false, "Too many bits for record wrapper");
	}
	size_ += bitSize_;
}

// key_part_map indicates which columns from the index to send back. For example, if the RecordWrapper was based on index_N
//	which indexed columns A, B, C, and the key_part_map equals 3 (in binary 011) that indicates we must only send back values for columns A and B.
void RecordWrapper::readUsingKeyPartMap(PartitionReader& reader, PartitionReader& stringReader, const key_part_map map,
	uint8_t* buffer, const bool keyFormat) const _THROW_(SparrowException) {
	uint8_t bitArray[SPARROW_MAX_BIT_SIZE];
	readBits(reader, bitArray);
	uint32_t bitOffset = 0;
	uint32_t f = 0;
	for (uint32_t i = 0; i < fields_.length(); ++i) {
		const FieldBase* field = fields_[i];
		if (field == 0) {
			continue;
		}
		const uint32_t nbits = field->getBits();
		const bool mapped = field->isMapped();
		if (mapped && (map & (1 << f)) == 0) {
			++f;
			field->skip(reader);
			bitOffset += nbits;
			continue;
		}
		if (mapped) {
			++f;
		}
		uint32_t bits = 0;
		for (uint32_t b = 0; b < nbits; ++b, ++bitOffset) {
			bits |= ((bitArray[bitOffset / 8] & (1 << (bitOffset % 8))) == 0 ? 0 : 1) << b;
		}
		field->readPersistent(reader, stringReader, bits, buffer, keyFormat);
		if (keyFormat) {
			buffer += (field->isNullable() ? 1 : 0) + field->getLength(true);
		}
	}
}

void RecordWrapper::readUsingTableBitmap(TABLE& table, PartitionReader& reader, PartitionReader& stringReader, const bool all,
	uint8_t* buffer, const bool keyFormat) const _THROW_(SparrowException) {
	uint8_t bitArray[SPARROW_MAX_BIT_SIZE];
	readBits(reader, bitArray);
	uint32_t bitOffset = 0;
	uint32_t f = 0;

	// In case of update, need to read all fields.
	const bool forUpdate = !bitmap_is_clear_all(table.write_set);
	for (uint32_t i = 0; i < fields_.length(); ++i) {
		const FieldBase* field = fields_[i];
		if (field == 0) {
			continue;
		}
		const uint32_t nbits = field->getBits();
		const bool mapped = field->isMapped();
		if (mapped && !all && !forUpdate && !bitmap_is_set(table.read_set, f)) {
			++f;
			field->skip(reader);
			bitOffset += nbits;
			continue;
		}
		if (mapped) {
			++f;
		}
		uint8_t bits = 0;
		for (uint32_t b = 0; b < nbits; ++b, ++bitOffset) {
			bits |= ((bitArray[bitOffset / 8] & (1 << (bitOffset % 8))) == 0 ? 0 : 1) << b;
		}
		field->readPersistent(reader, stringReader, bits, buffer, keyFormat);
		if (keyFormat) {
			buffer += (field->isNullable() ? 1 : 0) + field->getLength(true);
		}
	}
}

void RecordWrapper::readKeyValue(PartitionReader& reader, PartitionReader& stringReader, ByteBuffer& buffer, BinBuffer* binBuffer) const _THROW_(SparrowException) {
	uint8_t bitArray[SPARROW_MAX_BIT_SIZE];
	readBits(reader, bitArray);
	buffer << ByteBuffer(bitArray, getBitSize());
	int bitOffset = 0;
	for (uint32_t i = 0; i < fields_.length(); ++i) {
		const FieldBase* field = fields_[i];
		if (field == 0) {
			continue;
		}
		const int nbits = field->getBits();
		uint8_t bits = 0;
		for (int b = 0; b < nbits; ++b, ++bitOffset) {
			bits |= ((bitArray[bitOffset / 8] & (1 << (bitOffset % 8))) == 0 ? 0 : 1) << b;
		}
		field->copy(reader, stringReader, bits, buffer, binBuffer);
	}
}

int RecordWrapper::compare(ByteBuffer& buffer1, PartitionReader& stringReader1, ByteBuffer& buffer2,
	PartitionReader& stringReader2, BinBuffer* binBuffer) const _THROW_(SparrowException) {
	assert(&stringReader1 != &stringReader2);
	uint8_t bitArray1[SPARROW_MAX_BIT_SIZE];
	readBits(buffer1, bitArray1);
	uint8_t bitArray2[SPARROW_MAX_BIT_SIZE];
	readBits(buffer2, bitArray2);
	int bitOffset = 0;
	for (uint32_t i = 0; i < fields_.length(); ++i) {
		const FieldBase* field = fields_[i];
		if (field == 0) {
			continue;
		}
		const int nbits = field->getBits();
		uint8_t bits1 = 0;
		uint8_t bits2 = 0;
		for (int b = 0; b < nbits; ++b, ++bitOffset) {
			bits1 |= ((bitArray1[bitOffset / 8] & (1 << (bitOffset % 8))) == 0 ? 0 : 1) << b;
			bits2 |= ((bitArray2[bitOffset / 8] & (1 << (bitOffset % 8))) == 0 ? 0 : 1) << b;
		}
		const int cmp = field->compare(buffer1, bits1, stringReader1, buffer2, bits2, stringReader2, binBuffer);
		if (cmp != 0) {
			return cmp;
		}
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// DataFileReader
//////////////////////////////////////////////////////////////////////////////////////////////////////

DataFileReader::DataFileReader(const TableFields& fields, const ColumnIds& columnIds, const ColumnIds& skippedColumns)
	: fields_(fields), infos_(columnIds.length()), recordWrapper_(fields, skippedColumns) {
	for (uint32_t i = 0; i < columnIds.length(); ++i) {
		infos_.append(ColumnInfo());
	}
	uint32_t bitOffset = 0;
	uint32_t offset = 0;
	for (uint32_t i = 0; i < fields.length(); ++i) {
		const FieldBase* field = fields[i];
		if (field == 0) {
			continue;
		}
		const uint32_t size = field->getSize();
		const uint32_t nbits = field->getBits();
		for (uint32_t j = 0; j < columnIds.length(); ++j) {
			if (i == columnIds[j]) {
				infos_[j] = ColumnInfo(i, bitOffset, nbits, offset, size);
				break;
			}
		}
		bitOffset += nbits;
		offset += size;
	}
}

}
