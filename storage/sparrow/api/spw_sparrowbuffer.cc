#include "memalloc.h"
#include "spw_sparrowbuffer.h"
#include "spw_table.h"

#include  <cmath>

#define SPW_TRY		try {
#define SPW_CATCH	} catch ( const SparrowException& e ) {		\
	PRINT_DBUG( e.getText() );	\
	spwerror = e;		\
	return e.getErrcode(); \
	}


namespace Sparrow
{

#define BUFFER_SIZE	65536

spw_SparrowBuffer::spw_SparrowBuffer(const Table* table, uint32_t capacity) 
	: buffer_( capacity, BUFFER_SIZE ), table_(table), cursor_(0)
{
	SPW_ASSERT(table);

	clear();

	const spw_Table*	tbl = static_cast<const spw_Table*>(table);
	columns_ = tbl->getColumns();
}

spw_SparrowBuffer::~spw_SparrowBuffer(void)
{
}

void spw_SparrowBuffer::clear() 
{
	buffer_.clear();
	rows_		= 0;
	timestamp_	= 0;
	cursor_		= 0;
}

// throws an exception if input data type is not compatible with column
void spw_SparrowBuffer::checkCompatibility( int column, ColumnType inType ) _THROW_(SparrowException)
{
	if ( column != (int)(cursor_++%columns_.getCount())) {
		throw SparrowException::create(false, SPW_API_COLINDX_OOB, "Bad insertion sequence in table %s: inserted value for column %u"
			", expected value for column %u", (table_ != NULL ? table_->getTableName() : "???"), column, ((cursor_-1)%columns_.getCount()));
	}
	if ( column < 0 || static_cast<uint32_t>(column) > columns_.getCount() ) {
		throw SparrowException::create(false, SPW_API_COLINDX_OOB, "Column index %u is out of bounds for table %s", 
			column, (table_ != NULL ? table_->getTableName() : "???"));
	}
	const spw_Column&	col = columns_[column];
	bool	compatible = true;
	if ( col.getType() == COL_TIMESTAMP ) {
		if ( inType != COL_LONG ) {
			compatible = false;
		}
	} else if ( col.getType() != inType ) {
		compatible = false;
	}
	if ( !compatible ) {
		throw SparrowException::create(false, SPW_API_INCOMPATIBLE_TYPES, "Cannot insert %s data in %s.%s", 
			getType(inType), (table_ != NULL ? table_->getTableName() : "???"), col.getName());
	}
}

int spw_SparrowBuffer::addNull(int column)
{
	SPW_TRY
	cursor_++;
	if ( columns_[column].isFlagSet(COL_NULLABLE) ) {
		buffer_.put( (uint8_t)1 );
	} else {
		throw SparrowException::create(false, SPW_API_COL_NOT_NULLABLE, "Column \"%s\" cannot contain NULL.", columns_[column].getName());
	}
	SPW_CATCH
	return 0;
}

int spw_SparrowBuffer::addBool(int column, bool value)
{
	SPW_TRY
	checkCompatibility( column, COL_BYTE );
	if ( columns_[column].isFlagSet(COL_NULLABLE) ) {
		buffer_.put( (uint8_t)0 );
	}
	buffer_.put( (uint8_t)(value ? 1 : 0) );
	SPW_CATCH
	return 0;
}

int spw_SparrowBuffer::addByte(int column, uint8_t value)
{
	SPW_TRY
	checkCompatibility( column, COL_BYTE );
	if ( columns_[column].isFlagSet(COL_NULLABLE) ) {
		buffer_.put( (uint8_t)0 );
	}
	buffer_.put( value );
	SPW_CATCH
	return 0;
}

int spw_SparrowBuffer::addShort(int column, uint16_t value)
{
	SPW_TRY
	checkCompatibility( column, COL_SHORT );
	if ( columns_[column].isFlagSet(COL_NULLABLE) ) {
		buffer_.put( (uint8_t)0 );
	}
	buffer_.putShort( value );
	SPW_CATCH
	return 0;
}

int spw_SparrowBuffer::addInt(int column, uint32_t value)
{
	SPW_TRY
	checkCompatibility( column, COL_INT );
	if ( columns_[column].isFlagSet(COL_NULLABLE) ) {
		buffer_.put( (uint8_t)0 );
	}
	buffer_.putInt( value );
	SPW_CATCH
	return 0;
}

int spw_SparrowBuffer::addLong(int column, uint64_t value)
{
	SPW_TRY
	checkCompatibility( column, COL_LONG );
	if ( columns_[column].isFlagSet(COL_NULLABLE) ) {
		buffer_.put( (uint8_t)0 );
	}
	buffer_.putLong( value );
	SPW_CATCH
	return 0;
}

int spw_SparrowBuffer::addDouble(int column, double value)
{
	SPW_TRY
	checkCompatibility( column, COL_DOUBLE );
	if (std::isfinite(value)) {
		if ( columns_[column].isFlagSet(COL_NULLABLE) ) {
			buffer_.put( (uint8_t)0 );
		}
		buffer_.putDouble( value );
	} else {
		if ( columns_[column].isFlagSet(COL_NULLABLE) ) {
			buffer_.put( (uint8_t)1 );
		}
		buffer_.putDouble(0);
	}
	SPW_CATCH
	return 0;
}

int spw_SparrowBuffer::addString(int column, const char* value)
{
	SPW_TRY
	checkCompatibility( column, COL_STRING );
	bool		nullable = columns_[column].isFlagSet(COL_NULLABLE);
	if ( value == NULL ) {
		if ( nullable ) {
			buffer_.put( (uint8_t)1 );
		} else {
			throw SparrowException::create(false, SPW_API_COL_NOT_NULLABLE, "Column \"%s\" cannot contain NULL.", columns_[column].getName());
		}
	} else {
		if ( nullable ) {
			buffer_.put( (uint8_t)0 );
		}
		buffer_.put( value );
	}
	SPW_CATCH
	return 0;
}

int spw_SparrowBuffer::addBlob(int column, const uint8_t* value, uint32_t length)
{
	SPW_TRY
	checkCompatibility( column, COL_BLOB );
	bool		nullable = columns_[column].isFlagSet(COL_NULLABLE);
	if ( length == 0 && nullable ) {
		buffer_.put( (uint8_t)1 );
	} else {
		if ( nullable ) {
			buffer_.put( (uint8_t)0 );
		}
		buffer_.putInt( length );
		buffer_.put( ByteBuffer( value, length ) );
	}
	SPW_CATCH
	return 0;
}

// If it's a buffer full error, it is caught and replaced by an error code
int spw_SparrowBuffer::addRow( const SparrowRow& row, void* dummy /* =NULL */ )
{
	int		res = 0;
	const bool wasEmpty = timestamp_ == 0;
	buffer_.mark();
	if ( (res=row.decode( this, dummy )) != 0 ) {
		buffer_.reset();
		return res;
	}
	rows_++;
	if (wasEmpty) {
		timestamp_ = time(0);
	}

	return 0;

	/*try {
		row.decode( this, table_, dummy );
		rows_++;
		if (wasEmpty) {
			timestamp_ = time(0);
		}
	} catch ( const SparrowException& e ) {
		buffer_.reset();
		if ( e.getErrcode() != SPW_API_BUFFER_FULL ) {
			throw e;
		} else { 
			return false;
		}
	}
	return true;*/
}

}
