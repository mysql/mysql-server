/*
	Transient partition.
*/

#ifndef _engine_transient_h_
#define _engine_transient_h_

#include "master.h"
#include "cache.h"
#include "sort.h"
#include "binbuffer.h"

#include "../engine/log.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DataReader
//////////////////////////////////////////////////////////////////////////////////////////////////////

// To unmarshall inserted data.
class DataReader {
private:

	ByteBuffer& buffer_;
	BinBuffer& binBuffer_;

public:

	DataReader(ByteBuffer& buffer, BinBuffer& binBuffer)
		: buffer_(buffer), binBuffer_(binBuffer) {
	}

	DataReader& operator >> (int8_t& v) {
		buffer_ >> v;
		return *this;
	}

	DataReader& operator >> (uint8_t& v) {
		buffer_ >> v;
		return *this;
	}

	DataReader& operator >> (int16_t& v) {
		buffer_ >> v;
		return *this;
	}

	DataReader& operator >> (uint16_t& v) {
		buffer_ >> v;
		return *this;
	}

	DataReader& operator >> (int32_t& v) {
		buffer_ >> v;
		return *this;
	}

	DataReader& operator >> (uint32_t& v) {
		buffer_ >> v;
		return *this;
	}

	DataReader& operator >> (int64_t& v) {
		buffer_ >> v;
		return *this;
	}

	DataReader& operator >> (uint64_t& v) {
		buffer_ >> v;
		return *this;
	}

	DataReader& operator >> (double& v) {
		buffer_ >> v;
		return *this;
	}

	DataReader& operator >> (BinString*& string) {
		uint32_t length;
		buffer_ >> length;
		const uint64_t pos = buffer_.position();
		if (pos + length <= buffer_.limit()) {
			// The string is in the buffer.
			string = binBuffer_.insert(buffer_.getCurrentData(), length);
			buffer_.position(pos + length);
		} else {
			// The string crosses the buffer boundary; use a temporary buffer.
			ByteBuffer stringBuffer(static_cast<uint8_t*>(IOContext::getTempBuffer1(length)), length);
			buffer_ >> stringBuffer;
			string = binBuffer_.insert(stringBuffer.getData(), length);
		}
		return *this;
	}

	bool end() const {
		return buffer_.end();
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ColumnComparator
//////////////////////////////////////////////////////////////////////////////////////////////////////

class ColumnComparator {
public:

	// Blobs and strings.
	static int compare(const BinString& v1, const BinString& v2, CHARSET_INFO* cs) {
		return cs->coll->strnncollsp(cs, v1.getData(), v1.getLength(), v2.getData(), v2.getLength());
	}

	// Bytes.
	static int compare(const int8_t v1, const int8_t v2) {
		return v1 > v2 ? 1 : (v1 < v2 ? -1 : 0);
	}

	static int compare(const uint8_t v1, const uint8_t v2) {
		return v1 > v2 ? 1 : (v1 < v2 ? -1 : 0);
	}

	// Shorts.
	static int compare(const int16_t v1, const int16_t v2) {
		return v1 > v2 ? 1 : (v1 < v2 ? -1 : 0);
	}

	static int compare(const uint16_t v1, const uint16_t v2) {
		return v1 > v2 ? 1 : (v1 < v2 ? -1 : 0);
	}

	// Integers.
	static int compare(const int32_t v1, const int32_t v2) {
		return v1 > v2 ? 1 : (v1 < v2 ? -1 : 0);
	}

	static int compare(const uint32_t v1, const uint32_t v2) {
		return v1 > v2 ? 1 : (v1 < v2 ? -1 : 0);
	}

	// Longs.
	static int compare(const int64_t v1, const int64_t v2) {
		return v1 > v2 ? 1 : (v1 < v2 ? -1 : 0);
	}

	// Timestamps.
	static int compare(const uint64_t v1, const uint64_t v2) {
		return v1 > v2 ? 1 : (v1 < v2 ? -1 : 0);
	}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif 

	// Doubles.
	static int compare(const double v1, const double v2) {
		if (v1 < v2) {
            return -1;
		} else if (v1 > v2) {
            return 1;
		}
		// Handle properly special cases.
		const uint64_t u1 = *(const uint64_t*)&v1;
		const uint64_t u2 = *(const uint64_t*)&v2;
		return u1 == u2 ? 0 : (u1 < u2 ? -1 : 1);
	}
	
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif 

};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ColumnAccessor
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* Stores column values. ColumnAccessor is the base class defining the interface. 
	It has a default implementation for some basic methods (NULL management for example).
*/	

class ColumnAccessor {
private:

	const uint32_t columnId_;
	const Column column_;
	SYSbitVector<SYSallocator<uint64_t> > nulls_;

protected:

	virtual int compareRows(const uint32_t row1, const uint32_t row2) const = 0;
	virtual void clearData() = 0;
	virtual void shrinkData(const uint32_t length) = 0;
	virtual int64_t getSize() const = 0;

public:

	ColumnAccessor(const uint32_t columnId, const Column& column) : columnId_(columnId), column_(column) {
	}
	virtual ~ColumnAccessor() {
	}
	uint32_t getColumnId() const {
		return columnId_;
	}
	const Column& getColumn() const {
		return column_;
	}
	bool isNullable() const {
		return column_.isFlagSet(COL_NULLABLE);
	}
	bool isNull(const uint32_t row) const {
		return isNullable() ? (row < nulls_.length() ? nulls_[row] : false) : false;
	}
	bool areAllNulls() const {
		return (nulls_.length() == length() && nulls_.areAll(true));
	}
	void setNull(const uint32_t row) {
		if (isNullable()) {
			nulls_.setBit(row);
		}
	}
	void insertNull(const uint32_t row) {
		nulls_.setBit(row);
		insertDummy();
	}
	void resetNull(const uint32_t row) {
		nulls_.clearBit(row);
	}
	void insertNull() {
		insertNull(length());
	}
	virtual void insertDummy(bool real=false) = 0;
	virtual void insertValue(DataReader& reader) = 0;
	virtual int64_t insertAutoInc(const int64_t autoInc) = 0;
	int compare(const uint32_t row1, const uint32_t row2) const {
		if (isNull(row1)) {	// NULLs are the smallest values.
			return isNull(row2) ? 0 : -1;
		} else if (isNull(row2)) {
			return 1;
		}
		return compareRows(row1, row2);
	}
	virtual uint32_t length() const = 0;
	void shrink(const uint32_t length) {
		nulls_.shrink(length);
		shrinkData(length);
	}
	void clear() {
		nulls_.clear();
		clearData();
	}
	virtual uint8_t getBitValues(const uint32_t row) const = 0;
	virtual void write(ByteBuffer& buffer, const uint32_t row) const _THROW_(SparrowException) = 0;
	virtual uint64_t getValue(const uint32_t row) const = 0;
	virtual void writeValue(const uint32_t row, const uint64_t data) = 0;
	int64_t getTotalSize() const {
		return getSize() + nulls_.getSize();
	}
};

typedef SYSpVector<ColumnAccessor, 0> ColumnAccessors;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ColumnAccessorSimple
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* Generic implementation of ColumnAccessor for basic types. Values are stored in a SYSlvector<T>
*/

template<typename T> class ColumnAccessorSimple : public ColumnAccessor, public SYSlvector<T> {
protected:

	void insertDummy(bool real=false) override {
		this->append((T)0);
	}

	int compareRows(const uint32_t row1, const uint32_t row2) const override {
		return ColumnComparator::compare((*this)[row1], (*this)[row2]);
	}

	void clearData() override {
		SYSlvector<T>::clear();
	}

	void shrinkData(const uint32_t length) override {
		SYSlvector<T>::shrink(length);
	}

	int64_t getSize() const override {
		return SYSlvector<T>::getSize();
	}

public:

	ColumnAccessorSimple(const uint32_t columnId, const Column& column) : ColumnAccessor(columnId, column) {
	}

	uint8_t getBitValues(const uint32_t row) const override {
		return isNull(row) ? 1 : 0;
	}

	uint32_t length() const override {
		return SYSlvector<T>::length();
	}

	void insertValue(DataReader& reader) override {
		T t;
		reader >> t;
		this->append(t);
	}

	int64_t insertAutoInc(const int64_t autoInc) override {
		return autoInc;
	}

	void write(ByteBuffer& buffer, const uint32_t row) const override _THROW_(SparrowException) {
		if (isNull(row)) {
			buffer << (T)0;
		} else {
			buffer << (*this)[row];
		}
	}

	uint64_t getValue(const uint32_t row) const override {
		return static_cast<uint64_t>((*this)[row]);
	}

	void writeValue(const uint32_t row, const uint64_t data) override {
		(*this)[row] = static_cast<T>(data);
	}
};

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif 

// Specialization for doubles.
template<> inline uint64_t ColumnAccessorSimple<double>::getValue(const uint32_t row) const {
	return *(const uint64_t*)&(*this)[row];
}

template<> inline void ColumnAccessorSimple<double>::writeValue(const uint32_t row, const uint64_t data) {
	(*this)[row] = *(const double*)&data;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif 

// Specialization for int64_t.
template<> inline int64_t ColumnAccessorSimple<int64_t>::insertAutoInc(const int64_t autoInc) {
	append(autoInc);
	return autoInc + 1;
}

// Specialization for uint64_t.
template<> inline int64_t ColumnAccessorSimple<uint64_t>::insertAutoInc(const int64_t autoInc) {
	append(static_cast<uint64_t>(autoInc));
	return autoInc + 1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ColumnAccessorBin
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* Specialized implementation of the ColumnAccessor interface for Strings. 
	Works for small strings (length <= 16 bytes) and long strings. 
	. BinBuffer& buffer_ contains the actual strings in one BIG buffer. The buffer is a reference to TransientPartition::binBuffer_.
	. BinStrings is a vector having as many items as rows in the partition this ColumnAccessorBin is used by. 
	Each item in the BinStrings points to a string in buffer_.
*/

class ColumnAccessorBin : public ColumnAccessor, public BinStrings {
private:

	BinBuffer& buffer_;
	CHARSET_INFO* cs_;

protected:

	void insertDummy(bool real=false) override {
		static BinString	dummy;
		if ( real ) {
			append(&dummy);
		} else {
			append(0);
		}
	}

	int compareRows(const uint32_t row1, const uint32_t row2) const override {
		return ColumnComparator::compare(*(*this)[row1], *(*this)[row2], cs_);
	}

	void clearData() override {
		BinStrings::clear();
	}

	void shrinkData(const uint32_t length) override {
		BinStrings::shrink(length);
	}

	int64_t getSize() const override {
		return BinStrings::getSize();
	}

public:

	ColumnAccessorBin(const uint32_t columnId, const Column& column, BinBuffer& buffer)
		: ColumnAccessor(columnId, column), buffer_(buffer) {
		CHARSET_INFO* cs = get_charset_by_name(column.getCharset().c_str(), MYF(MY_WME));
		if (cs == 0) {
			if (column.getType() == COL_STRING) {
				cs = &my_charset_utf8mb4_bin;
			} else {
				cs = &my_charset_bin;
			}
		}
		cs_ = cs;
	}

	uint8_t getBitValues(const uint32_t row) const override {
		if (isNull(row)) {
			return 1;
		} else {
			const BinString& string = *(*this)[row];
			if (isNullable()) {
				return string.isSmall() ? (string.getLength() << 1) : 0;
			} else {
				return string.isSmall() ? string.getLength() : 0;
			}
		}
	}

	// Insert a new string in buffer_ (if it does not exist already) and add a reference to it in this column's current row.
	void insertValue(const uint32_t row, const char* s, const uint32_t length) {
		resetNull(row);
		(*this)[row] = buffer_.insert(reinterpret_cast<const uint8_t*>(s), length);
	}

	int64_t insertAutoInc(const int64_t autoInc) override {
		return autoInc;
	}

	uint32_t length() const override {
		return BinStrings::length();
	}

	void insertValue(DataReader& reader) override {
		BinString* string;
		reader >> string;
		append(string);
	}

	// Short string (length <= 16 bytes) are padded to 16 bytes with 0.
	// Long strings (length > 16 bytes) are stored in the format:
	//	. length on 1 or 2 bytes depending on string length
	//	. string content
	void write(ByteBuffer& buffer, const uint32_t row) const override _THROW_(SparrowException) {
		if (isNull(row)) {
			buffer << static_cast<uint64_t>(0) << static_cast<uint64_t>(0);
		} else {
			const BinString& string = *(*this)[row];
			const uint32_t length = string.getLength();
			if (string.isSmall()) {
				buffer << ByteBuffer(string.getData(), length);
				const uint32_t remainder = 16 - length;
				for (uint32_t i = 0; i < remainder; ++i) {
					buffer << static_cast<uint8_t>(0);
				}
			} else {
				buffer << string.getOffset() << static_cast<uint64_t>(length);
			}
		}
	}

	uint64_t getValue(const uint32_t row) const override {
		return (uint64_t)(*this)[row];
	}

	void writeValue(const uint32_t row, const uint64_t data) override {
		assert(0);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// RecordWriter
//////////////////////////////////////////////////////////////////////////////////////////////////////

class RecordWriter {
private:

	ColumnAccessors accessors_;
	uint32_t bits_;
	uint32_t size_;

public:

	RecordWriter(const ColumnAccessors& accessors, const ColumnIds* columnIds);

	uint32_t getSize() const {
		return size_;
	}

	void write(ByteBuffer& buffer, const uint32_t row) _THROW_(SparrowException);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ConcurrentIndirector
//////////////////////////////////////////////////////////////////////////////////////////////////////

// For partition snapshots.
typedef SYSlvector<uint32_t> ConcurrentIndirector;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Indirector
//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef SYSxvector<uint32_t> Indirector;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TransientPartition
//////////////////////////////////////////////////////////////////////////////////////////////////////

// The transient partition is a special partition.
// Incoming data are first written to this partition. When the partition is full or too old,
// indexes are generated and everything is written to disk.
// Also, the transient partition may be browsed when a query is being processed by the engine.
class PersistentPartition;
class FlushTask;
class DnsTask;
class PartitionSnapshot;
class SizeGuard;
class RowComparator;
class TransientMutationGuard;
typedef SYSvector<bool> IndexStringFlags;
class TransientPartition : public Partition {
	friend class SizeGuard;
	friend class RowComparator;
	friend class TransientMutationGuard;

private:

	// Parent master file.
	MasterGuard master_;

	// DNS configuration.
	DnsConfigurationGuard dnsConfiguration_;

	// Column accessors.
	ColumnAccessors accessors_;

	// Columns that contain no valid data (all values are NULL).
	ColumnIds	emptyColumnsIds_;

	// Timestamp accessor.
	ColumnAccessorSimple<uint64_t>* timestampAccessor_;

	// Min/max timestamps.
	uint64_t minTimestamp_;
	uint64_t maxTimestamp_;

	// DNS identifier accessor, may be null.
	ColumnAccessor* dnsIdAccessor_;

	// Buffer where strings and blobs are stored. This BinBuffer stores all strings and blobs for this partition, whatever column they belong to. 
	//	That includes IP addresses and IP lookups. 
	BinBuffer binBuffer_;

	// Column ids for all indexes.
	IndexIds indexIds_;
	ColumnIdsArray columnIds_;
	bool hasString_;
	IndexStringFlags indexStringFlags_;

	// Lock to protect data in transient columns.
	RWLock lock_;

	// Creation timestamp.
	const uint64_t timestamp_;

	// Records ready to be queried.
	uint32_t records_;

	// Counter to know when this partition is flushed to disk.
	volatile uint32_t jobCounter_;

	// Counter of errors while flushing. If an error occurs, partition is lost.
	volatile uint32_t errors_;

	volatile uint32_t flush_tries_;

	// Flag indicating if this partition is done (full or being flushed).
	bool done_;

	// Number of pending DNS queries for this partition.
	volatile uint32_t dnsPending_;

	// Flag indicating if flushing has started.
	bool flush_;

	// File sizes.
	uint64_t dataSize_;
	uint64_t indexSize_;

	// String info.
	uint64_t stringOffset_;
	uint64_t stringSize_;

	// To resolve IP addresses.
	SYSvector<ColumnAccessorBin*> dnsIpAccessors_;
	SYSvector<ColumnAccessorBin*> dnsLookupAccessors_;

	// Flush timestamp.
	uint64_t flushTimestamp_;

	// Partition size
	uint64_t size_;

	static TimePeriod voidPeriod_;

	// Condition to know if insertion is possible.
	static Lock condLock_;
	static Cond insertCond_;

	// To wait until flushs are completed.
	static volatile uint32_t flushs_;		// Counts the number of flush task pending or currently executing.
	static Cond flushCond_;				// Signaled when flushs_ reaches 0

	static Lock PartLock_;
	static SYSvector<TransientPartition*, 256> AllTransPartitions_;		// List of all transient partitions, first one is oldest, last is newest
	static SYSvector<TransientPartition*, 256> FlushingPartitions_;
	static uint64_t sizeFlushing_;

	static void addPartition(TransientPartition* partition) {
		Guard	guard(PartLock_);
		AllTransPartitions_.append(partition);
	}

	static void updateFlushingSize(TransientPartition* partition, const int64_t& delta) {
		Guard	guard(PartLock_);
		uint64_t	size = partition->getCachedSize();
		assert(delta > 0 || static_cast<int64_t>(size) >= delta);
		size = static_cast<uint64_t>(static_cast<int64_t>(size) + delta);
		DBUG_PRINT("sparrow_transient", ("Update %s.%s.%llu size to %lu", partition->getMaster()->getDatabase().c_str(), partition->getMaster()->getTable().c_str(), 
			static_cast<ulonglong>(partition->getSerial()), size));
		partition->setCachedSize(size);
		if (FlushingPartitions_.contains(partition)) {
			sizeFlushing_ += delta;
			DBUG_PRINT("sparrow_transient", ("Updating tot size to %llu (delta %lld).", static_cast<ulonglong>(sizeFlushing_), static_cast<longlong>(delta)));
		}
	}

	static void flushingPartitionNoLock(TransientPartition* partition) {
		if (AllTransPartitions_.remove(partition)) {
			const uint64_t		size = partition->getCachedSize();
			assert(FlushingPartitions_.contains(partition) == false);
			FlushingPartitions_.append(partition);
			sizeFlushing_ += size;
			DBUG_PRINT("sparrow_transient", ("Flushing partition %s.%s.%llu, size %llu, tot size %llu, nb transient %u, nb flushing %u.",
				partition->getMaster()->getDatabase().c_str(), partition->getMaster()->getTable().c_str(), static_cast<ulonglong>(partition->getSerial()), 
				static_cast<ulonglong>(size), static_cast<ulonglong>(sizeFlushing_), AllTransPartitions_.entries(), FlushingPartitions_.entries()));

		} else {
			assert(FlushingPartitions_.contains(partition) == true);
			assert(sizeFlushing_ >= static_cast<uint64_t>(partition->getCachedSize()));
			DBUG_PRINT("sparrow_transient", ("Flushing forced partition %s.%s.%llu, size %llu, tot size %llu, nb transient %u, nb flushing %u.",
				partition->getMaster()->getDatabase().c_str(), partition->getMaster()->getTable().c_str(), static_cast<ulonglong>(partition->getSerial()), 
				static_cast<ulonglong>(partition->getCachedSize()), static_cast<ulonglong>(sizeFlushing_), AllTransPartitions_.entries(), FlushingPartitions_.entries()));
		}
	}

	static void flushingPartition(TransientPartition* partition) {
		Guard	guard(PartLock_);
		flushingPartitionNoLock(partition);
	}

	static void flushedPartition(TransientPartition* partition) {
		Guard	guard(PartLock_);
		const uint64_t		size = partition->getCachedSize();
		if (partition->flush_) {
			assert(AllTransPartitions_.contains(partition) == false);
			[[maybe_unused]] bool	removed = FlushingPartitions_.remove(partition);
			assert(removed == true);
		} else {
		AllTransPartitions_.remove(partition);
			FlushingPartitions_.remove(partition);
		}
		sizeFlushing_ -= (sizeFlushing_ > size ? size : sizeFlushing_);
		DBUG_PRINT("sparrow_transient", ("Flushed partition %s.%s.%llu, size %llu, tot size %llu, nb transient %u, nb flushing %u.",
			partition->getMaster()->getDatabase().c_str(), partition->getMaster()->getTable().c_str(), static_cast<ulonglong>(partition->getSerial()), 
			static_cast<ulonglong>(partition->getCachedSize()), static_cast<ulonglong>(sizeFlushing_), AllTransPartitions_.entries(), FlushingPartitions_.entries()));
	}

	static uint64_t flushOldestPartitions(const uint64_t& sizeToFlush);

private:

	static Str getName(Master* master, const uint64_t serial, const char* name);

	void initialize();

	int compare(const ColumnPos& columnPos, const int row1, const int row2, const bool sortByRow) const {
		for (uint32_t i = 0; i < columnPos.length(); ++i) {
			const int cmp = accessors_[columnPos[i]]->compare(row1, row2);
			if (cmp != 0) {
				return cmp;
			}
		}
		if (sortByRow) {
			// Sort by row: in case of identical values, we get a better locality.
			return row1 > row2 ? 1 : (row1 < row2 ? -1 : 0);
		} else {
			return 0;
		}
	}

	uint64_t dnsLookup(const uint32_t start, const bool lastPass);

	void scheduleFlush(const uint64_t dnsTimestamp);

	void doFlush(PersistentPartitionGuard mainPartition);

	void refreshEmptyColumns();

	// Copy and assignment are forbidden.
	TransientPartition(const TransientPartition& right);
	TransientPartition& operator = (const TransientPartition& right);

public:

	TransientPartition(Master* master, const uint64_t serial);

	~TransientPartition();

	Master* getMaster() {
		return master_.get();
	}

	void clear();

	void detach() override {
		master_ = 0;
	}

	const ColumnAccessors& getAccessors() const {
		return accessors_;
	}

	const ColumnIds& getColumnIds(const uint32_t index) const {
		uint32_t id = UINT_MAX;
		for (uint32_t i = 0; i < indexIds_.length(); ++i) {
			if (indexIds_[i] == index) {
				id = i;
				break;
			}
		}
		return columnIds_[id];
	}

	void getColumnPos(ColumnPos& pos, const ColumnIds& ids) const {
		pos.clear();
		pos.resize(ids.entries());
		for (uint32_t i = 0; i < ids.length(); ++i) {
			uint32_t	id = ids[i];
			uint32_t	j = 0;
			for (; j < accessors_.entries(); ++j) {
				if (accessors_[j]->getColumnId() == id)
					break;
			}
			assert(j < accessors_.entries());
			if (j == accessors_.entries()) {
				throw SparrowException::create(false, "Can't find column id %u in valid column list for partition %s.%s.%llu.", 
					id, master_.get()->getDatabase().c_str(), master_.get()->getTable().c_str(), static_cast<ulonglong>(getSerial()));
			}
			pos.append(j);
		}
	}

	PartitionSnapshot* snapshot();

	// Attributes.

	uint32_t getRecords() const override {
		return records_;
	}
	uint32_t getRecordsSafe() const {
		ReadGuard guard(const_cast<RWLock&>(lock_));
		return records_;
	}
	TimePeriod getPeriodNoLock() const {
		if (records_ == 0) {
			return voidPeriod_;
		} else {
			return TimePeriod(minTimestamp_, maxTimestamp_);
		}
	}
	TimePeriod getPeriod() const override {
		ReadGuard guard(const_cast<RWLock&>(lock_));
		return getPeriodNoLock();
	}
	uint64_t getDataSize() const override {
		return dataSize_;
	}
	uint64_t getIndexSize() const override {
		return indexSize_;
	}

	bool isTransient() const override {
		return true;
	}

	bool isReady() const override {
		return master_->getIndexAlterSerial() == getIndexAlterSerial();
	}

	bool isIndexAlterable() const override {
		return false;
	}

	const ColumnIds& getEmptyColumns() const {
		return emptyColumnsIds_;
	}

	void setEmptyColumns(const ColumnIds& emptyColumnsIds) {
		emptyColumnsIds_ = emptyColumnsIds;
	}

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

	// Insertion and flushing.

	static bool waitForRoom(volatile bool& aborting);

	bool insert(ByteBuffer& buffer, const uint32_t rows, uint64_t& timestamp) _THROW_(SparrowException);

	bool insert(ByteBuffer& buffer, const uint32_t rows, const Names& columns, const ColumnIds& colIds, uint64_t& timestamp) _THROW_(SparrowException);

	void dnsUpdate();

	bool isDone() {
		ReadGuard guard(lock_);
		return done_;
	}

	void flushStrings(PersistentPartitionGuard mainPartition) _THROW_(SparrowException);

	void compute(PersistentPartitionGuard mainPartition, const uint32_t id);

	void write(PersistentPartitionGuard mainPartition, const uint32_t indexId, const Indirector* indirector) _THROW_(SparrowException);

	void updateDnsConfiguration(DnsConfiguration* dnsConfiguration);

	bool flush(const uint64_t timestamp, bool master_lock_taken=false, bool force=false);

	bool forceFlush(bool master_lock_taken=false);

	uint32_t getNbFlushTries() const { return flush_tries_; }

	void incJobCounter() {
		Atomic::inc32(&jobCounter_);
	}

	bool decJobCounter() {
		return (Atomic::dec32(&jobCounter_) == 0);
	}

	void setJobCounter(const uint32_t jobCounter) {
		jobCounter_ = jobCounter;
	}

	uint32_t getJobCounter() {
		return jobCounter_;
	}

	void endFlush(PersistentPartitionGuard mainPartition);

	void error() {
		Atomic::inc32(&errors_);
	}

	void resetFlush() {
		flush_ = false;
		Atomic::add32(&errors_, -static_cast<int32_t>(errors_));
	}

	bool mutate(PersistentPartitionGuard mainPartition) _THROW_(SparrowException);

	uint64_t getCachedSize() const {
		return size_;
	}

	void setCachedSize(const uint64_t size) {
		size_ = size;
	}

	int64_t getSize() const {
		int64_t size = binBuffer_.getSize();
		for (uint32_t i = 0; i < accessors_.length(); ++i) {
			size += accessors_[i]->getTotalSize();
		}
		return size;
	}

	static void waitForFlushs();

	static uint32_t getNbFlushs() {
		Guard flushGuard(TransientPartition::condLock_);
		return flushs_;
	}

	static uint64_t getSizeFlushing() { return sizeFlushing_; }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TransientMutationGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

class TransientMutationGuard {
public:

	TransientMutationGuard() {
	}

	~TransientMutationGuard() {
		Guard flushGuard(TransientPartition::condLock_);
		if (--TransientPartition::flushs_ == 0) {
			TransientPartition::flushCond_.signalAll(true);
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// RowComparator
//////////////////////////////////////////////////////////////////////////////////////////////////////

class RowComparator {
private:

	const TransientPartition& partition_;
	ColumnPos	columnPos_;

public:

	RowComparator(const TransientPartition& partition, const ColumnIds& columnIds)
		: partition_(partition) {
		partition_.getColumnPos(columnPos_, columnIds);
	}

	int compare(const uint32_t row1, const uint32_t row2) const {
		return partition_.compare(columnPos_, row1, row2, true);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TransientTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class TransientTask : public MasterTask {
private:

	// Keep the partition serial instead of a direct reference to the transient partition,
	// because such a reference will prevent it from being deleted until the last task is processed.
	const uint64_t serial_;

private:

	virtual void process(TransientPartition& partition, const uint64_t timestamp) _THROW_(SparrowException) = 0;

public:

	TransientTask(Master* master, const uint64_t serial) : MasterTask(Worker::getQueue(), master), serial_(serial) {
	}

	TransientTask(Master* master, const uint64_t serial, Queue<Job>& queue) : MasterTask(queue, master), serial_(serial) {
	}

	virtual ~TransientTask() {
	}

	virtual bool operator == (const TransientTask& right) const {
		return this == &right;
	}

	virtual bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override {
		return 0;
	}

	void run(const uint64_t timestamp) override _THROW_(SparrowException) {
		PartitionGuard partition = get()->getPartition(serial_);
		if (partition.get() != 0 && partition->isTransient()) {
			process(static_cast<TransientPartition&>(*partition), timestamp);
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsTask : public TransientTask {
private:

	void process(TransientPartition& partition, const uint64_t timestamp) override _THROW_(SparrowException) {
		partition.dnsUpdate();
	}

public:

	DnsTask(Master* master, const uint64_t serial) : TransientTask(master, serial) {
		Atomic::inc32(&SparrowStatus::get().tasksPendingDnsTasks_);
	}

	~DnsTask() {
		Atomic::dec32(&SparrowStatus::get().tasksPendingDnsTasks_);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PartitionSnapshot
//////////////////////////////////////////////////////////////////////////////////////////////////////

class PartitionSnapshot {
private:

	TransientPartitionGuard partition_;

	const uint32_t rows_;

	ConcurrentIndirector* indirector_;

public:

	PartitionSnapshot() : rows_(0), indirector_(0) {
	}

	PartitionSnapshot(TransientPartition* partition)
		: partition_(partition), rows_(0), indirector_(0) {
	}

	PartitionSnapshot(TransientPartition* partition, const uint32_t rows)
		: partition_(partition), rows_(rows), indirector_(0) {
		assert(rows_ > 0);
	}

	~PartitionSnapshot() {
		delete indirector_;
	}

	uint32_t getRows() const {
		return rows_;
	}

	const ConcurrentIndirector& getIndirector() const {
		return *indirector_;
	}

	void updateIndirector(const uint32_t index);

	bool operator == (const PartitionSnapshot& right) const {
		return partition_ == right.partition_;
	}

	uint32_t hash() const {
		return partition_->hash();
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SizeGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Updates sparrow_tuple_buffer_size.
class SizeGuard {
private:

	TransientPartition& partition_;
	const int64_t initialSize_;

public:

	SizeGuard(TransientPartition& partition) : partition_(partition), initialSize_(partition_.getSize()) {
	}

	~SizeGuard();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ComparatorTransient
//////////////////////////////////////////////////////////////////////////////////////////////////////

class ComparatorTransient {
private:

	Context& context_;
	const TransientPartition& partition_;
	const QueryInfo& queryInfo_;
	const TableFields& fields_;
	const KeyValue& key_;
	const ConcurrentIndirector& indirector_;
	KeyValue tempKey_;

public:

	ComparatorTransient(Context& context, const TransientPartition& partition, const ConcurrentIndirector& indirector, const KeyValue& key, uint8_t* buffer);

	int compareTo(const uint32_t row) _THROW_(SparrowException) {
		if (!partition_.readKey(context_, Position(0, indirector_[row]), true, key_.getMap(), tempKey_.getKey(), true)) {
			throw SparrowException::create(false, "MySQL error, data too large for column size?");
		}
		return queryInfo_.compareKeys(fields_, tempKey_, key_);
	}
};

}

#endif /* #ifndef _engine_transient_h_ */
