#ifndef _spw_api_impl_connection_h
#define _spw_api_impl_connection_h

#include "include/connection.h"
#include "include/exception.h"
#include "sema.h"
#include "thread.h"
#include "bufferlist.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif



#define SPARROW_SOCKET_RECEIVE_BUFFER_SIZE	(64*1024)
#define	SPARROW_SOCKET_SEND_BUFFER_SIZE		(4*1024*1024)

namespace Sparrow
{


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Response
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Response
{
private:

	// Response buffer. May be compressed initially.
	HeapBuffer	buffer_;

	// Length of decompressed response.
	uint32_t		length_;

	// Compression algorithm.
	uint32_t		compressionAlgorithm_;

public:
	Response(uint32_t compressedLength, uint32_t length, uint32_t compressionAlgorithm) : 
	  buffer_(compressedLength), length_(length), compressionAlgorithm_(compressionAlgorithm)
	 {}

	HeapBuffer& getBuffer() { return buffer_; }

	bool getException(SparrowException& e) {
		if ( length_ == 0 )
			return false;
		Str	msg;
		unsigned int	err_code = SPW_API_FAILED;
		buffer_ >> msg;
		if ( msg.length() == 0 ) 
			return false;
		if ( buffer_.position() != buffer_.limit() ) {
			buffer_ >> err_code;
			if ( err_code == 0 )
				return false;
		}
		e = SparrowException( msg.c_str(), (int32_t)err_code );
		return true;
	}

	void decompress()  _THROW_(SparrowException);
};


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Request
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Connection;
class Request : public RefCounted
{
	friend class Connection;

private:
	uint32_t		id_;
	Sema		sema_;
	Response*	response_;

	SparrowException*	exception_;

protected:
	void check() _THROW_(SparrowException);

public:
	Request(uint32_t id);
	~Request();

	uint32_t id() const { return id_; }

	// A response has been received for this request.
	void responseReceived(Response* response);

	// An I/O error was received for this request.
	void exceptionReceived(const SparrowException&);

	// Waits indefinitely for a response.
	Response* getResponse() _THROW_(SparrowException);

	Str getString() const;
};

typedef RefPtr<Request> RequestGuard;


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Connection Properties
//////////////////////////////////////////////////////////////////////////////////////////////////////

class spw_ConnectionProperties : public ConnectionProperties {
private:
	void completeWithDefaultValues();

public:
	Str		host_;
	Str		user_;
	Str		pssw_;
	Str		src_addr_;
	uint32_t	src_port_;
	uint32_t	port_;
	uint32_t	mysql_port_;

public:
	spw_ConnectionProperties() : src_port_(0), port_(0), mysql_port_(0) {}
	spw_ConnectionProperties(const ConnectionProperties&);
	spw_ConnectionProperties(const Str& host, const Str& user, const Str& pssw, 
		const Str& src_addr, uint32_t srcPort=0, uint32_t mysql_port=DEF_MYSQL_PORT, 
		uint32_t port=DEF_PORT);
	spw_ConnectionProperties(const char* host, const char* user, const char* psswd, 
		uint32_t mysqlPort=DEF_MYSQL_PORT, uint32_t spwPrt=DEF_PORT, 
		const char* srcAddr=DEF_SOURCE_ADDR, uint32_t srcPort=0); 

	const ConnectionProperties& operator = (const ConnectionProperties&);

	const char* getHost() const override { return host_.c_str(); }
	const char* getUser() const override { return user_.c_str(); }
	const char* getPsswd() const override { return pssw_.c_str(); }
	const char* getSrcAddr() const override { return src_addr_.c_str(); }
	uint32_t getSrcPort() const override { return src_port_; }
	uint32_t getPort() const override { return port_; }
	uint32_t getMySQLPort() const override { return mysql_port_; }

	Str getString() const;
};



//////////////////////////////////////////////////////////////////////////////////////////////////////
// Connection
//////////////////////////////////////////////////////////////////////////////////////////////////////

class spw_Table;
class spw_Connection : public Thread, public Connection
{
	friend class spw_Table;

public:
	enum Action {
		AUTH,
		INIT,
		INSERT_DATA,
		GET_MASTER,
		DISABLE_COALESCING,
		REMOVE_PARTITIONS,
		DISABLE_COALESCING_DB,
		INSERT_DATA_EX,
		SWITCH_PURGE_MODE
	};

private:

	static const uint8_t TAG[];

	static const uint8_t SPARROW_API_VERSION;

	// Protect access to the socket, fdSet_ , properties_ and the listening thread
	Lock		lockSckt_;

	// Protect access to the request list, requests_
	Lock		lockRqst_;

	// Socket to communicate with Sparrow
	my_socket			socket_;
	spw_ConnectionProperties	properties_;
	uint32_t		compressionAlgorithm_;

	// Listening thread
	fd_set		fdSet_;

	// Array of currently active requests
	SYSslist<RequestGuard>	requests_;
	RequestGuard getRequest(uint32_t id, bool remove);


	// Request counter (used to generate Request::id_)
	uint32_t		counter_;

	RequestGuard createNewRequest();
	void resetRqsts(const SparrowException& e);

	void sendHeader(SocketWriter& writer, uint32_t rqstId, uint32_t len, uint32_t comprLen, Action action);
	RequestGuard compressAndSendBuffer(Action action, const ByteBuffer& buffer);
	RequestGuard compressAndSendBuffer(Action action, const BufferList& buffer);

	RequestGuard authenticate() _THROW_(SparrowException);

	void disconnectAndResetRqsts(const SparrowException&, bool lock);
	void closeSocket(bool lock);

protected:

	bool process() override;

	void notifyStop() override {
		// I'm the only tread, so no-one to notify
	}

	bool deleteAfterExit() override {
		return false;
	}

	void initialize(const spw_Table&) _THROW_(SparrowException);


public:
	spw_Connection();
	~spw_Connection();

	int setProperties(const ConnectionProperties& properties, uint32_t compressionAlgo=0) override;
	int setProperties(const char* host, const char* user, const char* psswd, 
		uint32_t mysqlPort=DEF_MYSQL_PORT, uint32_t spwPrt=DEF_PORT, const char* srcAddr=DEF_SOURCE_ADDR, 
		uint32_t srcPort=0, uint32_t compressionAlgo=0) override;
	const ConnectionProperties* getProperties() const override { return &properties_;}
	
	int connect() override;
	void disconnect() override;

	bool isClosed() const override;

	Table* createTable() const override;
	Table* getTable(const char* database, const char* table) override;
	void releaseTable(const Table*) const override;

	SparrowBuffer* createBuffer(const Table* table, uint32_t capacity=UINT_MAX32) const override;
	void releaseBuffer(const SparrowBuffer*) const override;

	ColumnNames* createColumnNames(int size=0) override;
	void releaseColumnNames(const ColumnNames*) override;
	int insertData(const Table* table, const SparrowBuffer* buffers) override;
	int insertData(const Table* table, const ColumnNames* columns, const SparrowBuffer* buffers) override;

	int disableCoalescing(uint32_t timeout, bool wait=false) override;
	int disableCoalescing(uint32_t timeout, const char* database, bool wait=false) override;
	int removePartitions(const char* database, const char* table, const uint64_t start, const uint64_t end) override;
	int switchPurgeMode(uint32_t timeout, const char* database, PurgeMode mode) override;

	Master* getMasterFile(const char* database, const char* table) override;
	void releaseMasterFile(const Master*) const override;

	unsigned int getListeningThrdId() const override { return threadId_; }
};

}	// namespace Sparrow



#endif		// #ifndef _spw_api_impl_connection_h
