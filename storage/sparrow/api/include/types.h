#ifndef _spw_api_types_h_
#define _spw_api_types_h_

#include "global.h"

namespace Sparrow {


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Column
//////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEF_CHARSET		"utf8_bin"

// Column type as described in the storage adapter.
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

inline const char* getType( ColumnType type ) {
	switch ( type )
	{
	case COL_BLOB:		return "VARBINARY";
	case COL_BYTE:		return "TINYINT";
	case COL_DOUBLE:	return "DOUBLE";	
	case COL_INT:		return "INT"; 
	case COL_LONG:		return "BIGINT";
	case COL_STRING:	return "VARCHAR";
	case COL_TIMESTAMP:	return "TIMESTAMP";
	case COL_SHORT:		return "SMALLINT";
	case COL_UNKNOWN:	return "";
	}
	return "";
}


/**
 * Note on COL_IP_LOOKUP:
 * This column contains STRING values set using reverse DNS
 * lookups from another BLOB column containing IP
 * addresses. When this flag is set, the column's info attribute gives
 * the index of the source IP column.
 */

enum ColumnFlags {
	COL_NULLABLE = 1,		// Column can contain NULLs.
	COL_IP_ADDRESS = 2,		// Column contains IP addresses.
	COL_IP_LOOKUP = 4,		// Column contains IP address lookups. Index of column referencing the Ip address is set in info_
	COL_DNS_IDENTIFIER = 8,	// Column gives the DNS identifier.
	COL_AUTO_INC = 16,		// Column is auto incremental.
	COL_UNSIGNED = 32		// Column values are unsigned.
};

class Column
{
protected:
	virtual ~Column() {}

public:
	virtual const char* getName() const  = 0;
	virtual ColumnType getType() const = 0;
	virtual bool isString() const = 0;
	virtual uint32_t getStringSize() const = 0;
	virtual uint32_t getIndex() const = 0;
	virtual uint32_t getFlags() const = 0;
	virtual bool isFlagSet(const ColumnFlags flag) const = 0;
	virtual void addFlag(const ColumnFlags flag) = 0;
	virtual void removeFlag(const ColumnFlags flag) = 0;
	virtual uint32_t getInfo() const = 0;
	virtual void setInfo(const uint32_t info) = 0;
	virtual const char* getCharset() const = 0;
	virtual uint32_t getSerial() const = 0;
	virtual uint32_t getDropSerial() const = 0;
	virtual bool isDropped() const = 0;
	virtual const char* getDefaultValue() const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// List of columns names
//////////////////////////////////////////////////////////////////////////////////////////////////////

class ColumnNames {
public:
	virtual ~ColumnNames() {}
	virtual uint32_t size() const = 0;
	virtual uint32_t appendName(const char*) = 0;
	virtual const char* getName(int index) const = 0;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Index
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Index {
protected:
	virtual ~Index() {}

public:
	virtual const char* getName() const  = 0;

	// Returns the number of columns ids stored in colIds, or -1 if there is more than len
	virtual uint32_t getColumnIds(uint32_t* colIds, uint32_t len) const  = 0;
	virtual bool isUnique() const = 0;
	virtual bool isDropped() const = 0;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Alteration
//////////////////////////////////////////////////////////////////////////////////////////////////////

enum AlterationType {
	ALT_UNKNOWN,
	ALT_ADD_INDEX,
	ALT_DROP_INDEX
};


class Alteration {
protected:
	virtual ~Alteration() {}

public:

	virtual AlterationType getType() const = 0;
	virtual uint32_t getSerial() const = 0;
	virtual uint32_t getId() const = 0;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////
// ForeignKey
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Describes a table foreign key.
class ForeignKey {
protected:
	virtual ~ForeignKey() {}

public:
	virtual const char* getName() const = 0;
	virtual uint32_t getColumnId() const = 0;
	virtual const char* getDatabaseName() const = 0;
	virtual const char* getTableName() const = 0;
	virtual const char* getColumnName() const = 0;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsServer
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsServer {
protected:
	virtual ~DnsServer() {}

public:

	virtual const char* getHost() const = 0;
	virtual uint32_t getPort() const = 0;
	virtual const char* getSourceAddr() const = 0;
	virtual uint32_t getSourcePort() const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Partition
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Partition 
{
protected:
	virtual ~Partition() {}

public:
	virtual uint64_t getSerial() const = 0;
	virtual uint32_t getFilesystem() const = 0;
	virtual uint32_t getIndexAlterSerial() const = 0;
};

}

#endif /* #ifndef _spw_api_types_h_ */
