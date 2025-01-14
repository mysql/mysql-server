/*
	Generic partition.
*/

#ifndef _engine_partition_h_
#define _engine_partition_h_

#include "types.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Position
//////////////////////////////////////////////////////////////////////////////////////////////////////

using RowNumber = uint32_t;
#define INVALID_PARTITION	UINT_MAX
#define INVALID_ROW			UINT_MAX
#define INVALID_TREE_NODE	UINT_MAX

// Position in a partition file (data or index).
class Position {
private:

	uint32_t partition_;		// Partition number.
	RowNumber row_;			// Row number in data file.
	RowNumber indexHint_;	// Index hint.
	RowNumber startHint_;	// Start index hint.
	RowNumber endHint_;		// End index hint.
	uint32_t treeHint_;		// Tree hint.

public:

	Position(const uint32_t partition = INVALID_PARTITION, const RowNumber row = INVALID_ROW, const RowNumber indexHint = INVALID_ROW,
		const RowNumber startHint = INVALID_ROW, const RowNumber endHint = INVALID_ROW, const uint32_t treeHint = INVALID_TREE_NODE)
		: partition_(partition), row_(row), indexHint_(indexHint), startHint_(startHint), endHint_(endHint), treeHint_(treeHint) {
	}

	void clear() {
		*this = Position(partition_);
	}

	bool isValid() const {
		return partition_ != INVALID_PARTITION && row_ != INVALID_ROW;
	}

	bool hasIndexHint() const {
		return indexHint_ != INVALID_ROW;
	}

	bool hasIntervalHint() const {
		return startHint_ != INVALID_ROW && endHint_ != INVALID_ROW;
	}

	bool hasTreeHint() const {
		return treeHint_ != INVALID_TREE_NODE;
	}

	uint32_t getPartition() const {
		return partition_;
	}

	RowNumber getRow() const {
		return row_;
	}

	RowNumber getIndexHint() const {
		return indexHint_;
	}

	RowNumber getStartHint() const {
		return startHint_;
	}

	RowNumber getEndHint() const {
		return endHint_;
	}

	uint32_t getTreeHint() const {
		return treeHint_;
	}

	void setPartition(const uint32_t partition) {
		partition_ = partition;
	}

	void setRow(const RowNumber row) {
		row_ = row;
	}

	Str toString() const {
		char buffer[256];
		buffer[0] = 0;
		char* s = buffer + sprintf(buffer, "partition ");
		if (partition_ == INVALID_PARTITION) {
			s += sprintf(s, "N/A");
		} else {
			s += sprintf(s, "%u", partition_);
		}
		s += sprintf(s, ", row ");
		if (row_ == INVALID_ROW) {
			s += sprintf(s, "N/A");
		} else {
			s += sprintf(s, "%llu", static_cast<ulonglong>(row_));
		}
		s += sprintf(s, " (hints: index=");
		if (indexHint_ == INVALID_ROW) {
			s += sprintf(s, "N/A");
		} else {
			s += sprintf(s, "%llu", static_cast<ulonglong>(indexHint_));
		}
		s += sprintf(s, ", start=");
		if (startHint_ == INVALID_ROW) {
			s += sprintf(s, "N/A");
		} else {
			s += sprintf(s, "%llu", static_cast<ulonglong>(startHint_));
		}
		s += sprintf(s, ", end=");
		if (endHint_ == INVALID_ROW) {
			s += sprintf(s, "N/A");
		} else {
			s += sprintf(s, "%llu", static_cast<ulonglong>(endHint_));
		}
		s += sprintf(s, ", tree=");
		if (treeHint_ == INVALID_TREE_NODE) {
			s += sprintf(s, "N/A");
		} else {
			s += sprintf(s, "%u", treeHint_);
		}
		s += sprintf(s, ")");
		return Str(buffer);
	}
};

typedef SYSvector<Position> Positions;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// KeyValue
//////////////////////////////////////////////////////////////////////////////////////////////////////

class KeyValue {
private:

	uint8_t* key_;
	key_part_map map_;

public:

	KeyValue(uint8_t* key = 0, const key_part_map map = 0) : key_(key), map_(map) {
	}
	explicit KeyValue(const key_range* range) : key_(const_cast<uint8_t*>(range->key)), map_(range->keypart_map) {
	}
	const uint8_t* getKey() const {
		return key_;
	}
	uint8_t* getKey() {
		return key_;
	}
	key_part_map getMap() const {
		return map_;
	}		 
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SearchFlag
//////////////////////////////////////////////////////////////////////////////////////////////////////

class SearchFlag {
public:

	enum Type {
		EQ,
		GE,
		LE,
		LE_LAST,
		GT,
		LT
	};

private:

	const enum Type type_;

public:

	static Type getType(const enum ha_rkey_function findFlag) {
		switch(findFlag) {
			case HA_READ_KEY_EXACT: return EQ;
			case HA_READ_KEY_OR_NEXT: return GE;
			case HA_READ_KEY_OR_PREV: return LE;
			case HA_READ_AFTER_KEY: return GT;
			case HA_READ_BEFORE_KEY: return LT;
			case HA_READ_PREFIX: return GE;
			case HA_READ_PREFIX_LAST: return LE_LAST;
			case HA_READ_PREFIX_LAST_OR_PREV: return LE_LAST;
			default: assert(0); return EQ;
		}
	}
	explicit SearchFlag(const enum ha_rkey_function findFlag) : type_(getType(findFlag)) {
	}
	SearchFlag(const Type type) : type_(type) {
	}
	bool operator == (const Type type) const {
		return type_ == type;
	}
	operator Type() const {
		return type_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Partition
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Context;
class QueryInfo;
class Partition : public RefCounted {
protected:

	uint64_t serial_;
	uint64_t dataSerial_;
	uint32_t filesystem_;
	uint32_t indexAlterSerial_;	// Index alteration serial number; to be compared to Master::indexAlterSerial_.
	uint32_t columnAlterSerial_;	// Column alteration serial number; to be compared to Column::serial_.

public:

	Partition(const uint64_t serial, const uint64_t dataSerial, const uint32_t filesystem, const uint32_t indexAlterSerial,
		const uint32_t columnAlterSerial)
		: serial_(serial), dataSerial_(dataSerial), filesystem_(filesystem), indexAlterSerial_(indexAlterSerial),
		columnAlterSerial_(columnAlterSerial) {
	}

	virtual ~Partition() {
	}

	virtual void detach() = 0;

	// Attributes.

	uint64_t getSerial() const {
		return serial_;
	}
	
	uint64_t getDataSerial() const {
		return dataSerial_;
	}

	bool isMain() const {
		return getSerial() == getDataSerial();
	}

	uint32_t getFilesystem() const {
		return filesystem_;
	}

	void setFilesystem(const uint32_t filesystem) {
		filesystem_ = filesystem;
	}

	uint32_t getIndexAlterSerial() const {
		return indexAlterSerial_;
	}

	void setIndexAlterSerial(const uint32_t indexAlterSerial) {
		indexAlterSerial_ = indexAlterSerial;
	}

	uint32_t getColumnAlterSerial() const {
		return columnAlterSerial_;
	}

	void setColumnAlterSerial(const uint32_t columnAlterSerial) {
		columnAlterSerial_ = columnAlterSerial;
	}

	virtual TimePeriod getPeriod() const = 0;

	virtual uint32_t getRecords() const = 0;

	virtual uint64_t getDataSize() const = 0;

	virtual uint64_t getIndexSize() const = 0;

	virtual bool isTransient() const = 0;

	virtual bool isReady() const = 0;

	virtual bool isIndexAlterable() const = 0;

	bool isTemporary() const {
		return getPeriod().getMax() == 0;
	}

	// Data access.

	virtual Position indexFind(Context& context, const uint32_t partition, const KeyValue& key, const SearchFlag searchFlag) const = 0;

	virtual Position indexFirst(Context& context, const uint32_t partition) const = 0;

	virtual Position indexLast(Context& context, const uint32_t partition) const = 0;

	virtual Position indexNext(Context& context, const Position& position) const = 0;

	virtual Position indexPrevious(Context& context, const Position& position) const = 0;

	virtual Position moveNext(Context& context, const Position& position) const = 0;

	virtual Position movePrevious(Context& context, const Position& position) const = 0;

	virtual Position moveAbsolute(Context& context, const Position& position) const = 0;

	virtual Position moveFirst(Context& context, const uint32_t partition) const = 0;

	virtual Position moveLast(Context& context, const uint32_t partition) const = 0;

	virtual uint32_t recordsInRange(Context& context, const uint32_t partition, const key_range* minKey, const key_range* maxKey) const = 0;

	virtual bool readKey(Context& context, const Position& position, const bool forward,
		const key_part_map keyPartMap, uint8_t* buffer, const bool keyFormat) const = 0;

	virtual bool readData(Context& context, const Position& position, uint8_t* buffer, const BlockCacheHint& hint) const = 0;

	virtual bool updateData(Context& context, const Position& position, const uint8_t* buffer) = 0;

	// Comparison.
	bool operator == (const Partition& right) const {
		return serial_ == right.serial_;
	}

	bool operator < (const Partition& right) const {
		return serial_ < right.serial_;
	}

	// Hash.
	uint32_t hash() const {
		return 31 + static_cast<uint32_t>(serial_ ^ (serial_ >> 32));
	}
};

typedef SYSpSortedVector<Partition, 256> Partitions;
typedef SYSpSortedVector<Partition, 0> ChildPartitions;
typedef RefPtr<Partition> PartitionGuard;
typedef SYSsortedVector<PartitionGuard, 256> ReferencedPartitions;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PartitionKey
//////////////////////////////////////////////////////////////////////////////////////////////////////

class PartitionKey : public Partition {
public:

	PartitionKey(const uint64_t serial) : Partition(serial, 0, 0, 0, 0) {
	}

	void detach() override {
	}

	TimePeriod getPeriod() const override {
		return TimePeriod();
	}

	uint32_t getRecords() const override {
		return 0;
	}

	uint64_t getDataSize() const override {
		return 0;
	}

	uint64_t getIndexSize() const override {
		return 0;
	}

	bool isTransient() const override {
		return true;
	}

	bool isReady() const override {
		return true;
	}

	bool isIndexAlterable() const override {
		return false;
	}

	Position indexFind(Context& context, const uint32_t partition, const KeyValue& key, const SearchFlag searchFlag) const override {
		return Position(partition);
	}

	Position indexFirst(Context& context, const uint32_t partition) const override {
		return Position(partition);
	}

	Position indexLast(Context& context, const uint32_t partition) const override {
		return Position(partition);
	}

	Position indexNext(Context& context, const Position& position) const override {
		return Position(position.getPartition());
	}

	Position indexPrevious(Context& context, const Position& position) const override {
		return Position(position.getPartition());
	}

	Position moveNext(Context& context, const Position& position) const override {
		return Position(position.getPartition());
	}

	Position movePrevious(Context& context, const Position& position) const override {
		return Position(position.getPartition());
	}

	Position moveAbsolute(Context& context, const Position& position) const override {
		return Position(position.getPartition());
	}

	Position moveFirst(Context& context, const uint32_t partition) const override {
		return Position(partition);
	}

	Position moveLast(Context& context, const uint32_t partition) const override {
		return Position(partition);
	}

	uint32_t recordsInRange(Context& context, const uint32_t partition, const key_range* minKey, const key_range* maxKey) const override {
		return 0;
	}

	bool readKey(Context& context, const Position& position, const bool forward,
		const key_part_map keyPartMap, uint8_t* buffer, const bool keyFormat) const override {
		return false;
	}

	bool readData(Context& context, const Position& position, uint8_t* buffer, const BlockCacheHint& hint) const override {
		return false;
	}

	bool updateData(Context& context, const Position& position, const uint8_t* buffer) override {
		return false;
	}
};

}

#endif /* #ifndef _engine_partition_h_ */
