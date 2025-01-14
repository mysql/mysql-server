/*
	Engine types.
*/

#ifndef _engine_types_h_
#define _engine_types_h_


#include "interval.h"
#include "lock.h"
#include "misc.h"
#include "vec.h"
#include "hash.h"
#include "serial.h"
#include "exception.h"
#include "treeorder.h"
#include "../handler/plugin.h"	// For configuration parameters.

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Column
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Column type as described in the storage adapter.
// Because of the @!*$& MySQL macros, we have to add a "COL_" suffix...
enum ColumnType {
	COL_BLOB, 
	COL_BYTE,
	COL_DOUBLE, 
	COL_INT, 
	COL_LONG,
	COL_STRING,
	COL_TIMESTAMP,
	COL_SHORT,
	COL_UNKNOWN
};

enum ColumnFlags {
	COL_NULLABLE = 1,		// Column can contain NULLs.
	COL_IP_ADDRESS = 2,		// Column contains IP addresses.
	COL_IP_LOOKUP = 4,		// Column contains IP address lookups.
	COL_DNS_IDENTIFIER = 8,	// Column gives the DNS identifier.
	COL_AUTO_INC = 16,		// Column is auto incremental.
	COL_UNSIGNED = 32		// Column values are unsigned.
};

enum FieldType {
	FIELD_NONE,
	FIELD_NORMAL,			// Field returns value read from record.
	FIELD_DEFAULT,			// Field returns column's default value.
	FIELD_SKIP				// Field skips value in record.
};

// Describes a table column.
class Column {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, Column& column);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const Column& column);

private:

	Str name_;
	Str charset_;		
	ColumnType type_;
	uint32_t flags_;						// Flags. See ColumnFlags.
	uint32_t info_;						// Additional info; meaning depends on flags_.
	uint32_t serial_;						// Alteration serial number of creation.
	uint32_t dropSerial_;					// Alteration serial number of drop.
	Str defaultValue_;					// Default value.

public:

	Column() : type_(COL_UNKNOWN), flags_(0), info_(0), serial_(0), dropSerial_(0) {
	}

	Column(const Str& name) : name_(name), type_(COL_UNKNOWN), flags_(0), info_(0), serial_(0), dropSerial_(0) {
	}

	Column(const char* name, const ColumnType type, const uint32_t flags, const uint32_t info, const char* charset, const Str& defaultValue)
		: name_(name), charset_(charset), type_(type), flags_(flags), info_(info), serial_(0), dropSerial_(0), defaultValue_(defaultValue) {
	}

	~Column() {
	}
	const Str& getName() const {
		return name_;
	}
	ColumnType getType() const {
		return type_;
	}
	bool isString() const {
		return type_ == COL_BLOB || type_ == COL_STRING;
	}
	uint32_t getFlags() const {
		return flags_;
	}
	bool isFlagSet(const ColumnFlags flag) const {
		return (flags_ & flag) != 0;
	}
	void addFlag(const ColumnFlags flag) {
		flags_ |= flag;
	}
	uint32_t getInfo() const {
		return info_;
	}
	void setInfo(const uint32_t info) {
		info_ = info;
	}
	uint32_t getSerial() const {
		return serial_;
	}
	void setSerial(const uint32_t serial) {
		serial_ = serial;
	}
	uint32_t getDropSerial() const {
		return dropSerial_;
	}
	bool isDropped() const {
		return getDropSerial() != 0;
	}
	void drop(const uint32_t serial) {
		assert(dropSerial_ == 0);
		assert(serial > getSerial());
		dropSerial_ = serial;
	}
	const Str& getDefaultValue() const {
		return defaultValue_;
	}
	uint32_t getDataSize() const {
		switch (getType()) {
			case COL_BLOB: return 16;
			case COL_BYTE: return 1;
			case COL_SHORT: return 2;
			case COL_DOUBLE: return 8;
			case COL_INT: return 4;
			case COL_LONG: return 8;
			case COL_STRING: return 16;
			case COL_TIMESTAMP: return 8;
			default: assert(0); return 0; 
		}
	}
	uint32_t getBits() const {
		if (isString()) {
			return isFlagSet(COL_NULLABLE) ? 6 : 5;
		} else {
			return isFlagSet(COL_NULLABLE) ? 1 : 0;
		}
	}
	const Str& getCharset() const {
		return charset_;
	}

	// To check if new table definition is compatible with existing one.
	// - Column size does not matter.
	// - Unsigned flag does not matter.
	// Note the name can be empty when upgrading from older versions.
	bool operator == (const Column& right) const {
		return type_ == right.type_
			&& (flags_ & COL_NULLABLE) == (right.flags_ & COL_NULLABLE)
			&& (name_.length() == 0 || right.name_.length() == 0 || name_ == right.name_);
	}
	bool operator != (const Column& right) const {
		return !(*this == right);
	}
};

typedef SYSvector<Column> Columns;

inline ByteBuffer& operator >> (ByteBuffer& buffer, Column& column) {
	if (buffer.getVersion() >= 6) {
		buffer >> column.name_;
	}
	if (buffer.getVersion() == 1) {
		Str dummy;
		buffer >> dummy;
	}
	int type;
	buffer >> type;
	column.type_ = static_cast<ColumnType>(type);
	if (buffer.getVersion() < 7) {
		uint32_t size;
		buffer >> size;
	}
	buffer >> column.flags_ >> column.info_ >> column.charset_;
	if (buffer.getVersion() >= 16) {
		buffer >> column.serial_;
		if (buffer.getVersion() >= 19) {
			buffer >> column.dropSerial_;
		} else {
			bool dummy;
			buffer >> dummy;
		}
	}
	if (buffer.getVersion() >= 18) {
		buffer >> column.defaultValue_;
	}
	if (buffer.getVersion() < 24 && type == COL_TIMESTAMP) {
		column.info_ = UINT_MAX;
	}
	return buffer;
}

inline ByteBuffer& operator << (ByteBuffer& buffer, const Column& column) {
	buffer << column.name_ << static_cast<int>(column.type_) << column.flags_ << column.info_
		<< column.charset_ << column.serial_ << column.dropSerial_ << column.defaultValue_;
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ColumnEx
//////////////////////////////////////////////////////////////////////////////////////////////////////

class ColumnEx : public Column {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, ColumnEx& column);

private:

	uint32_t stringSize_;

public:

	ColumnEx() : Column() {
	}

	~ColumnEx() {
	}

	uint32_t getStringSize() const {
		return stringSize_;
	}

	static const char* getSqlType(const ColumnType type);

	Str getDefinition() const;
};

typedef SYSvector<ColumnEx> ColumnExs;

inline ByteBuffer& operator >> (ByteBuffer& buffer, ColumnEx& column) {
	buffer >> static_cast<Column&>(column);
	buffer >> column.stringSize_;
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Index
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Describes a table index.
typedef SYSvector<uint32_t> IndexIds;
typedef SYSvector<uint32_t> ColumnIds;
typedef SYSvector<uint32_t> ColumnPos;
typedef SYSvector<ColumnIds> ColumnIdsArray;
class Index {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, Index& index);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const Index& index);

private:

	Str name_;
	ColumnIds columns_;
	bool unique_;
	bool dropped_;

public:

	Index() : unique_(false), dropped_(false) {
	}

	Index(const char* name, const ColumnIds& columns, bool unique)
		: name_(name), columns_(columns), unique_(unique), dropped_(false) {
	}

	Index(const Index& right) {
		*this = right;
	}

	Index& operator = (const Index& right) = default;

	bool operator == (const Index& right) const {
		return dropped_ == right.dropped_ && unique_ == right.unique_ && columns_ == right.columns_;
	}

	const Str& getName() const {
		return name_;
	}

	void setName(const Str& name) {
		name_ = name;
	}

	const ColumnIds& getColumnIds() const {
		return columns_;
	}

	bool isUnique() const {
		return unique_;
	}

	bool isDropped() const {
		return dropped_;
	}

	void drop() {
		dropped_ = true;
	}
};

typedef SYSvector<Index> Indexes;

inline ByteBuffer& operator >> (ByteBuffer& buffer, Index& index) {
	uint32_t id;
	const uint32_t version = buffer.getVersion();
	if (version < 11) {
		buffer >> id;
	}
	if (version < 6) {
		char buff[16];
		snprintf(buff, sizeof(buff), "index_%u", id);
		index.name_ = Str(buff);
	} else {
		buffer >> index.name_;
	}
	buffer >> index.columns_ >> index.unique_;
	if (version < 12) {
		index.dropped_ = index.columns_.isEmpty();
	} else {
		buffer >> index.dropped_;
	}
	return buffer;
}

inline ByteBuffer& operator << (ByteBuffer& buffer, const Index& index) {
	buffer << index.name_ << index.columns_ << index.unique_ << index.dropped_;
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Alteration
//////////////////////////////////////////////////////////////////////////////////////////////////////

enum AlterationType {
	ALT_UNKNOWN,
	ALT_ADD_INDEX,
	ALT_DROP_INDEX
};

class AlterationStats {
private:

	int64_t deltaDataSize_;
	int64_t deltaIndexSize_;

public:

	AlterationStats() : deltaDataSize_(0), deltaIndexSize_(0) {
	}

	AlterationStats(const int64_t deltaDataSize, const int64_t deltaIndexSize)
		: deltaDataSize_(deltaDataSize), deltaIndexSize_(deltaIndexSize) {
	}

	AlterationStats& operator += (const AlterationStats& right) {
		Atomic::add64(reinterpret_cast<uint64_t*>(&deltaDataSize_), right.deltaDataSize_);
		Atomic::add64(reinterpret_cast<uint64_t*>(&deltaIndexSize_), right.deltaIndexSize_);
		return *this;
	}

	int64_t getDeltaDataSize() const {
		return deltaDataSize_;
	}

	int64_t getDeltaIndexSize() const {
		return deltaIndexSize_;
	}
};

class Master;
class PersistentPartition;
class PrintBuffer;
class Alteration {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, Alteration& alteration);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const Alteration& alteration);

private:
	
	AlterationType type_;
	uint32_t serial_;
	uint32_t id_;

public:

	Alteration() : type_(ALT_UNKNOWN), serial_(0), id_(0) {
	}

	Alteration(const AlterationType type, const uint32_t serial, const uint32_t id)
		: type_(type), serial_(serial), id_(id) {
	}

	Alteration(const Alteration& right) {
		*this = right;
	}

	Alteration& operator = (const Alteration& right) = default;

	AlterationType getType() const {
		return type_;
	}

	uint32_t getSerial() const {
		return serial_;
	}

	uint32_t getId() const {
		return id_;
	}

	Str getDescription(const Master& master) const;
};

typedef SYSvector<Alteration> Alterations;

inline ByteBuffer& operator >> (ByteBuffer& buffer, Alteration& alteration) {
	int type;
	buffer >> type;
	alteration.type_ = static_cast<AlterationType>(type);
	buffer >> alteration.serial_ >> alteration.id_;
	return buffer;
}

inline ByteBuffer& operator << (ByteBuffer& buffer, const Alteration& alteration) {
	buffer << static_cast<int>(alteration.type_) << alteration.serial_ << alteration.id_;
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ForeignKey
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Describes a table foreign key.
class ForeignKey {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, ForeignKey& foreignKey);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const ForeignKey& foreignKey);

private:

	Str name_;
	int columnId_;
	Str databaseName_;
	Str tableName_;
	Str columnName_;

public:

	ForeignKey() : columnId_(0) {
	}

	ForeignKey(const char* name, int columnId, const Str& databaseName, const Str& tableName,
		const Str& columnName)
		: name_(name), columnId_(columnId), databaseName_(databaseName),
		tableName_(tableName), columnName_(columnName) {
	}

	ForeignKey(const ForeignKey& right) {
		*this = right;
	}

	ForeignKey& operator = (const ForeignKey& right) = default;

	const Str& getName() const {
		return name_;
	}

	int getColumnId() const {
		return columnId_;
	}

	const Str& getDatabaseName() const {
		return databaseName_;
	}

	const Str& getTableName() const {
		return tableName_;
	}

	const Str& getColumnName() const {
		return columnName_;
	}

	bool operator == (const ForeignKey& right) const {
		return name_ == right.name_ && columnId_ == right.columnId_ && databaseName_ == right.databaseName_
			&& tableName_ == right.tableName_ && columnName_ == right.columnName_;
	}
};

typedef SYSvector<ForeignKey> ForeignKeys;

inline ByteBuffer& operator >> (ByteBuffer& buffer, ForeignKey& foreignKey) {
	buffer >> foreignKey.name_ >> foreignKey.columnId_ >> foreignKey.databaseName_
		>> foreignKey.tableName_>> foreignKey.columnName_;
	return buffer;
}

inline ByteBuffer& operator << (ByteBuffer& buffer, const ForeignKey& foreignKey) {
	buffer << foreignKey.name_ << foreignKey.columnId_ << foreignKey.databaseName_
		<< foreignKey.tableName_ << foreignKey.columnName_;
	return buffer;
}

// Time period: milliseconds since epoch (1970).
typedef Interval<uint64_t> TimePeriod;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSection
//////////////////////////////////////////////////////////////////////////////////////////////////////

class FileSection {
	friend ByteBuffer& operator << (ByteBuffer& buffer, const FileSection& section);
	friend ByteBuffer& operator >> (ByteBuffer& buffer, FileSection& section);

private:

	uint64_t offset_;		// Offset in file.
	uint64_t size_;		// Size, in bytes.

public:

	FileSection() : offset_(0), size_(0) {
	}

	FileSection(const uint64_t offset, const uint64_t size) : offset_(offset), size_(size) {
	}

	void setOffset(uint64_t offset) {
		offset_ = offset;
	}

	uint64_t getOffset() const {
		return offset_;
	}

	void setSize(uint64_t size) {
		size_ = size;
	}

	uint64_t getSize() const {
		return size_;
	}

	uint64_t getCount(const char* name, const uint32_t size) const _THROW_(SparrowException) {
		if (size == 0 || (size_ % size) != 0) {
			throw SparrowException::create(false, "size of section \"%s\" (%llu) is not a multiple of record size (%u)", name, static_cast<ulonglong>(size_), size);
		}
		return size_ / size;
	}

	bool contains(const uint64_t offset) const {
		return offset >= offset_ && offset < offset_ + size_;
	}
};

inline ByteBuffer& operator << (ByteBuffer& buffer, const FileSection& section) {
	buffer << section.offset_ << section.size_;
	return buffer;
}

inline ByteBuffer& operator >> (ByteBuffer& buffer, FileSection& section) {
	buffer >> section.offset_ >> section.size_;
	return buffer;
}

#define DATA_FILE	UINT_MAX
#define STRING_FILE	(UINT_MAX - 1)

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FileHeaderBase
//////////////////////////////////////////////////////////////////////////////////////////////////////

class FileReader;
class FileHeaderBase {
public:

	virtual ~FileHeaderBase() {
	}

	virtual uint64_t getStart() const = 0;

	virtual uint64_t getEnd() const = 0;

	virtual uint64_t getRecords() const = 0;

	virtual uint32_t getRecordSize() const = 0;

	virtual bool isTreeComplete() const = 0;

	virtual uint32_t getNodes() const = 0;

	virtual uint32_t getMinNode() const = 0;

	virtual uint32_t getMaxNode() const = 0;

	virtual uint32_t getPrevNode(const uint32_t node) const = 0;

	virtual uint32_t getNextNode(const uint32_t node) const = 0;

	virtual uint64_t seekTree(FileReader& reader, const uint64_t node) const = 0;

	virtual uint64_t seekTreeData(FileReader& reader, const uint64_t node) const = 0;

	virtual uint64_t seekRecord(FileReader& reader, const uint64_t record) const = 0;

	virtual uint64_t seekRecordData(FileReader& reader, const uint64_t record) const = 0;

	virtual uint64_t seekBin(FileReader& reader, const uint64_t offset) const = 0;

	virtual const FileSection& getStringsSection() const = 0;

	virtual uint64_t getTotalSize() const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BlockCacheHint
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Hint to initialize a FileBlock from disk.
class BlockCacheHint {
public:

	enum Size {
		SMALL,
		MEDIUM,
		LARGE
	};

	enum Direction {
		FORWARD,
		BACKWARD,
		AROUND
	};

	static const BlockCacheHint smallForward0_;
	static const BlockCacheHint largeForward0_;
	static const BlockCacheHint largeAround0_;
	static const BlockCacheHint largeForward1_;
	static const BlockCacheHint largeBackward1_;
	static const BlockCacheHint largeAround1_;
	static const BlockCacheHint smallAround2_;
	static const BlockCacheHint mediumAround2_;
	static const BlockCacheHint largeForward2_;
	static const BlockCacheHint largeBackward2_;
	static const BlockCacheHint smallForward3_;

private:

	const Size size_;			// Block size.
	const Direction direction_;	// Read direction.
	const uint32_t level_;		// Cache level (0..3).

private:

	BlockCacheHint(const BlockCacheHint&);
	BlockCacheHint& operator = (const BlockCacheHint&);

	BlockCacheHint();

	BlockCacheHint(const Size size, const Direction direction, const uint32_t level)
		: size_(size), direction_(direction), level_(level) {
	}

public:

	Size getSize() const {
		return size_;
	}

	Direction getDirection() const {
		return direction_;
	}

	uint32_t getLevel() const {
		return level_;
	}

	uint32_t getReadBlockSize() const {
		switch (getSize()) {
			case SMALL:
				return sparrow_small_read_block_size;
			case MEDIUM:
				return sparrow_medium_read_block_size;
			case LARGE:
				return sparrow_large_read_block_size;
			default:
				assert(0);
				return 0;
		}
	}

	bool operator == (const BlockCacheHint& right) const {
		return size_ == right.size_ && direction_ == right.direction_ && level_ == right.level_;
	}

	bool operator != (const BlockCacheHint& right) const {
		return !(*this == right);
	}
};

}

#endif /* #ifndef _engine_types_h_ */

