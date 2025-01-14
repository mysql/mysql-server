#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <sstream>

#include "column_subset.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MyRow4
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MyRow4 : public SparrowRow
{
private:
	uint64_t	ts_;
	testDefinition&	params_;		// determines the selection of columns
public:
	MyRow4(const uint64_t& ts, testDefinition& params) : ts_(ts), params_(params) {;}
	int decode(SparrowBuffer* buffer, void* dummy) const override;
};


int MyRow4::decode(SparrowBuffer* buffer, void* dummy) const
{
	for (uint i=0; i<params_.columns_.size(); ++i)
	{
		int	res = 0;
		uint	col = params_.columns_[i];
		switch (col)
		{
		case 0:
			res = buffer->addLong(col, ts_);
			break;
		case 1:
			res = buffer->addByte(col, -(int)params_.id_);
			break;
		case 2:
			res = buffer->addByte(col, params_.id_);
			break;
		case 3:
			res = buffer->addDouble(col, -(double)params_.id_);
			break;
		case 4:
			res = buffer->addDouble(col, params_.id_);
			break;
		case 5:
			res = buffer->addInt(col, -(int)params_.id_);
			break;
		case 6:
			res = buffer->addInt(col, params_.id_);
			break;
		case 7:
			res = buffer->addLong(col, -(int)params_.id_);
			break;
		case 8:
			res = buffer->addLong(col, params_.id_);
			break;
		case 9:
			{
				char	str[256];
				sprintf( str, "%d", params_.id_ );
				res = buffer->addString(col, str);
			}
			break;
		case 10:
			res = buffer->addShort(col, -(int)params_.id_);
			break;
		case 11:
			res = buffer->addShort(col, params_.id_);
			break;
		case 12:
			{
				uint8_t	blob[255];
				memset( blob, 0x12, sizeof(blob) );
				res = buffer->addBlob(col, blob, sizeof(blob));
			}
			break;
		}
		if (res != 0) return res;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MyRow5
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MyRow5
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MyRow5 : public SparrowRow
{
private:
	uint64_t	ts_;
	testDefinitionM&	params_;		// determines the selection of columns
public:
	MyRow5(const uint64_t& ts, testDefinitionM& params) : ts_(ts), params_(params) {;}
	int decode(SparrowBuffer* buffer, void* dummy) const override;
};


int MyRow5::decode(SparrowBuffer* buffer, void* dummy) const
{
	buffer->addLong(0, ts_);		// Timestamp column is always set to a valid value
	for ( uint i=0; i<params_.columns_.size(); ++i ) {
		// The value inserted is the column index
		int		res = buffer->addDouble( params_.columns_[i], params_.columns_[i] );
		if ( res != 0 ) return res;
	}
	return 0;
}

//-----------------------------------------------------------------------------

void TestColumnSubset::run() {
	try
	{
		insertSelectColumns( "table_11", true );
		insertSelectColumns( "table_12", false );
		insertSelectColumnsMassive( "table_13", true, 512 );	// Limit set by SPARROW_MAX_BIT_SIZE
		insertSelectColumnsMassive( "table_14", false, 2000 );

	} catch ( const MyException& e ) {
		printf( "Test failed: %s : %s\n", e.getText(), errmsg() );
	}
}

Table* TestColumnSubset::createTable( const char* table_name, bool nullable )
{
	Table*	table = connect_->createTable();
	if ( !table )
		throw MyException::create( false, "Failed to create Sparrow table object for '%s'.'%s'", sql_params_.getSchema(), table_name );

	int		res;
	uint64_t	maxLifetime			= 48*3600*1000;
	uint64_t	coalescingPeriod	= 24*3600*1000;
	uint32_t	aggregationPeriod	= 300;

	// Global parameters
	table->setDatabaseName( sql_params_.getSchema() );
	table->setTableName( table_name );
	table->setMaxLifetime( maxLifetime );
	table->setCoalescPeriod( coalescingPeriod );
	table->setAggregPeriod( aggregationPeriod );

	// Columns
	uint32_t		flags = (nullable ? COL_NULLABLE : 0);
	int		col = 0;
	table->appendColumn( "timestamp", col++, COL_TIMESTAMP, 3 );
	table->appendColumn( "byte", col++, COL_BYTE, 0, flags);
	table->appendColumn( "byte_u", col++, COL_BYTE, 0, flags | COL_UNSIGNED);
	table->appendColumn( "double", col++, COL_DOUBLE, 0, flags);
	table->appendColumn( "double_u", col++, COL_DOUBLE, 0, flags | COL_UNSIGNED);
	table->appendColumn( "int", col++, COL_INT, 0, flags);
	table->appendColumn( "int_u", col++, COL_INT, 0, flags | COL_UNSIGNED);
	table->appendColumn( "long", col++, COL_LONG, 0, flags);
	table->appendColumn( "long_u", col++, COL_LONG, 0, flags | COL_UNSIGNED);
	table->appendColumn( "string", col++, COL_STRING, 32, flags);
	table->appendColumn( "short", col++, COL_SHORT, 0, flags);
	table->appendColumn( "short_u", col++, COL_SHORT, 0, flags | COL_UNSIGNED);
	table->appendColumn( "blob", col++, COL_BLOB, 255, flags);

	// Indexes
	int		indxId;
	if ( (indxId=table->appendIndex( "index_1", 0, false )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );
	if ( (indxId=table->appendIndex( "index_2", 0, true )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );
	table->addColToIndex( indxId, 1 );

	// Foreign keys
	table->appendFK( "fk_1", 1, "DB2", "TEST2", "COL2" );

	// DNS configuration
	int		indxDnsEntry;
	indxDnsEntry = table->addDnsEntry( -1 );
	table->addDnsServer( indxDnsEntry, "dns1", 12, "10.12.14.16", 2500 );
	table->addDnsServer( indxDnsEntry, "dns2", 15, "20.22.24.26", 4500 );

	indxDnsEntry = table->addDnsEntry( 0 );
	table->addDnsServer( indxDnsEntry, "dns3", 13, "10.12.14.36", 3500 );
	table->addDnsServer( indxDnsEntry, "dns4", 16, "20.22.24.46", 5500 );

	if ( (res=table->create( connect_ )) != 0 )
		throw MyException::create( false, "Failed to create table '%s'.'%s', error code %u", sql_params_.getSchema(), table_name, res );

	return table;
}

void TestColumnSubset::getColumnNames(ColumnNames* columns, Table* table, const testDefinition& params) {
	for ( uint i=0; i<params.columns_.size(); ++i ) {
		const Column&	column = table->getColumn( params.columns_[i] );
		columns->appendName( column.getName() );
	}
}

void TestColumnSubset::testSelColumns( Table* table, testDefinition& params, int test_number )
{
	// Insert no column
	uint32_t		buffSize = 64*1024;
	try
	{
		Sparrow::AutoPtr<SparrowBuffer>	spwBuffer( connect_->createBuffer( table, buffSize ) );
		if ( spwBuffer.get() == NULL ) 
			throw MyException::create( false, "Failed to create SparrowBuffer object." );

		std::cout << "" << std::endl;
		std::cout << "Inserting rows using config " << params << std::endl;
		time_t	ltime;
		time( &ltime );
		uint64_t	now = ltime;
		now *= 1000;
		now += test_number;

		const uint		nbRows = 1;		// No need to create N times the same row
		for (uint i=0; i<nbRows; ++i) {
			MyRow4		row( now, params );
			if ( spwBuffer->addRow( row ) < 0 )
				throw MyException::create( false, "failed to add row to Sparrow buffer" );
		}
		std::cout << "Filled buffer. Retrieving sub-set of columns." << std::endl;

		Sparrow::AutoPtr<ColumnNames>	colNames(connect_->createColumnNames(params.columns_.size()));
		getColumnNames( colNames.get(), table, params );
		{
			std::cout << "Our column selection: {";
			bool	first = true;
			for (uint i=0; i<colNames->size(); ++i) {
				if (!first) std::cout << ", ";
				first = false;
				std::cout << colNames->getName(i);
			}
			std::cout << "}" << std::endl;
		}

		// Send the content of the resulting Sparrow Buffer
		int	res = connect_->insertData( table, colNames.get(), spwBuffer.get() );
		if ( res != 0 ) {
			std::cout << "Test " << params << " ... Failed: insert returned " << res << std::endl;
		} else {
			std::cout << "Test " << params << " ... OK" << std::endl;
		}
	} 
	catch ( const MyException& e )
	{
		std::cout << "Test " << params << " ... Failed: exception " << e.getText() << ", " << errmsg() << std::endl;
	}
}

void TestColumnSubset::insertSelectColumns( const char* tableName, bool nullable )
{
	try
	{
		dropTable(tableName);

		// Create table with only one column, timestamp
		std::cout << "Creating data table " << tableName << " ..." << std::endl;
		Sparrow::AutoPtr<Table>		tbl_1( createTable( tableName, nullable ) );
		if ( tbl_1->create( connect_ ) != 0 )
			throw MyException::create( false, "Failed to create Data Table %s.", tableName );
		std::cout << "Created data table " << tableName << std::endl;

		uint	tests[][32] = {{0}, {1,0}, {1,1}, {2,0,1}, {3,0,1,1}, {2,0,2}, {2,0,3}, {2,0,4}, {2,0,5}, {2,0,6}, {2,0,7},
		{2,0,8}, {2,0,9}, {2,0,10}, {2,0,11}, {2,0,12}, {13,0,1,2,3,4,5,6,7,8,9,10,11,12}, {14,0,1,2,3,4,5,6,7,8,9,10,11,12,12}};
		uint	nb_tests = sizeof(tests)/sizeof(tests[0]);
		for ( uint i=0; i<nb_tests; ++i ) {
			uint	nbCols = tests[i][0];
			if ( nbCols == 0 ) {
				testDefinition		params(i+1, nullable, nbCols, NULL);
				testSelColumns( tbl_1.get(), params, i );
			} else {
				testDefinition		params(i+1, nullable, nbCols, &tests[i][1]);
				testSelColumns( tbl_1.get(), params, i );
			}
		}
	}
	catch(const MyException& e)
	{
		printf( "MyException %u, %s", e.getErrcode(), e.getText() );
	}
}

Table* TestColumnSubset::createTableMassive( const char* table_name, bool nullable, uint nbColumns )
{
	Table*	table = connect_->createTable();
	if ( !table )
		throw MyException::create( false, "Failed to create Sparrow table object for '%s'.'%s'", sql_params_.getSchema(), table_name );

	int		res;
	uint64_t	maxLifetime			= 48*3600*1000;
	uint64_t	coalescingPeriod	= 24*3600*1000;
	uint32_t	aggregationPeriod	= 300;

	// Global parameters
	table->setDatabaseName( sql_params_.getSchema() );
	table->setTableName( table_name );
	table->setMaxLifetime( maxLifetime );
	table->setCoalescPeriod( coalescingPeriod );
	table->setAggregPeriod( aggregationPeriod );

	// Columns
	uint32_t		flags = (nullable ? COL_NULLABLE : 0);
	table->appendColumn( "timestamp", 0, COL_TIMESTAMP, 3 );
	for ( uint i=1; i<=nbColumns; ++i ) {
		std::ostringstream	colName;
		colName << "col_" << i;
		table->appendColumn( colName.str().c_str(), i, COL_DOUBLE, 0, flags);
	}

	// Indexes
	int		indxId;
	if ( (indxId=table->appendIndex( "index_1", 0, false )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );
	if ( (indxId=table->appendIndex( "index_2", 0, true )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );
	table->addColToIndex( indxId, 1 );

	if ( (res=table->create( connect_ )) != 0 )
		throw MyException::create( false, "Failed to create table '%s'.'%s', error code %u", sql_params_.getSchema(), table_name, res );

	return table;
}

void TestColumnSubset::getColumnNamesM( ColumnNames* columns, Table* table, const testDefinitionM& params ) {
	const Column&	column = table->getColumn(0);		// Timestamp column is always set
	columns->appendName( column.getName() );
	for ( uint i=0; i<params.columns_.size(); ++i ) {
		const Column&	column = table->getColumn( params.columns_[i] );
		columns->appendName( column.getName() );
	}
}

void TestColumnSubset::testSelColumnsMassive( Table* table, testDefinitionM& params, uint nbRows )
{
	// Insert no column
	uint32_t		buffSize = 1024*1024;
	try
	{
		Sparrow::AutoPtr<SparrowBuffer>	spwBuffer( connect_->createBuffer( table, buffSize ) );
		if ( spwBuffer.get() == NULL ) 
			throw MyException::create( false, "Failed to create SparrowBuffer object." );

		Sparrow::AutoPtr<ColumnNames>	colNames(connect_->createColumnNames(params.columns_.size()));
		getColumnNamesM( colNames.get(), table, params );

		time_t	ltime;
		time( &ltime );
		uint64_t	now = ltime;
		now *= 1000;

		for (uint i=0; i<nbRows; ++i) {
			MyRow5		row( now, params );
			int	res = spwBuffer->addRow( row );
			if ( res < 0 ) {
				if ( res == SPW_API_BUFFER_FULL ) {
					int	res = connect_->insertData( table, colNames.get(), spwBuffer.get() );
					if ( res != 0 ) {
						std::cout << "Test " << params << " ... Failed: insert returned " << res << std::endl;
					} else {
						std::cout << "Test " << params << " ... OK" << std::endl;
					}
					spwBuffer = connect_->createBuffer( table, buffSize );
					--i;
				} else {
					throw MyException::create( false, "failed to add row to Sparrow buffer %d", res );
				}
			}
		}


		// Send the content of the resulting Sparrow Buffer
		int	res = connect_->insertData( table, colNames.get(), spwBuffer.get() );
		if ( res != 0 ) {
			std::cout << "Test " << params << " ... Failed: insert returned " << res << std::endl;
		} else {
			std::cout << "Test " << params << " ... OK" << std::endl;
		}
	} 
	catch ( const MyException& e )
	{
		std::cout << "Test " << params << " ... Failed: exception " << e.getText() << ", " << errmsg() << std::endl;
	}
}

void TestColumnSubset::insertSelectColumnsMassive( const char* tableName, bool nullable, uint nbCols )
{
	const uint		nbRows = 1000;
	try
	{
		dropTable(tableName);

		// Create table with only one column, timestamp
		Sparrow::AutoPtr<Table>		tbl_1( createTableMassive( tableName, nullable, nbCols ) );
		if ( tbl_1->create( connect_ ) != 0 )
			throw MyException::create( false, "Failed to create Data Table %s.", tableName );
		std::cout << "Created Data Table " << tableName << std::endl;

		uint	tests[][32] = {{512, 1, 10, 50, 100, 250, UINT_MAX}, 
		{2048, 1, 100, 1000, UINT_MAX}};
		uint	nb_tests = sizeof(tests)/sizeof(tests[0]);

		// Choose the right set of tests depending on the number of columns in the table
		uint	k = 0;
		for ( ; k<nb_tests; ++k ) {
			if ( nbCols <= tests[k][0] )
				break;
		}
		if ( k == nb_tests ) {
			throw MyException::create( false, "Too many columns in table %s.", tableName );
		}

		uint	i = 1;
		do {
			uint		nbValues = (tests[k][i] == UINT_MAX ? nbCols : tests[k][i]);
			testDefinitionM		params(i, nullable, nbCols, nbValues);
			testSelColumnsMassive( tbl_1.get(), params, nbRows );
		} while ( tests[k][i++] != UINT_MAX );
	}
	catch(const MyException& e)
	{
		printf( "MyException %u, %s", e.getErrcode(), e.getText() );
	}
}



