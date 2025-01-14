#ifndef _spw_api_connection_h
#define _spw_api_connection_h

#include "global.h"
#include "master.h"
#include "table.h"
#include "sparrowbuffer.h"

#include <limits.h>

/*
	Exceptions are not used to remove any potential problems if the client application does not use the same
	compiler as the API.

	Most object methods return a error code: 0 if success, -1 if an error occurred except if specified differently.
	Additional error information can be retrieved using the const char* errmsg() function.

	Methods that return pointers to objects return NULL if the object allocation failed. Client application
	is responsible for calling delete on the pointer when the object is not necessary anymore.

	Tested against current MySQL version 5.5.7.
*/

namespace Sparrow
{

inline bool networkErr(int32_t errCode) {
	return (errCode == SPW_API_SOCKET_CONN_CLOSED || 
		errCode == SPW_API_SOCKET_READ_ERR ||
		errCode == SPW_API_SOCKET_WRITE_ERR );
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// ConnectionProperties
//////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEF_HOSTNAME		"localhost"
#define DEF_PORT			11000
#define DEF_USERNAME		"admin"
#define DEF_PASSWORD		"admin"
//#define DEF_SOURCE_ADDR		"0.0.0.0"
#define DEF_SOURCE_ADDR		""
#define DEF_SOURCE_PORT		0
#define DEF_MYSQL_PORT		3306

/* Declare a class that inherits from the ConnectionProperties interface. Then an object of that
	class can be used to give connection settings to the Connection object through the 
	setProperties() method.
*/

class ConnectionProperties {

protected:
	virtual ~ConnectionProperties(){}

public:
	ConnectionProperties() {}

	// Hostname of the MySQL/Sparrow server. Can be either the name of the host or its IP address. 
	//	Null or empty string means localhost
	virtual const char* getHost() const = 0;

	// User cannot be empty. Password can.
	virtual const char* getUser() const = 0;
	virtual const char* getPsswd() const = 0;

	// Source address in IP format 
	virtual const char* getSrcAddr() const = 0;

	virtual uint32_t getSrcPort() const = 0;

	// Port used when connecting to Sparrow.
	virtual uint32_t getPort() const = 0;

	// Port used when connecting to MySQL.
	virtual uint32_t getMySQLPort() const = 0;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Connection
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Connection
{
public:
	enum PurgeMode {
		PURGE_MODE_ON_INSERTION,
		PURGE_MODE_CONSTANTLY
	};

public:
	virtual ~Connection() {}

	// Set connection properties. Must be called before connect(). Can be called again with different 
	//	values to connect to another Sparrow engine.
	virtual int setProperties(const ConnectionProperties& properties, uint32_t compressionAlgo=0) = 0;
	virtual int setProperties(const char* host, const char* user, const char* psswd, 
		uint32_t mysqlPort=DEF_MYSQL_PORT, uint32_t spwPrt=DEF_PORT, const char* srcAddr=DEF_SOURCE_ADDR, 
		uint32_t srcPort=0, uint32_t compressionAlgo=0) = 0;
	virtual const ConnectionProperties* getProperties() const = 0;

	virtual int connect() = 0;
	virtual void disconnect() = 0;

	// Returns true if the connection to the Sparrow engine is established
	virtual bool isClosed() const = 0;

	// Creates an empty Table object
	virtual Table* createTable() const = 0;
	virtual Table* getTable(const char* database, const char* table) = 0;
	virtual void releaseTable(const Table*) const = 0;

	// Creates a SparrowBuffer object for the given Table. 
	virtual SparrowBuffer* createBuffer(const Table* table, uint32_t capacity=UINT_MAX) const = 0;
	virtual void releaseBuffer(const SparrowBuffer*) const = 0;

	virtual ColumnNames* createColumnNames(int size=0) = 0;
	virtual void releaseColumnNames(const ColumnNames*) = 0;
	virtual int insertData(const Table* table, const SparrowBuffer* buffers) = 0;
	virtual int insertData(const Table* table, const ColumnNames* columns, const SparrowBuffer* buffers) = 0;
	
	// Disables the coalescing globally (= for all databases). After timeout seconds the coalescing switches back on automatically
	virtual int disableCoalescing(uint32_t timeout, bool wait=false) = 0;
	
	// Disables the coalescing for a specific database
	virtual int disableCoalescing(uint32_t timeout, const char* database, bool wait=false) = 0;
	virtual int removePartitions(const char* database, const char* table, const uint64_t start, const uint64_t end) = 0;
	virtual int switchPurgeMode(uint32_t timeout, const char* database, PurgeMode mode) = 0;

	// Returns NULL if failed.
	virtual Master* getMasterFile(const char* database, const char* table) = 0;
	virtual void releaseMasterFile(const Master*) const = 0;

	virtual unsigned int getListeningThrdId() const = 0;
};


extern "C"
{
	// Must be the first API called before any other. It initializes internal static variables
	//	MySQL C library and some network resources. It is not thread-safe so it should be called
	//	before any client thread is created
	SPW_API_PUBLIC_FUNC int initialize();

	// Creates the API root object. Client application is responsible for calling delete on the 
	//	resulting pointer when the object is no longer necessary.
	SPW_API_PUBLIC_FUNC Connection* createConnect();

	// Frees memory used by a Connection object. Client application should not call delete on the 
	//	Connection*, but this method instead.
	SPW_API_PUBLIC_FUNC  void releaseConnect(const Connection*);

	// Returns the last error message. 
	SPW_API_PUBLIC_FUNC const char* errmsg();
}

}	// namespace Sparrow



#endif		// #ifndef _spw_api_connection_h
