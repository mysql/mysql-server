#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <sstream>

#include "errors.h"
#include "all_types.h"


class MyRowErr : public MyRow {
private:
	mutable int	skipCol_;		// Column to skip during insertion

public:
	MyRowErr() : MyRow(), skipCol_(-1) {;}
	MyRowErr(uint64_t timestamp, uint64_t slotId, uint8_t status, uint16_t samples, const char* name, const char* name_null, double value, 
		const uint8_t* blob, int blobLen, const uint8_t* blob_null, int blob_null_Len) : 
		MyRow(timestamp, slotId, status, samples, name, name_null, value, blob, blobLen, blob_null, blob_null_Len), skipCol_(-1) {;}

	int		getSkippedCol() const { return skipCol_; }
	void	setSkippedCol(int col) { skipCol_ = col; }

	int decode(SparrowBuffer* buffer, void* dummy) const override;
};

inline int MyRowErr::decode( SparrowBuffer* buffer, void* /*dummy*/ ) const {

	const int	nbCols = 13;

	int		col = -1;
	int		res;
	if ( (++col != skipCol_%nbCols) && (res=buffer->addLong( col, timestamp_ )) != 0 ) return res;
	if ( (++col != skipCol_%nbCols) && (res=buffer->addLong( col, slotId_ )) != 0 ) return res;
	if ( (++col != skipCol_%nbCols) && (res=buffer->addByte( col, status_ )) != 0 ) return res;
	if ( (++col != skipCol_%nbCols) && (res=buffer->addShort( col, samples_ )) != 0 ) return res;
	if ( (++col != skipCol_%nbCols) && (res=buffer->addDouble( col, value_ )) != 0 ) return res;
	if ( (++col != skipCol_%nbCols) && (res=buffer->addString( col, name_ )) != 0 ) return res;
	if ( (++col != skipCol_%nbCols) && (res=buffer->addByte( col, status_ )) != 0 ) return res;
	if ( (++col != skipCol_%nbCols) ) {
		if ( name_null_ == NULL ) {
			buffer->addNull( col++ );
		} else {
			if ( (res=buffer->addString( col, name_null_ )) != 0 ) return res;
		}
	}
	if ( (++col != skipCol_%nbCols) && (res=buffer->addByte( col, status_ )) != 0 ) return res;
	if ( (++col != skipCol_%nbCols) && (res=buffer->addBlob( col, blob_, blobLen_ )) != 0 ) return res;
	if ( (++col != skipCol_%nbCols) && (res=buffer->addByte( col, status_ )) != 0 ) return res;
	if ( (++col != skipCol_%nbCols) ) {
		if ( blob_null_ == NULL ) {
			buffer->addNull( col++ );
		} else { 
			if ( (res=buffer->addBlob( col, blob_null_, blob_null_Len_ )) != 0 ) return res;
		}
	}
	if ( (++col != skipCol_%nbCols) && (res=buffer->addByte( col, status_ )) != 0 ) return res;

	return 0;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////
// Create TEST Table

void TestErrors::run() {
	try
	{
		const char*		table_name = "table_errors";

		TestAlltypes	allTypesTests(sql_params_);

		printf("Creating table %s...", table_name);
		AutoPtr<Table>	table( allTypesTests.createTable(table_name) );
		printf("OK\n");

		runInsertionTests( table.get() );

	} catch ( const MyException& e ) {
		printf( "Test failed: %s : %s\n", e.getText(), errmsg() );
	}
}

void TestErrors::runInsertionTests( const Table* table ) {

	// Error cases
	const uint32_t		buffSize = 64*1024*512;
	try
	{
		Sparrow::AutoPtr<SparrowBuffer>	spwBuffer( connect_->createBuffer( table, buffSize ) );

		time_t	ltime;
		time( &ltime );
		uint64_t	now = ltime;
		now *= 1000;
		const int		blobLen = 11;		// columns length + 1
		uint8_t		blob[blobLen];
		for ( int i=0; i<blobLen; ++i )
			blob[i] = i+1;

		uint32_t		nbCols = table->getNbColumns();
		const uint32_t	nbRows = 1;
		MyRowErr		data[nbRows];
		data[0] = MyRowErr( now, 1, 0xCF, 0xFFFF, "abcdefghij", "abcdefghij", 1.1, (uint8_t*)blob, 10, (uint8_t*)blob, 10 );

		for (uint j=0; j<nbCols; ++j) {

			try {
				spwBuffer->clear();

				printf("Skipping value for column %u in insertion buffer for table %s...\r\n", j, table->getTableName());
				data[0].setSkippedCol(j);

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

			} catch ( const MyException& e ) {
				printf( "Exception! %s : %s\n", e.getText(), errmsg() );
			}
		}
	} 
	catch ( const MyException& e )
	{
		printf( "Exception! %s : %s\n", e.getText(), errmsg() );
	}
}


