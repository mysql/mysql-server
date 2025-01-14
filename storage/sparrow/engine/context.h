/*
	Table handler context.
*/

#ifndef _engine_context_h_
#define _engine_context_h_

#include "sql/handler.h"

#include "../handler/field.h"
#include "partition.h"
#include "list.h"
#include "fileutil.h"
#include "sort.h"
#include "condition.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// QueryInfo
//////////////////////////////////////////////////////////////////////////////////////////////////////

class SparrowHandler;
class PartitionSnapshot;
class TransientPartition;
class TableShare;
class QueryInfo {
private:

	uint32_t mysqlIndex_;
	uint32_t index_;
	const KEY* keyInfo_;
	SYSvector<uint8_t> keyBuffer_;
	KeyValue currentKey_;
	KeyValue minKey_;
	KeyValue maxKey_;
	key_part_map dataMap_;
	bool indexHasTimestamps_;
	SYSpHash<PartitionSnapshot> snapshots_;

private:

	QueryInfo(const QueryInfo& right);
	QueryInfo& operator = (const QueryInfo& right);

public:

	void clear(const bool resetContext);

	QueryInfo() : snapshots_(4) {
		clear(false);
	}

	~QueryInfo();

	void update(TableShare& share, TABLE& table, const uint32_t index);

	uint32_t getMySQLIndex() const {
		return mysqlIndex_;
	}

	uint32_t getIndex() const {
		return index_;
	}

	bool isCoveringIndex() const {
		return dataMap_ != 0;
	}

	key_part_map getDataMap() const {
		return dataMap_;
	}

	const KEY& getKeyInfo() const {
		return *keyInfo_;
	}

	const KeyValue& getCurrentKey() const {
		return currentKey_;
	}

	const KeyValue& getMinKey() const {
		return minKey_;
	}

	const KeyValue& getMaxKey() const {
		return maxKey_;
	}

	KeyValue& getCurrentKey() {
		return currentKey_;
	}

	KeyValue& getMinKey() {
		return minKey_;
	}

	KeyValue& getMaxKey() {
		return maxKey_;
	}

	uint32_t getKeyLength() const {
		return keyInfo_ == 0 ? 0 : keyInfo_->key_length;
	}

	void saveKey(key_part_map map);

	void saveKey(KeyValue& keyValue) const;

	bool stopOnFirstMatch(const KeyValue& keyValue) const {
		// While searching indexes, we can stop on first match only if the key is complete
		// (i.e. it uses all index columns) AND the current index does not contain any
		// TIMESTAMP key, because we store milliseconds and MySQL has only seconds.

		// TODO remove the check on TIMESTAMP key when MySQL supports milliseconds.
		// (see http://bugs.mysql.com/bug.php?id=8523).
		//return !indexHasTimestamps_ && (keyValue.getMap() + 1 == static_cast<key_part_map>(1 << getKeyInfo().user_defined_key_parts));
		return (keyValue.getMap() + 1 == static_cast<key_part_map>(1 << getKeyInfo().user_defined_key_parts));
	}

	void updateIndirectors();

	bool snapshotTransientPartition(TransientPartition* partition);

	const PartitionSnapshot* getSnapshot(const TransientPartition* partition) const;

	const SYSpHash<PartitionSnapshot>& getSnapshots() const {
		return snapshots_;
	}

	int compareKeys(const TableFields& fields, const KeyValue& leftKey, const KeyValue& rightKey) const;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// KeyIndirector
//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef SYSvector<uint32_t, 256> KeyIndirector;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Context
//////////////////////////////////////////////////////////////////////////////////////////////////////

class KeyComparator;
class SearchKeyComparator;
typedef SYSarray<uint8_t> KeyValues;
class TableShare;
class RecordWrapper;
class SerialRecordWrapper;
class PartSerialRecordWrapper;
typedef SYSsortedVector<uint32_t> Ids;
typedef GrowingByteBuffer InsertBuffer;
class Context {
	friend class KeyComparator;
	friend class SearchKeyComparator;

private:

	TABLE* table_;				// MySQL Table object.
	TableShare* share_;

	bool loaded_;
	bool writing_;
	bool altered_;

	ReferencedPartitions partitions_;			// Partitions loaded in this context.
	ReferencedPartitions mainPartitions_;		// Additional main partitions referenced by partitions_.
	PartitionReaders readers_;					// Multiple readers per partition.

	Ids unusableIndexes_;			// Indexes being added (alter) at the time this context is loaded.
	Ids updatableColumns_;

	QueryInfo queryInfo_;			// Query information.
	Position position_;				// Current position.
	Positions positions_;			// Candidate positions for each partition when scanning indexes.
	KeyValues keyValues_;			// Index key values for each position.
	KeyIndirector indirector_;		// Indirector to have sorted key values.
	uint32_t indirectorIndex_;		// Index in indirector for position_.

	SYSpHash<SerialRecordWrapper> recordWrappers_;
	SYSpHash<PartSerialRecordWrapper> recordWrappersPerPartition_;

	uint32_t insertRows_;
	InsertBuffer insertBuffer_;

protected:

	void load();
	void loadTimePeriods(TimePeriods& periods);

	uint8_t* keyValue(const uint32_t partition) {
		return keyValues_.data() + partition * queryInfo_.getKeyLength();
	}

	void sortKeyValues();

	void updateIndirector(const Position& position);

public:

	Context();

	void initialize(TABLE* table, TableShare* share);

	~Context();

	void clone(const Context& context);

	void reset();

	void getStats(ha_statistics& stats, const uint flag);

	const ReferencedPartitions& getPartitions() const {
		return partitions_;
	}

	ReferencedPartitions& getPartitions() {
		return partitions_;
	}

	QueryInfo& getQueryInfo() {
		return queryInfo_;
	}

	const QueryInfo& getQueryInfo() const {
		return queryInfo_;
	}

	TABLE& getTable() {
		return *table_;
	}

	const TableShare& getShare() const {
		return *share_;
	}

	TableShare& getShare() {
		return *share_;
	}

	PartitionReader* getReader(const uint32_t partition, const uint32_t index, const bool isString, const BlockCacheHint& hint) _THROW_(SparrowException);

	bool isUpdatableColumn(const uint32_t id) const {
		return updatableColumns_.contains(id);
	}

	uint64_t savePosition() const {
		return (static_cast<uint64_t>(position_.getPartition()) << 32) | static_cast<uint64_t>(position_.getRow());
	}

	Position restorePosition(const uint64_t pos) const {
		return Position(static_cast<uint32_t>(pos >> 32), static_cast<uint32_t>(pos));
	}

	static Str getQueryString();

	void resetPosition();

	bool moveNext(uint8_t* buffer);

	bool movePrevious(uint8_t* buffer);

	bool moveAbsolute(const uint64_t position, uint8_t* buffer);

	void setActiveIndex(const uint index);

	bool findRecord(const KeyValue& key, const enum ha_rkey_function findFlag, uint8_t* buffer);

	bool findFirstRecord(uint8_t* buffer);

	bool findLastRecord(uint8_t* buffer);

	bool findNextRecord(uint8_t* buffer);

	bool findPreviousRecord(uint8_t* buffer);

	uint64_t recordsTotal();

	uint64_t recordsInRange(const uint index, const key_range* minKey, const key_range* maxKey);

	const RecordWrapper& getRecordWrapper(const uint32_t serial, const uint32_t index, const bool tree);

	const RecordWrapper& getRecordWrapper(const uint32_t alterSerial, const uint32_t partSerial, const ColumnIds& skippedColumns);

	void writeLock();

	bool updateRecord(const uint8_t* buffer);

	void unlock();

	void startInsert(const uint32_t rows);

	bool insertRecord(const uint8_t* buffer);

	bool endInsert();

	void setAltered(bool altered) { altered_ = altered; }
	bool getAltered() const { return altered_; }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// KeyComparator
//////////////////////////////////////////////////////////////////////////////////////////////////////

class KeyComparator {
private:

	Context& context_;
	const QueryInfo& queryInfo_;
	const TableFields& fields_;

public:

	KeyComparator(Context& context);

	int compare(const uint32_t row1, const uint32_t row2) const {
		const Position& pos1 = context_.positions_[row1];
		const Position& pos2 = context_.positions_[row2];
		if (!pos1.isValid()) {
			return pos2.isValid() ? -1 : 0;
		}
		if (!pos2.isValid()) {
			return 1;
		}
		const KeyValue keyValue1(context_.keyValue(row1), HA_WHOLE_KEY);
		const KeyValue keyValue2(context_.keyValue(row2), HA_WHOLE_KEY);
		const int cmp = queryInfo_.compareKeys(fields_, keyValue1, keyValue2);
		if (cmp == 0) {
			return row1 > row2 ? 1 : (row1 < row2 ? -1 : 0);
		} else {
			return cmp;
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SearchKeyComparator
//////////////////////////////////////////////////////////////////////////////////////////////////////

class SearchKeyComparator {
private:

	Context& context_;
	const QueryInfo& queryInfo_;
	const TableFields& fields_;
	const KeyValue keyValue_;

public:

	SearchKeyComparator(Context& context, const Position& position);

	int compareTo(const uint32_t row) _THROW_(SparrowException) {
		const uint32_t partition = context_.indirector_[row >= context_.indirectorIndex_ ? row + 1 : row];
		const Position& pos = context_.positions_[partition];
		if (!pos.isValid()) {
			return -1;
		}
		return queryInfo_.compareKeys(fields_, KeyValue(context_.keyValue(partition), HA_WHOLE_KEY), keyValue_);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PartitionReaderGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

class PartitionReaderGuard {
private:

	PartitionReader* reader_;
	const bool owned_;

public:

	// For query processing.
	PartitionReaderGuard(Context& context, const uint32_t partition, const uint32_t index, const bool isString, const BlockCacheHint& hint) _THROW_(SparrowException)
		: reader_(context.getReader(partition, index, isString, hint)), owned_(false) {
	}

	// For coalescing and index alteration.
	PartitionReaderGuard(const PersistentPartition& partition, const uint32_t index, const bool isString, const BlockCacheHint& hint) _THROW_(SparrowException);

	// For coalescing.
	PartitionReaderGuard(PartitionReader* reader) : reader_(reader), owned_(false) {
	}

	~PartitionReaderGuard() {
		reader_->release();
		if (owned_) {
			delete reader_;
		}
	}

	PartitionReader& get() {
		return *reader_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ChangeIndexGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

class ChangeIndexGuard {
private:

	Context& context_;
	uint32_t savedMySqlIndex_;

public:

	ChangeIndexGuard(Context& context, const uint index) : context_(context) {
		QueryInfo& queryInfo = context.getQueryInfo();
		if (queryInfo.getMySQLIndex() == index) {
			savedMySqlIndex_ = UINT_MAX;
		} else {
			savedMySqlIndex_ = queryInfo.getMySQLIndex();
			context_.setActiveIndex(index);
		}
	}

	~ChangeIndexGuard() {
		if (savedMySqlIndex_ != UINT_MAX) {
			context_.setActiveIndex(savedMySqlIndex_);
		}
	}

};

}

#endif /* #ifndef _engine_context_h_ */
