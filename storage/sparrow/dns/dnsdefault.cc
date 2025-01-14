/*
	Default DNS server.
*/

#include "dnsdefault.h"

#ifdef _WIN32
#include <windows.h>
#include <windns.h>
#elif defined(__linux__) || defined(__MACH__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <strings.h>
#elif defined(__SunOS)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <netdb.h>
#include <strings.h>
#endif
#include <stdio.h>
#include <string.h>

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsDefault
//////////////////////////////////////////////////////////////////////////////////////////////////////

int DnsDefault::n_ = 0;
char** DnsDefault::servers_ = DnsDefault::initialize();

// STATIC
char** DnsDefault::initialize() {
#ifdef _WIN32				// Windows.
	BYTE buffer[16384];
	memset(buffer, 0, sizeof(buffer));
	DWORD size = sizeof(buffer);
	DNS_STATUS status = DnsQueryConfig(DnsConfigDnsServerList, 0, 0, 0, buffer, &size);
	IP4_ARRAY& response = *(IP4_ARRAY*)(buffer);
	if (status == 0 && response.AddrCount > 0) {
		n_ = response.AddrCount;
		servers_ = new char*[n_ ];
		for (int i = 0; i < n_; ++i) {
			IP4_ADDRESS address = response.AddrArray[i];
			servers_[i] = new char[16];
			_snprintf(servers_[i], 16, "%u.%u.%u.%u", address & 0xff, (address >> 8) & 0xff,
				(address >> 16) & 0xff, (address >> 24) & 0xff);
		}
	}
	// TODO IPv6 - http://groups.google.com/group/microsoft.public.platformsdk.networking.ipv6/browse_frm/thread/0ce031e8b4ee2fd9?hl=en#
#elif defined(__linux__) || defined(__SunOS) || defined(__MACH__)	// Linux, Solaris and MacOS.
	struct __res_state* resState = reinterpret_cast<struct __res_state*>(new unsigned char[sizeof(struct __res_state)]);
	memset(resState, 0, sizeof(struct __res_state));
	if (res_ninit(resState) == 0 && resState->nscount > 0) {
		n_ = resState->nscount;
		servers_ = new char*[n_ ];
		for (int i = 0; i < n_; ++i) {
			struct in_addr address = resState->nsaddr_list[i].sin_addr;
			servers_[i] = new char[INET6_ADDRSTRLEN];

			// Try IPv6 if IPv4 fails.
			if (inet_ntop(AF_INET, &address, servers_[i], INET6_ADDRSTRLEN) == 0) {
				inet_ntop(AF_INET6, &address, servers_[i], INET6_ADDRSTRLEN);
			}
		}
	}
#else
#error Missing DnsDefault for this platform.
#endif
	return n_ == 0 ? 0 : servers_;
}

}
