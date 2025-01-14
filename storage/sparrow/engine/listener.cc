/*
	Listener thread.
*/

#include "listener.h"
#include "serial.h"
#include "types.h"
#include "internalapi.h"
#include "socketutil.h"
#include "compress.h"
#include "binbuffer.h"
#include "coalescing.h"
#include "purge.h"
#include "../dns/dnsdefault.h"
#include "../handler/plugin.h"		// For configuration parameters.

#include <my_aes.h>
//#include <my_net.h>

#include "../engine/log.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Listener
//////////////////////////////////////////////////////////////////////////////////////////////////////

Listener* Listener::listener_ = 0;

template class Sort<IndirectorTest, ComparatorTest>;

// Initializes the listener thread.
// STATIC
void Listener::initialize() _THROW_(SparrowException) {
	SPARROW_ENTER("Listener::initialize");

	// Create socket.
	SocketAddress socketAddress = SocketUtil::getAddress(sparrow_listener_address, sparrow_listener_port);
	my_socket socketId = SocketUtil::create(SOCK_STREAM, socketAddress);

	// Listen socket.
	if (listen(socketId, 5)) {	// 5 connections max in the queue.
		closesocket(socketId);
		throw SparrowException::create(true, "Cannot listen socket");
	}

	// Create and start listener thread.
	listener_ = new Listener(socketId);
	if (!listener_->start()) {
		throw SparrowException::create(false, "Cannot start listener thread");
	}

	// Log info message.
	spw_print_information("Sparrow is ready and listening %s", socketAddress.print().c_str());
}

// Timeout is in milliseconds.
Listener::Listener(my_socket socketId) : Thread("Listener::listener_"), socket_(socketId) {
	SPARROW_ENTER("Listener::Listener");
	FD_ZERO(&fdSet_);
	FD_SET(socket_, &fdSet_);
	my_socket stopSocket = SocketUtil::getStopSocket();
	if (stopSocket != INVALID_SOCKET) {
		FD_SET(stopSocket, &fdSet_);
	}
}

Listener::~Listener() {
	SPARROW_ENTER("Listener::~Listener");
	::shutdown(socket_, SHUT_RDWR);
	closesocket(socket_);
	while (!handlers_.isEmpty()) {
		ConnectionHandler* handler = handlers_.first();
		handler->stop(2000);
		delete handler;
	}
}

bool Listener::process() {
	SPARROW_ENTER("Listener::process");
	fd_set fdSet = fdSet_;
	my_socket stopSocket = SocketUtil::getStopSocket();
	my_socket maxSocket = stopSocket == INVALID_SOCKET ? socket_ : std::max(socket_, stopSocket);
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	int rc = select(static_cast<int>(maxSocket + 1), &fdSet, 0, 0, &tv);
	if (rc > 0 && FD_ISSET(socket_, &fdSet)) {
		// Accept incoming connection.
		my_socket newSocketId = accept(socket_, 0, 0);
		if (newSocketId >= 0) {
			ConnectionHandler* handler = new ConnectionHandler(newSocketId);
			if (addHandler(handler)) {
				DBUG_PRINT("sparrow_api", ("Accepting incoming connection on TCP socket %u", static_cast<uint32_t>(newSocketId)));
				// Spawn a connection handler thread.
				if (!handler->start()) {
					spw_print_error("Sparrow: Cannot start connection handler");
					delete handler;
				}
			} else {
				delete handler;
				spw_print_error("sparrow_api: Connection refused; maximum number of connections (%u) reached", sparrow_max_connections);
			}
		} else {
			try {
				throw SparrowException::create(true, "Cannot accept new connection");
			} catch(const SparrowException& e) {
				e.toLog();
			}
		}
	}
	return true;
}

bool Listener::notifyStop() {
	return SocketUtil::notifyStopSocket();
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// ConnectionHandler
//////////////////////////////////////////////////////////////////////////////////////////////////////

ConnectionHandler::ConnectionHandler(my_socket socketId) : Thread(ConnectionHandler::getName().c_str()), connection_(new Connection(socketId)) {
	SPARROW_ENTER("ConnectionHandler::ConnectionHandler");
	FD_ZERO(&fdSet_);
	FD_SET(connection_->getSocket(), &fdSet_);
	my_socket stopSocket = SocketUtil::getStopSocket();
	if (stopSocket != INVALID_SOCKET) {
		FD_SET(stopSocket, &fdSet_);
	}
}

volatile uint32_t ConnectionHandler::counter_ = 0;

// STATIC
Str ConnectionHandler::getName() {
	char tmp[128];
	snprintf(tmp, sizeof(tmp), "ConnectionHandler(%u)", Atomic::inc32(&counter_));
	return Str(tmp);
}

ConnectionHandler::~ConnectionHandler() {
	SPARROW_ENTER("ConnectionHandler::~ConnectionHandler");
	Listener::removeHandler(this);
	DBUG_PRINT("sparrow_api", ("Stopping connection handler thread"));
}

bool ConnectionHandler::process() {
	SPARROW_ENTER("ConnectionHandler::process");
	if (connection_->isClosed()) {
		return false;
	}
	fd_set fdSet = fdSet_;
	my_socket stopSocket = SocketUtil::getStopSocket();
	my_socket socketId = connection_->getSocket();
	my_socket maxSocket = stopSocket == INVALID_SOCKET ? socketId : std::max(socketId, stopSocket);
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	int rc = select(static_cast<int>(maxSocket + 1), &fdSet, 0, 0, &tv);
	if (rc <= 0) {
		return true;
	} else if (FD_ISSET(socketId, &fdSet)) {
		try {
			DBUG_PRINT("sparrow_api", ("Receiving request..."));
			Request* request = new Request(connection_.get());
			ApiWorker::sendJob(request);
			return true;
		} catch(const SparrowException& e) {
			DBUG_PRINT("sparrow_api", ("Request aborted"));
			e.toLog();
			return false;	// This will kill and destroy this thread.
		}
	} else {
		return false;
	}
}

bool ConnectionHandler::notifyStop() {
	return SocketUtil::notifyStopSocket();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Connection
//////////////////////////////////////////////////////////////////////////////////////////////////////

Connection::Connection(my_socket socketId) : socket_(socketId), authentified_(false), closed_(false), lock_(false, Connection::getName().c_str()) {
	setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, (char*)&sparrow_socket_rcvbuf_size, sizeof(sparrow_socket_rcvbuf_size));
	setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, (char*)&sparrow_socket_sndbuf_size, sizeof(sparrow_socket_sndbuf_size));
	Atomic::inc32(&SparrowStatus::get().apiActiveConnections_);
	Atomic::inc64(&SparrowStatus::get().apiConnections_);
}

volatile uint32_t Connection::counter_ = 0;

// STATIC
Str Connection::getName() {
	char tmp[128];
	snprintf(tmp, sizeof(tmp), "Connection(%u)::lock_", Atomic::inc32(&counter_));
	return Str(tmp);
}

void Connection::close() {
	Guard guard(lock_);
	if (!closed_) {
		closed_ = true;
		::shutdown(socket_, SHUT_RDWR);
		::closesocket(socket_);
		Atomic::dec32(&SparrowStatus::get().apiActiveConnections_);
		DBUG_PRINT("sparrow_api", ("Closed TCP socket %u, remaining %u active connections", socket_, SparrowStatus::get().apiActiveConnections_));
	}
}

Connection::~Connection() {
	SPARROW_ENTER("Connection::~Connection");
	close();
}

void Connection::authenticate(const Str& username, const ByteBuffer& encryptedPassword) _THROW_(SparrowException) {
	const uint32_t length = static_cast<uint32_t>(encryptedPassword.limit());
	static const char* secretKey = "49#28!86@14\"&";
	const uint32_t aesLength = static_cast<uint32_t>(my_aes_get_size(length, my_aes_128_ecb));
	char* decryptedPassword = static_cast<char*>(IOContext::getTempBuffer2(aesLength));
	const int result = my_aes_decrypt(reinterpret_cast<const unsigned char*>(encryptedPassword.getData()),
		length, reinterpret_cast<unsigned char*>(decryptedPassword), 
		reinterpret_cast<const unsigned char*>(secretKey), static_cast<int>(strlen(secretKey)), my_aes_128_ecb, NULL);
	if (result <= 0) {
		throw SparrowException("Cannot decrypt password", false);
	}
	const Str password(decryptedPassword, result);
	try {
		// Dummy query to check username/password against MySQL.
		MySQLGuard guard(username.c_str(), password.c_str());
		guard.execute("select 1");
		{
			Guard lockGuard(lock_);
			username_ = username;
			password_ = password;
			authentified_ = true;
		}
	} catch(const SparrowException& e) {
		// Replace MySQL error by a generic authentication error.
		char tmp[1080];
		snprintf(tmp, sizeof(tmp), "Cannot validate username and password: %s", e.getText());
		throw SparrowException(tmp, false);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Request
//////////////////////////////////////////////////////////////////////////////////////////////////////

const uint8_t Request::TAG[] = { 83, 80, 65, 82, 82, 79, 87 };	// "SPARROW"

const uint8_t Request::SPARROW_API_VERSION = 1;

Request::Request(Connection* connection) _THROW_(SparrowException) : connection_(connection) {
	SPARROW_ENTER("Request::Request");
	uint32_t compressedLength;

	// Read header.
	{
		uint8_t header[28];
		ByteBuffer buffer(header, sizeof(header));
		SocketReader reader(*connection_, buffer);
		for (uint32_t i = 0; i < sizeof(TAG); ++i) {
			uint8_t check;
			reader >> check;
			if (check != TAG[i]) {
				dump_buffer(buffer);
				throw SparrowException::create(false, "Malformed API request");
			}
		}
		uint8_t version;
		reader >> version;
		if (version != SPARROW_API_VERSION) {
			throw SparrowException::create(false, "Unsupported API version: %u", static_cast<uint32_t>(version));
		}
		reader >> id_;
		reader >> length_ >> compressionAlgorithm_ >> compressedLength >> action_;
		if (length_ > 100 * 1024 * 1024 || compressedLength > 100 * 1024 * 1024) {
			const Str size(Str::fromSize(length_));
			throw SparrowException::create(false, "Received too much data (%s)", size.c_str());
		}
	}

	// Read compressed request data.
	{
		buffer_ = RequestBuffer(compressedLength);
		ByteBuffer buffer(buffer_.data(), compressedLength);
		SocketReader reader(*connection_, buffer);
		reader.advance(compressedLength);
	}

	// Update stats.
	Atomic::inc64(&SparrowStatus::get().apiRequests_);
	Atomic::add64(&SparrowStatus::get().apiInputBytes_, 16 + compressedLength);
	Atomic::add64(&SparrowStatus::get().apiInputUncompressedBytes_, 16 + length_);
}

void Request::dump_buffer(const ByteBuffer& buffer) {
	spw_print_information("Malformed API request. Dump of packet header:");
	uint64_t	size = buffer.limit();
	if (size != 0) {
		char*	str = new char[size*3+1];
		const uint8_t*	data = buffer.getData();
		uint	j = 0;
		for (uint64_t i=0; i<size; ++i) {
			uint8_t	c;
			c = *data >> 4;
			if (c < 10) {
				str[j++] = c + '0';
			} else {
				str[j++] = c - 10 + 'A';
			}

			c = *data & 0x0F;
			if (c < 10) {
				str[j++] = c + '0';
			} else {
				str[j++] = c - 10 + 'A';
			}
			str[j++] = ' ';
			data++;
		}
		str[j] = '\0';
		spw_print_information("%s", str);
		delete [] str;
	}
}

void Request::process() {
	SPARROW_ENTER("Request::process");
	try {
		// Decompress request.
		if (buffer_.length() < length_) {
			if (compressionAlgorithm_ != 1) {
				throw SparrowException::create(false, "Only lzjb compression is supported");
			}
			RequestBuffer buffer(length_);
			const int result = LZJB::decompress(buffer_.data(), buffer.data(), buffer_.length(), length_);
			if (result != 0) {
				throw SparrowException::create(false, "Cannot decompress data");
			}
			buffer_ = buffer;
		}

		// Actually process the request and generate the response.
		GrowingByteBuffer response;
		doProcess(response);

		// Compress the response using the input compression algorithm and send it.
		const uint32_t length = static_cast<uint32_t>(response.position());
		if (compressionAlgorithm_ == 0) {
			// No compression.
			SocketWriterGuard guard(*connection_);
			ByteBuffer& writer = guard.get();
			for (uint32_t i = 0; i < sizeof(TAG); ++i) {
				writer << TAG[i];
			}
			writer << SPARROW_API_VERSION;
			writer << id_ << length << compressionAlgorithm_ << length;
			writer << ByteBuffer(response.getData(), length);

			// Update stats.
			Atomic::inc64(&SparrowStatus::get().apiResponses_);
			Atomic::add64(&SparrowStatus::get().apiOutputBytes_, length);
			Atomic::add64(&SparrowStatus::get().apiOutputUncompressedBytes_, length);
		} else {
			RequestBuffer compressedBuffer(length);
			const uint32_t compressedLength = static_cast<uint32_t>(LZJB::compress(response.getData(), compressedBuffer.data(), length, length));
			{
				SocketWriterGuard guard(*connection_);
				ByteBuffer& writer = guard.get();
				for (uint32_t i = 0; i < sizeof(TAG); ++i) {
					writer << TAG[i];
				}
				writer << SPARROW_API_VERSION;
				writer << id_ << length << compressionAlgorithm_ << compressedLength;
				if (compressedLength < length) {
					writer << ByteBuffer(compressedBuffer.data(), compressedLength);
				} else {
					writer << ByteBuffer(response.getData(), length);
				}
			}

			// Update stats.
			Atomic::inc64(&SparrowStatus::get().apiResponses_);
			Atomic::add64(&SparrowStatus::get().apiOutputBytes_, compressedLength);
			Atomic::add64(&SparrowStatus::get().apiOutputUncompressedBytes_, length);
		}
	} catch(const SparrowException& e) {
		e.toLog();
	}
}

void Request::doProcess(ByteBuffer& response) _THROW_(SparrowException) {
	SPARROW_ENTER("Request::doProcess");
	MasterGuard master;
	GrowingByteBuffer	resp_data;
	bool ok = false;
	try {
		ByteBuffer buffer(buffer_.data(), buffer_.length());
		if (action_ != MSG_ID_AUTH && !connection_->isAuthentified()) {
			throw SparrowException::create(false, "Connection is not authenticated");
		}
		if (action_ == MSG_ID_AUTH) {
			DBUG_PRINT("sparrow_api", ("MSG_ID_AUTH"));
			Str username;
			buffer >> username;
			uint32_t length;
			buffer >> length;
			ByteBuffer encryptedPassword(static_cast<uint8_t*>(IOContext::getTempBuffer1(length)), length);
			buffer >> encryptedPassword;
			connection_->authenticate(username, encryptedPassword);
			DBUG_PRINT("sparrow_api", ("Successfully authenticated connection"));
		} else if (action_ == MSG_ID_INIT) {
			Str database;
			Str table;
			buffer >> database >> table;
			DBUG_PRINT("sparrow_api", ("MSG_ID_INIT, table %s.%s", database.c_str(), table.c_str()));
			ColumnExs columns;
			Indexes indexes;
			ForeignKeys foreignKeys;
			DnsConfiguration dnsConfiguration;
			uint32_t aggregationPeriod;
			uint64_t defaultWhere;
			uint64_t stringOptimization;
			uint64_t maxLifetime;
			uint64_t coalescingPeriod;
			buffer >> columns >> indexes >> foreignKeys >> dnsConfiguration >> aggregationPeriod >> defaultWhere
				>> stringOptimization >> maxLifetime >> coalescingPeriod;
			InternalApi::init(connection_->getUsername().c_str(), connection_->getPassword().c_str(), database.c_str(), table.c_str(),
				columns, indexes, foreignKeys, dnsConfiguration, aggregationPeriod, defaultWhere, stringOptimization, maxLifetime, coalescingPeriod);
			DBUG_PRINT("sparrow_api", ("Successfully initialized table %s.%s", database.c_str(), table.c_str()));
		} else if (action_ == MSG_ID_DATA) {
			Str database;
			Str table;
			buffer >> database >> table;
			DBUG_PRINT("sparrow_api", ("MSG_ID_DATA, table %s.%s", database.c_str(), table.c_str()));
			uint32_t rows;
			buffer >> rows;
			uint32_t length;
			buffer >> length;
			InternalApi::write(database.c_str(), table.c_str(), buffer, rows);
			DBUG_PRINT("sparrow_api", ("Successfully inserted %u rows into table %s.%s", rows, database.c_str(), table.c_str()));
		} else if (action_ == MSG_ID_DATA_EX) {
			Str database;
			Str table;
			buffer >> database >> table;
			uint32_t nbCols;
			buffer >> nbCols;
			Names	colNames(nbCols);
			for (uint i=0; i<nbCols; ++i) {
				Str	colName;
				buffer >> colName;
				colNames.append(colName);
			}
			uint32_t rows;
			buffer >> rows;
			uint32_t length;
			buffer >> length;
			DBUG_PRINT("sparrow_api", ("MSG_ID_DATA_EX, table %s.%s", database.c_str(), table.c_str()));
			InternalApi::write(database.c_str(), table.c_str(), colNames, buffer, rows);
			DBUG_PRINT("sparrow_api", ("Successfully inserted %u rows into table %s.%s on a selection of %u columns", rows, database.c_str(), table.c_str(), colNames.entries()));
		} else if (action_ == MSG_ID_GET_MASTER) {
			Str database;
			Str table;
			buffer >> database >> table;
			DBUG_PRINT("sparrow_api", ("MSG_ID_GET_MASTER, table %s.%s", database.c_str(), table.c_str()));
			try {
				master = InternalApi::get(database.c_str(), table.c_str(), false, false, 0);
			} catch(const SparrowException& e) {
				// The master file may have been copied after database startup: discover master files again and retry.
				InternalApi::setup();
				master = InternalApi::get(database.c_str(), table.c_str(), false, false, 0);
			}
			if (master != 0) {
				master.get()->retrieve(resp_data);
			}
			DBUG_PRINT("sparrow_api", ("Successfully retrieved master file from table %s.%s", database.c_str(), table.c_str()));
		} else if (action_ == MSG_ID_DISABLE_COALESCING) {
			uint32_t timeout;
			bool	wait_until_end = false;
			buffer >> timeout;
			if (!buffer.end()) {
				buffer >> wait_until_end;
			}
			DBUG_PRINT("sparrow_api", ("MSG_ID_DISABLE_COALESCING, timeout %us, wait %u", timeout, wait_until_end));
			CoalescingControlTask::disable(timeout, wait_until_end);
		} else if (action_ == MSG_ID_DISABLE_COALESCING_DB) {
			uint32_t	timeout;
			Str		database;	
			bool	wait_until_end = false;
			buffer >> timeout >> database;
			if (!buffer.end()) {
				buffer >> wait_until_end;
			}
			DBUG_PRINT("sparrow_api", ("MSG_ID_DISABLE_COALESCING_DB, timeout %us, database %s, wait %u", timeout, database.c_str(), wait_until_end));
			CoalescingControlTaskPerDB::disable(timeout, database, wait_until_end);
		} else if (action_ == MSG_ID_REMOVE_PARTITIONS) {
			Str database;
			Str table;
			uint64_t low;
			uint64_t up;
			buffer >> database >> table >> low >> up;
			const TimePeriod period(low == 0 ? 0 : &low, up == 0 ? 0 : &up, low != 0, up != 0); 
#ifndef NDEBUG
			const Str sPeriod = Str::fromTimePeriod(period);
			DBUG_PRINT("sparrow_api", ("MSG_ID_REMOVE_PARTITIONS, table %s.%s, period %s", database.c_str(), table.c_str(), sPeriod.c_str()));
#endif
			InternalApi::removePartitions(database.c_str(), table.c_str(), period);
		} else if (action_ == MSG_ID_SWITCH_PURGE_MODE) {
			uint32_t		timeout;
			Str			database;
			int			mode;
			buffer >> timeout >> database >> mode;
			DBUG_PRINT("sparrow_api", ("MSG_ID_SWITCH_PURGE_MODE to mode %u, timeout %us, database %s", mode, timeout, database.c_str()));
			PurgeModeControlTaskPerDB::switchMode(timeout, database, static_cast<PurgeMode>(mode));
		} else {
			throw SparrowException::create(false, "Unknown action code %u", action_);
		}
		ok = true;
	} catch(const SparrowException& e) {
		e.toLog();
		response << Str(e.getText(), false) << e.get_err_code();
	}
	if (ok) {
		response << static_cast<uint32_t>(0);
		if (resp_data.position() != 0)
		{
			resp_data.limit(resp_data.position()+1);
			resp_data.position(0);
			response << resp_data;
		}
		//if (master != 0) {	// For MSG_ID_GET_MASTER.
		//	master.get()->retrieve(response);
		//}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingControlTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

Lock CoalescingControlTask::lock_(true, "CoalescingControlTask::lock_");
CoalescingControlTask* CoalescingControlTask::task_ = 0;

void CoalescingControlTask::run(const uint64_t timestamp) _THROW_(SparrowException) {
	if (!sparrow_coalescing && initialCoalescing_) {
		CoalescingWorker::initialize();
	} else if (sparrow_coalescing && !initialCoalescing_) {
		CoalescingWorker::shutdown();
	}
	sparrow_coalescing = initialCoalescing_;
	{
		Guard guard(lock_);
		task_ = 0;
	}
}

// STATIC
void CoalescingControlTask::disable(const uint32_t timeout, bool wait) {
	{
		Guard guard(lock_);
		// If the task already exist, reschedule it. If the scheduler can't find it, it means the task is being processed,
		//	therefore do not do anything. 
		if (task_ != 0) {
			DBUG_PRINT("sparrow_coalescing_control_task", ("Rescheduling coalescing ctrl task %p for %us", task_, timeout));
			if (!Scheduler::moveTask(task_, Scheduler::now() + timeout * 1000)) {
				DBUG_PRINT("sparrow_coalescing_control_task", ("Could not reschedule coalescing ctrl task %p.", task_));
			}
		} else {
			task_ = new CoalescingControlTask();
			DBUG_PRINT("sparrow_coalescing_control_task", ("Scheduling coalescing ctrl task %p for %us", task_, timeout));
			Scheduler::addTask(task_, Scheduler::now() + timeout * 1000, true);
		}
	}

	if (timeout != 0)
	{
		InternalApi::StopCoalescingTasks();

		if (wait) {
			Masters		masters = InternalApi::getAll();
			const uint32_t nbMasters = masters.length();
			while (true) {
				uint32_t i = 0;
				for (i=0; i<nbMasters; ++i) {
					Master&		master = *masters[i];
					if (master.getNbCoalescingTasks() != 0)
						break;
				}
				if (i == nbMasters)
					break;
				my_sleep(100000);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingControlTaskPerDB
//////////////////////////////////////////////////////////////////////////////////////////////////////

Lock CoalescingControlTaskPerDB::lock_(true, "CoalescingControlTaskPerDB::lock_");
CoalescingControlTasks CoalescingControlTaskPerDB::tasks_;

void CoalescingControlTaskPerDB::run(const uint64_t timestamp) _THROW_(SparrowException) {
	Guard guard(lock_);
	DBUG_PRINT("sparrow_coalescing_control_task", ("Running task %p for schema %s", this, schema_.c_str()));
	tasks_.remove( this );
}

// STATIC
void CoalescingControlTaskPerDB::disable(const uint32_t timeout, const Str& schema, bool wait) {
	{
		Guard guard(lock_);
		CoalescingControlTaskPerDB	tsk(schema);
		CoalescingControlTaskPerDB*	task = tasks_.find( &tsk );
		// If the task already exists, reschedule it. If the scheduler can't find it, it means the task is being processed,
		//	therefore we have to create a new one. 
		if (task != 0) {
			DBUG_PRINT("sparrow_coalescing_control_task", ("Rescheduling coalescing ctrl task %p for schema %s for %us", task, schema.c_str(), timeout));
			if (!Scheduler::moveTask(task, Scheduler::now() + timeout * 1000)) {
				DBUG_PRINT("sparrow_coalescing_control_task", ("Could not reschedule coalescing ctrl task %p.", task));
				task = 0;
			}
		}
		if ( task == 0 ) {
			task = new CoalescingControlTaskPerDB( schema );
			DBUG_PRINT("sparrow_coalescing_control_task", ("Scheduling coalescing ctrl task %p for schema %s for %us", task, schema.c_str(), timeout));
			tasks_.append( task );
			Scheduler::addTask(task, Scheduler::now() + timeout * 1000, true);
		}
	}

	if (timeout != 0)
	{
		InternalApi::StopCoalescingTasks(schema.c_str());

		if (wait) {
			Masters		masters = InternalApi::getAll();
			const uint32_t nbMasters = masters.length();
			while (true) {
				uint32_t i = 0;
				for (i=0; i<nbMasters; ++i) {
					Master&		master = *masters[i];
					if (master.getDatabase() == schema && master.getNbCoalescingTasks() != 0)
						break;
				}
				if (i == nbMasters)
					break;
				my_sleep(100000);
			}
		}
	}
}


// STATIC
bool CoalescingControlTaskPerDB::isDisabled(const Str& schema) {
	CoalescingControlTaskPerDB	task(schema);
	Guard guard(lock_);
	return tasks_.contains( &task );
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// PurgeModeControlTaskPerDB
//////////////////////////////////////////////////////////////////////////////////////////////////////

Lock PurgeModeControlTaskPerDB::lock_(true, "PurgeModeControlTaskPerDB::lock_");
PurgeModeControlTasks PurgeModeControlTaskPerDB::tasks_;

void PurgeModeControlTaskPerDB::run(const uint64_t timestamp) _THROW_(SparrowException) {
	Guard guard(lock_);
	tasks_.remove( this );
}

// STATIC
void PurgeModeControlTaskPerDB::switchMode(const uint32_t timeout, const Str& schema, PurgeMode mode) {
	Guard guard(lock_);
	PurgeModeControlTaskPerDB	tsk(schema);
	PurgeModeControlTaskPerDB*	task = tasks_.find( &tsk );

	// If the task already exists, reschedule it. If the scheduler can't find it, it means the task is being processed,
	//	therefore we have to create a new one. 
	if (task != 0) {
		DBUG_PRINT("sparrow_purge_control_task", ("Rescheduling purge ctrl task %p for schema %s, mode %u for %us", task, schema.c_str(), mode, timeout));
		task->mode_ = mode;
		if (!Scheduler::moveTask(task, Scheduler::now() + timeout * 1000)) {
			DBUG_PRINT("sparrow_purge_control_task", ("Could not reschedule purge ctrl task %p.", task));
			task = 0;
		}
	}
	if ( task == 0 ) {
		task = new PurgeModeControlTaskPerDB( schema, mode );
		DBUG_PRINT("sparrow_purge_control_task", ("Scheduling purge ctrl task %p for schema %s, mode %u for %us", task, schema.c_str(), mode, timeout));
		tasks_.append( task );
		Scheduler::addTask(task, Scheduler::now() + timeout * 1000, true);
	}
}

// STATIC
PurgeMode PurgeModeControlTaskPerDB::getMode(const Str& schema) {
	PurgeModeControlTaskPerDB	key(schema);
	Guard guard(lock_);
	PurgeModeControlTaskPerDB*	task = tasks_.find(&key);
	if (task != NULL) {
		return task->mode_;
	}
	return (sparrow_purge_constantly ? PURGE_MODE_CONSTANTLY : PURGE_MODE_ON_INSERTION);
}

}
