/*
	Master file.
*/

#ifndef _engine_master_h_
#define _engine_master_h_

#include "types.h"
#include "scheduler.h"
#include "context.h"
#include "list.h"
#include "hash.h"
#include "intervaltree.h"
#include "../dns/dnsconfiguration.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Master
//////////////////////////////////////////////////////////////////////////////////////////////////////

class TransientPartition;
class CoalescingIndexTask;
class CoalescingMainTask;
typedef SYSvector<Str, 16> Names;
typedef RefPtr<TransientPartition> TransientPartitionGuard;
typedef SYSvector<TransientPartitionGuard, 256> TransientPartitions;
class PersistentPartition;
typedef RefPtr<PersistentPartition> PersistentPartitionGuard;
typedef SYSvector<PersistentPartitionGuard, 256> PersistentPartitions;
typedef SYSvector<int, 0> IndexMappings;
typedef SYSsortedVector<uint64_t, 256> Serials;
typedef SYSpVector<AbstractInterval<uint64_t>, 256> Intervals;
class MasterDependency;
typedef SYSidlist<MasterDependency> MasterDependencies;
typedef Pair<Pair<uint32_t, uint32_t>, uint64_t> CoalescingInfo;
class CoalescablePartitions;
typedef SYShash<CoalescablePartitions> CoalescingCandidates;
typedef SYShashIterator<CoalescablePartitions> CoalescingCandidatesIterator;
typedef SYSvector<uint64_t, 0> PartitionIds;
typedef SYSvector<CoalescingIndexTask*,16>	IndexCoalescingTasks;
typedef SYSvector<CoalescingMainTask*,16>	MainCoalescingTasks;

#define SAME_AS_SERIAL ULLONG_MAX
class Master : public RefCounted {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, Master& master);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const Master& master);

private:
	
	uint32_t id_;							// Unique transient identifier, for block cache handling.

	RWLock* lock_;						// R/W lock protecting this master.

	Lock* updateLock_;					// Lock and conditions to make sure data update does not occur during
	Cond* canUpdate_;					// transient partition flush or coalescing of data files (old format).
	Cond* updateOnGoing_;
	volatile bool updating_;			// If true, an SQL update is on going.
	volatile uint32_t updateBlockers_;	// Number of operations currently preventing SQL update.

	Lock* flushLock_;					// Lock and condition to serialize flushs to a given main partition.
	Cond* canFlush_;
	Serials flushSerials_;

	Str database_;
	Str table_;

	Columns columns_;
	ColumnIds mappedColumnIds_;
	Indexes indexes_;
	IndexMappings indexMappings_;	// Mapping between MySQL index id and our index id.
	ForeignKeys foreignKeys_;

	DnsConfigurationGuard dnsConfiguration_;

	uint64_t maxLifetime_;
	uint32_t aggregationPeriod_;

	int64_t autoInc_;

	volatile uint64_t serial_;		// Current partition serial number.

	uint64_t timeCreated_;			// Seconds since epoch.
	uint64_t timeUpdated_;			// Seconds since epoch.

	uint64_t dataSize_;
	uint64_t indexSize_;
	uint64_t records_;

	// Partitions.
	Partitions partitions_;
	IntervalTree<uint64_t> intervals_;
	SYSpVector<TransientPartition, 256> transientPartitions_;

	// Online index modifications.
	uint32_t indexAlterSerial_;
	Alterations indexAlterations_;
	uint64_t indexAlterStarted_;			// Milliseconds since epoch.
	uint64_t indexAlterElapsed_;			// Elapsed time doing alterations.

	uint64_t coalescingPeriod_;
	uint64_t defaultWhere_;

	uint64_t stringOptimization_;			// String optimization size.

	// Serial numbers of partitions being coalesced.
	Serials coalescedSerials_;
	
	// List of coalescing tasks (either pending or being processed)
	IndexCoalescingTasks	indexCoalescingTask_;
	MainCoalescingTasks		mainCoalescingTask_;

	// All data before this timestamp are already coalesced.
	uint64_t coalescingTimestamp_;

	// Version for serialization.
	const uint32_t version_;

	// Dependencies to remove upon deletion.
	Lock* depLock_;
	Cond* depCond_;
	MasterDependencies dependencies_;

	static thread_local TABLE_SHARE* threadKey_;

public:

	static const uint32_t currentVersion_;

private:

	void logPartitions() const;

	void listPartitionsForMain(const PersistentPartition& mainPartition, PersistentPartitions& partitions) const _THROW_(SparrowException);

	bool removePartitions(const PersistentPartitions& partitions);

	void getCoalescingCandidates(const TimePeriod&, CoalescingCandidates& candidates, const bool fake) const;

	static bool formatFileName(char* to, const char* name, const char* dir, const char *extension);

public:

	static void initialize();

	Master(const char* database, const char* table, const bool key);

	void setup(const bool full);

	void prepareForDeletion();

	~Master();

	void closeFiles();

	void rename(const char* newDatabase, const char* newTable) _THROW_(SparrowException);

	bool isKey() const {
		return !database_.isOwned();
	}

	uint32_t getId() const {
		return id_;
	}

	RWLock& getLock() const { return *lock_; }

	void startUpdate() {
		Guard guard(*updateLock_);
		while (updateBlockers_ > 0) {
			canUpdate_->wait(true);
		}
	}

	void endUpdate() {
		Guard guard(*updateLock_);
		updating_ = false;
		updateOnGoing_->signalAll(true);
	}

	void blockUpdate() {
		Guard guard(*updateLock_);
		while (updating_) {
			updateOnGoing_->wait(true);
		}
		updateBlockers_++;
	}

	void allowUpdate() {
		Guard guard(*updateLock_);
		if (--updateBlockers_ == 0) {
			canUpdate_->signalAll(true);
		}
	}

	// Check if a job is already flushing data to that main partition (identified by its serial number)
	//	If there is, wait until it has finished. 
	void startFlush(const uint64_t serial) {
		Guard guard(*flushLock_);
		while (flushSerials_.contains(serial)) {
			canFlush_->wait(true);
		}
		flushSerials_.insert(serial);
	}

	bool isFlushing(const uint64_t serial) {
		Guard guard(*flushLock_);
		return flushSerials_.contains(serial);
	}

	void endFlush(const uint64_t serial) {
		Guard guard(*flushLock_);
		assert(flushSerials_.contains(serial));
		flushSerials_.remove(serial);
		canFlush_->signalAll(true);
	}

	Lock& getDepLock() const { return *depLock_; }

	static const char* getMasterFileName(char* buffer, const char* database, const char* table,
		const bool appendExtension) _THROW_(SparrowException);

	const char* getMasterFileName(char* buffer) const _THROW_(SparrowException);

	const char* getDataDirectory(const uint32_t filesystem, char* buffer) const _THROW_(SparrowException);

	const char* getFileName(const uint32_t version, const uint32_t filesystem, const TimePeriod& period,
		const uint32_t fileId, const uint64_t serial, const uint64_t dataSerial, char* buffer) const _THROW_(SparrowException);

	static bool checkForCorruption(struct tm& t);

	void toDisk() _THROW_(SparrowException);

	static Master* fromDisk(const char* database, const char* table, TABLE_SHARE* s) _THROW_(SparrowException);

	// Caution: MySQL changes db and table names to lower case, so use case-insensitive comparison
	// (note Str::hash is already case-insensitive).
	bool operator == (const Master& right) const {
		return database_.compareTo(right.database_, true) == 0
			&& table_.compareTo(right.table_, true) == 0;
	}

	bool operator < (const Master& right) const {
		if (database_.compareTo(right.database_, true) < 0) {
			return true;
		} else if (right.database_.compareTo(database_, true) < 0) {
			return false;
		}
		return table_.compareTo(right.table_, true) < 0;
	}

	PersistentPartitionGuard findMainPartition(uint64_t serial, TimePeriod period, uint32_t columnAlterSerial, uint32_t indexAlterSerial, const ColumnIds& emptyColumnIds);

	void getPartitionsForTimePeriod(const TimePeriod& period, ReferencedPartitions& entries, QueryInfo& queryInfo) const;

	void coalesce();

	void registerCoalescingTask(CoalescingIndexTask* task);
	void registerCoalescingTask(CoalescingMainTask* task);
	void unregisterCoalescingTask(CoalescingIndexTask* task);
	void unregisterCoalescingTask(CoalescingMainTask* task);
	uint getNbCoalescingTasks() {
		ReadGuard guard(getLock());
		return indexCoalescingTask_.entries() + mainCoalescingTask_.entries();
	}

	void stopCoalescingTasks();

	void coalescingDone(PersistentPartition* coalescedPartition, const PersistentPartitions& partitions) _THROW_(SparrowException);

	void coalescingFailed(const PersistentPartitions& partitions) _THROW_(SparrowException);

	TransientPartitionGuard getTransientPartition(const uint64_t& timestamp);

	void removePartitions(const TimePeriod& period) _THROW_(SparrowException);

	bool forceFlush();

	bool forceFlushNoLock(const TransientPartitions&, bool master_lock_taken=false);

	void waitForFlush();
	void waitForFlush(const PartitionIds&);

	const Str& getDatabase() const {
		return database_;
	}

	const Str& getTable() const {
		return table_;
	}

	const Columns& getColumns() const {
		return columns_;
	}

	Columns& getColumns() {
		return columns_;
	}

	bool compareColumns(const ColumnExs& columns) const {
		uint32_t j = 0;
		for (uint32_t i = 0; i < columns_.length(); ++i) {
			const Column& column = columns_[i];
			if (column.isDropped()) {
				continue;
			}
			if (j >= columns.length() || column != columns[j]) {
				return false;
			}
			++j;
		}
		return true;
	}

	void computeMappedColumnIds() {
		const uint32_t nbColumns = columns_.length();
		mappedColumnIds_ = ColumnIds(nbColumns);
		uint32_t pos = 0;
		for (uint32_t i = 0; i < nbColumns; ++i) {
			if (columns_[i].isDropped()) {
				mappedColumnIds_.append(SYS_NPOS);
			} else {
				mappedColumnIds_.append(pos++);
			}
		}
	}

	void setColumns(const Columns& columns) {
		columns_ = columns;
		computeMappedColumnIds();
	}

	void updateColumns(const ColumnExs& columns) {
		uint32_t j = 0;
		for (uint32_t i = 0; i < columns_.length(); ++i) {
			Column& column = columns_[i];
			if (column.isDropped()) {
				continue;
			}
			const uint32_t save = column.getSerial();
			column = columns[j++];
			column.setSerial(save);
		}
	}

	uint32_t getColumn(const Str& name) const {
		for (uint32_t i = 0; i < columns_.length(); ++i) {
			const Column& column = columns_[i];
			if (column.isDropped()) {
				continue;
			}
			if (column.getName().compareTo(name, true) == 0) {
				return i;
			}
		}
		return SYS_NPOS;
	}

	uint32_t getColumn(int colPos) const {
		int		pos = -1;
		for (uint32_t i = 0; i < columns_.length(); ++i) {
			const Column& column = columns_[i];
			if (column.isDropped()) {
				continue;
			}
			if (++pos == colPos) {
				return i;
			}
		}
		return SYS_NPOS;
	}

	void getColumnIds(const Names& colNames, ColumnIds& colIds) const _THROW_(SparrowException);

	void shiftColumnIds(ColumnIds& ids) const {
		for (uint32_t i = 0; i < ids.length(); ++i) {
			ids[i] = mappedColumnIds_[ids[i]];
		}
	}

	ColumnIds updateColumnIds(const ColumnIds& ids, const ColumnExs& columns) {
		ColumnIds result(ids.length());
		for (uint32_t i = 0; i < ids.length(); ++i) {
			const Column& column = columns[ids[i]];
			const uint32_t id = getColumn(column.getName());
			assert(id != SYS_NPOS);
			result.append(id);
		}
		return result;
	}

	const Indexes& getIndexes() const {
		return indexes_;
	}

	void getFields(const uint32_t serial, const bool coalescing, TableFields& fields, const ColumnIds* skippedColumnIds) const {
		FieldBase::createFields(serial, coalescing, 0, columns_, fields, skippedColumnIds);
	}

	const ForeignKeys& getForeignKeys() const {
		return foreignKeys_;
	}

	const DnsConfiguration* getDnsConfiguration() const {
		return dnsConfiguration_.get();
	}

	DnsConfiguration* getDnsConfiguration() {
		return dnsConfiguration_.get();
	}

	uint64_t getMaxLifetime() const {
		if (maxLifetime_ == 0) {
			 return static_cast<uint64_t>(sparrow_default_max_lifetime) * static_cast<uint64_t>(86400000);
		} else {
			return maxLifetime_;
		}
	}

	void setMaxLifetime(const uint64_t maxLifetime) {
		SPARROW_ENTER("Master::setMaxLifetime");
		DBUG_PRINT("sparrow_master", ("Set max lifetime of table %s.%s to %llu milliseconds",
			getDatabase().c_str(), getTable().c_str(), static_cast<ulonglong>(maxLifetime)));
		maxLifetime_ = maxLifetime;
	}

	uint32_t getAggregationPeriod() const {
		return aggregationPeriod_;
	}

	void setAggregationPeriod(const uint32_t aggregationPeriod) {
		aggregationPeriod_ = aggregationPeriod;
	}

	int64_t getAutoInc() const {
		return autoInc_;
	}

	void setAutoInc(const int64_t autoInc) {
		autoInc_ = autoInc;
	}

	uint64_t getOldest(const bool persistentOnly = false) const;

	uint64_t getNewest(const bool persistentOnly = false) const;

	uint64_t getAge() const {
		const uint64_t low = getOldest();
		const uint64_t high = getNewest();
		return (low != 0 && high != 0 && high > low) ? high - low : 0;
	}

	uint64_t getAge(const uint64_t t) const {
		const uint64_t low = getOldest();
		return (low != 0 && t != 0 && t > low) ? t - low : 0;
	}

	void setIndexes(const Indexes& indexes) {
		indexes_ = indexes;
	}

	void setForeignKeys(const ForeignKeys& foreignKeys) {
		foreignKeys_ = foreignKeys;
	}

	TransientPartitions setDnsConfiguration(const DnsConfiguration& configuration);

	void deinitialize();

	const IndexMappings& getIndexMappings() const {
		return indexMappings_;
	}

	void setIndexMappings(const IndexMappings& indexMappings) {
		indexMappings_ = indexMappings;
		uint32_t i = 0;
		while (i < indexMappings_.length()) {
			if (indexMappings_[i] == -1) {
				indexMappings_.removeAt(i);
			} else {
				i++;
			}
		}
	}

	int getIndexId(const uint32_t mySqlIndexId) const {
		return mySqlIndexId == MAX_KEY ? DATA_FILE : indexMappings_[mySqlIndexId];
	}

	int getMySqlIndexId(const uint32_t indexId) const {
		for (uint32_t i = 0; i < indexMappings_.length(); ++i) {
			if (static_cast<uint32_t>(indexMappings_[i]) == indexId) {
				return static_cast<int>(i);
			}
		}
		return MAX_KEY;
	}

	uint64_t getDataSize() const {
		return dataSize_;
	}

	uint64_t getIndexSize() const {
		return indexSize_;
	}

	uint64_t getRecords() const {
		return records_;
	}

	uint64_t getTransientRecords() const;

	void setDataSize(uint64_t dataSize) {
		dataSize_ = dataSize;
	}

	void setIndexSize(uint64_t indexSize) {
		indexSize_ = indexSize;
	}

	void setRecords(uint64_t records) {
		records_ = records;
	}

	const Partitions& getPartitions() const {
		return partitions_;
	}

	PartitionGuard getPartitionNoLock(const uint64_t serial) const;

	PartitionGuard getPartition(const uint64_t serial) const;

	void getTransientPartitions(TransientPartitions&) const;

	uint64_t getTimeCreated() const {
		return timeCreated_;
	}

	uint64_t getTimeUpdated() const {
		return timeUpdated_;
	}

	void retrieve(ByteBuffer& buffer) const;

	uint32_t hash() const {
		uint32_t result = 1;
		result = 31 + database_.hash();
		result = 31 * result + table_.hash();
		return result;
	}

	uint64_t getNormalizedSize() const;

	bool needToPurge(const uint64_t limit, const uint64_t total, const uint64_t totalNormalized, bool& force, const bool mode) const;

	bool purge(PersistentPartitions& partitions, const bool force, const bool mode);

	uint64_t listPartitionsForFilesystem(const uint32_t filesystem, PersistentPartitions& partitions,
		const uint64_t limit, const uint64_t totalNormalized) const;

	bool purgePartitionsForFilesystem(const PersistentPartitions& partitions);

	uint32_t getIndexAlterSerial() const {
		return indexAlterSerial_;
	}

	void setIndexAlterSerial(const uint32_t indexAlterSerial) {
		indexAlterSerial_ = indexAlterSerial;
	}

	const Alterations& getIndexAlterations() const {
		return indexAlterations_;
	}

	void setIndexAlterations(const Alterations& indexAlterations) {
		indexAlterations_ = indexAlterations;
	}

	bool startIndexAlter(const bool check) _THROW_(SparrowException);

	void indexAlterationDone();

	bool getIndexAlterStatus(uint64_t& elapsed, uint64_t& left, double& percentage) const;
	bool getIndexAlterStatus(SYSslist<Str>& strings) const;
	
	void dropColumn(const char* name) _THROW_(SparrowException);
	void addColumn(const char* after, Column& newColumn) _THROW_(SparrowException);
	void renameColumn(const char* from, const char* to) _THROW_(SparrowException);

	uint32_t getColumnAlterSerial() const {
		uint32_t serial = 0;
		for (uint32_t i = 0; i < columns_.length(); ++i) {
			const Column& column = columns_[i];
			const uint32_t columnSerial = column.isDropped() ? column.getDropSerial() : column.getSerial();
			serial = std::max(serial, columnSerial);
		}
		return serial;
	}

	PersistentPartition* newPersistentPartition(const uint32_t version, const uint64_t dataSerial, const uint32_t filesystem,
		const TimePeriod& period, const uint32_t records, const uint32_t indexAlterSerial, const uint32_t columnAlterSerial,
		const uint64_t dataRecords, const uint64_t recordOffset, const ColumnIds& emptyColumnIds);

	PersistentPartition* newTemporaryPersistentPartition(const PersistentPartition& partition, const uint32_t records, const uint64_t recordOffset);

	void mutatePartition(TransientPartition* transientPartition, PersistentPartition* mainPartition, PersistentPartition* newPartition);

	bool hasBuiltInTimestampIndex() const;

	uint32_t getTreeNodeSize(const uint32_t index) const;

	uint64_t getCoalescingPeriod() const {
		return coalescingPeriod_;
	}

	void setCoalescingPeriod(const uint64_t coalescingPeriod) {
		coalescingPeriod_ = coalescingPeriod;
		coalescingTimestamp_ = 0;
	}

	void resetCoalescingTimestamp() {
		coalescingTimestamp_ = 0;
	}

	uint64_t getDefaultWhere() const {
		return defaultWhere_;
	}

	void setDefaultWhere(const uint64_t defaultWhere) {
		defaultWhere_ = defaultWhere;
	}

	uint64_t getStringOptimization() const {
		return stringOptimization_ == 0 ? sparrow_default_string_optimization_size : stringOptimization_;
	}

	void setStringOptimization(const uint64_t stringOptimization) {
		stringOptimization_ = stringOptimization;
	}

	double getCoalescingPercentage() const;

	void addDependency(MasterDependency* dependency) {
		Guard guard(getDepLock());
		dependencies_.append(dependency);
	}

	void removeDependency(MasterDependency* dependency) {
		Guard guard(getDepLock());
		dependencies_.remove(dependency);
		if (dependencies_.isEmpty()) {
			depCond_->signalAll(true);
		}
	}

	void repair();
};

ByteBuffer& operator >> (ByteBuffer& buffer, Master& master);
ByteBuffer& operator << (ByteBuffer& buffer, const Master& master);

typedef RefPtr<Master> MasterGuard;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// UpdateGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

class UpdateGuard {
private:

	Master& master_;

public:

	UpdateGuard(Master& master) : master_(master) {
		master_.blockUpdate();
	}

	~UpdateGuard() {
		master_.allowUpdate();
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MasterDependency
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MasterDependency : public MasterGuard, public SYSidlink<MasterDependency> {
public:

	MasterDependency() {
	}

	MasterDependency(Master* master) :  MasterGuard(master) {
		if (master != 0) {
			master->addDependency(this);
		}
	}

	virtual ~MasterDependency() {
		Master* master = get();
		if (master != 0) {
			master->removeDependency(this);
		}
	}

	virtual void stop() = 0;

	MasterDependency& operator = (const MasterDependency& right) {
		if (this != &right) {
			Master* master = get();
			Master* rightMaster = right.get();
			if (master != rightMaster) {
				if (master != 0) {
					master->removeDependency(this);
				}
				if (rightMaster != 0) {
					rightMaster->addDependency(this);
				}
				*static_cast<MasterGuard*>(this) = rightMaster;
			}
		}
		return *this;
	}

	MasterDependency(const MasterDependency& right) : MasterGuard() {
		*this = right;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MasterKeepAlive
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MasterKeepAlive : public MasterDependency {
private:

	volatile bool stopping_;

public:

	MasterKeepAlive() : stopping_(false) {
	}

	MasterKeepAlive(Master* master) : MasterDependency(master), stopping_(false) {
	}

	~MasterKeepAlive() {
	}

	void stop() override {
		stopping_ = true;
	}

	volatile bool& isStopping() {
		return stopping_;
	}

	MasterKeepAlive& operator = (const MasterKeepAlive& right) {
		if (this != &right) {
			*static_cast<MasterDependency*>(this) = right;
			stopping_ = right.stopping_;
		}
		return *this;
	}

	MasterKeepAlive(const MasterKeepAlive& right) : MasterDependency() {
		*this = right;
	}
};

typedef SYSvector<MasterKeepAlive> Masters;
typedef SYSsortedVector<MasterKeepAlive> SortedMasters;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MasterTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MasterTask : public Task, protected MasterDependency {
public:

	MasterTask(Queue<Job>& queue, Master* master) : Task(queue), MasterDependency(master) {
	}

	~MasterTask() {
	}

	void stop() override {
		Task::stop();
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MasterRepairTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MasterRepairTask : public MasterTask {
public:
	MasterRepairTask(Master* master) : MasterTask(Worker::getQueue(), master) {;}

	bool operator == (const MasterRepairTask& right) const {
		return this->get() == right.get();
	}

	bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override { return 0; }

	void run(const uint64_t timestamp) override _THROW_(SparrowException);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MasterId
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MasterId {
private:

	static volatile uint32_t counter_;
	static SYShash<MasterId> idHash_;
	static RWLock idLock_;

	uint32_t id_;
	Master* master_;

public:

	MasterId(const uint32_t id) : id_(id), master_(0) {
	}

	MasterId(Master* master) : id_(master->getId()), master_(master) {
	}

	bool operator == (const MasterId& right) const {
		return id_ == right.id_;
	}

	uint32_t hash() const {
		return 31 + static_cast<uint32_t>(id_);
	}

	static uint32_t newId() {
		return Atomic::inc32(&MasterId::counter_);
	}

	static void remove(Master* master) {
		WriteGuard guard(idLock_);
		idHash_.remove(MasterId(master));
	}

	static void insert(Master* master) {
		WriteGuard guard(idLock_);
		idHash_.insert(MasterId(master));
	}

	static MasterGuard get(const uint32_t id) {
		ReadGuard guard(idLock_);
		MasterId* masterId = idHash_.find(MasterId(id));
		return MasterGuard(masterId == 0 ? 0 : masterId->master_);
	}
};


}

#endif /* #ifndef _engine_master_h_ */
