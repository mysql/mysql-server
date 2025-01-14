/*
	Persistent partition.
*/

#ifndef _engine_persistent_h_
#define _engine_persistent_h_

#include "search.h"
#include "fileutil.h"
#include "master.h"
#include "vec.h"
#include "../handler/hasparrow.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PersistentPartition
//////////////////////////////////////////////////////////////////////////////////////////////////////

class PersistentPartition : public Partition, public AbstractInterval<uint64_t> {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, PersistentPartition& partition);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const PersistentPartition& partition);

private:

	uint32_t version_;		// See PersistentPartition::currentVersion_.
	MasterGuard master_;
	TimePeriod period_;		// Partition period. May change if main partition.
	uint64_t fileTime_;		// File timestamp. Does not change after partition is created.
	PersistentPartition* mainPartition_;
	ChildPartitions childPartitions_;	// List of child partitions. Populated only for main partitions. 
	uint32_t records_;
	uint64_t dataSize_;
	uint64_t indexSize_;
	uint64_t dataRecords_;	// Number of records in data file.
	uint64_t recordOffset_;	// Record offset in main partition.
	ColumnIds	skippedColumnIds_;	// Skipped columns. Columns for which all values are NULL are not stored.

public:

	static const uint32_t currentVersion_;

	static const uint32_t appendVersion_;

private:

	Position searchTree(Context& context, const uint32_t partition, PartitionReader& reader,
		PartitionReader& stringReader, const KeyValue& key, const SearchFlag searchFlag, const bool refine) const;

	uint64_t createIndexFile(const uint32_t index, const Task* task,
		const TableFields& fields, const ColumnIds& columnIds, const uint32_t startRow, const uint32_t rows) _THROW_(SparrowException);

	void rebuildIndex(const uint32_t index) _THROW_(SparrowException);

	FileHeaderBase* readHeader2(const uint32_t fileId, FileReader& reader) const _THROW_(SparrowException);

public:

	PersistentPartition(const uint32_t version, Master* master, const uint64_t serial, PersistentPartition* mainPartition,
		const uint32_t filesystem, const uint32_t indexAlterSerial, const uint32_t columnAlterSerial, const TimePeriod& period,
		const uint32_t records, const uint64_t dataSize, const uint64_t indexSize, const uint64_t dataRecords, const uint64_t recordOffset, const ColumnIds& skippedColumns)
		: Partition(serial, mainPartition == 0 ? serial : mainPartition->getSerial(), filesystem, indexAlterSerial, columnAlterSerial), version_(version),
		master_(master), period_(period), fileTime_(period.getMin()), records_(records), dataSize_(dataSize), indexSize_(indexSize),
		dataRecords_(dataRecords), recordOffset_(recordOffset), skippedColumnIds_(skippedColumns) {
		assert(period_.getLow() != 0 && period_.getUp() != 0);
		mainPartition_ = mainPartition == 0 ? this : mainPartition;
	}

	// Deserialization constructor.
	PersistentPartition(Master* master) : Partition(0, 0, 0, 0, 0), master_(master), mainPartition_(0) {
	}

	~PersistentPartition();

	void detach() override {
		master_ = 0;
	}

	// Attributes.

	TimePeriod getPeriod() const override {
		return period_;
	}

	uint32_t getRecords() const override {
		return records_;
	}

	uint64_t getDataSize() const override {
		return dataSize_;
	}

	uint64_t getIndexSize() const override {
		return indexSize_;
	}

	bool isTransient() const override {
		return false;
	}

	bool isIndexAlterable() const override {
		return !isMain() || getVersion() < PersistentPartition::appendVersion_;
	}

	bool isReady() const override {
		return master_->getIndexAlterSerial() == getIndexAlterSerial() || !isIndexAlterable();
	}

	const char* getFileName(uint32_t fileId, char* name) const {
		if (version_ == 0 && fileId != DATA_FILE && fileId != STRING_FILE) {
			--fileId;
		}
		return master_->getFileName(version_, getFilesystem(), TimePeriod(fileTime_, period_.getMax()), fileId, getSerial(), getDataSerial(), name);
	}

	CoalescingInfo getCoalescingInfo() const {
		return CoalescingInfo(Pair<uint32_t, uint32_t>(getVersion(), getColumnAlterSerial()), getVersion() >= PersistentPartition::appendVersion_ ? getDataSerial() : 0);
	}

	const ColumnIds& getSkippedColumns() const {
		return skippedColumnIds_;
	}

	bool makeChecks() const;


	// Data access.

	Position indexFind(Context& context, const uint32_t partition, const KeyValue& key, const SearchFlag searchFlag) const override;

	Position indexFirst(Context& context, const uint32_t partition) const override;

	Position indexLast(Context& context, const uint32_t partition) const override;

	Position indexNext(Context& context, const Position& position) const override;

	Position indexPrevious(Context& context, const Position& position) const override;

	Position moveNext(Context& context, const Position& position) const override;

	Position movePrevious(Context& context, const Position& position) const override;

	Position moveAbsolute(Context& context, const Position& position) const override;

	Position moveFirst(Context& context, const uint32_t partition) const override;

	Position moveLast(Context& context, const uint32_t partition) const override;

	uint32_t recordsInRange(Context& context, const uint32_t partition, const key_range* minKey, const key_range* maxKey) const override;

	bool readKey(Context& context, const Position& position, const bool forward,
		const key_part_map keyPartMap, uint8_t* buffer, const bool keyFormat) const override;

	bool readData(Context& context, const Position& position, uint8_t* buffer, const BlockCacheHint& hint) const override;

	bool updateData(Context& context, const Position& position, const uint8_t* buffer) override;

	// Implementation of AbstractInterval<uint64_t>
	uint64_t getMin() const override {
		return *period_.getLow();
	}

	uint64_t getMax() const override {
		return *period_.getUp();
	}

	int compareTo(const AbstractInterval<uint64_t>& right) const override {
		if (getMin() < right.getMin()) {
			return -1;
		} else if (getMin() > right.getMin()) {
			return 1;
		} else {
			const uint64_t serial = getSerial();
			const uint64_t rightSerial = static_cast<const PersistentPartition&>(right).getSerial();
			return serial == rightSerial ? 0 : (serial < rightSerial ? -1 : 1);
		}
	}

	// Alteration.

	void alter(const Task* task) _THROW_(SparrowException);

	PersistentPartitionGuard alterationDone(const AlterationStats& stats, const uint32_t newIndexAlterSerial);

	AlterationStats createIndex(const uint32_t index, const Task* task) _THROW_(SparrowException);

	AlterationStats dropIndex(const uint32_t index);

	// Specific accessors

	uint32_t getVersion() const {
		return version_;
	}

	const Master& getMaster() const {
		return *master_.get();
	}

	Master& getMaster() {
		return *master_.get();
	}

	uint64_t getFileTime() const {
		return fileTime_;
	}

	void decRecords(const uint32_t records) {
		records_ -= records;
	}

	void addDataRecords(const uint32_t records) {
		dataRecords_ += records;
	}

	void setDataSize(const uint64_t dataSize) {
		dataSize_ = dataSize;
	}

	void addDataSize(const uint64_t dataSize) {
		Atomic::add64(&dataSize_, dataSize);
	}

	void addIndexSize(const uint64_t indexSize) {
		Atomic::add64(&indexSize_, indexSize);
	}
	
	uint64_t getDataRecords() const {
		return dataRecords_;
	}

	uint64_t getRecordOffset() const {
		return recordOffset_;
	}

	void addChildPartition(Partition* partition) {
		assert(!childPartitions_.contains(partition));
		childPartitions_.insert(partition);
	}

	void removeChildPartition(Partition* partition) {
		[[maybe_unused]] Partition*	p = childPartitions_.remove(partition);
		assert(p != NULL);
	}

	const ChildPartitions& getChildPartitions() const {
		return childPartitions_;
	}

	void extendPeriod(const TimePeriod& period) {
		period_ = TimePeriod(std::min(period_.getMin(), period.getMin()), std::max(period_.getMax(), period.getMax()));
	}

	static void writeFileFormat(ByteBuffer& buffer) _THROW_(SparrowException) {
		buffer << static_cast<uint8_t>(0) << FileHeader::currentFileFormat_
			<< static_cast<uint8_t>(0) << static_cast<uint8_t>(0);
	}

	PersistentPartition* getMainPartition() const {
		return mainPartition_;
	}

	void setMainPartition(PersistentPartition* mainPartition) {
		assert(mainPartition != 0);
		mainPartition_ = mainPartition;
	}

	uint32_t getFileId(const uint32_t index, const bool isString) const {
		return getVersion() >= PersistentPartition::appendVersion_ && isString ? STRING_FILE : index;
	}

	PartitionReader* createReader(const uint32_t index, const bool isString, const BlockCacheHint& hint) const _THROW_(SparrowException);

	FileHeaderBase* readHeader(const uint32_t fileId, FileReader& reader) const _THROW_(SparrowException);
};

inline ByteBuffer& operator >> (ByteBuffer& buffer, PersistentPartition& partition) {
	const uint32_t version = buffer.getVersion();
	if (version < 13) {
		partition.version_ = 0;
	} else {
		buffer >> partition.version_;
	}
	buffer >> partition.serial_;
	if (version >= 20) {
		buffer >> partition.dataSerial_;
		buffer >> partition.dataRecords_;
		buffer >> partition.recordOffset_;
	} else {
		partition.dataSerial_ = partition.serial_;
		partition.recordOffset_ = 0;
	}
	buffer >> partition.period_;
	if (version >= 20) {
		buffer >> partition.fileTime_;
	} else {
		partition.fileTime_ = partition.period_.getMin();
	}
	buffer >> partition.records_
		>> partition.dataSize_ >> partition.indexSize_;
	if (version < 20) {
		partition.dataRecords_ = partition.records_;
	}
	bool ready = true;
	if (version >= 6 && version < 9) {
		buffer >> ready;
	}
	if (version >= 8) {
		buffer >> partition.filesystem_;
	}
	if (version >= 9) {
		buffer >> partition.indexAlterSerial_;
	} else {
		partition.indexAlterSerial_ = ready ? partition.master_->getIndexAlterSerial() : 0;
	}
	if (version >= 17) {
		buffer >> partition.columnAlterSerial_;
	} else {
		partition.columnAlterSerial_ = 0;
	}
	if (version >= 22 ) {
		buffer >> partition.skippedColumnIds_;
	} 

	// Force partition reference count to 1.
	partition.resetRef(1);
	return buffer;
}

inline ByteBuffer& operator << (ByteBuffer& buffer, const PersistentPartition& partition) {
	buffer << partition.version_ << partition.serial_ << partition.dataSerial_ << partition.dataRecords_
		<< partition.recordOffset_ << partition.period_ << partition.fileTime_ << partition.records_
		<< partition.dataSize_ << partition.indexSize_ << partition.filesystem_ << partition.indexAlterSerial_
		<< partition.columnAlterSerial_ << partition.skippedColumnIds_;
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ComparatorPersistent
//////////////////////////////////////////////////////////////////////////////////////////////////////

class ComparatorPersistent {
private:

	const QueryInfo& queryInfo_;
	const TableFields& fields_;
	const RecordWrapper& recordWrapper_;
	PartitionReader& reader_;
	PartitionReader& stringReader_;
	const KeyValue& key_;
	KeyValue tempKey_;

public:

	ComparatorPersistent(Context& context, const RecordWrapper& recordWrapper, PartitionReader& reader, PartitionReader& stringReader,
		const KeyValue& key, uint8_t* buffer)
		: queryInfo_(context.getQueryInfo()), fields_(context.getShare().getMappedFields()),
		recordWrapper_(recordWrapper), reader_(reader), stringReader_(stringReader), key_(key), tempKey_(buffer, key.getMap()) {
	}

	int compareTo(const uint32_t row) _THROW_(SparrowException) {
		reader_.seekRecordData(row);
		recordWrapper_.readUsingKeyPartMap(reader_, stringReader_, key_.getMap(), tempKey_.getKey(), true);
		return queryInfo_.compareKeys(fields_, tempKey_, key_);
	}
};

}

#endif /* #ifndef _engine_persistent_h_ */
