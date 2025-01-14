#ifndef _spw_test_all_types_h
#define _spw_test_all_types_h

#include "common.h"

using namespace Sparrow;


//////////////////////////////////////////////////////////////////////////////////////////////////////
// MyRow
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MyRow : public SparrowRow
{
public:
	uint64_t	timestamp_;
	uint64_t	slotId_;
	uint8_t	status_;
	uint16_t	samples_;
	char*	name_;				// string
	char*	name_null_;			// string
	double	value_;
	uint8_t*	blob_;
	int		blobLen_;
	uint8_t*	blob_null_;
	int		blob_null_Len_;

public:
	MyRow();
	MyRow(uint64_t, uint64_t, uint8_t, uint16_t, const char*, const char*, double, const uint8_t*, int, const uint8_t*, int);
	MyRow(const MyRow&);
	~MyRow();

	const MyRow& operator =(const MyRow&);

	int decode(SparrowBuffer* buffer, void* dummy) const override;
};

inline MyRow::MyRow() 
	: timestamp_(0), slotId_(0), status_(0), samples_(0), name_(NULL), name_null_(NULL),
	value_(0.0), blob_(NULL), blobLen_(0), blob_null_(NULL), blob_null_Len_(0)
{
}

inline MyRow::MyRow( uint64_t timestamp, uint64_t slotId, uint8_t status, uint16_t samples, 
	const char* name, const char* name_null, double value, 
	const uint8_t* blob, int blobLen, const uint8_t* blob_null, int blob_null_Len) 
	: timestamp_(timestamp), slotId_(slotId), status_(status), samples_(samples), name_(NULL), name_null_(NULL),
	value_(value), blob_(NULL), blobLen_(0), blob_null_(NULL), blob_null_Len_(0)
{
	if ( name != NULL ) {
		name_ = new char[strlen(name)+1];
		strcpy( name_, name );
	}
	if ( name_null != NULL ) {
		name_null_ = new char[strlen(name_null)+1];
		strcpy( name_null_, name_null );
	}
	if ( blob != NULL ) {
		int		len = blobLen > 0 ? blobLen : 1;
		blob_ = new uint8_t[len];
		memcpy( blob_, blob, blobLen );
		blobLen_ = blobLen;
	}
	if ( blob_null != NULL ) {
		int		len = blob_null_Len > 0 ? blob_null_Len : 1;
		blob_null_ = new uint8_t[len];
		memcpy( blob_null_, blob_null, blob_null_Len );
		blob_null_Len_ = blob_null_Len;
	}
}


inline MyRow::MyRow( const MyRow& right )
{
	timestamp_	= right.timestamp_;
	slotId_		= right.slotId_;
	status_		= right.status_;
	samples_	= right.samples_;
	value_		= right.value_;

	if ( right.name_ != NULL ) {
		name_ = new char[strlen(right.name_)+1];
		strcpy( name_, right.name_ );
	} else {
		name_ = NULL;
	}

	if ( right.name_null_ != NULL ) {
		name_null_ = new char[strlen(right.name_null_)+1];
		strcpy( name_null_, right.name_null_ );
	} else {
		name_null_ = NULL;
	}

	if ( right.blob_ != NULL ) {
		int		len = right.blobLen_ > 0 ? right.blobLen_ : 1;
		blob_ = new uint8_t[len];
		memcpy( blob_, right.blob_, right.blobLen_ );
		blobLen_ = right.blobLen_;
	} else {
		blob_ = NULL;
		blobLen_ = 0;
	}

	if ( right.blob_null_ != NULL ) {
		int		len = right.blob_null_Len_ > 0 ? right.blob_null_Len_ : 1;
		blob_null_ = new uint8_t[len];
		memcpy( blob_null_, right.blob_null_, right.blob_null_Len_ );
		blob_null_Len_ = right.blob_null_Len_;
	} else {
		blob_null_ = NULL;
		blob_null_Len_ = 0;
	}
}

inline const MyRow& MyRow::operator = ( const MyRow& right )
{
	if ( this == &right )
		return *this;

	timestamp_	= right.timestamp_;
	slotId_		= right.slotId_;
	status_		= right.status_;
	samples_	= right.samples_;
	value_		= right.value_;

	if ( name_ != NULL ) {
		delete [] name_; name_ = NULL;
	}
	if ( right.name_ != NULL ) {
		name_ = new char[strlen(right.name_)+1];
		strcpy( name_, right.name_ );
	} else {
		name_ = NULL;
	}

	if ( name_null_ != NULL ) {
		delete [] name_null_;
	}
	if ( right.name_null_ != NULL ) {
		name_null_ = new char[strlen(right.name_null_)+1];
		strcpy( name_null_, right.name_null_ );
	} else {
		name_null_ = NULL;
	}

	if ( blob_ != NULL ) {
		delete [] blob_; blob_ = NULL;
		blobLen_ = 0;
	}
	if ( right.blob_ != NULL ) {
		int		len = right.blobLen_ > 0 ? right.blobLen_ : 1;
		blob_ = new uint8_t[len];
		memcpy( blob_, right.blob_, right.blobLen_ );
		blobLen_ = right.blobLen_;
	} else {
		blob_ = NULL;
		blobLen_ = 0;
	}

	if ( blob_null_ != NULL ) {
		delete [] blob_null_;
	}
	if ( right.blob_null_ != NULL ) {
		int		len = right.blob_null_Len_ > 0 ? right.blob_null_Len_ : 1;
		blob_null_ = new uint8_t[len];
		memcpy( blob_null_, right.blob_null_, right.blob_null_Len_ );
		blob_null_Len_ = right.blob_null_Len_;
	} else {
		blob_null_ = NULL;
		blob_null_Len_ = 0;
	}

	return *this;
}

inline MyRow::~MyRow()
{
	if ( name_ != NULL ) {
		delete [] name_;
	}
	if ( name_null_ != NULL ) {
		delete [] name_null_;
	}
	if ( blob_ != NULL ) {
		delete [] blob_;
	}
	if ( blob_null_ != NULL ) {
		delete [] blob_null_;
	}
}

inline int MyRow::decode( SparrowBuffer* buffer, void* /*dummy*/ ) const {

	int		col = 0;
	int		res;
	if ( (res=buffer->addLong( col++, timestamp_ )) != 0 ) return res;
	if ( (res=buffer->addLong( col++, slotId_ )) != 0 ) return res;
	if ( (res=buffer->addByte( col++, status_ )) != 0 ) return res;
	if ( (res=buffer->addShort( col++, samples_ )) != 0 ) return res;
	if ( (res=buffer->addDouble( col++, value_ )) != 0 ) return res;
	if ( (res=buffer->addString( col++, name_ )) != 0 ) return res;
	if ( (res=buffer->addByte( col++, status_ )) != 0 ) return res;
	if ( name_null_ == NULL ) {
		buffer->addNull( col++ );
	} else {
		if ( (res=buffer->addString( col++, name_null_ )) != 0 ) return res;
	}
	if ( (res=buffer->addByte( col++, status_ )) != 0 ) return res;
	if ( (res=buffer->addBlob( col++, blob_, blobLen_ )) != 0 ) return res;
	if ( (res=buffer->addByte( col++, status_ )) != 0 ) return res;
	if ( blob_null_ == NULL ) {
		buffer->addNull( col++ );
	} else { 
		if ( (res=buffer->addBlob( col++, blob_null_, blob_null_Len_ )) != 0 ) return res;
	}
	if ( (res=buffer->addByte( col++, status_ )) != 0 ) return res;

	return 0;
}

//-----------------------------------------------------------------------------

class TestAlltypes : public Test {
	friend class TestManyPartitions;
	friend class TestErrors;
public:
	TestAlltypes(const SQLparams& sql_params) : Test(sql_params) {;}

	void run();

public:
	void runMasterFileTest(const Table*);
	void runGetTableTest(const Table* table);
	void runDisableCoalescingGlobTest(bool loop=false);
	void runDisableCoalescingSchemaTest(const char* schema, bool loop=false);
	Table* createTable(const char* table_name);
	void sendSampleData(const Table* table);
	void sendErrorData(const Table* table);
	void sendDataFlow(const Table* table);
};

#endif	// _spw_test_all_types_h