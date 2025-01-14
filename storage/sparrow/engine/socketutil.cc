/*
	Socket utilities.
*/

#include "socketutil.h"
#include "../functions/ipaddress.h"

#ifndef _WIN32
#include <arpa/inet.h>
#endif

namespace Sparrow {

using namespace IvFunctions;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketUtil
//////////////////////////////////////////////////////////////////////////////////////////////////////

my_socket SocketUtil::stopSocket_ = INVALID_SOCKET;
SocketAddress SocketUtil::stopSocketAddress_;
bool SocketUtil::v6_ = false;

// STATIC
void SocketUtil::initialize() _THROW_(SparrowException) {
#ifdef _WIN32
	// Initialize Winsock 2.2.
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	// Checks whether IPv6 is supported by trying to create a dummy IPv6 socket.
	my_socket socketId = socket(AF_INET6, SOCK_STREAM, IPPROTO_IPV6);
	if (socketId != INVALID_SOCKET) {
		closesocket(socketId);
		v6_ = true;
	}

	// Create the stop socket.
	try {
		stopSocket_ = SocketUtil::create(SOCK_DGRAM, SocketUtil::getAddress("127.0.0.1", 0));
		stopSocketAddress_ = SocketUtil::getAddress(stopSocket_);
	} catch(const SparrowException&) {
		// Ignore error: without stop socket, shutdown will be slower - not a big deal.
	}
}

// Creates and binds a socket of the given type to the given address.
// STATIC
my_socket SocketUtil::create(int type, const SocketAddress& socketAddress) _THROW_(SparrowException) {
	// Create socket.
	my_socket socketId = socket(socketAddress.isV6() ? AF_INET6 : AF_INET, type, 0);
	if (socketId == INVALID_SOCKET) {
		throw SparrowException::create(true, "Cannot create socket");
	}

	// Bind socket.
	int dummy = 1;
	setsockopt(socketId, SOL_SOCKET, SO_REUSEADDR, (char*)&dummy, sizeof(dummy));
	if (bind(socketId, socketAddress.getSockAddr(), socketAddress.getSockAddrLength()) != 0) {
		SparrowException e = SparrowException::create(true, "Cannot bind socket to address %s", socketAddress.print().c_str());
		closesocket(socketId);
		throw e;
	}
	return socketId;
}

// Gets binding address from its textual representation.
// STATIC
SocketAddress SocketUtil::getAddress(const char* address, uint32_t port) _THROW_(SparrowException) {
	uint8_t buffer[16] = {0};
	IpAddress ipAddress(buffer, sizeof(buffer));
	const bool isIpAddress = address == 0 ? false : ipAddress.parse(address, static_cast<uint32_t>(strlen(address)));
	if (v6_) {
		struct sockaddr_in6 socketAddress;
		memset(&socketAddress, 0, sizeof(socketAddress));
		socketAddress.sin6_family = AF_INET6;
		socketAddress.sin6_port = htons(static_cast<unsigned short>(port));
		if (address == 0 || strlen(address) == 0) {
			// No address.
			socketAddress.sin6_addr = in6addr_any;
		} else if (isIpAddress) {
			// Address given.
#ifdef _WIN32
			int s = sizeof(socketAddress);
			const bool ok = WSAStringToAddress(const_cast<LPSTR>(address), AF_INET6, 0, reinterpret_cast<struct sockaddr*>(&socketAddress), &s) == 0;
#else
			const bool ok = inet_pton(AF_INET6, address, &socketAddress.sin6_addr) == 1;
#endif
			if (!ok) {
				throw SparrowException::create(true, "Cannot convert address \"%s\" to IPv6 internal format", address);
			}
		} else {
			// Address given as a name: try DNS resolution.
			char buffer[16];
			snprintf(buffer, sizeof(buffer), "%u", port);
			struct addrinfo* result;
			struct addrinfo hints;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_INET6;
			const int code = getaddrinfo(address, buffer, &hints, &result);
			if (code != 0) {
				throw SparrowException::create(false, "Cannot get address info for \"%s\", port %u (%s)",
					address, port, gai_strerror(code));
			}
			memcpy(&socketAddress, result->ai_addr, result->ai_addrlen);
			freeaddrinfo(result);
		}
		return SocketAddress(socketAddress);
	} else {
		struct sockaddr_in socketAddress;
		memset(&socketAddress, 0, sizeof(socketAddress));
		socketAddress.sin_family = AF_INET;
		socketAddress.sin_port = htons(static_cast<unsigned short>(port));
		if (address == 0 || strlen(address) == 0) {
			// No address.
			socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
		} else if (isIpAddress) {
			// Address given.
			ulong addr = inet_addr(address);
			if (addr == static_cast<ulong>(INADDR_NONE)) {
				addr = htonl(INADDR_ANY);
			}
			socketAddress.sin_addr.s_addr = addr;
		} else {
			// Address given as a name: try DNS resolution.
			char buffer[16];
			snprintf(buffer, sizeof(buffer), "%u", port);
			struct addrinfo* result = 0;
			struct addrinfo hints;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_INET;
			const int code = getaddrinfo(address, buffer, &hints, &result);
			if (code != 0) {
				throw SparrowException::create(false, "Cannot get address info for \"%s\", port %u (%s)",
					address, port, gai_strerror(code));
			}
			memcpy(&socketAddress, result->ai_addr, result->ai_addrlen);
			freeaddrinfo(result);
		}
		return SocketAddress(socketAddress);
	}
}

// Gets the address the given socket is bound to.
// STATIC
SocketAddress SocketUtil::getAddress(my_socket socket) _THROW_(SparrowException) {
	struct sockaddr_in6 socketAddress;
	socklen_t length = sizeof(socketAddress);
	if (getsockname(socket, reinterpret_cast<sockaddr*>(&socketAddress), &length) != 0) {
		throw SparrowException::create(true, "Cannot get bind address of socket");
	}
	if (length == sizeof(socketAddress)) {
		return SocketAddress(socketAddress);
	} else {
		return SocketAddress(*reinterpret_cast<sockaddr_in*>(&socketAddress));
	}
}

// STATIC
bool SocketUtil::notifyStopSocket() {
	if (stopSocket_ == INVALID_SOCKET) {
		return false;
	}
	const char* message = "stop";
	int sent = sendto(stopSocket_, message, static_cast<int>(strlen(message)), 0,
		stopSocketAddress_.getSockAddr(), stopSocketAddress_.getSockAddrLength());
	if (sent < 0) {
		try {
			throw SparrowException::create(true, "Cannot notify stop socket");
		} catch(const SparrowException& e) {
			e.toLog();
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketAddress
//////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace IvFunctions;

// Prints an IP address and port number.
Str SocketAddress::print() const {
	char buffer[128];
	char* s = buffer;
	unsigned int port;
	if (v6_) {
		port = static_cast<unsigned int>(ntohs(raw_.v6_.sin6_port));
		*s++ = '[';
		s += IpAddress(reinterpret_cast<const uint8_t*>(&raw_.v6_.sin6_addr), 16).print(s);
		*s++ = ']';
	} else {
		port = static_cast<unsigned int>(ntohs(raw_.v4_.sin_port));
		s += IpAddress(reinterpret_cast<const uint8_t*>(&raw_.v4_.sin_addr), 4).print(s);
	}
	sprintf(s, ":%u", port);
	return Str(buffer, static_cast<int>(strlen(buffer)));
}

}

