#ifndef _spw_api_table_h
#define _spw_api_table_h

#include "global.h"
#include "types.h"


namespace Sparrow
{

// Most methods returns 0 if success, -1 if error except if specified otherwise
// appendXXX() methods returns item index (>=0) if success, -1 if error
class Connection;
class Table
{
public:
	virtual ~Table() {}

	// Methods for specifying the table
	virtual void setDatabaseName(const char*) = 0;
	virtual void setTableName(const char*) = 0;
	virtual void setMaxLifetime(uint64_t) = 0;
	virtual void setCoalescPeriod(uint64_t) = 0;
	virtual void setAggregPeriod(uint32_t) = 0;
	virtual void setDefaultWhere(uint64_t) = 0;
	virtual void setStringOptimization(uint64_t) = 0;

	virtual const char* getDatabaseName() const = 0;
	virtual const char* getTableName() const = 0;
	virtual uint64_t getMaxLifetime() const = 0;
	virtual uint64_t getCoalescPeriod() const = 0;
	virtual uint32_t getAggregPeriod() const = 0;
	virtual uint64_t getDefaultWhere() const = 0;
	virtual uint64_t getStringOptimization() const = 0;

	virtual int appendColumn(const char* name, uint32_t index, ColumnType type, uint32_t stringSize=0,
		uint32_t flags=0, uint32_t info=0, const char* charset=DEF_CHARSET) = 0;
	virtual uint32_t getNbColumns() const  = 0;
	virtual const Column& getColumn(uint32_t index) const  = 0;
	virtual Column& getColumn(uint32_t index) = 0;

	virtual int appendIndex(const char* name, uint32_t colIndex, bool unique) = 0;

	virtual int addColToIndex(uint32_t indexId, uint32_t colIndex) = 0;

	virtual int appendFK(const char* name, uint32_t colIndex, const char* databaseName, const char* tableName,
		const char* columnName) = 0;

	virtual int addDnsEntry(uint32_t dnsEntry) = 0;
	virtual int addDnsServer(uint32_t entryIndex, const char* name, uint32_t port, const char* sourcAddr, uint32_t sourcePort) = 0;

	// Creates the table and/or database schema if they don't exist. Updates the table if it exists.
	virtual int create(Connection* connection) = 0;
};


}	// namespace Sparrow

#endif	// #define  _spw_api_table_h
