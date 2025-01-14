#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <sstream>

#include "all_types.h"
#include "my_sys.h"
#include "my_systime.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Create TEST Table

void TestAlltypes::run() {
	try
	{
		const char*		table_name = "table_1";

		printf("Creating table %s...", table_name);
		AutoPtr<Table>	table( createTable(table_name) );
		printf("OK\n");

		runMasterFileTest( table.get() );

		runGetTableTest( table.get() );

		sendSampleData( table.get() );
		sendErrorData( table.get() );
		sendDataFlow( table.get() );

	} catch ( const MyException& e ) {
		printf( "Test failed: %s : %s\n", e.getText(), errmsg() );
	}
}

void TestAlltypes::runMasterFileTest( const Table* table ) {
	try
	{
		printf("Retrieving Master file for %s...", table->getTableName());
		const Master*		master = connect_->getMasterFile( table->getDatabaseName(), table->getTableName() );
		if ( !master )
			throw MyException::create( false, "Failed to get Master file." );
		// Do some processing on the retrieved Master File information
		//	...

		// TODO: use connect->releaseXXX() instead
		delete master; master = NULL;
		printf("OK\n");

	} catch ( const MyException& e ) {
		printf( "Failed to get Master file: %s : %s\n", e.getText(), errmsg() );
	}
}

void TestAlltypes::runGetTableTest( const Table* table ) {
	try
	{
		printf("Getting Table object for %s...", table->getTableName());
		const Table* tbl = connect_->getTable( table->getDatabaseName(), table->getTableName() );
		if ( !tbl ) 
			throw MyException::create( false, "Failed to get Table description." );
		delete tbl; tbl = NULL;
		printf("OK\n");

#ifdef TEST_COALESCING
		printf( "Disabling coalescing..." );
		connect->disableCoalescing(10);
		printf( "done" );

		getchar();

		printf( "Enabling coalescing..." );
		connect->disableCoalescing(0);
		printf( "done" );
#endif	// TEST_COALESCING


	} catch ( const MyException& e ) {
		printf( "Failed to get Table: %s : %s\n", e.getText(), errmsg() );
	}
}

void TestAlltypes::runDisableCoalescingGlobTest(bool loop) {
	try
	{
		if (!loop)
		{
			printf( "Disabling coalescing..." );
			connect_->disableCoalescing(10);
			printf( "OK\n" );

			getchar();

			printf( "Enabling coalescing..." );
			connect_->disableCoalescing(0);
			printf( "OK\n" );
		} else 
		{
			do 
			{
				connect_->disableCoalescing(10);
				my_sleep(1000000);		// 1sec
				connect_->disableCoalescing(0);
			} while (true);
		}
	} catch ( const MyException& e ) {
		printf( "Failed to get Table: %s : %s\n", e.getText(), errmsg() );
	}
}


void TestAlltypes::runDisableCoalescingSchemaTest(const char* schema, bool loop)
{
	if (schema == NULL || strlen(schema) == 0)
		return;

	try
	{
		if (!loop)
		{
			printf( "Disabling coalescing for %s...", schema );
			connect_->disableCoalescing(10, schema);
			printf( "OK\n" );

			getchar();

			printf( "Enabling coalescing..." );
			connect_->disableCoalescing(0, schema);
			printf( "OK\n" );
		}
		else
		{
			do {
				connect_->disableCoalescing(1, schema);

				my_sleep(1000000);

				connect_->disableCoalescing(0, schema);
			} while (true);
		}

	} catch ( const MyException& e ) {
		printf( "Failed to get Table: %s : %s\n", e.getText(), errmsg() );
	}
}


Table* TestAlltypes::createTable( const char* table_name )
{
	Table*	table = connect_->createTable();
	if ( !table )
		throw MyException::create( false, "Failed to create Sparrow table object for '%s'.'%s'", sql_params_.getSchema(), table_name );

	int	res;
	uint64_t	maxLifetime			= 24*3600*1000;
	uint64_t	coalescingPeriod	= 3600*1000;
	uint32_t	aggregationPeriod	= 300;
	uint64_t	defaultWhere		= 3600*24*1000;

	// Global parameters
	table->setDatabaseName( sql_params_.getSchema() );
	table->setTableName( table_name );
	table->setMaxLifetime( maxLifetime );
	table->setCoalescPeriod( coalescingPeriod );
	table->setAggregPeriod( aggregationPeriod );
	table->setDefaultWhere(defaultWhere);

	// Columns
	int		col = 0;
	table->appendColumn( "timestamp", col++, COL_TIMESTAMP, 3 );
	table->appendColumn( "slotId", col++, COL_LONG, 0, COL_UNSIGNED );
	table->appendColumn( "status", col++, COL_BYTE, 0, COL_UNSIGNED );
	table->appendColumn( "samples", col++, COL_SHORT, 0, COL_UNSIGNED );
	table->appendColumn( "value", col++, COL_DOUBLE );
	table->appendColumn( "name", col++, COL_STRING, 32 );
	table->appendColumn( "status2", col++, COL_BYTE, 0, COL_UNSIGNED );
	table->appendColumn( "name_nullable", col++, COL_STRING, 10, COL_NULLABLE );
	table->appendColumn( "status3", col++, COL_BYTE, 0, COL_UNSIGNED );
	table->appendColumn( "blob", col++, COL_BLOB, 32 );
	table->appendColumn( "status4", col++, COL_BYTE, 0, COL_UNSIGNED );
	table->appendColumn( "blob_nullable", col++, COL_BLOB, 255, COL_NULLABLE );
	table->appendColumn( "status5", col++, COL_BYTE, 0, COL_UNSIGNED );

	// Indexes
	int		indxId;
	if ( (indxId=table->appendIndex( "index_1", 0, false )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );
	if ( (indxId=table->appendIndex( "index_2", 0, true )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );
	table->addColToIndex( indxId, 1 );
	if ( (indxId=table->appendIndex( "index_3", 4, false )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );
	if ( (indxId=table->appendIndex( "index_4", 2, false )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );
	if ( (indxId=table->appendIndex( "index_5", 3, false )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );
	if ( (indxId=table->appendIndex( "index_6", 5, false )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );
	if ( (indxId=table->appendIndex( "index_7", 6, false )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );

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

void TestAlltypes::sendSampleData( const Table* table ) {

	const uint32_t		buffSize = 64*1024*512;

	const int	nb_iter = 2;
	for ( int k=0; k<nb_iter; ++k )
	{
		printf("Sending various data samples to table %s [iteration %u]...", table->getTableName(), k+1);

		time_t	ltime;
		time( &ltime );
		uint64_t	now = ltime;
		now *= 1000;
		const int		blobLen = 11;		// columns length + 1
		uint8_t		blob[blobLen];
		for ( int i=0; i<blobLen; ++i )
			blob[i] = i+1;

		try
		{
			Sparrow::AutoPtr<SparrowBuffer>	spwBuffer( connect_->createBuffer( table, buffSize ) );
			if ( spwBuffer.get() == NULL ) 
				throw MyException::create( false, "Failed to create SparrowBuffer object." );

			const uint32_t	nbRows = 4;
			MyRow		data[nbRows];

			data[0] = MyRow( now+100, 0, 0x12, 0, "", NULL, 0.0, (uint8_t*)blob, 0, NULL, 0 );
			data[1] = MyRow( now+10, 0, 0x12, 1, "", "", 0.0, (uint8_t*)blob, 1, NULL, 0 );
			data[2] = MyRow( now+1234, 1, 0x12, 0x1234, "a", "a", 1.1, (uint8_t*)blob, 1, (uint8_t*)blob, 1 );
			data[3] = MyRow( now+0x100, 1, 0xCF, 0xFFFF, "abcdefghij", "abcdefghij", 1.1, (uint8_t*)blob, 10, (uint8_t*)blob, 10 );


			// Copy the data to the Sparrow Buffer, one row after another
			for ( uint32_t i=0; i<nbRows; ++i ) {
				int	res = spwBuffer->addRow( data[i] );
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
		//getchar();
	}
}

void TestAlltypes::sendErrorData( const Table* table ) {

	// Error cases
	const uint32_t		buffSize = 64*1024*512;
	try
	{
		printf("Sending erroneous data samples to table %s [1]...", table->getTableName());
		Sparrow::AutoPtr<SparrowBuffer>	spwBuffer( connect_->createBuffer( table, buffSize ) );

		time_t	ltime;
		time( &ltime );
		uint64_t	now = ltime;
		now *= 1000;
		//const int		blobLen = 11;		// columns length + 1
		//uint8_t		blob[blobLen];
		// for ( int i=0; i<blobLen; ++i )
			// blob[i] = i+1;

		const uint32_t	nbRows = 1;
		MyRow		data[nbRows];
		data[0] = MyRow( now, 0, 0, 0, NULL, NULL, 0.0, NULL, 0, NULL, 0 );

		// Copy the data to the Sparrow Buffer, one row after another
		for ( uint32_t i=0; i<nbRows; ++i ) {
			int	res = spwBuffer->addRow( data[i] );
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
		printf("ASSERT!\n");
	} 
	catch ( const MyException& e )
	{
		printf( "Exception! %s : %s\n", e.getText(), errmsg() );
	}


	// Error cases
	try
	{
		printf("Sending erroneous data samples to table %s [2]...", table->getTableName());
		Sparrow::AutoPtr<SparrowBuffer>	spwBuffer( connect_->createBuffer( table, buffSize ) );

		time_t	ltime;
		time( &ltime );
		uint64_t	now = ltime;
		now *= 1000;
		const int		blobLen = 11;		// columns length + 1
		uint8_t		blob[blobLen];
		for ( int i=0; i<blobLen; ++i )
			blob[i] = i+1;

		const uint32_t	nbRows = 1;
		MyRow		data[nbRows];
		data[0] = MyRow( now, 1, 0xFF, 0xFFFF, "abcdefghijk", "abcdefghijk", 1.1, (uint8_t*)blob, 11, (uint8_t*)blob, 11 );

		// Copy the data to the Sparrow Buffer, one row after another
		for ( uint32_t i=0; i<nbRows; ++i ) {
			int	res = spwBuffer->addRow( data[i] );
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
		printf("ASSERT!\n");
	} 
	catch ( const MyException& e )
	{
		printf( "Exception! %s : %s\n", e.getText(), errmsg() );
	}

}

void TestAlltypes::sendDataFlow( const Table* table )
{
	const uint		buffSize = 64*1024*512;
	const uint		nbRows = 10000;
	try
	{
		printf("Sending data flow to table %s...", table->getTableName());
		Sparrow::AutoPtr<SparrowBuffer>	spwBuffer( connect_->createBuffer( table, buffSize ) );
		if ( spwBuffer.get() == NULL ) 
			throw MyException::create( false, "Failed to create SparrowBuffer object." );

		time_t	ltime;
		time( &ltime );
		uint64_t	now = ltime;
		now *= 1000;
		const int		blobLen = 11;		// columns length + 1
		uint8_t		blob[blobLen];
		for ( int i=0; i<blobLen; ++i )
			blob[i] = i+1;

		for ( uint i=0; i<nbRows; ++i ) {
			MyRow		row( now+i, i+1, 0xCF, 0xFFFF, "abcdefghij", "abcdefghij", 1.1, (uint8_t*)blob, 10, (uint8_t*)blob, 10 );
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


