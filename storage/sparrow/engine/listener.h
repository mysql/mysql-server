/*
	Listener thread.
*/

#ifndef _engine_listener_h_
#define _engine_listener_h_

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif
#include "scheduler.h"
#include "thread.h"
#include "exception.h"
#include "misc.h"
#include "hash.h"
#include "lock.h"
#include "purge.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Connection
//////////////////////////////////////////////////////////////////////////////////////////////////////

class SocketWriterGuard;
class Connection : public RefCounted {
	friend class SocketWriterGuard;

private:

	my_socket socket_;
	bool authentified_;
	bool closed_;
	Str username_;
	Str password_;
	Lock lock_;			// To serialize writes and to make authentication atomic.

	static volatile uint32_t counter_;

private:

	static Str getName();

public:

	Connection(my_socket socketId);

	void authenticate(const Str& username, const ByteBuffer& encryptedPassword) _THROW_(SparrowException);

	my_socket getSocket() const {
		return socket_;
	}

	bool isAuthentified() const {
		return authentified_;
	}

	const Str& getUsername() const {
		return username_;
	}

	const Str& getPassword() const {
		return password_;
	}

	void close();

	bool isClosed() const {
		return closed_;
	}

	~Connection();
};

typedef RefPtr<Connection> ConnectionGuard;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketWriterGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

class SocketWriterGuard : private Guard {
private:

	SocketWriter writer_;

public:

	SocketWriterGuard(Connection& connection) : Guard(connection.lock_), writer_(connection) {
	}

	ByteBuffer& get() {
		return writer_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Request
//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef SYSarray<uint8_t> RequestBuffer;

class Request : public Job {
private:

	ConnectionGuard connection_;
	uint32_t id_;
	uint32_t action_;
	RequestBuffer buffer_;
	uint32_t length_;
	uint32_t compressionAlgorithm_;

	static const uint8_t TAG[];

	static const uint8_t SPARROW_API_VERSION;

private:

	void doProcess(ByteBuffer& response) _THROW_(SparrowException);

	void dump_buffer(const ByteBuffer&);

public:

	Request(Connection* connection) _THROW_(SparrowException);

	void process() override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ConnectionHandler
//////////////////////////////////////////////////////////////////////////////////////////////////////

#define MSG_ID_AUTH						0
#define MSG_ID_INIT						1
#define MSG_ID_DATA						2
#define MSG_ID_GET_MASTER				3
#define MSG_ID_DISABLE_COALESCING		4
#define MSG_ID_REMOVE_PARTITIONS		5
#define MSG_ID_DISABLE_COALESCING_DB	6
#define MSG_ID_DATA_EX					7
#define MSG_ID_SWITCH_PURGE_MODE		8

class ConnectionHandler : public Thread, public SYSidlink<ConnectionHandler> {
private:

	ConnectionGuard connection_;
	fd_set fdSet_;
	static volatile uint32_t counter_;

private:

	static Str getName();

protected:

	bool process() override;

	bool notifyStop() override;

	bool deleteAfterExit() override {
		return true;
	}

public:

	ConnectionHandler(my_socket socketId);

	~ConnectionHandler();
};

typedef SYSidlist<ConnectionHandler> ConnectionHandlers;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Listener
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Listener : public Thread {
private:

	static Listener* listener_;

	my_socket socket_;
	fd_set fdSet_;

	ConnectionHandlers handlers_;

protected:

	bool process() override;

	bool notifyStop() override;

	bool deleteAfterExit() override {
		return false;
	}

public:

	static void initialize() _THROW_(SparrowException);

	static void shutdown() {
		if (listener_ != 0) {
			listener_->stop();
			delete listener_;
			listener_ = 0;
		}
	}

	Listener(my_socket socketId);

	~Listener();

	bool addHandler(ConnectionHandler* handler) {
		Guard guard(lock_);
		handlers_.append(handler);
		return handlers_.entries() <= sparrow_max_connections;
	}

	static void removeHandler(ConnectionHandler* handler) {
		Guard guard(listener_->lock_);
		listener_->handlers_.remove(handler);
	}
};


//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingControlTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CoalescingControlTask : public Task {
private:

	static Lock lock_;
	static CoalescingControlTask* task_;		// Task that disables the coalescing globally
	bool initialCoalescing_;

public:

	CoalescingControlTask() : Task(Worker::getQueue()), initialCoalescing_(sparrow_coalescing) {
		sparrow_coalescing = 0;
	}

	virtual bool operator == (const CoalescingControlTask& right) const {
		return true;
	}

	virtual bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override {
		return 0;
	}

	void run(const uint64_t timestamp) override _THROW_(SparrowException);

	static void disable(const uint32_t timeout, bool wait);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CoalescingControlTaskPerDB
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CoalescingControlTaskPerDB;
typedef SYSpVector<CoalescingControlTaskPerDB,0>	CoalescingControlTasks;

class CoalescingControlTaskPerDB : public Task {
private:

	static Lock lock_;
	static CoalescingControlTasks tasks_;
	Str		schema_;

public:

	CoalescingControlTaskPerDB(const Str& schema) : Task(Worker::getQueue()), schema_(schema) {
		DBUG_PRINT("sparrow_coalescing_control_task", ("New task %p for schema %s", this, schema_.c_str()));
	}

	~CoalescingControlTaskPerDB() {
		DBUG_PRINT("sparrow_coalescing_control_task", ("Destroying task %p for schema %s", this, schema_.c_str()));
	}

	virtual bool operator == (const CoalescingControlTaskPerDB& right) const {
		return schema_ == right.schema_;
	}

	virtual bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override {
		return 0;
	}

	void run(const uint64_t timestamp) override _THROW_(SparrowException);

	static void disable(const uint32_t timeout, const Str& schema, bool wait);

	static bool isDisabled(const Str& schema);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PurgeModeControlTaskPerDB
//////////////////////////////////////////////////////////////////////////////////////////////////////

class PurgeModeControlTaskPerDB;
typedef SYSpVector<PurgeModeControlTaskPerDB,0>	PurgeModeControlTasks;

class PurgeModeControlTaskPerDB : public Task {
private:

	static Lock lock_;
	static PurgeModeControlTasks tasks_;
	Str			schema_;
	PurgeMode	mode_;

public:

	PurgeModeControlTaskPerDB(const Str& schema) : Task(Worker::getQueue()), schema_(schema) {;}

	PurgeModeControlTaskPerDB(const Str& schema, PurgeMode mode) : Task(Worker::getQueue()), schema_(schema), mode_(mode) {;}

	virtual bool operator == (const PurgeModeControlTaskPerDB& right) const {
		return schema_ == right.schema_;
	}

	virtual bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override {
		return 0;
	}

	void run(const uint64_t timestamp) override _THROW_(SparrowException);

	static void switchMode(const uint32_t timeout, const Str& schema, PurgeMode mode);

	static PurgeMode getMode(const Str& schema);
};

}

#endif /* #ifndef _engine_listener_h_ */
