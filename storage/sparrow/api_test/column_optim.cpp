#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <sstream>

#include "column_optim.h"

#include "my_sys.h"
#include "my_systime.h"
#include "sql.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MyRow6
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MyRow6 : public SparrowRow
{
private:
	uint64_t	ts_;
	mutable int		counter_;
	int		null_col_;
	int		nb_col_;

public:
	MyRow6(int null_col, int nb_col) : counter_(0), null_col_(null_col), nb_col_(nb_col) {
		time_t	ltime;
		time( &ltime );
		ts_ = ltime*1000;
	}

	int decode(SparrowBuffer* buffer, void* /*dummy*/) const override;
};

int MyRow6::decode(SparrowBuffer* buffer, void* /*dummy*/) const
{
	counter_++;

	int		col = 0;
	int		res = 0;
	if ((res=buffer->addLong(col++, ts_)) != 0)
		return res;
	if (null_col_ <= col && (null_col_+nb_col_) > col) {
		buffer->addNull(col++);
	} else {
		if ((res=buffer->addByte(col++, (unsigned char)counter_)) != 0)
			return res;
	}

	if (null_col_ <= col && (null_col_+nb_col_) > col) {
		buffer->addNull(col++);
	} else {
		if ((res=buffer->addShort(col++, (unsigned short)counter_)) != 0)
			return res;
	}

	if (null_col_ <= col && (null_col_+nb_col_) > col) {
		buffer->addNull(col++);
	} else {
		if ((res=buffer->addInt(col++, counter_)) != 0)
			return res;
	}

	if (null_col_ <= col && (null_col_+nb_col_) > col) {
		buffer->addNull(col++);
	} else {
		if ((res=buffer->addLong(col++, counter_)) != 0)
			return res;
	}

	if (null_col_ <= col && (null_col_+nb_col_) > col) {
		buffer->addNull(col++);
	} else {
		if ((res=buffer->addDouble(col++, counter_)) != 0)
			return res;
	}

	if (null_col_ <= col && (null_col_+nb_col_) > col) {
		buffer->addNull(col++);
	} else {
		uint8_t	blob[4];
		blob[0] = counter_>>24;
		blob[1] = (counter_>>16)&0xFF;
		blob[2] = (counter_>>8)&0xFF;
		blob[3] = counter_&0xFF;
		if ((res=buffer->addBlob(col++, blob, sizeof(blob))) != 0)
			return res;
	}

	if (null_col_ <= col && (null_col_+nb_col_) > col) {
		buffer->addNull(col++);
	} else {
		char	str[32];
		sprintf(str, "%u", counter_);
		if ((res=buffer->addString(col++, str)) != 0)
			return res;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MyRow7
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MyRow7 : public SparrowRow
{
private:
	uint64_t	ts_;
	uint8_t*	buffer_;
	uint	length_;

public:
	MyRow7(uint64_t ts, uint8_t* buffer, uint length) : ts_(ts), buffer_(buffer), length_(length) {;}

	int decode(SparrowBuffer* buffer, void* /*dummy*/) const override;
};

int MyRow7::decode(SparrowBuffer* buffer, void* /*dummy*/) const
{
	int		col = 0;
	buffer->addLong(col++, ts_);
	buffer->addBlob(col++, buffer_, length_);
	buffer->addInt(col++, 1);

	return 0;
}




//////////////////////////////////////////////////////////////////////////////////////////////////////
//	TestColumnOptim

TestColumnOptim::TestColumnOptim(const SQLparams& sql_params) : Test(sql_params) {
	flushInterval_ = getFlushInterval();
}

void TestColumnOptim::runSimple() {
	try
	{
		const char*		table_name = "table_null_cols";
		const uint		nbRows = 10;
		const uint		null_col = 1;

		// Initialize environment
		dropTable(table_name);

		printf("Creating table %s. Nulled columns is %u\n", table_name, null_col);
		AutoPtr<Table>		table( createTable(table_name) );
		sendData(table.get(), nbRows, null_col, 1);
		waitForFlush();

	} catch ( const MyException& e ) {
		printf( "Test failed: %s : %s\n", e.getText(), errmsg() );
	}
}

/* Checks that the partition can still be read correctly after adding or removing columns */
void TestColumnOptim::runtAlterTests(const int null_col) {
	try
	{
		const char*		table_name = "table_alter";
		const uint		nbRows = 10;

		// Initialize environment
		dropTable(table_name);

		printf("Creating table %s. Null column %u\n", table_name, null_col);
		AutoPtr<Table>		table( createTable(table_name) );
		sendData(table.get(), nbRows, null_col, 1);
		waitForFlush();
		printf("Check data can be read correctly...\n"); 
		getchar();

		printf("Inserting column after %u\n", null_col);
		table = createTable(table_name, -1, null_col+1);
		printf("Check data can still be read correctly...\n"); 
		getchar();

		printf("Inserting column before %u\n", null_col);
		table = createTable(table_name, -1, null_col-1);
		printf("Check data can still be read correctly...\n"); 
		getchar();

		printf("Dropping column after %u\n", null_col);
		table = createTable(table_name, null_col+1);
		printf("Check data can still be read correctly...\n"); 
		getchar();

		printf("Dropping column before %u\n", null_col);
		table = createTable(table_name, null_col-1);
		printf("Check data can still be read correctly...\n"); 
		getchar();

		printf("Dropping null column %u\n", null_col);
		table = createTable(table_name, null_col);
		printf("Check data can still be read correctly...\n"); 
		getchar();

	} catch ( const MyException& e ) {
		printf( "Test failed: %s : %s\n", e.getText(), errmsg() );
	}
}

void TestColumnOptim::runtAlterTests() {
	runtAlterTests(2);		// Data in column 2 is nulled
	runtAlterTests(7);		// Data in column 7 is nulled
}

/* Creates partitions with different null columns. Check all data can be read correctly */
void TestColumnOptim::runMultiPartTests() {
	try
	{
		const char*		table_name = "table_multi_part";
		const uint		nbRows = 10;
		
		// Initialize environment
		dropTable(table_name);

		printf("Creating table %s. Inserting valid data in all columns\n", table_name);
		AutoPtr<Table>	table( createTable(table_name) );
		uint	nbCols = table->getNbColumns() - 1;

		sendData(table.get(), nbRows, 0, 0);
		waitForFlush();

		for (uint n=1; n<=nbCols; ++n) {
			for (uint i=0; i<=nbCols-n; ++i) {
				printf("Inserting Null data in columns %u\n", i);
				sendData(table.get(), nbRows, i+1, n);
				waitForFlush();
			}
		}

	} catch ( const MyException& e ) {
		printf( "Test failed: %s : %s\n", e.getText(), errmsg() );
	}
}

void TestColumnOptim::runCoalescingTests(const uint nbRows) {
	try
	{
		const char*		table_name = "table_coalesc";

		// Initialize environment
		dropTable(table_name);

		printf("Creating table %s.\n", table_name);
		AutoPtr<Table>	table( createTable(table_name) );

		for (uint i=0; i<2; ++i) {
			printf("Inserting valid data in all columns\n");
			sendData(table.get(), nbRows, 0, 0);
			waitForFlush();

			printf("Inserting Null data in columns 1\n");
			sendData(table.get(), nbRows, 1, 1);
			waitForFlush();

			printf("Inserting Null data in columns 7\n");
			sendData(table.get(), nbRows, 7, 1);
			waitForFlush();
		}

	} catch ( const MyException& e ) {
		printf( "Test failed: %s : %s\n", e.getText(), errmsg() );
	}
}

void TestColumnOptim::runDNSTests() {
	try
	{
		const char*		table_name = "table_dns";
		const uint		nbRows = 10;

		// Initialize environment
		dropTable(table_name);

		printf("Creating table %s. Inserting valid data in all columns\n", table_name);
		AutoPtr<Table>	table( createTableDNS(table_name) );
		sendDataDNS(table.get(), nbRows);
		waitForFlush();

	} catch ( const MyException& e ) {
		printf( "Test failed: %s : %s\n", e.getText(), errmsg() );
	}
}

void TestColumnOptim::runVolumeTests() {
	runCoalescingTests( 1000000 );
}



// Create TEST Table

Table* TestColumnOptim::createTable( const char* table_name, int droppedCol, int insertCol, uint64_t coalescingPeriod )
{
	Table*	table = connect_->createTable();
	if ( !table )
		throw MyException::create( false, "Failed to create Sparrow table object for '%s'.'%s'", sql_params_.getSchema(), table_name );

	int	res;
	uint64_t	maxLifetime			= 24*3600*1000;
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
	int		col = 0, i = 0, last_indexable = 0;
	table->appendColumn( "timestamp", col++, COL_TIMESTAMP, 3 );
	++i;
	if (insertCol == i) {
		table->appendColumn( "insert_byte", col++, COL_BYTE, 0, COL_UNSIGNED | COL_NULLABLE );
	}
	if (droppedCol != i) {
		table->appendColumn( "value_byte", col++, COL_BYTE, 0, COL_UNSIGNED | COL_NULLABLE );
	}
	++i;
	if (insertCol == i) {
		table->appendColumn( "insert_short", col++, COL_SHORT, 0, COL_UNSIGNED | COL_NULLABLE );
	}
	if (droppedCol != i) {
		table->appendColumn( "value_short", col++, COL_SHORT, 0, COL_UNSIGNED | COL_NULLABLE );
	}
	++i;
	if (insertCol == i) {
		table->appendColumn( "insert_int", col++, COL_INT, 0, COL_UNSIGNED | COL_NULLABLE );
	}
	if (droppedCol != i) {
		table->appendColumn( "value_int", col++, COL_INT, 0, COL_UNSIGNED | COL_NULLABLE );
	}
	++i;
	if (insertCol == i) {
		table->appendColumn( "insert_long", col++, COL_LONG, 0, COL_UNSIGNED | COL_NULLABLE );
	}
	if (droppedCol != i) {
		table->appendColumn( "value_long", col++, COL_LONG, 0, COL_UNSIGNED | COL_NULLABLE );
	}
	++i;
	if (insertCol == i) {
		table->appendColumn( "insert_double", col++, COL_DOUBLE, 0, COL_NULLABLE );
	}
	if (droppedCol != i) {
		table->appendColumn( "value_double", col++, COL_DOUBLE, 0, COL_NULLABLE );
	}
	last_indexable = col - 1;
	++i;
	if (insertCol == i) {
		table->appendColumn( "insert_blob", col++, COL_BLOB, 0, COL_NULLABLE );
	}
	if (droppedCol != i) {
		table->appendColumn( "value_blob", col++, COL_BLOB, 0, COL_NULLABLE );
	}
	++i;
	if (insertCol == i) {
		table->appendColumn( "insert_str", col++, COL_STRING, 0, COL_NULLABLE );
	}
	if (droppedCol != i) {
		table->appendColumn( "value_str", col++, COL_STRING, 0, COL_NULLABLE );
	}
	++i;
	if (insertCol == i) {
		table->appendColumn( "insert_str_last", col++, COL_STRING, 0, COL_NULLABLE );
	}

	// Indexes
	int		indxId;
	if ( (indxId=table->appendIndex( "index_1", 0, false )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );

	for (int j=0; j<last_indexable-1; ++j) {
		char	index_name[64];
		sprintf(index_name, "index_%u", j+2);
		if ( (indxId=table->appendIndex( index_name, j, true )) < 0 ) 
			throw MyException::create( false, "Failed to create index %s", index_name );
		table->addColToIndex( indxId, j+1 );
	}

	if ( (res=table->create( connect_ )) != 0 )
		throw MyException::create( false, "Failed to create table '%s'.'%s', error code %u", sql_params_.getSchema(), table_name, res );

	return table;
}

Table* TestColumnOptim::createTableDNS( const char* table_name, uint64_t coalescingPeriod )
{
	Table*	table = connect_->createTable();
	if ( !table )
		throw MyException::create( false, "Failed to create Sparrow table object for '%s'.'%s'", sql_params_.getSchema(), table_name );

	int		res;
	uint64_t	maxLifetime			= 24*3600*1000;
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
	table->appendColumn( "IP", col++, COL_BLOB, 0, COL_IP_ADDRESS | COL_NULLABLE );
	table->appendColumn( "dnsId", col++, COL_INT, 0, COL_DNS_IDENTIFIER | COL_NULLABLE );
	table->appendColumn( "lookup", col++, COL_STRING, 256, COL_IP_LOOKUP | COL_NULLABLE, 1 );

	// Indexes
	int		indxId;
	if ( (indxId=table->appendIndex( "index_1", 0, false )) < 0 ) 
		throw MyException::create( false, "Failed to create index" );

	int		indxDnsEntry;
	indxDnsEntry = table->addDnsEntry( 1 );
	table->addDnsServer( indxDnsEntry, "frluldc03", 0, "10.1.26.218", 0 );
	table->addDnsServer( indxDnsEntry, "frluldc02", 0, "10.1.26.18", 0 );

	indxDnsEntry = table->addDnsEntry( 2 );
	table->addDnsServer( indxDnsEntry, "frluldc02", 0, "10.1.26.18", 0 );
	table->addDnsServer( indxDnsEntry, "frluldc03", 0, "10.1.26.218", 0 );

	if ( (res=table->create( connect_ )) != 0 )
		throw MyException::create( false, "Failed to create table '%s'.'%s', error code %u", sql_params_.getSchema(), table_name, res );

	return table;
}


void TestColumnOptim::sendData( const Table* table, uint nbRows, int null_col, int nb_col )
{
	uint32_t		buffSize = 16*1024*512;
	try
	{
		Sparrow::AutoPtr<SparrowBuffer>	spwBuffer( connect_->createBuffer( table, buffSize ) );
		if ( spwBuffer.get() == NULL ) 
			throw MyException::create( false, "Failed to create SparrowBuffer object." );

		printf("Inserting %u rows in table %s.\n", nbRows, table->getTableName());
		MyRow6	rows(null_col, nb_col);
		for ( uint i=0; i<nbRows; ++i ) {
			int	res = spwBuffer->addRow(rows);
			if ( res == SPW_API_BUFFER_FULL ) {
				printf("Flushing buffers (%u%% done).\n", (uint)(i*100.0/nbRows));
				connect_->insertData( table, spwBuffer.get() );
				spwBuffer->clear();
				// This row is not complete. Insert it again in next batch. 
				i--;
			} else if ( res < 0 ) {
				throw MyException::create( false, "failed to add row to Sparrow buffer %d", res );
			}
		}

		// Send the content of the resulting Sparrow Buffer
		connect_->insertData( table, spwBuffer.get() );
		printf("Done.\n");
	} 
	catch ( const MyException& e )
	{
		printf( "Exception! %s : %s\n", e.getText(), errmsg() );
	}
}


void TestColumnOptim::sendDataDNS( const Table* table, uint nbRows )
{
	uint32_t		buffSize = 64*1024*512;
	try
	{
		Sparrow::AutoPtr<SparrowBuffer>	spwBuffer( connect_->createBuffer( table, buffSize ) );
		if ( spwBuffer.get() == NULL ) 
			throw MyException::create( false, "Failed to create SparrowBuffer object." );

		time_t	ltime;
		time( &ltime );
		uint64_t	ts = ltime*1000;

		for ( uint i=1; i<nbRows; ++i ) {
			uint32_t		ip = (i<<24)|(12<<16)|(1<<8)|10;
			MyRow7	rows(ts, (uint8_t*)&ip, sizeof(ip));
			int	res = spwBuffer->addRow(rows);
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

void TestColumnOptim::waitForFlush() {
	printf("Waiting %us for partition flush...", flushInterval_);
	my_sleep((flushInterval_+1)*1000000ULL);		// We add one second to the theoretical flush period to be sure.
	printf("Ok\n");
}