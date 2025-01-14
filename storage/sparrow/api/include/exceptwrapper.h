/*
	Wrappers classes that transform return codes into exceptions.
*/

#ifndef _sparrow_api_exceptwrapper_h_
#define _sparrow_api_exceptwrapper_h_

#include "global.h"
#include "sparrowbuffer.h"
#include "exception.h"
#include "connection.h"

#ifdef SPARROW_API_EXPORTS
//#define SPW_EXCPTMSG	Sparrow::spwerror.getText()
#define SPW_EXCPTMSG	Sparrow::errmsg()
#else
#define SPW_EXCPTMSG	Sparrow::errmsg()
#endif

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SparrowBufferEx - SparrowBuffer with exceptions
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Encapsulates above class methods and transforms error codes into exceptions.
class SparrowBufferEx
{
private:
	SparrowBuffer*	buffer_;

public:
	SparrowBufferEx(SparrowBuffer* buffer) : buffer_(buffer)
	{;}

	operator SparrowBuffer* () {
		return buffer_;
	}

	SparrowBuffer* operator -> () { return buffer_; }
	SparrowBuffer* get() { return buffer_; }


	// Called by the client application's implementation of SparrowRow::decode()
	// All methods can throw a SparrowException
	void addNull(int column) _THROW_(SparrowException);
	void addBool(int column, bool value) _THROW_(SparrowException);
	void addByte(int column, uint8_t value) _THROW_(SparrowException);
	void addShort(int column, uint16_t value) _THROW_(SparrowException);
	void addInt(int column, uint32_t value) _THROW_(SparrowException);
	void addLong(int column, uint64_t value) _THROW_(SparrowException);
	void addDouble(int column, double value) _THROW_(SparrowException);
	void addString(int column, const char* value) _THROW_(SparrowException);
	void addBlob(int column, const uint8_t* value, uint32_t length) _THROW_(SparrowException);

	// Called by the client application
	bool addRow(const SparrowRow& row, void* dummy=NULL) _THROW_(SparrowException);
};

inline void SparrowBufferEx::addNull( int column ) _THROW_(SparrowException) {
	int		res = buffer_->addNull( column );
	if ( res != 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
}

inline void SparrowBufferEx::addBool( int column, bool value ) _THROW_(SparrowException) {
	int		res = buffer_->addBool( column, value );
	if ( res != 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
}

inline void SparrowBufferEx::addByte( int column, uint8_t value ) _THROW_(SparrowException) {
	int		res = buffer_->addByte( column, value );
	if ( res != 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
}

inline void SparrowBufferEx::addShort( int column, uint16_t value ) _THROW_(SparrowException) {
	int		res = buffer_->addShort( column, value );
	if ( res != 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
}

inline void SparrowBufferEx::addInt( int column, uint32_t value ) _THROW_(SparrowException) {
	int		res = buffer_->addInt( column, value );
	if ( res != 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
}

inline void SparrowBufferEx::addLong( int column, uint64_t value ) _THROW_(SparrowException) {
	int		res = buffer_->addLong( column, value );
	if ( res != 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
}

inline void SparrowBufferEx::addDouble( int column, double value ) _THROW_(SparrowException) {
	int		res = buffer_->addDouble( column, value );
	if ( res != 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
}

inline void SparrowBufferEx::addString( int column, const char* value ) _THROW_(SparrowException) {
	int		res = buffer_->addString( column, value );
	if ( res != 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
}

inline void SparrowBufferEx::addBlob( int column, const uint8_t* value, uint32_t length ) _THROW_(SparrowException) {
	int		res = buffer_->addBlob( column, value, length );
	if ( res != 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
}

inline bool SparrowBufferEx::addRow( const SparrowRow& row, void* dummy ) _THROW_(SparrowException) {
	int		res = buffer_->addRow( row, dummy );
	if ( res == SPW_API_BUFFER_FULL ) {
		return false;
	}
	else if ( res != 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// TableEx - Table with exceptions
//////////////////////////////////////////////////////////////////////////////////////////////////////

class TableEx
{
private:
	Table*	table_;

public:
	TableEx(Table* table) : table_(table)
	{;}

	operator Table* () {
		return table_;
	}

	Table* operator -> () { return table_; }
	Table* get()  { return table_; }


	// Methods for specifying the table
	void setDatabaseName(const char*);
	void setTableName(const char*);
	void setMaxLifetime(uint64_t);
	void setCoalescPeriod(uint64_t);
	void setAggregPeriod(uint32_t);

	const char* getDatabaseName() const;
	const char* getTableName() const;
	uint64_t getMaxLifetime() const;
	uint64_t getCoalescPeriod() const;
	uint32_t getAggregPeriod() const;

	int appendColumn(const char* name, uint32_t index, ColumnType type, uint32_t stringSize=0,
		uint32_t flags=0, uint32_t info=0, const char* charset=DEF_CHARSET) _THROW_(SparrowException);
	uint32_t getNbColumns() const;
	const Column& getColumn(uint32_t index);

	int appendIndex(const char* name, uint32_t colIndex, bool unique) _THROW_(SparrowException);

	int addColToIndex(uint32_t indexId, uint32_t colIndex) _THROW_(SparrowException);

	int appendFK(const char* name, uint32_t colIndex, const char* databaseName, const char* tableName,
		const char* columnName) _THROW_(SparrowException);

	int addDnsEntry(uint32_t dnsEntry) _THROW_(SparrowException);
	int addDnsServer(uint32_t entryIndex, const char* name, uint32_t port, const char* sourcAddr, uint32_t sourcePort) _THROW_(SparrowException);

	// Creates the table and/or database schema if they don't exist. Updates the table if it exists.
	int create(Connection* connection) _THROW_(SparrowException);
};



inline void TableEx::setDatabaseName( const char* name ) {
	table_->setDatabaseName( name );
}

inline void TableEx::setTableName( const char* name ) {
	table_->setTableName( name );
}

inline void TableEx::setMaxLifetime( uint64_t value ) {
	table_->setMaxLifetime( value );
}

inline void TableEx::setCoalescPeriod( uint64_t value ) {
	table_->setCoalescPeriod( value );
}

inline void TableEx::setAggregPeriod( uint32_t value ) {
	table_->setAggregPeriod( value );
}

inline const char* TableEx::getDatabaseName() const {
	return table_->getDatabaseName();
}

inline const char* TableEx::getTableName() const {
	return table_->getTableName();
}

inline uint64_t TableEx::getMaxLifetime() const {
	return table_->getMaxLifetime();
}

inline uint64_t TableEx::getCoalescPeriod() const {
	return table_->getCoalescPeriod();
}

inline uint32_t TableEx::getAggregPeriod() const {
	return table_->getAggregPeriod();
}

inline int TableEx::appendColumn(const char* name, uint32_t index, ColumnType type, uint32_t stringSize,
								 uint32_t flags, uint32_t info, const char* charset) _THROW_(SparrowException)
{
	 int		res = table_->appendColumn( name, index, type, stringSize, flags, info, charset );
	 if ( res < 0 ) {
		 throw SparrowException( SPW_EXCPTMSG, false, res );
	 }
	 return res;
}

inline uint32_t TableEx::getNbColumns() const {
	return table_->getNbColumns();
}

inline const Column& TableEx::getColumn( uint32_t index ) {
	return table_->getColumn( index );
}

inline int TableEx::appendIndex( const char* name, uint32_t colIndex, bool unique ) _THROW_(SparrowException)
{
	int		res = table_->appendIndex( name, colIndex, unique );
	if ( res < 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
	return res;
}

inline int TableEx::appendFK( const char* name, uint32_t colIndex, const char* databaseName, const char* tableName,
			 const char* columnName ) _THROW_(SparrowException)
{
	int		res = table_->appendFK( name, colIndex, databaseName, tableName, columnName );
	if ( res < 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
	return res;
}

inline int TableEx::addDnsEntry(uint32_t dnsEntry) _THROW_(SparrowException)
{
	int		res = table_->addDnsEntry( dnsEntry );
	if ( res < 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
	return res;
}

inline int TableEx::addDnsServer(uint32_t entryIndex, const char* name, uint32_t port, const char* sourcAddr, uint32_t sourcePort) _THROW_(SparrowException)
{
	int		res = table_->addDnsServer( entryIndex, name, port, sourcAddr, sourcePort );
	if ( res < 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
	return res;
}

inline int TableEx::create(Connection* connection) _THROW_(SparrowException)
{
	int		res = table_->create( connection );
	if ( res < 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
	return res;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////
// ConnectionEx - Connection with exceptions
//////////////////////////////////////////////////////////////////////////////////////////////////////

class ConnectionEx
{
private:
	Connection*		conn_;

public:
	ConnectionEx(Connection* conn) : conn_(conn)
	{;}

	operator Connection* () {
		return conn_;
	}

	Connection* operator -> () { return conn_; }
	Connection* get()  { return conn_; }

	int insertData(const Table* table, const SparrowBuffer* buffers);

};

inline int ConnectionEx::insertData(const Table* table, const SparrowBuffer* buffers) {
	int		res = conn_->insertData( table, buffers );
	if ( res < 0 ) {
		throw SparrowException( SPW_EXCPTMSG, false, res );
	}
	return res;
}


}	// namespace Sparrow

#endif /* #ifndef _sparrow_api_exceptwrapper_h_ */
