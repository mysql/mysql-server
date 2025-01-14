/*
	Connection to a DNS server.
*/

#ifndef _dns_dnsserver_h_
#define _dns_dnsserver_h_

#include "../engine/types.h"
#include "../engine/hash.h"
#include "../engine/socketutil.h"
#include "../engine/thread.h"
#include "../engine/scheduler.h"
#include "dnsdefault.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsSocket
//////////////////////////////////////////////////////////////////////////////////////////////////////

// UDP socket bound on a specific source address and source port.
class DnsSocket {
private:

	static SYSpHash<DnsSocket> hash_;

	my_socket socketId_;
	SocketAddress address_;
	uint32_t references_;

public:

	DnsSocket(const char* address, uint32_t port) _THROW_(SparrowException);

	~DnsSocket();

	void open() _THROW_(SparrowException);

	static DnsSocket* acquire(const char* address, uint32_t port) _THROW_(SparrowException);

	static void release(DnsSocket* socket);

	bool operator == (const DnsSocket& right) const {
		return address_ == right.address_;
	}

	uint32_t hash() const {
		return address_.hash();
	}

	my_socket get() {
		return socketId_;
	}

	static my_socket fillFdSet(fd_set* fdSet, SYSvector<my_socket>& socketIds);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsServerConnection
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsServer;
class DnsServerConnection {
private:

	static SYSpHash<DnsServerConnection> hash_;

	DnsSocket* socket_;
	SocketAddress address_;
	uint32_t references_;

public:

	DnsServerConnection(const DnsServer& dns, const bool key) _THROW_(SparrowException);

	~DnsServerConnection();

	static DnsServerConnection* acquire(const DnsServer& dns) _THROW_(SparrowException);

	static void release(DnsServerConnection* server);

	bool operator == (const DnsServerConnection& right) const {
		return *socket_ == *right.socket_ && address_ == right.address_;
	}

	uint32_t hash() const {
		return socket_->hash() + address_.hash();
	}

	void send(const uint8_t* buffer, const uint32_t length) _THROW_(SparrowException);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsServer
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsServer {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, DnsServer& dns);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const DnsServer& dns);

private:

	Str host_;
	uint32_t port_;
	Str sourceAddress_;
	uint32_t sourcePort_;
	DnsServerConnection* connection_;

public:

	DnsServer() : port_(0), sourcePort_(0), connection_(0) {
	}

	DnsServer(const char* host, const uint32_t port, const char* sourceAddress, const uint32_t sourcePort)
		: host_(host), port_(port), sourceAddress_(sourceAddress), sourcePort_(sourcePort), connection_(0) {
	}

	DnsServer& operator = (const DnsServer& right) {
		SPARROW_ENTER("DnsServer::operator =");
		const bool started = (connection_ != 0);
		if (started) {
			DnsServerConnection::release(connection_);
			connection_ = 0;
		}
		host_ = right.host_;
		port_ = right.port_;
		sourceAddress_ = right.sourceAddress_;
		sourcePort_ = right.sourcePort_;
		if (started) {
			try {
				start();
			} catch(const SparrowException&) {
				// Ignore error.
				connection_ = 0;
			}
		}
		return *this;
	}

	bool operator == (const DnsServer& right) const {
		return host_ == right.host_ && port_ == right.port_
			&& sourceAddress_ == right.sourceAddress_ && sourcePort_ == right.sourcePort_;
	}

	bool operator < (const DnsServer& right) const {
		int cmp = host_.compareTo(right.host_, false);
		if (cmp < 0) {
			return true;
		} else if (cmp > 0) {
			return false;
		}
		if (port_ < right.port_) {
			return true;
		} else if (port_ > right.port_) {
			return false;
		}
		cmp = sourceAddress_.compareTo(right.sourceAddress_, false);
		if (cmp < 0) {
			return true;
		} else if (cmp > 0) {
			return false;
		}
		if (sourcePort_ < right.sourcePort_) {
			return true;
		} else if (sourcePort_ > right.sourcePort_) {
			return false;
		}
		return false;
	}

	DnsServer(const DnsServer& right) : connection_(0) {
		*this = right;
	}

	~DnsServer() {
		if (connection_ != 0) {
			DnsServerConnection::release(connection_);
		}
	}

	const char* getHost() const {
		return host_.c_str();
	}

	uint32_t getPort() const {
		return port_;
	}

	const char* getSourceAddress() const {
		return sourceAddress_.c_str();
	}

	uint32_t getSourcePort() const {
		return sourcePort_;
	}

	// Initiate the connection with this DNS server.
	void start() _THROW_(SparrowException) {
		if (connection_ == 0) {
			connection_ = DnsServerConnection::acquire(*this);
		}
	}

	DnsServerConnection& getConnection() {
		return *connection_;
	}

	Str print() const {
		char buffer[1024];
		snprintf(buffer, sizeof(buffer), "host=%s, port=%u, sourceAddress=%s, sourcePort=%u", getHost(), getPort(), getSourceAddress(), getSourcePort());
		return Str(buffer);
	}
};

inline ByteBuffer& operator << (ByteBuffer& buffer, const DnsServer& dns) {
	buffer << dns.host_ << dns.port_ << dns.sourceAddress_ << dns.sourcePort_;
	return buffer;
}

inline ByteBuffer& operator >> (ByteBuffer& buffer, DnsServer& dns) {
	buffer >> dns.host_ >> dns.port_ >> dns.sourceAddress_ >> dns.sourcePort_;
	return buffer;
}

typedef SYSsortedVector<DnsServer> DnsServers;

}

#endif /* #ifndef _dns_dnsserver_h_ */
