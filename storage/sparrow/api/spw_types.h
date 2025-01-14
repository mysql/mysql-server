#ifndef _spw_api_impl_types_h_
#define _spw_api_impl_types_h_

#include "include/types.h"
#include "interval.h"
#include "lock.h"
#include "misc.h"
#include "vec.h"
#include "hash.h"
#include "serial.h"
#include "include/exception.h"
#include "str.h"
//#include "treeorder.h"

namespace Sparrow {

// last global error
extern SparrowException spwerror;


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Column implementation class
//////////////////////////////////////////////////////////////////////////////////////////////////////

//const char* getType( ColumnType type );


class spw_Table;

// Describes a table column.
class spw_Column : public Column
{
	friend ByteBuffer& operator >> (ByteBuffer& buffer, spw_Column& column);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const spw_Column& column);
	friend class spw_Table;

private:

	// Column name
	Str name_;

	// Column index, starting at 0.
	uint32_t index_;

	// Character set
	Str charset_;		

	// Column type
	ColumnType type_;

	// Column flags. The integer value is computed by ORing 2 power Flag ordinal values. See ColumnFlags.
	uint32_t flags_;

	// Additional column info. The meaning of this attribute depends on the column flags.
	uint32_t info_;

	// String size, used with variable-length types.
	uint32_t stringSize_;		

	// Alteration serial number of creation.
	uint32_t serial_;

	// Alteration serial number of drop.
	uint32_t dropSerial_;

	// Default value.
	Str defaultValue_;

public:

	spw_Column() : index_(0), type_(COL_UNKNOWN), flags_(0), info_(0), stringSize_(0), serial_(0), dropSerial_(0) {
	}

	spw_Column(const char* name) : name_(name), index_(0), type_(COL_UNKNOWN), flags_(0), info_(0), stringSize_(0), serial_(0), dropSerial_(0) {
	}

	spw_Column(const char* name, uint32_t index, const ColumnType type, const uint32_t stringSize=0, const uint32_t flags=0, const uint32_t info=0, const char* charset=DEF_CHARSET, const char* defaultValue="")
		: name_(name), index_(index), charset_(charset), type_(type), flags_(flags), info_(info), stringSize_(stringSize), serial_(0), dropSerial_(0), defaultValue_(defaultValue) {
	}

	~spw_Column() {
	}

	const char* getName() const override {
		return name_.c_str();
	}

	ColumnType getType() const override {
		return type_;
	}
	bool isString() const override {
		return type_ == COL_BLOB || type_ == COL_STRING;
	}
	uint32_t getStringSize() const override {
		return stringSize_;
	}

	uint32_t getIndex() const override {
		return index_;
	}

	uint32_t getFlags() const override {
		return flags_;
	}

	bool isFlagSet(const ColumnFlags flag) const override {
		return (flags_ & flag) != 0;
	}

	void addFlag(const ColumnFlags flag) override {
		flags_ |= flag;
	}

	void removeFlag(const ColumnFlags flag) override {
		flags_ &= ~flag;
	}

	uint32_t getInfo() const override {
		return info_;
	}

	void setInfo(const uint32_t info) override {
		info_ = info;
	}

	uint32_t getSerial() const override {
		return serial_;
	}

	uint32_t getDropSerial() const override {
		return dropSerial_;
	}

	bool isDropped() const override {
		return getDropSerial() != 0;
	}

	const char* getDefaultValue() const override {
		return defaultValue_.c_str();
	}

	/*uint32_t getDataSize() const {
		switch (getType()) {
			case COL_BLOB: return 16;
			case COL_BYTE: return 1;
			case COL_SHORT: return 2;
			case COL_DOUBLE: return 8;
			case COL_INT: return 4;
			case COL_LONG: return 8;
			case COL_STRING: return 16;
			case COL_TIMESTAMP: return 8;
			default: SPW_ASSERT(0); return 0; 
		}
	}

	uint32_t getBits() const {
		if (isString()) {
			return isFlagSet(COL_NULLABLE) ? 6 : 5;
		} else {
			return isFlagSet(COL_NULLABLE) ? 1 : 0;
		}
	}*/

	const char* getCharset() const override {
		return charset_.c_str();
	}

	bool operator == (const spw_Column& right) const {
		// Size does not matter. Name can be empty when upgrading from older versions.
		return type_ == right.type_ && (flags_ & COL_NULLABLE) == (right.flags_ & COL_NULLABLE)
			&& (name_.length() == 0 || right.name_.length() == 0 || name_ == right.name_);
	}
};

typedef SYSvector<spw_Column> Columns;

inline ByteBuffer& operator >> (ByteBuffer& buffer, spw_Column& column) {
	buffer >> column.name_;
	int type;
	buffer >> type;
	column.type_ = static_cast<ColumnType>(type);
	buffer >> column.flags_ >> column.info_ >> column.charset_ >> column.serial_ >> column.dropSerial_ >> column.defaultValue_;
	return buffer;
}

inline ByteBuffer& operator << (ByteBuffer& buffer, const spw_Column& column) {
	buffer << column.name_ << static_cast<int>(column.type_)
		<< column.flags_ << column.info_ << column.charset_ << column.serial_ << column.dropSerial_ << column.defaultValue_
		<< column.stringSize_;
	return buffer;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// List of columns names
//////////////////////////////////////////////////////////////////////////////////////////////////////

class spw_ColumnNames : public ColumnNames {
private:
	SYSvector<Str>		names_;
public:
	spw_ColumnNames(int size=0) : names_(size) {;}
	~spw_ColumnNames() {;}
	uint32_t size() const override { return names_.entries(); }
	uint32_t appendName(const char* name) override {
		return names_.append(Str(name));
	}
	const char* getName(int index) const override {
		return names_[index].c_str();
	}
	const SYSvector<Str>& getNames() const { return names_; }

};


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Index
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Describes a table index.
typedef SYSvector<uint32_t> IndexIds;
typedef SYSvector<uint32_t> ColumnIds;
typedef SYSvector<ColumnIds> ColumnIdsArray;
class spw_Index : public Index
{
	friend ByteBuffer& operator >> (ByteBuffer& buffer, spw_Index& index);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const spw_Index& index);
	friend class spw_Table;

private:

	// Index name
	Str name_;

	// Column Ids this index is made of
	ColumnIds columns_;

	// true is the index is UNIQUE
	bool unique_;

	// true is this index has been dropped
	bool dropped_;

public:

	spw_Index() : unique_(false), dropped_(false) {
	}

	spw_Index(const char* name, uint32_t colIndex, bool unique)
		: name_(name), unique_(unique), dropped_(false) {
		columns_.append( colIndex );
	}

	spw_Index(const char* name, const ColumnIds& columns, bool unique)
		: name_(name), columns_(columns), unique_(unique), dropped_(false) {
	}

	spw_Index& operator = (const spw_Index& right) {
		if (this == &right) {
			return *this;
		}
		name_ = right.name_;
		columns_ = right.columns_;
		unique_ = right.unique_;
		dropped_ = right.dropped_;
		return *this;
	}

	spw_Index(const spw_Index& right) {
		*this = right;
	}

	bool operator == (const spw_Index& right) const {
		return dropped_ == right.dropped_ && unique_ == right.unique_ && columns_ == right.columns_;
	}

	const char* getName() const override {
		return name_.c_str();
	}

	void setName(const Str& name) {
		name_ = name;
	}

	uint32_t getColumnIds(uint32_t* ids, uint32_t len) const override {
		if ( columns_.length() > len ) return -1;
		for ( uint32_t i=0; i<columns_.length(); ++i ) {
			ids[i] = columns_[i];
		}
		return columns_.length();
	}

	const ColumnIds& getColumnIds() const {
		return columns_;
	}

	ColumnIds& getColumnIds() {
		return columns_;
	}

	bool isUnique() const override {
		return unique_;
	}

	bool isDropped() const override {
		return dropped_;
	}

	void drop() {
		dropped_ = true;
	}
};

typedef SYSvector<spw_Index> Indexes;

inline ByteBuffer& operator >> (ByteBuffer& buffer, spw_Index& index) {
	buffer >> index.name_ >> index.columns_ >> index.unique_ >> index.dropped_;
	return buffer;
}

inline ByteBuffer& operator << (ByteBuffer& buffer, const spw_Index& index) {
	buffer << index.name_ << index.columns_ << index.unique_ << index.dropped_;
	return buffer;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Alteration implementation class
//////////////////////////////////////////////////////////////////////////////////////////////////////


class spw_Alteration : public Alteration
{
	friend ByteBuffer& operator >> (ByteBuffer& buffer, spw_Alteration& alteration);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const spw_Alteration& alteration);

private:
	
	// Alteration type.
	AlterationType type_;

	// Alteration serial.
	uint32_t serial_;

	// Alteration id.
	uint32_t id_;

public:

	spw_Alteration() : type_(ALT_UNKNOWN), serial_(0), id_(0) {
	}

	spw_Alteration(const AlterationType type, const uint32_t serial, const uint32_t id)
		: type_(type), serial_(serial), id_(id) {
	}

	spw_Alteration& operator = (const spw_Alteration& right) {
		if (this == &right) {
			return *this;
		}
		type_ = right.type_;
		serial_ = right.serial_;
		id_ = right.id_;
		return *this;
	}

	spw_Alteration(const spw_Alteration& right) {
		*this = right;
	}

	AlterationType getType() const override {
		return type_;
	}

	uint32_t getSerial() const override {
		return serial_;
	}

	uint32_t getId() const override {
		return id_;
	}
};

typedef SYSvector<spw_Alteration> Alterations;

inline ByteBuffer& operator >> (ByteBuffer& buffer, spw_Alteration& alteration) {
	int type;
	buffer >> type;
	alteration.type_ = static_cast<AlterationType>(type);
	buffer >> alteration.serial_ >> alteration.id_;
	return buffer;
}

inline ByteBuffer& operator << (ByteBuffer& buffer, const spw_Alteration& alteration) {
	buffer << static_cast<int>(alteration.type_) << alteration.serial_ << alteration.id_;
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ForeignKey
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Describes a table foreign key.
class spw_ForeignKey : public ForeignKey
{
	friend ByteBuffer& operator >> (ByteBuffer& buffer, spw_ForeignKey& foreignKey);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const spw_ForeignKey& foreignKey);
	friend class spw_Table;

private:

	// Foreign key name.
	Str name_;

	// Index of the column the foreign key is defined on.
	uint32_t columnId_;

	// Referenced database
	Str databaseName_;

	// Referenced table
	Str tableName_;

	// Referenced column
	Str columnName_;

public:

	spw_ForeignKey() : columnId_(0) {
	}

	spw_ForeignKey(const char* name, uint32_t columnId, const char* databaseName, const char* tableName,
		const char* columnName)
		: name_(name), columnId_(columnId), databaseName_(databaseName),
		tableName_(tableName), columnName_(columnName) {
	}

	spw_ForeignKey& operator = (const spw_ForeignKey& right) {
		if (this == &right) {
			return *this;
		}
		name_ = right.name_;
		columnId_ = right.columnId_;
		databaseName_ = right.databaseName_;
		tableName_ = right.tableName_;
		columnName_ = right.columnName_;
		return *this;
	}

	spw_ForeignKey(const ForeignKey& right) {
		*this = right;
	}

	const char* getName() const override {
		return name_.c_str();
	}

	uint32_t getColumnId() const override {
		return columnId_;
	}

	const char* getDatabaseName() const override {
		return databaseName_.c_str();
	}

	const char* getTableName() const override {
		return tableName_.c_str();
	}

	const char* getColumnName() const override {
		return columnName_.c_str();
	}

	bool operator == (const spw_ForeignKey& right) const {
		return name_ == right.name_ && columnId_ == right.columnId_ && databaseName_ == right.databaseName_
			&& tableName_ == right.tableName_ && columnName_ == right.columnName_;
	}
};

typedef SYSvector<spw_ForeignKey> ForeignKeys;

inline ByteBuffer& operator >> (ByteBuffer& buffer, spw_ForeignKey& foreignKey) {
	buffer >> foreignKey.name_ >> foreignKey.columnId_ >> foreignKey.databaseName_
		>> foreignKey.tableName_>> foreignKey.columnName_;
	return buffer;
}

inline ByteBuffer& operator << (ByteBuffer& buffer, const spw_ForeignKey& foreignKey) {
	buffer << foreignKey.name_ << foreignKey.columnId_ << foreignKey.databaseName_
		<< foreignKey.tableName_ << foreignKey.columnName_;
	return buffer;
}

// Time period: milliseconds since epoch (1970).
typedef Interval<uint64_t> TimePeriod;



//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsServer
//////////////////////////////////////////////////////////////////////////////////////////////////////

class spw_DnsServer : public DnsServer 
{
	friend ByteBuffer& operator >> (ByteBuffer& buffer, spw_DnsServer& dns);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const spw_DnsServer& dns);
	friend class spw_Table;

private:

	Str host_;
	uint32_t port_;
	Str sourceAddress_;
	uint32_t sourcePort_;

public:

	spw_DnsServer() : port_(0), sourcePort_(0) {
	}

	spw_DnsServer(const char* host, const uint32_t port, const char* sourceAddress, const uint32_t sourcePort)
		: host_(host), port_(port), sourceAddress_(sourceAddress), sourcePort_(sourcePort) {
	}

	spw_DnsServer& operator = (const spw_DnsServer& right) {
		host_ = right.host_;
		port_ = right.port_;
		sourceAddress_ = right.sourceAddress_;
		sourcePort_ = right.sourcePort_;
		return *this;
	}

	bool operator == (const spw_DnsServer& right) const {
		return host_ == right.host_ && port_ == right.port_
			&& sourceAddress_ == right.sourceAddress_ && sourcePort_ == right.sourcePort_;
	}

	bool operator < (const spw_DnsServer& right) const {
		int cmp = host_.compareTo(right.host_, false);
		if (cmp < 0) {
			return true;
		} else if (cmp > 0) {
			return false;
		}
		if (port_ < right.port_) {
			return true;
		} else if (port_ > right.port_) {
			return false;
		}
		cmp = sourceAddress_.compareTo(right.sourceAddress_, false);
		if (cmp < 0) {
			return true;
		} else if (cmp > 0) {
			return false;
		}
		if (sourcePort_ < right.sourcePort_) {
			return true;
		} else if (sourcePort_ > right.sourcePort_) {
			return false;
		}
		return false;
	}

	spw_DnsServer(const spw_DnsServer& right) {
		*this = right;
	}

	~spw_DnsServer() {
	}

	const char* getHost() const override {
		return host_.c_str();
	}

	uint32_t getPort() const override {
		return port_;
	}

	const char* getSourceAddr() const override {
		return sourceAddress_.c_str();
	}

	uint32_t getSourcePort() const override {
		return sourcePort_;
	}

	Str print() const {
		char buffer[1024];
		snprintf(buffer, sizeof(buffer), "host=%s, port=%u, sourceAddress=%s, sourcePort=%u", getHost(), getPort(), getSourceAddr(), getSourcePort());
		return Str(buffer);
	}
};

inline ByteBuffer& operator << (ByteBuffer& buffer, const spw_DnsServer& dns) {
	buffer << dns.host_ << dns.port_ << dns.sourceAddress_ << dns.sourcePort_;
	return buffer;
}

inline ByteBuffer& operator >> (ByteBuffer& buffer, spw_DnsServer& dns) {
	buffer >> dns.host_ >> dns.port_ >> dns.sourceAddress_ >> dns.sourcePort_;
	return buffer;
}

typedef SYSsortedVector<spw_DnsServer> DnsServers;


//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsConfigId
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsConfigId {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, DnsConfigId& id);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const DnsConfigId& id);

private:

	int id_;						// DNS identifier.
	DnsServers servers_;			// DNS servers.

public:

	DnsConfigId() : id_(-1) {
	}

	DnsConfigId(int id) : id_(id) {
	}

	DnsConfigId(int id, const DnsServers& servers) : id_(id), servers_(servers) {
	}

	int getId() const {
		return id_;
	}

	bool operator == (const DnsConfigId& right) const {
		return id_ == right.id_;
	}

	const DnsServers& getServers() const {
		return servers_;
	}

	DnsServers& getServers() {
		return servers_;
	}

	uint32_t hash() const {
		return id_;
	}
};

inline ByteBuffer& operator << (ByteBuffer& buffer, const DnsConfigId& id) {
	buffer << id.id_ << id.servers_;
	return buffer;
}

inline ByteBuffer& operator >> (ByteBuffer& buffer, DnsConfigId& id) {
	buffer >> id.id_ >> id.servers_;
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsConfiguration
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsConfiguration : public SYSvector<DnsConfigId> {

public:
	DnsConfiguration() : SYSvector<DnsConfigId>(1) 
	{}
};



//////////////////////////////////////////////////////////////////////////////////////////////////////
// Partition
//////////////////////////////////////////////////////////////////////////////////////////////////////

class spw_Partition : public Partition 
{
protected:

	uint64_t serial_;
	uint64_t dataSerial_;
	uint32_t filesystem_;
	uint32_t indexAlterSerial_;	// Index alteration serial number; to be compared to Master::indexAlterSerial_.
	uint32_t columnAlterSerial_;	// Column alteration serial number; to be compared to Column::serial_.

public:

	spw_Partition(const uint64_t serial, const uint64_t dataSerial, const uint32_t filesystem, const uint32_t indexAlterSerial,
		const uint32_t columnAlterSerial)
		: serial_(serial), dataSerial_(dataSerial), filesystem_(filesystem),indexAlterSerial_(indexAlterSerial),
		columnAlterSerial_(columnAlterSerial) {
	}

	virtual ~spw_Partition() {
	}

	// Attributes.
	uint64_t getSerial() const override {
		return serial_;
	}

	uint64_t getDataSerial() const {
		return dataSerial_;
	}

	bool isMain() const {
		return getSerial() == getDataSerial();
	}

	uint32_t getFilesystem() const override {
		return filesystem_;
	}

	uint32_t getIndexAlterSerial() const override {
		return indexAlterSerial_;
	}

	uint32_t getColumnAlterSerial() const {
		return columnAlterSerial_;
	}

	// Comparison.
	bool operator == (const spw_Partition& right) const {
		return serial_ == right.serial_;
	}

	bool operator < (const spw_Partition& right) const {
		return serial_ < right.serial_;
	}

	// Hash.
	uint32_t hash() const {
		return 31 + static_cast<uint32_t> (serial_ ^ (serial_ >> 32));
	}
};

typedef SYSpSortedVector<spw_Partition, 256> Partitions;


//////////////////////////////////////////////////////////////////////////////////////////////////////
// PersistentPartition
//////////////////////////////////////////////////////////////////////////////////////////////////////

class spw_PersistentPartition : public spw_Partition {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, spw_PersistentPartition& partition);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const spw_PersistentPartition& partition);

private:

	uint32_t version_;		// See PersistentPartition::currentVersion_.
	TimePeriod period_;
	uint64_t fileTime_;
	uint32_t records_;
	uint64_t dataSize_;
	uint64_t indexSize_;
	uint64_t dataRecords_;	// Number of records in data file.
	uint64_t recordOffset_;	// Record offset in main partition.
	ColumnIds	skippedColumnIds_;	// Skipped columns. Columns for which all values are NULL are not stored.

public:

	spw_PersistentPartition(uint32_t version, const uint64_t serial, const uint64_t dataSerial, const uint32_t filesystem, const uint32_t indexAlterSerial,
		const uint32_t columnAlterSerial, const TimePeriod& period, const uint32_t records, const uint64_t dataSize, const uint64_t indexSize,
		const uint64_t dataRecords, const uint64_t recordOffset)
		: spw_Partition(serial, dataSerial, filesystem, indexAlterSerial, columnAlterSerial), version_(version),
		period_(period), fileTime_(period.getMin()), records_(records), dataSize_(dataSize), indexSize_(indexSize), dataRecords_(dataRecords), recordOffset_(recordOffset) {
	}

	// Deserialization constructor.
	spw_PersistentPartition() : spw_Partition(0, 0, 0, 0, 0) {
	}

	~spw_PersistentPartition() {
	}

	uint32_t getVersion() const {
		return version_;
	}

	TimePeriod getPeriod() const {
		return period_;
	}

	uint64_t getFileTime() const {
		return fileTime_;
	}

	uint32_t getRecords() const {
		return records_;
	}

	uint64_t getDataSize() const {
		return dataSize_;
	}

	uint64_t getIndexSize() const {
		return indexSize_;
	}

	uint64_t getDataRecords() const {
		return dataRecords_;
	}

	uint64_t getRecordOffset() const {
		return recordOffset_;
	}

	const ColumnIds& getSkippedColumns() {
		return skippedColumnIds_;
	}
};

inline ByteBuffer& operator >> (ByteBuffer& buffer, spw_PersistentPartition& partition) {
	buffer >> partition.version_ >> partition.serial_ >> partition.dataSerial_ >> partition.dataRecords_
		>> partition.recordOffset_ >> partition.period_ >> partition.fileTime_ >> partition.records_ >> partition.dataSize_
		>> partition.indexSize_ >> partition.filesystem_ >> partition.indexAlterSerial_
		>> partition.columnAlterSerial_ >> partition.skippedColumnIds_;
	return buffer;
}

inline ByteBuffer& operator << (ByteBuffer& buffer, const spw_PersistentPartition& partition) {
	buffer << partition.version_ << partition.serial_ << partition.dataSerial_ << partition.dataRecords_
		<< partition.recordOffset_ << partition.period_ << partition.fileTime_ << partition.records_ << partition.dataSize_
		<< partition.indexSize_ << partition.filesystem_ << partition.indexAlterSerial_
		<< partition.columnAlterSerial_ << partition.skippedColumnIds_;
	return buffer;
}

}

#endif /* #ifndef _spw_api_impl_types_h_ */
