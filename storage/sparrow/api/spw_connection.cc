#include "memalloc.h"
#include "spw_connection.h"
#include "spw_table.h"
#include "spw_master.h"
#include "spw_sparrowbuffer.h"
#include "socketutil.h"
#include "compress.h"

#include "my_io.h"

// When building in standalone, use the files from sub-dir 'mysql'
// else, use the headers from MySQL source project
//#include "mysql/my_aes.h"
#include <my_aes.h>

//#include <my_net.h>


namespace Sparrow
{

// FROM PUBLIC INTERFACE
int initialize()
{
	try {
		Lock::initializeStatics();
		Cond::initializeStatics();

		spw_Table::initialize();

		// Some global socket layer initialization
		SocketUtil::initialize();
	} catch ( const SparrowException& e ) {
		spwerror = e;
		return e.getErrcode();
	}

	return 0;
}

// FROM PUBLIC INTERFACE
Connection* createConnect()
{
	return new spw_Connection();
}

// FROM PUBLIC INTERFACE
void releaseConnect( const Connection* connect )
{
	if ( connect != NULL ) {
		delete connect;
	}
}

// FROM PUBLIC INTERFACE
const char* errmsg() {
	return spwerror.getText();
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Connection Implementation
//////////////////////////////////////////////////////////////////////////////////////////////////////

const uint8_t spw_Connection::TAG[] = { 83, 80, 65, 82, 82, 79, 87 };	// "SPARROW"

const uint8_t spw_Connection::SPARROW_API_VERSION = 1;

spw_Connection::spw_Connection() : Thread("Listener"), 
	lockSckt_(false, "socket lock"), lockRqst_(false, "requests lock"), compressionAlgorithm_(0)
{
	socket_ = INVALID_SOCKET;
	counter_ = 0;
}

spw_Connection::~spw_Connection(void)
{
	disconnectAndResetRqsts( SparrowException( "shutdown" ), true );
}

// [PUBLIC] 
int spw_Connection::setProperties( const ConnectionProperties& properties, uint32_t compressionAlgo /* =0 */ )
{
	Guard			lockGuard( lockSckt_ );
	try {
		if ( compressionAlgo != 0 && compressionAlgo != 1 ) {
			throw SparrowException( "Invalid compression algorithm" );
		}
		compressionAlgorithm_ = compressionAlgo;
		properties_ = properties;
	} catch ( const SparrowException& e ) {
		spwerror = e;
		return e.getErrcode();
	}

	return 0;
}

// [PUBLIC] 
int spw_Connection::setProperties( const char* host, const char* user, const char* psswd, 
	uint32_t mysqlPort /*=DEF_MYSQL_PORT*/, uint32_t spwPrt /*=DEF_PORT*/, 
	const char* srcAddr /*=DEF_SOURCE_ADDR*/, uint32_t srcPort /*=0*/, uint32_t compressionAlgo /*=0*/ )
{
	Guard			lockGuard( lockSckt_ );
	try {
		if ( compressionAlgo != 0 && compressionAlgo != 1 ) {
			throw SparrowException( "Invalid compression algorithm" );
		}
		compressionAlgorithm_ = compressionAlgo;
		properties_ = spw_ConnectionProperties( host, user, psswd, mysqlPort, spwPrt, srcAddr, srcPort );
	} catch ( const SparrowException& e ) {
		spwerror = e;
		return e.getErrcode();
	}

	return 0;
}

bool spw_Connection::isClosed() const
{
	return socket_ == INVALID_SOCKET;
}


// [PUBLIC] Create the socket, connect to Sparrow, starts the listening thread, send an authentication message.
int spw_Connection::connect()
{
	{
		Guard			lockGuard( lockSckt_ );
		try
		{
			if ( socket_ != INVALID_SOCKET ) {
				disconnectAndResetRqsts( SparrowException( "reconnecting" ), false );
			}

			// Get address from string
			SocketAddress	addr	= SocketUtil::getAddress( properties_.host_.c_str(), properties_.port_ );
			SocketAddress	srcAddr;
			if ( properties_.src_addr_.length() == 0 ) {
				srcAddr = SocketUtil::getAddress( NULL, properties_.src_port_ );
			} else if ( properties_.src_addr_.length() != 0 ) {
				srcAddr	= SocketUtil::getAddress( properties_.src_addr_.c_str(), properties_.src_port_ );
				if ( srcAddr.isV6() != addr.isV6() ) {
					throw SparrowException::create( false, SPW_API_FAILED, "cannot connect, target addr '%s' and source addr '%s' are of different types.",
						properties_.host_.c_str(), properties_.src_addr_.c_str() );
				}
			}

			// Connect to Sparrow server
			socket_ = SocketUtil::createAndConnect( SOCK_STREAM, addr, srcAddr );

			// Initialize the fd_set structure
			FD_ZERO(&fdSet_);
			FD_SET(socket_, &fdSet_);
			my_socket stopSocket = SocketUtil::getStopSocket();
			if (stopSocket != INVALID_SOCKET) {
				FD_SET(stopSocket, &fdSet_);
			}

			// Start listening thread
			start();
		} catch(const SparrowException& e) {
			disconnectAndResetRqsts( e, false );

			spwerror = e;
			return e.getErrcode();
		}
	}

	try
	{
		// Authenticate
		RequestGuard	request = authenticate();
		request->getResponse();

	} catch(const SparrowException& e) {
		spwerror = e;
		return e.getErrcode();
	}

	return 0;
}

// [PUBLIC] Complete cleanup. Stop listening thread, close socket and abort all pending requests (i.e. API calls)
void spw_Connection::disconnect()
{
	disconnectAndResetRqsts( SparrowException( "user disconnection" ), true );
}


// Complete cleanup. Stop listening thread, close socket and abort all pending requests (i.e. API calls)
void spw_Connection::disconnectAndResetRqsts( const SparrowException& e, bool lock )
{
	closeSocket( lock );
	resetRqsts( e );
}


// Closes connection socket to Sparrow and ends listenning thread if it's still running
void spw_Connection::closeSocket( bool lock )
{
	if ( lock ) lockSckt_.lock();

	//if ( endThread && isRunning() ) {
	if ( isRunning() ) {
		PRINT_DBUG("[spw_Connection::disconnect] Stopping thread...");
		stop();
	}

	if ( socket_ != INVALID_SOCKET ) {
		PRINT_DBUG("[spw_Connection::disconnect] Closing socket...");
		::shutdown(socket_, SHUT_RDWR);
		FD_CLR(socket_, &fdSet_);
		closesocket( socket_ );
		socket_ = INVALID_SOCKET;
	}

	if ( lock ) lockSckt_.unlock();
}


// Creates a new request object
RequestGuard spw_Connection::createNewRequest() 
{
	uint32_t			id = Atomic::inc32( &counter_ );
	RequestGuard	request( new Request( id ) );

	{
		// Add this new request to our list of pending requests
		Guard	lockGuard( lockRqst_ );
		requests_.append( request );
	}

	return request;
}

void spw_Connection::resetRqsts( const SparrowException& e )
{
	Guard	lockGuard( lockRqst_ );

	PRINT_DBUG("[spw_Connection::disconnect] Reseting %u requests: %s...", requests_.entries(), e.getText() );
	SYSslistIterator<RequestGuard>	iterator(requests_);
	while (++iterator) {
		iterator.key()->exceptionReceived( e );
	}
	requests_.clear();
	PRINT_DBUG("[spw_Connection::disconnect] Disconnected.");
}


void spw_Connection::sendHeader( SocketWriter& writer, uint32_t rqstId, uint32_t len, uint32_t comprLen, Action action )
{
	for (uint32_t i = 0; i < sizeof(TAG); ++i) {
		writer << TAG[i];
	}
	writer << SPARROW_API_VERSION;

	writer << rqstId << len << compressionAlgorithm_ << comprLen << action;
}

// Sends a single ByteBuffer
RequestGuard spw_Connection::compressAndSendBuffer(Action action, const ByteBuffer& buffer)
{
	RequestGuard	request( createNewRequest() );

	Guard			lockGuard( lockSckt_ );
	if ( socket_ == INVALID_SOCKET ) 
		throw SparrowException( "Not connected", true, SPW_API_SOCKET_CONN_CLOSED );

	try
	{
		SocketWriter	writer( socket_ );
		// Compress (eventually) the request buffer and send it
		uint32_t			length = buffer.position();
		if ( length == 0 ) 
		{
			sendHeader( writer, request->id(), 0, 0, action );
		} 
		else if ( compressionAlgorithm_ == 0 ) 
		{
			sendHeader( writer, request->id(), length, length, action );
			writer.send( ByteBuffer( buffer.getData(), length ) );
		} 
		else 
		{
			HeapBuffer	compressedBuffer(length);
			const uint32_t compressedLength = static_cast<uint32_t>(LZJB::compress(buffer.getData(), compressedBuffer.getData(), length, length));
			{
				sendHeader( writer, request->id(), length, compressedLength, action );
				if ( compressedLength < length ) {
					writer.send( ByteBuffer( compressedBuffer.getData(), compressedLength ) );
				} else {
					writer.send( ByteBuffer( buffer.getData(), length ) );
				}
			}
		}

		// writer.flush is automatically done in its destructor

	} catch ( const SparrowException& e ) {
		// Remove request from queue.
		getRequest( request->id(), true );

		// If socket error, closeSocket, which implies reseting all pending requests
		if ( networkErr( e.getErrcode() ) ) {
			disconnectAndResetRqsts( e, false );
		}
		throw;
	}

	return request;
}

// Sends a single ByteBuffer list
RequestGuard spw_Connection::compressAndSendBuffer(Action action, const BufferList& buffer)
{
	const SYSvector<RefByteBuffer>&		buffers( buffer.getBuffers() );
	if ( buffers.length() == 0 ) {
		return compressAndSendBuffer( action, ByteBuffer() );
	}
	if ( buffers.length() == 1 ) {
		return compressAndSendBuffer( action, *buffers[0] );
	}

	RequestGuard	request( createNewRequest() );

	Guard			lockGuard( lockSckt_ );
	if ( socket_ == INVALID_SOCKET ) 
		throw SparrowException( "Not connected", true, SPW_API_SOCKET_CONN_CLOSED );

	try
	{
		SocketWriter	writer( socket_ );

		uint32_t			length = buffer.getPosition();		// Total size of data to send in bytes

		// If no compression required, then send buffers as they are
		if ( compressionAlgorithm_ == 0 )
		{
			sendHeader( writer, request->id(), length, length, action );
			for ( uint32_t i=0; i<buffers.length(); ++i ) {
				const ByteBuffer&	b( *buffers[i] );
				writer.send( ByteBuffer( b.getData(), b.position() ) );
			}
		}
		else 
		{
			// If the BufferList contains several buffers, copy all buffers into a single big buffer 
			//	to simplify and improve performance of LZJB::compress().
			ByteBufferGuard		uncompressedBuffer(length);
			for ( uint32_t i=0; i<buffers.length(); ++i ) {
				const ByteBuffer&	b( *buffers[i] );
				uncompressedBuffer.get() << ByteBuffer( b.getData(), b.position() );
			}
			SPW_ASSERT( uncompressedBuffer.get().limit() == length );

			// Buffer to contain compressed data
			ByteBufferGuard		compressedBuffer(length);

			// Compress data
			uint32_t	compressedLength = static_cast<uint32_t>(LZJB::compress(uncompressedBuffer.get().getData(), 
				compressedBuffer.get().getData(), length, length));
			{
				// Write compression header
				sendHeader( writer, request->id(), length, compressedLength, action );

				// Send compressed data if compression decreased buffer size, otherwise send uncompressed data
				if ( compressedLength < length ) {
					writer.send( ByteBuffer( compressedBuffer.get().getData(), compressedLength ) );
				} else {
					writer.send( uncompressedBuffer.get() );
				}
			}
		}
	} catch ( const SparrowException& e ) {
		// Remove request from queue.
		getRequest( request->id(), true );

		// If socket error, closeSocket, which implies reseting all pending requests
		if ( networkErr( e.getErrcode() ) ) {
			disconnectAndResetRqsts( e, false );
		}
		throw;
	}

	// writer.flush is automatically done in its destructor

	return request;
}

RequestGuard spw_Connection::authenticate() _THROW_(SparrowException)
{
	uint8_t		data[1024];
	ByteBuffer	buffer( data, sizeof(data) );
	
	{
		Guard			lockGuard( lockSckt_ );
		buffer << properties_.user_;

		const uint32_t length = properties_.pssw_.length();
		static const char* secretKey = "49#28!86@14\"&";
		char tmp[256];
		const uint32_t aesLength = static_cast<uint32_t>(my_aes_get_size(length, my_aes_128_ecb));
		const bool onStack = aesLength <= sizeof(tmp);
		AutoPtr<char, true> passwordGuard(onStack ? tmp : new char[aesLength]);
		char* encryptedPassword = onStack ? passwordGuard.release() : passwordGuard.get();
		const int result = my_aes_encrypt( reinterpret_cast<const unsigned char*>(properties_.pssw_.c_str()), static_cast<int>(length), 
				reinterpret_cast<unsigned char*>(encryptedPassword), 
				reinterpret_cast<const unsigned char*>(secretKey), static_cast<int>(strlen(secretKey)), 
				my_aes_128_ecb, NULL );
		if (result <= 0) {
			throw SparrowException::create(false, SPW_API_FAILED, "Cannot encrypt password: error code %d", result);
		}
		encryptedPassword[result] = '\0';
		ByteBuffer	buff( (uint8_t*)encryptedPassword, aesLength );
		buffer << buff.limit() << buff;
	}

	return compressAndSendBuffer( AUTH, buffer );
}

// Reads incoming data on socket
bool spw_Connection::process()
{
	if ( socket_ == INVALID_SOCKET )
		return false;

	fd_set fdSet = fdSet_;

	my_socket stopSocket = SocketUtil::getStopSocket();
	my_socket maxSocket = stopSocket == INVALID_SOCKET ? socket_ : std::max(socket_, stopSocket);

	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	// Wait for something to arrive on the socket_
	//PRINT_DBUG("[spw_Connection::process] select ...");
	int rc = select(static_cast<int>(maxSocket + 1), &fdSet, 0, 0, &tv);
	//PRINT_DBUG("[spw_Connection::process] select %u", rc);
	if ( rc == 0) {
		//PRINT_DBUG("[spw_Connection::process] timeout");
		return true;
	}
	else if ( rc < 0 ) {
		// Error - closed socket or something
		PRINT_DBUG("[spw_Connection::process] SOCKET ERROR! %d", rc);
		return false;
	} else if (FD_ISSET(socket_, &fdSet)) {
		//PRINT_DBUG("[spw_Connection::process] PACKET REC !");
		// Read Header
		Request*	request = NULL;
		uint32_t		length, code, compressedLength;

		try
		{
			{
				uint8_t			header[24];
				ByteBuffer		buffer(header, sizeof(header));
				SocketReader	reader(socket_, buffer);

				// Check tag.
				for (uint32_t i = 0; i < sizeof(TAG); ++i) {
					uint8_t check;
					reader >> check;
					if (check != TAG[i]) {
						throw SparrowException::create(false, SPW_API_FAILED, "Malformed API response");
					}
				}
				uint8_t version;
				reader >> version;
				if (version != SPARROW_API_VERSION) {
					throw SparrowException::create(false, SPW_API_FAILED, "Unsupported API version: %u", static_cast<uint32_t>(version));
				}

				// Read request ID and find corresponding request object in our list
				uint32_t			id;
				reader >> id;
				request = getRequest( id, true );
				if ( request == NULL ) {
					throw SparrowException::create(false, SPW_API_FAILED, "Received response for unknown request %u", id );
				}

				reader >> length >> code >> compressedLength;
				if (length > 100 * 1024 * 1024 || compressedLength > 100 * 1024 * 1024) {
					const Str size(Str::fromSize(length));
					throw SparrowException::create(false, SPW_API_FAILED, "Received too much data (%s)", size.c_str());
				}
			}

			// Read compressed request data.
			try
			{
				Response*			response = new Response( compressedLength, length, code );
				SocketReader		reader( socket_, response->getBuffer() );	// Reads whatever data is currently available in socket input buffer
				reader.advance( compressedLength );							// Reads data from socket up to compressedLength bytes, blocking if necessary
				request->responseReceived( response );
			} catch ( const SparrowException& e ) {
				request->exceptionReceived( e );
				throw;
			}
		}
		catch ( const SparrowException& e )
		{
			stopping();
			PRINT_ERR("An exception occurred (%s). Disconnecting.", e.getText());
			disconnectAndResetRqsts( e, true );
			return false;
		}
	} else {
		// "stop" socket notification, so stop.
		stopping();
		PRINT_DBUG("[spw_Connection::process] STOP NOTIF!");
		return false;
	}

	//PRINT_DBUG("[spw_Connection::process] return TRUE");
	return true;
}


// [PUBLIC] 
void spw_Connection::initialize( const spw_Table& table ) _THROW_(SparrowException)
{
	// Downcast to spw_Table type
	//const spw_Table& table = *(static_cast<const spw_Table*>(&tbl));

	HeapBuffer	buffer;

	buffer << table.getDbNameStr() << table.getTableNameStr();

	// Write columns
	const Columns&	columns = table.getColumns();
	buffer << columns.length();
	for ( uint32_t i=0; i<columns.length(); ++i ) {
		buffer << columns[i];
	}

	// Write indexes
	const Indexes&	indexes = table.getIndexes();
	buffer << indexes.length();
	for ( uint32_t i=0; i<indexes.length(); ++i ) {
		buffer << indexes[i];
	}


	// Write indexes
	const ForeignKeys&	foreignKeys = table.getForeignKeys();
	buffer << foreignKeys.length();
	for ( uint32_t i=0; i<foreignKeys.length(); ++i ) {
		char	fkName[256];
		snprintf( fkName, sizeof(fkName), "FK_%s_%s_%d", table.getDatabaseName(),
			table.getTableName(), i );
		const spw_ForeignKey&	fk = foreignKeys[i];
		spw_ForeignKey			fk2( fkName, fk.getColumnId(), fk.getDatabaseName(), 
									fk.getTableName(), fk.getColumnName() );
		buffer << fk2;
	}

	// Write DNS configuration
	buffer << table.getDns();

	// Write other parameters
	buffer << table.getAggregPeriod() << table.getDefaultWhere() << table.getStringOptimization()
		<< table.getMaxLifetime() << table.getCoalescPeriod();

	RequestGuard	request = compressAndSendBuffer( INIT, buffer );
	request->getResponse();
}

// [PUBLIC] 
Table* spw_Connection::createTable() const {
	return new spw_Table();
}

// [PUBLIC] 
void spw_Connection::releaseTable( const Table* table ) const
{
	if ( table != NULL ) {
		delete table;
	}
}

// [PUBLIC] 
ColumnNames* spw_Connection::createColumnNames( int size ) {
	return new spw_ColumnNames( size );
}

// [PUBLIC] 
void spw_Connection::releaseColumnNames( const ColumnNames* names ) {
	if ( names != NULL ) {
		delete names;
	}
}

// [PUBLIC] 
SparrowBuffer* spw_Connection::createBuffer( const Table* table, uint32_t capacity /*=UINT_MAX32*/) const {
	if ( !table || capacity == 0 ) {
		spwerror = SparrowException::create( false, SPW_API_FAILED, "Invalid arg to createBuffer(%p, %u).", table, capacity );
		return NULL;
	}
	return new spw_SparrowBuffer( table, capacity );
}

void spw_Connection::releaseBuffer( const SparrowBuffer* buffer ) const
{
	if ( buffer != NULL ) {
		delete buffer;
	}
}

// [PUBLIC] 
int spw_Connection::disableCoalescing( uint32_t timeout, bool wait /*=false*/ )
{
	if ( isClosed() ) {
		spwerror = SparrowException( "Not connected.", true, SPW_API_SOCKET_CONN_CLOSED );
		return SPW_API_SOCKET_CONN_CLOSED;
	}

	uint8_t		data[1024];
	ByteBuffer	buffer( data, sizeof(data) );

	buffer << timeout << wait;

	try {

		RequestGuard	request = compressAndSendBuffer( DISABLE_COALESCING, buffer );
		request->getResponse();

	} catch ( const SparrowException& e ) {
		PRINT_ERR( "%s", e.getText() );
		spwerror = e;
		return e.getErrcode();
	}

	return 0;
}

// [PUBLIC] 
int spw_Connection::disableCoalescing( uint32_t timeout, const char* database, bool wait /*=false*/ )
{
	if ( isClosed() ) {
		spwerror = SparrowException( "Not connected.", true, SPW_API_SOCKET_CONN_CLOSED );
		return SPW_API_SOCKET_CONN_CLOSED;
	}

	uint8_t		data[1024];
	ByteBuffer	buffer( data, sizeof(data) );

	buffer << timeout << Str(database) << wait;

	try {

		RequestGuard	request = compressAndSendBuffer( DISABLE_COALESCING_DB, buffer );
		request->getResponse();

	} catch ( const SparrowException& e ) {
		PRINT_ERR( "%s", e.getText() );
		spwerror = e;
		return e.getErrcode();
	}

	return 0;
}

// [PUBLIC] 
int spw_Connection::removePartitions( const char* database, const char* table, const uint64_t start, const uint64_t end )
{
	if ( isClosed() ) {
		spwerror = SparrowException( "Not connected.", true, SPW_API_SOCKET_CONN_CLOSED );
		return SPW_API_SOCKET_CONN_CLOSED;
	}

	uint8_t		data[1024];
	ByteBuffer	buffer( data, sizeof(data) );

	buffer << static_cast<uint32_t>(strlen(database)) << database
			<< static_cast<uint32_t>(strlen(table)) << table
			<< start << end;

	try {

		RequestGuard	request = compressAndSendBuffer( REMOVE_PARTITIONS, buffer );
		request->getResponse();

	} catch ( const SparrowException& e ) {
		PRINT_ERR( "%s", e.getText() );
		spwerror = e;
		return e.getErrcode();
	}

	return 0;
}

// [PUBLIC] 
int spw_Connection::switchPurgeMode( uint32_t timeout, const char* database, PurgeMode mode )
{
	if ( isClosed() ) {
		spwerror = SparrowException( "Not connected.", true, SPW_API_SOCKET_CONN_CLOSED );
		return SPW_API_SOCKET_CONN_CLOSED;
	}

	uint8_t		data[1024];
	ByteBuffer	buffer( data, sizeof(data) );

	buffer << timeout << Str(database) << static_cast<int>(mode);

	try {

		RequestGuard	request = compressAndSendBuffer( SWITCH_PURGE_MODE, buffer );
		request->getResponse();

	} catch ( const SparrowException& e ) {
		PRINT_ERR( "%s", e.getText() );
		spwerror = e;
		return e.getErrcode();
	}

	return 0;
}

// [PUBLIC] 
Master* spw_Connection::getMasterFile( const char* database, const char* table )
{
	if ( isClosed() ) {
		spwerror = SparrowException( "Not connected.", true, SPW_API_SOCKET_CONN_CLOSED );
		return NULL;
	}

	if ( !database || !table ) {
		spwerror = SparrowException( "Invalid arg to getMasterFile." );
		return NULL;
	}

	uint8_t		data[1024];
	ByteBuffer	buffer( data, sizeof(data) );
	buffer << static_cast<uint32_t>(strlen(database)) << database
			<< static_cast<uint32_t>(strlen(table)) << table;

	spw_Master*		masterFile = NULL;
	try {

		RequestGuard	request = compressAndSendBuffer( GET_MASTER, buffer );
		Response*	resp = request->getResponse();

		// Decode the response
		masterFile = new spw_Master();
		resp->getBuffer() >> *masterFile;

	} catch ( const SparrowException& e ) {
		PRINT_ERR( "%s", e.getText() );
		spwerror = e;
	}

	return masterFile;
}

// [PUBLIC] 
Table* spw_Connection::getTable( const char* database, const char* tablename )
{
	RefPtr<spw_Master> master(static_cast<spw_Master*>(getMasterFile(database, tablename)));
	if ( master == NULL ) return NULL;

	spw_Table* table = new spw_Table();
	if ( !table ) return NULL;

	table->setDatabaseName( database );
	table->setTableName( tablename );
	table->setMaxLifetime( master->getMaxLifetime() );
	table->setCoalescPeriod( master->getCoalescingPeriod() );
	table->setAggregPeriod( master->getAggregPeriod() );

	// Table columns are non-dropped columns from the master file.
	Columns columns;
	for (uint32_t i = 0; i < master->getNbColumns(); ++i) {
		const Column& column = master->getColumn(i);
		if (!column.isDropped()) {
			columns.append(static_cast<const spw_Column&>(column));
		}
	}
	table->setColumns(columns);
	table->setIndexes( master->getIndexes() );
	table->setForeignKeys( master->getForeignKeys() );
	table->setDns( master->getDnsConfiguration() );

	return table;
}

// FROM PUBLIC INTERFACE
void spw_Connection::releaseMasterFile( const Master* master ) const
{
	if ( master != NULL ) {
		delete master;
	}
}


// [PUBLIC] 
int spw_Connection::insertData( const Table* tbl, const SparrowBuffer* dt )
{
	if ( isClosed() ) {
		spwerror = SparrowException( "Not connected.", true, SPW_API_SOCKET_CONN_CLOSED );
		return SPW_API_SOCKET_CONN_CLOSED;
	}

	if ( !tbl || !dt ) {
		spwerror = SparrowException( "Invalid arg to insertData.", true, SPW_API_INVALID_ARG );
		return SPW_API_INVALID_ARG;
	}

	const spw_Table&			table = *(static_cast<const spw_Table*>(tbl));
	const spw_SparrowBuffer&	data = *(static_cast<const spw_SparrowBuffer*>(dt));

	BufferList		buffer( UINT_MAX32, 1024 );

	RefByteBuffer	header( new HeapBuffer() );
	*header << table.getDbNameStr() << table.getTableNameStr();
	*header << data.getRows() << data.getSize();

	buffer.append( header );
	buffer.append( data.getBuffers() );

	try {

		RequestGuard	request = compressAndSendBuffer( INSERT_DATA, buffer );
		request->getResponse();

	} catch ( const SparrowException& e ) {
		PRINT_ERR( "%s", e.getText() );
		spwerror = e;
		return e.getErrcode();
	}

	return 0;
}


// [PUBLIC] 
int spw_Connection::insertData( const Table* tbl, const ColumnNames* cols, const SparrowBuffer* dt )
{
	if ( isClosed() ) {
		spwerror = SparrowException( "Not connected.", true, SPW_API_SOCKET_CONN_CLOSED );
		return SPW_API_SOCKET_CONN_CLOSED;
	}

	if ( !tbl || !dt || !cols ) {
		spwerror = SparrowException( "Invalid arg to insertData.", true, SPW_API_INVALID_ARG );
		return SPW_API_INVALID_ARG;
	}

	const spw_ColumnNames&		columns = *(static_cast<const spw_ColumnNames*>(cols));
	const spw_Table&			table = *(static_cast<const spw_Table*>(tbl));
	const spw_SparrowBuffer&	data = *(static_cast<const spw_SparrowBuffer*>(dt));

	BufferList		buffer( UINT_MAX32, 1024 );

	RefByteBuffer	header( new HeapBuffer(20*1024) );
	*header << table.getDbNameStr() << table.getTableNameStr();
	const SYSvector<Str>&	colNames = columns.getNames();
	*header << colNames.length();
	for (uint32_t i=0; i<colNames.length(); ++i) {
		*header << colNames[i];
	}
	*header << data.getRows() << data.getSize();
	buffer.append( header );
	buffer.append( data.getBuffers() );

	try {

		RequestGuard	request = compressAndSendBuffer( INSERT_DATA_EX, buffer );
		request->getResponse();

	} catch ( const SparrowException& e ) {
		PRINT_ERR( "%s", e.getText() );
		spwerror = e;
		return e.getErrcode();
	}

	return 0;
}


RequestGuard spw_Connection::getRequest( uint32_t id, bool remove )
{
	RequestGuard	request;

	Guard	lockGuard( lockRqst_ );
	SYSslistIterator<RequestGuard>	iterator(requests_);
	while (++iterator) {
		if ( iterator.key()->id() == id ) {
			request = iterator.key();
			if ( remove )
				iterator.remove();
			break;
		}
	}

	return request;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////
// spw_ConnectionProperties
//////////////////////////////////////////////////////////////////////////////////////////////////////

spw_ConnectionProperties::spw_ConnectionProperties(const ConnectionProperties& prop)
	: host_(prop.getHost()), user_(prop.getUser()), pssw_(prop.getPsswd()), 
	src_addr_(prop.getSrcAddr()), src_port_(prop.getSrcPort()), port_(prop.getPort()), 
	mysql_port_(prop.getMySQLPort())

{
	completeWithDefaultValues();
}

spw_ConnectionProperties::spw_ConnectionProperties( const Str& host, const Str& user, const Str& pssw
										   , const Str& src_addr, uint32_t src_port /*=0*/
										   , uint32_t mysql_port /* = DEF_MYSQL_PORT */
										   , uint32_t port /* = DEF_PORT */ )
	: host_(host), user_(user), pssw_(pssw), src_addr_(src_addr), src_port_(src_port), port_(port), mysql_port_(mysql_port)
{
	completeWithDefaultValues();
}

spw_ConnectionProperties::spw_ConnectionProperties( const char* host, const char* user, const char* pssw, 
						 uint32_t mysql_port /*=DEF_MYSQL_PORT*/, uint32_t spw_port /*=DEF_PORT*/, 
						 const char* src_addr /*=DEF_SOURCE_ADDR*/, uint32_t src_port /*=0*/ )
	: host_(host), user_(user), pssw_(pssw), src_addr_(src_addr), src_port_(src_port), port_(spw_port), mysql_port_(mysql_port)
{
	completeWithDefaultValues();
}

const ConnectionProperties& spw_ConnectionProperties::operator = ( const ConnectionProperties& prop ){
	if ( &prop == this )
		return *this;

	host_		= prop.getHost();
	user_		= prop.getUser();
	pssw_		= prop.getPsswd();
	src_addr_	= prop.getSrcAddr();
	src_port_	= prop.getSrcPort();
	port_		= prop.getPort();
	mysql_port_	= prop.getMySQLPort();

	completeWithDefaultValues();

	return *this;
}

void spw_ConnectionProperties::completeWithDefaultValues() {

	if ( host_.length() == 0 ) {
		host_ = DEF_HOSTNAME;
	}
	if ( user_.length() == 0 ) {
		user_ = DEF_USERNAME;
	}
	if ( src_addr_.length() == 0 ) {
		src_addr_ = DEF_SOURCE_ADDR;
	}
	if ( port_ == 0 )
		port_ = DEF_PORT;
	if ( mysql_port_ == 0 )
		mysql_port_ = DEF_MYSQL_PORT;
}

Str spw_ConnectionProperties::getString() const
{
	char	desc[1024];
	snprintf( desc, sizeof(desc), "%s (%u), user '%s', src addr. %s, spw port %u", 
		host_.c_str(), mysql_port_, user_.c_str(), src_addr_.c_str(), port_);
	return Str( desc );
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Response
//////////////////////////////////////////////////////////////////////////////////////////////////////

void Response::decompress() _THROW_(SparrowException)
{
	// Decompress response if required.
	if (buffer_.limit() < length_) {
		if (compressionAlgorithm_ != 1) {
			throw SparrowException::create(false, SPW_API_FAILED, "Only lzjb compression is supported");
		}
		HeapBuffer buffer(length_);
		const int result = LZJB::decompress(buffer_.getData(), buffer.getData(), buffer_.limit(), length_);
		if (result != 0) {
			throw SparrowException::create(false, SPW_API_FAILED, "Cannot decompress data");
		}
		buffer_ = buffer;
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// Request
//////////////////////////////////////////////////////////////////////////////////////////////////////

inline Request::Request( uint32_t id ) : id_(id), sema_(""), response_(NULL), exception_(NULL)
{
}

inline Request::~Request()
{
	if ( exception_ != NULL ) {
		delete exception_;
	}
	if ( response_ != NULL ) {
		delete response_;
	}
}


inline void Request::responseReceived( Response* response )
{
	response_ = response;
	sema_.post();
}

inline void Request::exceptionReceived( const SparrowException& e )
{
	if ( exception_ == NULL ) {
		exception_ = new SparrowException( e );
		//PRINT_DBUG("[Request::exceptionReceived] EXCEPT %s", exception_->getText());
	}
	sema_.post();
}

inline void Request::check() _THROW_(SparrowException)
{
	SparrowException	e;
	if ( response_->getException( e ) ) {
		//PRINT_DBUG("[Request::check] exception in packet: %d, %s", e.getErrcode(), e.getText());
		throw e;
	}
}

Response* Request::getResponse() _THROW_(SparrowException)
{
	//PRINT_DBUG("[Request::getResponse] waiting...");
	sema_.wait();
	if ( exception_ == NULL ) {
		//PRINT_DBUG("[Request::getResponse] notif response!");
		SPW_dbgASSERT( response_ );
		response_->decompress();
		check();
	} else {
		//PRINT_DBUG("[Request::getResponse] notif EXCEPT!");
		throw SparrowException( *exception_ );
	}
	return response_;
}

Str Request::getString() const
{
	char	desc[1024];
	sprintf( desc, "%u", id_ );
	if ( response_ != NULL ) {
		strcat( desc, ", response" );
	}
	if ( exception_ != NULL ) {
		strcat( desc, ", except " );
		strcat( desc, exception_->getText() );
	}
	return Str( desc );
}

}		// namespace Saprrow
