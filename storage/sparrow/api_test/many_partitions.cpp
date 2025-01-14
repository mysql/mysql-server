#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <sstream>

#include "all_types.h"
#include "many_partitions.h"

//-----------------------------------------------------------------------------
void TestManyPartitions::run() {
	try
	{
		const char*		table_name = "table_many_partitions";

		TestAlltypes	allTypesTests(sql_params_);

		printf("Creating table %s...", table_name);
		AutoPtr<Table>	table( allTypesTests.createTable( table_name ) );
		printf("OK\n");

		createManyPartitions( table.get(), 5 );
		insertInterlacedTimestamps( table.get() );
		insertInterlacedTimestampsMassive( table.get() );

	} catch ( const MyException& e ) {
		printf( "Test failed: %s : %s\n", e.getText(), errmsg() );
	}
}

void TestManyPartitions::createManyPartitions( const Table* table, uint nbPartitions )
{
	uint32_t		buffSize = 64*1024*512;
	try
	{
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

		uint64_t	coalescing_period = table->getCoalescPeriod();

		for ( uint i=0; i<nbPartitions; ++i ) {
			MyRow		row( now+i, i+1, 0xCF, 0xFFFF, "abcdefghij", "abcdefghij", 1.1, (uint8_t*)blob, 10, (uint8_t*)blob, 10 );
			int	res = spwBuffer->addRow( row );
			if ( res < 0 )
				throw MyException::create( false, "failed to add row to Sparrow buffer %d", res );
			MyRow		row2( now+i-coalescing_period, i+1, 0xCF, 0xFFFF, "abcdefghij", "abcdefghij", 1.1, (uint8_t*)blob, 10, (uint8_t*)blob, 10 );
			res = spwBuffer->addRow( row2 );
			if ( res < 0 )
				throw MyException::create( false, "failed to add row to Sparrow buffer %d", res );
		}

		// Send the content of the resulting Sparrow Buffer
		connect_->insertData( table, spwBuffer.get() );
	} 
	catch ( const MyException& e )
	{
		printf( "Exception! %s : %s\n", e.getText(), errmsg() );
	}
}

void TestManyPartitions::insertInterlacedTimestamps( const Table* table )
{
	uint32_t		buffSize = 64*1024*1024;
	try
	{
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

		uint64_t	coalescing_period = table->getCoalescPeriod();
		uint64_t	t0 = now - now%coalescing_period;
		const int	increment[] = {2, -2, 0, -1, 3, -3, 1, -1};
		const uint	nb_incr = sizeof(increment)/sizeof(increment[0]);
		for ( uint i=0; i<nb_incr; ++i ) {
			MyRow		row( t0+increment[i], i+1, 0xCF, 0xFFFF, "abcdefghij", "abcdefghij", 1.1, (uint8_t*)blob, 10, (uint8_t*)blob, 10 );
			int	res = spwBuffer->addRow( row );
			if ( res < 0 )
				throw MyException::create( false, "failed to add row to Sparrow buffer %d", res );
		}

		// Send the content of the resulting Sparrow Buffer
		connect_->insertData( table, spwBuffer.get() );
	} 
	catch ( const MyException& e )
	{
		printf( "Exception! %s : %s\n", e.getText(), errmsg() );
	}
}

void TestManyPartitions::insertInterlacedTimestampsMassive( const Table* table )
{
	const uint	nbrows = 100;
	uint32_t		buffSize = 64*1024*1024;
	try
	{
		time_t	ltime;
		time( &ltime );
		uint64_t	now = ltime;
		now *= 1000;
		const int		blobLen = 11;		// columns length + 1
		uint8_t		blob[blobLen];
		for ( int i=0; i<blobLen; ++i )
			blob[i] = i+1;

		uint64_t	coalescing_period = table->getCoalescPeriod();
		uint64_t	t0 = now - now%coalescing_period;
		Sparrow::AutoPtr<SparrowBuffer>	spwBuffer( connect_->createBuffer( table, buffSize ) );
		if ( spwBuffer.get() == NULL ) 
			throw MyException::create( false, "Failed to create SparrowBuffer object." );

		for ( uint i=0; i<nbrows; ++i ) {
			MyRow		row( t0-(i%24)*3600000+i/24, i+1, 0xCF, 0xFFFF, "abcdefghij", "abcdefghij", 1.1, (uint8_t*)blob, 10, (uint8_t*)blob, 10 );
			int	res = spwBuffer->addRow( row );
			if ( res < 0 ) {
				if ( res == SPW_API_BUFFER_FULL ) {
					connect_->insertData( table, spwBuffer.get() );
					spwBuffer = connect_->createBuffer( table, buffSize );
				} else {
					throw MyException::create( false, "failed to add row to Sparrow buffer %d", res );
				}
			}
		}

		// Send the content of the resulting Sparrow Buffer
		connect_->insertData( table, spwBuffer.get() );
	} 
	catch ( const MyException& e )
	{
		printf( "Exception! %s : %s\n", e.getText(), errmsg() );
	}
}


