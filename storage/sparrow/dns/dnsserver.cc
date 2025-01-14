/*
	Connection to a DNS server.
*/

#include "dnsserver.h"
#include "dns.h"

#define MYSQL_SERVER 1
//#include <sql_priv.h>
#include "sql/query_options.h"		// For mysqld options.
#include <mysys_err.h>
//#include <my_net.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsSocket
//////////////////////////////////////////////////////////////////////////////////////////////////////

SYSpHash<DnsSocket> DnsSocket::hash_(256);

DnsSocket::DnsSocket(const char* address, uint32_t port) _THROW_(SparrowException)
	: socketId_(INVALID_SOCKET), address_(SocketUtil::getAddress(address, port)), references_(0) {
	SPARROW_ENTER("DnsSocket::DnsSocket");
}

void DnsSocket::open() _THROW_(SparrowException) {
	SPARROW_ENTER("DnsSocket::open");
	if (socketId_ == INVALID_SOCKET) {
		socketId_ = SocketUtil::create(SOCK_DGRAM, address_);
		setsockopt(socketId_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&sparrow_socket_rcvbuf_size), sizeof(sparrow_socket_rcvbuf_size));
#ifndef NDEBUG
		const Str address = address_.print();
		DBUG_PRINT("sparrow_dns", ("Opening datagram socket %s: id is %u", address.c_str(), static_cast<uint32_t>(socketId_)));
#endif
	}
}

DnsSocket::~DnsSocket() {
	SPARROW_ENTER("DnsSocket::~DnsSocket");
	if (socketId_ != INVALID_SOCKET) {
#ifndef NDEBUG
		const Str address = address_.print();
		DBUG_PRINT("sparrow_dns", ("Closing datagram socket %s: id is %u", address.c_str(), static_cast<uint32_t>(socketId_)));
#endif
		::shutdown(socketId_, SHUT_RDWR);
		closesocket(socketId_);
	}
}

// Gets a socket for a given (source address, source port) couple out of the dictionary.
// STATIC
DnsSocket* DnsSocket::acquire(const char* address, uint32_t port) _THROW_(SparrowException) {
	SPARROW_ENTER("DnsSocket::acquire");
	DnsSocket key(address, port);
	DnsSocket* socket = hash_.find(&key);
	if (socket == 0) {
		try {
			socket = new DnsSocket(address, port);
		} catch(const SparrowException& e) {
			e.toLog();
		}
		hash_.insert(socket);
	}
	socket->references_++;
#ifndef NDEBUG
	const Str saddress = socket->address_.print();
	DBUG_PRINT("sparrow_dns", ("Datagram socket %s has now %u references", saddress.c_str(), socket->references_));
#endif
	return socket;
}

// Releases a socket when it is no longer used.
// STATIC
void DnsSocket::release(DnsSocket* socket) {
	SPARROW_ENTER("DnsSocket::release");
	--socket->references_;
#ifndef NDEBUG
	const Str address = socket->address_.print();
	DBUG_PRINT("sparrow_dns", ("Datagram socket %s has now %u references", address.c_str(), socket->references_));
#endif
	if (socket->references_ == 0) {
		delete hash_.remove(socket);
	}
}

// Helper to fill a set of socket ids. Returns the max socket id, to pass to the select() call.
// The stop socket is present in the fd set, but not in the socket vector.
// This way, stop data is properly ignored.
// STATIC
my_socket DnsSocket::fillFdSet(fd_set* fdSet, SYSvector<my_socket>& socketIds) {
	SPARROW_ENTER("DnsSocket::fillFdSet");
	my_socket maxSocketId = INVALID_SOCKET;
	FD_ZERO(fdSet);
	socketIds.resize(hash_.entries());
	socketIds.forceLength(0);
	SYSpHashIterator<DnsSocket> iterator(hash_);
	bool first = true;
	while (++iterator) {
		DnsSocket& socket = *iterator.key();
		my_socket socketId = socket.get();
		FD_SET(socketId, fdSet);
		socketIds.append(socketId);
		if (first || socketId + 1 > maxSocketId) {
			first = false;
			maxSocketId = socketId + 1;
		}
	}
	my_socket stopSocket = SocketUtil::getStopSocket();
	if (stopSocket != INVALID_SOCKET) {
		FD_SET(stopSocket, fdSet);
		if (first || stopSocket + 1 > maxSocketId) {
			maxSocketId = stopSocket + 1;
		}
	}
	return maxSocketId;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsServerConnection
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Dictionary of DNS connections.
SYSpHash<DnsServerConnection> DnsServerConnection::hash_(256);

// If this new DNS connection is not a search key, the socket is actually opened.
DnsServerConnection::DnsServerConnection(const DnsServer& dns, const bool key) _THROW_(SparrowException)
	: address_(SocketUtil::getAddress(dns.getHost(), dns.getPort())), references_(0) {
	SPARROW_ENTER("DnsServerConnection::DnsServerConnection");

	// Acquire socket only after we are sure address_ is valid.
	socket_ = DnsSocket::acquire(dns.getSourceAddress(), dns.getSourcePort());
	if (!key) {
		socket_->open();
	}
}

DnsServerConnection::~DnsServerConnection() {
	SPARROW_ENTER("DnsServerConnection::~DnsServerConnection");
	DnsSocket::release(socket_);
}

// Gets a connection to a DNS server out of the dictionary.
// STATIC
DnsServerConnection* DnsServerConnection::acquire(const DnsServer& dns) _THROW_(SparrowException) {
	SPARROW_ENTER("DnsServerConnection::acquire");
	Guard guard(Dns::getLock());
	const DnsServerConnection key(dns, true);
	DnsServerConnection* server = hash_.find(&key);
	if (server == 0) {
		server = new DnsServerConnection(dns, false);
		hash_.insert(server);
		Dns::incSerial();
	}
	server->references_++;
	return server;
}

// Releases a connection to a DNS server.
// STATIC
void DnsServerConnection::release(DnsServerConnection* server) {
	SPARROW_ENTER("DnsServerConnection::release");
	Guard guard(Dns::getLock());
	if (--server->references_ == 0) {
		delete hash_.remove(server);
		Dns::incSerial();
	}
}

// Sends a packet to a DNS server.
void DnsServerConnection::send(const uint8_t* buffer, const uint32_t length) _THROW_(SparrowException) {
	SPARROW_ENTER("DnsServerConnection::send");
	const my_socket socketId = socket_->get();
	uint32_t sent = 0;
	while (sent < length) {
		int result = sendto(socketId, reinterpret_cast<const char*>(buffer) + sent, length - sent, 0,
			address_.getSockAddr(), address_.getSockAddrLength());
		if (result == -1) {
			throw SparrowException::create(true, "Cannot send UDP packet to %s", address_.print().c_str());
		}
		sent += result;
	}
}

}
