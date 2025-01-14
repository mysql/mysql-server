#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <sstream>

#include "too_many_columns.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MyRow2
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MyRow2 : public SparrowRow
{
private:
	uint	nbCols_;
	uint	iter_;
	uint64_t	timestamp_;

public:
	MyRow2(uint64_t timestamp, uint nbCols, uint i) : nbCols_(nbCols), iter_(i), timestamp_(timestamp)
	{;}

	int decode(SparrowBuffer* buffer, void* /*dummy*/) const override;
};

int MyRow2::decode( SparrowBuffer* buffer, void* /*dummy*/ ) const {
	int		col = 0;
	int res;
	if ( (res=buffer->addLong( col++, timestamp_ )) != 0 ) return res;
	for ( uint i=0; i<nbCols_; ++i ) {
		double	value = 1.0*(nbCols_+1)+0.0001*(i+1);
		if ( (res=buffer->addDouble( col++, value )) != 0 ) return res;
	}
	return 0;
}

//-----------------------------------------------------------------------------//

void TestTooManyColumns::run() {
	try
	{
		const char*		table_name = "table_too_many_cols";

		printf("Creating table %s...", table_name);
		uint	nbCols = 0;
		AutoPtr<Table>	table( createTableTooLong( "table_2", nbCols ) );
		printf("with %u columns, OK\n", nbCols);

		sendDataFlow( table.get(), nbCols );

	} catch ( const MyException& e ) {
		printf( "Test failed: %s : %s\n", e.getText(), errmsg() );
	}
}


Table* TestTooManyColumns::createTableTooLong( const char* table_name, uint& nbCols )
{
	Table*	table = connect_->createTable();
	if ( !table )
		throw MyException::create( false, "Failed to create Sparrow table object for '%s'.'%s'", sql_params_.getSchema(), table_name );

	int		res;
	uint64_t	maxLifetime			= 24*3600*1000;
	uint64_t	coalescingPeriod	= 3600*1000;
	uint32_t	aggregationPeriod	= 300;

	// Global parameters
	table->setDatabaseName( sql_params_.getSchema() );
	table->setTableName( table_name );
	table->setMaxLifetime( maxLifetime );
	table->setCoalescPeriod( coalescingPeriod );
	table->setAggregPeriod( aggregationPeriod );

	nbCols = 0xFFFF/8;

	// Columns
	/*Columns		columns;
	columns.resize( nbCols+1 );
	int		col = 0;
	columns.appendColumn( "timestamp", col++, COL_TIMESTAMP, 3 );
	for ( uint i=0; i<nbCols; ++i ) {
		char	col_name[64];
		sprintf( col_name, "value_%4u", i+1 );
		columns.appendColumn( col_name, col++, COL_DOUBLE );
	}
	table->setColumns( columns );*/

	int		col = 0;
	table->appendColumn( "timestamp", col++, COL_TIMESTAMP, 3 );
	for ( uint i=0; i<nbCols; ++i ) {
		char	col_name[64];
		sprintf( col_name, "value_%4u", i+1 );
		table->appendColumn( col_name, col++, COL_DOUBLE );
	}

	// Indexes
	int		indxId;
	if ( (indxId=table->appendIndex( "index_1", 0, false )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );

	if ( (res=table->create( connect_ )) != 0 )
		throw MyException::create( false, "Failed to create table '%s'.'%s', error code %u", sql_params_.getSchema(), table_name, res );

	return table;
}

Table* TestTooManyColumns::createTableManyCol( const char* table_name, uint nbCol, bool nullable )
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
	for ( uint k=0; k<nbCol-1; ++k ) {
		table->appendColumn( "double", col++, COL_DOUBLE, 0, flags);
	}

	// Indexes
	int		indxId;
	if ( (indxId=table->appendIndex( "index_1", 0, false )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );

	if ( (res=table->create( connect_ )) != 0 )
		throw MyException::create( false, "Failed to create table '%s'.'%s', error code %u", sql_params_.getSchema(), table_name, res );

	return table;
}


void TestTooManyColumns::sendDataFlow( const Table* table, uint nbCols )
{
	const uint32_t		buffSize = 64*1024*512;
	const uint32_t		nbRows = 10000;
	try
	{
		printf("Sending data flow to table %s...", table->getTableName());
		AutoPtr<SparrowBuffer>	spwBuffer( connect_->createBuffer( table, buffSize ) );
		if ( spwBuffer.get() == NULL ) 
			throw MyException::create( false, "Failed to create SparrowBuffer object." );

		time_t	ltime;
		time( &ltime );
		uint64_t	now = ltime;
		now *= 1000;

		for ( uint i=0; i<nbRows; ++i ) {
			MyRow2		row( now+i, nbCols, i );
			int	res = spwBuffer->addRow( row );
			if ( res == SPW_API_BUFFER_FULL ) {
				printf("%u%%, ", (uint)(i*100.0/nbRows));
				connect_->insertData( table, spwBuffer.get() );
				spwBuffer->clear();
			} else if ( res < 0 ) {
				throw MyException::create( false, "failed to add row to Sparrow buffer %d", res );
			}
		}

		// Send the content of the resulting Sparrow Buffer
		connect_->insertData( table, spwBuffer.get() );
		printf("OK\n");
	} 
	catch ( const MyException& e )
	{
		printf( "Exception! %s : %s\n", e.getText(), errmsg() );
	}
}


