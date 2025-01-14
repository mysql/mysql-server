/*
	Socket utilities.
*/

#ifndef _engine_socketutil_h_
#define _engine_socketutil_h_

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include "exception.h"
#include "misc.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketAddress
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Address to which a socket is bound. Can be used as a key.
class SocketAddress {
private:

	union RawSocketAddress {
		struct sockaddr_in v4_;
		struct sockaddr_in6 v6_;
	} raw_;
	bool v6_;

public:

	SocketAddress() : v6_(false) {
		memset(&raw_, 0, sizeof(raw_));
	}

	SocketAddress(struct sockaddr_in& v4) : v6_(false) {
		memset(&raw_, 0, sizeof(raw_));
		raw_.v4_ = v4;
	}

	SocketAddress(struct sockaddr_in6& v6) : v6_(true) {
		memset(&raw_, 0, sizeof(raw_));
		raw_.v6_ = v6;
	}

	bool isV6() const {
		return v6_;
	}

	const struct sockaddr_in& getV4() const {
		return raw_.v4_;
	}

	struct sockaddr_in& getV4() {
		return raw_.v4_;
	}

	const struct sockaddr_in6& getV6() const {
		return raw_.v6_;
	}

	struct sockaddr_in6& getV6() {
		return raw_.v6_;
	}

	const struct sockaddr* getSockAddr() const {
		return isV6() ? (const struct sockaddr*)(&getV6()) : (const struct sockaddr*)(&getV4());
	}

	struct sockaddr* getSockAddr() {
		return isV6() ? (struct sockaddr*)(&getV6()) : (struct sockaddr*)(&getV4());
	}

	int getSockAddrLength() const {
		return static_cast<int>(isV6() ? sizeof(getV6()) : sizeof(getV4()));
	}

	bool operator == (const SocketAddress& right) const {
		return v6_ == right.v6_ && memcmp(&raw_, &right.raw_, sizeof(raw_)) == 0;
	}

	uint32_t hash() const {
		uint32_t h = 1;
		uint32_t i = sizeof(raw_);
		const uint8_t* raw = (const uint8_t*)&raw_;
		while (i-- > 0) {
			h = 31 * h + raw[i];
		}
		return h;
	}

	Str print() const;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketUtil
//////////////////////////////////////////////////////////////////////////////////////////////////////

class SocketUtil {
private:

	static my_socket stopSocket_;
	static SocketAddress stopSocketAddress_;
	static bool v6_;

public:

	static void initialize() _THROW_(SparrowException);

	static my_socket create(int type, const SocketAddress& address) _THROW_(SparrowException);

	static SocketAddress getAddress(const char* address, uint32_t port) _THROW_(SparrowException);

	static SocketAddress getAddress(my_socket socket) _THROW_(SparrowException);

	static my_socket getStopSocket() {
		return stopSocket_;
	}

	static bool notifyStopSocket();
};

}

#endif /* #ifndef _engine_socketutil_h_ */
