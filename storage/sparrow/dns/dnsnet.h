/*
	DNS network utilities.
*/

#ifndef _dns_net_h_
#define _dns_net_h_

#include "../handler/plugin.h"	// For configuration parameters.
#include "../engine/types.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsNet
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsCacheEntry;
class DnsNet {
private:

	static const char* nibbles_;
	static const char* format_;
	static const char ipV4Suffix_[];
	static const char ipV6Suffix_[];

private:

	static uint32_t decodeCName(const uint8_t* ref, uint32_t sourceOffset, const uint32_t maxSourceOffset, uint8_t* dest, const uint32_t maxDestOffset);
	static uint32_t encodeCName(uint8_t* buffer, const uint8_t* address, const bool v6);

public:

	static uint32_t forgeQuery(uint8_t* buffer, uint32_t length, uint16_t id, bool recurse,
		const uint8_t* address, bool v6);

	static bool decodeResponse(uint64_t now, const uint8_t* buffer, uint32_t length, DnsCacheEntry& entry);
};

}

#endif /* #ifndef _dns_net_h_ */
