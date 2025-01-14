#ifndef _spw_api_impl_table_h
#define _spw_api_impl_table_h

#include "include/table.h"
#include "spw_types.h"


namespace Sparrow
{

//////////////////////////////////////////////////////////////////////////////////////////////////////
// InitMySQLib
//////////////////////////////////////////////////////////////////////////////////////////////////////

class InitMySQLib
{
private:
	bool	initialized_;
	void clear();

public:
	InitMySQLib() : initialized_(false) {}
	~InitMySQLib() {
		clear();
	}

	void initialize();
};


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Table
//////////////////////////////////////////////////////////////////////////////////////////////////////

class spw_Connection;
class spw_Table : public Table
{
private:
	// Name of the database 
	Str			databaseName_;

	// Name of the table
	Str			tableName_;

	// Columns this table is made of
	Columns		columns_;

	// Indexes defined on this table
	Indexes		indexes_;

	// Foreign key defined on this table
	ForeignKeys	foreignKeys_;

	// DNS configuration for this table
	DnsConfiguration	dns_;

	// Maximum lifetime of data in this table, in milliseconds.
	uint64_t maxLifetime_;

	// Coalescing period, in milliseconds.
	uint64_t coalescingPeriod_;

	// Aggregation period, in seconds, of data in this table. If 0, there is no aggregation.
	uint32_t aggregationPeriod_;

	// Default where period, in milliseconds.
	uint64_t defaultWhere_;

	// String optimization size, in bytes.
	uint64_t stringOptimization_;

	static InitMySQLib	initMySQL_;

protected:
	void getColumnDefinition(Str& str, const spw_Column& column);

public:
	spw_Table();
	spw_Table(const Str& database, const Str& table);
	spw_Table(const Str& database, const Str& table, 
		const Columns& columns, const Indexes& indexes, const ForeignKeys& foreignKeys,
		const DnsConfiguration& dns, uint64_t maxLifetime, uint64_t coalescingPeriod,
		uint32_t aggregationPeriod, uint64_t defaultWhere, uint64_t stringOptimization);

	~spw_Table() {}

	static void initialize() _THROW_(SparrowException) { 
		initMySQL_.initialize(); 
	}

	int create(Connection* connection) override;

	void setDatabaseName( const char* name ) override {
		databaseName_ = name;
	}
	const char* getDatabaseName() const override { 
		return databaseName_.c_str(); }

	const Str& getDbNameStr() const { 
		return databaseName_; }

	void setTableName( const char* name ) override {
		tableName_ = name;
	}
	const char* getTableName() const override { return tableName_.c_str(); }

	const Str& getTableNameStr() const { return tableName_; }
	
	void setMaxLifetime( uint64_t maxLifetime ) override {
		maxLifetime_ = maxLifetime;
	}
	uint64_t getMaxLifetime() const override { return maxLifetime_; }

	void setCoalescPeriod( uint64_t coalescingPeriod ) override {
		coalescingPeriod_ = coalescingPeriod;
	}
	uint64_t getCoalescPeriod() const override { return coalescingPeriod_; }

	void setAggregPeriod( uint32_t aggregationPeriod ) override {
		aggregationPeriod_ = aggregationPeriod;
	}
	uint32_t getAggregPeriod() const override { return aggregationPeriod_; }
	
	void setDefaultWhere(uint64_t defaultWhere) override {
		defaultWhere_ = defaultWhere;
	}
	uint64_t getDefaultWhere() const override { return defaultWhere_; }
			
	void setStringOptimization(uint64_t stringOptimization) override {
		stringOptimization_ = stringOptimization;
	}
	uint64_t getStringOptimization() const override { return stringOptimization_; }
			
	int appendColumn(const char* name, uint32_t index, ColumnType type, uint32_t stringSize=0,
		uint32_t flags=0, uint32_t info=0, const char* charset=DEF_CHARSET) override {
		return columns_.append( spw_Column( name, index, type, stringSize, flags, info, charset ) );
	}
	uint32_t getNbColumns() const override { return columns_.length(); }
	const Column& getColumn(uint32_t index) const override {
		SPW_ASSERT(index < columns_.length());
		return columns_[index];
	}

	Column& getColumn(uint32_t index) override {
		SPW_ASSERT(index < columns_.length());
		return columns_[index];
	}

	const Columns& getColumns() const { return columns_; }
	void setColumns(const Columns& columns) { columns_ = columns; }


	int appendIndex( const char* name, uint32_t colIndex, bool unique ) override {
		return indexes_.append( spw_Index( name, colIndex, unique ) );
	}
	int addColToIndex( uint32_t indexId, uint32_t colIndex ) override {
		SPW_ASSERT( indexId < indexes_.length() );
		return indexes_[indexId].getColumnIds().append( colIndex );
	}
	const Indexes& getIndexes() const { return indexes_; }
	void setIndexes(const Indexes& indexes) { indexes_ = indexes; }


	virtual int appendFK( const char* name, uint32_t colIndex, const char* databaseName, 
		const char* tableName, const char* columnName) override {
		return foreignKeys_.append( spw_ForeignKey( name, colIndex, databaseName, tableName, columnName ) );
	}
	const ForeignKeys& getForeignKeys() const { return foreignKeys_; }
	void setForeignKeys(const ForeignKeys& fks) { foreignKeys_ = fks; }


	int addDnsEntry(uint32_t dnsEntry) override {
		return dns_.append( DnsConfigId( dnsEntry ) );
	}
	int addDnsServer(uint32_t entryIndex, const char* name, uint32_t port, const char* sourcAddr, uint32_t sourcePort) override {
		SPW_ASSERT( entryIndex < dns_.length() );
		return dns_[entryIndex].getServers().append( spw_DnsServer( name, port, sourcAddr, sourcePort ) );
	}
	const DnsConfiguration& getDns() const { return dns_; }
	void setDns(const DnsConfiguration& dns) { dns_ = dns; }
};

}		// namespace Sparrow

#endif	// #define  _spw_api_impl_table_h
