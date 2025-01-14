#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <sstream>

#include "vl.h"


//////////////////////////////////////////////////////////////////////////////////////////////////////
// MyRow3
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MyRow3 : public SparrowRow
{
private:
	int		idCount_;
	int		dataCount_;
	int		instance_;
	uint64_t	timestamp_;

public:
	MyRow3(int idCount, int dataCount, uint64_t timestamp, int instance) : idCount_(idCount), dataCount_(dataCount), 
		instance_(instance), timestamp_(timestamp)
	{;}

	int decode(SparrowBuffer* buffer, void* /*dummy*/) const override;
};

int MyRow3::decode( SparrowBuffer* buffer, void* /*dummy*/ ) const {
	int		col = 0;
	int res;
	if ( (res=buffer->addLong( col++, timestamp_ )) != 0 ) return res;
	for ( int i=0; i<idCount_; ++i ) {
		uint64_t	value = instance_+10*i;
		if ( (res=buffer->addLong( col++, value )) != 0 ) return res;
	}
	for ( int i=0; i<dataCount_; ++i ) {
		double	value = 12.0;
		if ( (res=buffer->addDouble( col++, value )) != 0 ) return res;
	}
	return 0;
}

//-----------------------------------------------------------------------------

void TestVL::run() {
	try
	{
		const char*		table_name = "table_vl";

		AutoPtr<Table>	table(createTableAndSend( table_name, 4, 3, 2500 ));

	} catch ( const MyException& e ) {
		printf( "Test failed: %s : %s\n", e.getText(), errmsg() );
	}
}

void TestVL::testColaescing() {
	try
	{
		const char*	table_name = "table_vl";
		const int	idCount = 3, dataCount = 100;
		uint		nbRows = 1024*1024/(8*100);
		AutoPtr<Table>	table(createTableAndSend( table_name, nbRows, idCount, dataCount ));
		getchar();

		sendData( table.get(), nbRows, idCount, dataCount );
		getchar();

		connect_->disableCoalescing(30, sql_params_.getSchema(), true);

	} catch ( const MyException& e ) {
		printf( "Test failed: %s : %s\n", e.getText(), errmsg() );
	}
}

Table* TestVL::createTable( const char* table_name, int idCount, int dataCount )
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

	// Columns
	int		col = 0;
	table->appendColumn( "time", col++, COL_TIMESTAMP, 3 );
	for ( int i=0; i<idCount; ++i ) {
		char	colName[64];
		if ( i == 0 ) {
			sprintf( colName, "id" );
		} else {
			sprintf( colName, "grp_%u", i );
		}
		table->appendColumn( colName, col++, COL_LONG, 0 );
	}

	for ( int i=0; i<dataCount; ++i ) {
		char	colName[64];
		sprintf( colName, "data_%u", i );
		table->appendColumn( colName, col++, COL_DOUBLE, 0 );
	}

	// Indexes
	for ( int i=0; i<idCount+1; ++i ) {
		char	indxName[64];
		sprintf( indxName, "index_%u", i );
		int		indxId = table->appendIndex( indxName, i, false );
		if ( indxId < 0 ) 
			throw MyException::create( false, "Failed to create index" );
	}

	// Foreign keys
	for ( int i=0; i<idCount; ++i ) {
		char	fkName[64];
		char	tableName[64];
		char	colName[64];
		sprintf( fkName, "FK_%s_ids", table_name );
		sprintf( tableName, "%s_ids", table_name );
		if ( i == 0 ) sprintf( colName, "id" );
		else sprintf( colName, "grp_%uId", i );
		table->appendFK( fkName, i+1, sql_params_.getSchema(), tableName, colName );
	}	

	if ( (res=table->create( connect_ )) != 0 )
		throw MyException::create( false, "Failed to create table '%s'.'%s', error code %u", sql_params_.getSchema(), table_name, res );

	return table;
}


void TestVL::sendData( const Table* table, int nbRows, int idCount, int dataCount )
{
	printf("Sending %u rows to table %s...", nbRows, table->getTableName());
	const uint	buffSize = 64*1024*512;
	const int	nbInstances = 2;
	try
	{
		Sparrow::AutoPtr<SparrowBuffer>	spwBuffer( connect_->createBuffer( table, buffSize ) );
		if ( spwBuffer.get() == NULL ) 
			throw MyException::create( false, "Failed to create SparrowBuffer object." );

		time_t	ltime;
		time( &ltime );
		uint64_t	now = ltime;
		now *= 1000;

		for ( int i=0; i<nbRows; ++i ) {
			for ( int j=0; j<nbInstances; ++j ) {
				MyRow3		row( idCount, dataCount, now + i*60000, j+1 );
				//MyRow3		row( idCount, dataCount, now, j+1 );
				int	res = spwBuffer->addRow( row );
				if ( res == SPW_API_BUFFER_FULL ) {
					printf("%u%%, ", (uint)(i*100.0/nbRows));
					connect_->insertData( table, spwBuffer.get() );
					spwBuffer->clear();
				} else if ( res < 0 ) {
					throw MyException::create( false, "failed to add row to Sparrow buffer %d", res );
				}
				if ( spwBuffer->addRow( row ) < 0 )
					throw MyException::create( false, "failed to add row to Sparrow buffer" );
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

Table* TestVL::createTableAndSend( const char* table_name, int nbRows, int idCount, int dataCount )
{
	static int lastIdCount = 0;
	static int lastDataCount = 0;
	
	Table*		table = NULL;

	try {
		printf("Creating table %s, %u id, %u data columns...", table_name, idCount, dataCount);
		Master*		master = connect_->getMasterFile( sql_params_.getSchema(), table_name );
		if ( master != NULL && idCount == lastIdCount && dataCount == lastDataCount ) {
			table = connect_->getTable( sql_params_.getSchema(), table_name );
			if ( table->getNbColumns() != (uint32_t)(1 + idCount + dataCount) ) {
				table = NULL;
			}
			delete master; master = NULL;
		}
		if ( table == NULL ) {
			lastIdCount = idCount;
			lastDataCount = dataCount;
			table = createTable( table_name, idCount, dataCount );
		}
		printf("OK\n");

		sendData( table, nbRows, idCount, dataCount );

	} catch ( const MyException& e ) {
		printf( "MyException %u, %s", e.getErrcode(), e.getText() );
	}
	return table;
}
