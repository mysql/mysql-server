/*
	DNS network utilities.
	See http://www.tcpipguide.com/free/t_DNSMessageHeaderandQuestionSectionFormat.htm
	for more information about DNS message format.
*/

#include "dnsnet.h"
#include "dnscache.h"
#include "../functions/ipaddress.h"

#define MYSQL_SERVER 1
//#include <sql_priv.h>
#include "sql/query_options.h"		// For mysqld options.
#include "../engine/log.h"
#include <mysys_err.h>
#include <my_dir.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace Sparrow {

using namespace IvFunctions;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsNet
//////////////////////////////////////////////////////////////////////////////////////////////////////

const char* DnsNet::nibbles_ = "0123456789abcdef";

// To write IPv4 address CNAME without sprintf.
const char* DnsNet::format_ = ""
"0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111122222222222222222222222222222222222222222222222222222222"
"0000000000111111111122222222223333333333444444444455555555556666666666777777777788888888889999999999000000000011111111112222222222333333333344444444445555555555666666666677777777778888888888999999999900000000001111111111222222222233333333334444444444555555"
"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345";


const char DnsNet::ipV4Suffix_[] = { '\x07', 'i', 'n', '-', 'a', 'd', 'd', 'r', '\x04', 'a', 'r', 'p', 'a', '\x00' };

const char DnsNet::ipV6Suffix_[] = { '\x03', 'i', 'p', '6', '\x04', 'a', 'r', 'p', 'a', '\x00' };

// STATIC
uint32_t DnsNet::forgeQuery(uint8_t* buffer, uint32_t length, uint16_t id, bool recurse,
	const uint8_t* address, bool v6) {
	// Check length.
	// - 12 bytes for the header.
	// - 4 + 2 + 28 bytes max for the IPv4 PTR record.
	// - 4 + 2 + 72 bytes for the IPv6 PTR record.
	// -> Total max length is 90 bytes.
	if (length < 90) {
		return 0;
	}
	uint8_t* saved = buffer;

	// Write header info.

	// Identifier.
	*buffer++ = static_cast<uint8_t>(id >> 8);
	*buffer++ = static_cast<uint8_t>(id & 0xff);

	// Flags.
	uint16_t flags = recurse ? 0x100 : 0;
	*buffer++ = static_cast<uint8_t>(flags >> 8);
	*buffer++ = static_cast<uint8_t>(flags & 0xff);

	// Number of questions.
	*buffer++ = 0;
	*buffer++ = 1;

	// Number of answer RRs, authority RRs and additional RRs are all null.
	memset(buffer, 0, 6);
	buffer += 6;

	// CNAME.
	buffer += encodeCName(buffer, address, v6);
	
	// PTR type.
	*buffer++ = 0;
	*buffer++ = static_cast<uint8_t>(0xc);

	// IN class.
	*buffer++ = 0;
	*buffer++ = 1;
	return static_cast<uint32_t>(buffer - saved);
}

// STATIC
bool DnsNet::decodeResponse(uint64_t now, const uint8_t* buffer, uint32_t length, DnsCacheEntry& entry) {
	uint8_t tmp[256];
	uint8_t tmp2[256];
	const uint8_t* ref = buffer;

	// This has been checked before.
	assert(length > 12);
	const uint32_t maxOffset = length - 1;

	// Skip identifier.
	buffer += 2;

	// Check flags: need a response without error.
	uint16_t flags = *buffer++ << 8;
	flags |= *buffer++;
	if ((flags & 0x800f) != 0x8000) {
		switch (flags & 0xf) {
			case 1: Atomic::inc64(&SparrowStatus::get().dnsErrorsFormat_); break;
			case 2: Atomic::inc64(&SparrowStatus::get().dnsErrorsFailure_); break;
			case 3: Atomic::inc64(&SparrowStatus::get().dnsErrorsName_); break;
			case 4: Atomic::inc64(&SparrowStatus::get().dnsErrorsNotImplemented_); break;
			case 5: Atomic::inc64(&SparrowStatus::get().dnsErrorsRefused_); break;
			case 6: Atomic::inc64(&SparrowStatus::get().dnsErrorsYXDomain_); break;
			case 7: Atomic::inc64(&SparrowStatus::get().dnsErrorsYXRRSet_); break;
			case 8: Atomic::inc64(&SparrowStatus::get().dnsErrorsNXRRSet_); break;
			case 9: Atomic::inc64(&SparrowStatus::get().dnsErrorsNotAuth_); break;
			case 10: Atomic::inc64(&SparrowStatus::get().dnsErrorsNotZone_); break;
			default: Atomic::inc64(&SparrowStatus::get().dnsErrorsUnknown_); break;
		}
		return false;
	}

	// Need one question and at least one answer.
	uint16_t questions = *buffer++ << 8;
	questions |= *buffer++;
	if (questions != 1) {
		Atomic::inc64(&SparrowStatus::get().dnsErrorsDecoding_);
		return false;
	}
	uint16_t answers = *buffer++ << 8;
	answers |= *buffer++;
	if (answers == 0) {
		Atomic::inc64(&SparrowStatus::get().dnsNoAnswer_);
		return false;
	}

	// Skip Authority RRs and additional RRs.
	buffer += 4;
	length -= 12;

	// Skip query CNAME.
	uint32_t nameLength = decodeCName(ref, static_cast<uint32_t>(buffer - ref), maxOffset, tmp, sizeof(tmp) - 1);
	if (nameLength == 0) {
		Atomic::inc64(&SparrowStatus::get().dnsErrorsDecoding_);
		return false;
	}
	buffer += nameLength;
	assert(length > nameLength);
	length -= nameLength;

	// Check query is a PTR record with IN class.
	if (length < 4) {
		Atomic::inc64(&SparrowStatus::get().dnsErrorsDecoding_);
		return false;
	}
	length -= 4;
	uint16_t type = *buffer++ << 8;
	type |= *buffer++;
	uint16_t klass = *buffer++ << 8;
	klass |= *buffer++;
	if (type != 0x0c || klass != 1) {
		Atomic::inc64(&SparrowStatus::get().dnsErrorsDecoding_);
		return false;
	}

	// Get first PTR record and check address.
	uint8_t address[80];
	const uint32_t addressLength = encodeCName(address, entry.getAddress(), entry.isV6());
	assert(addressLength <= sizeof(address));
	const uint32_t addressLength2 = decodeCName(address, 0, addressLength - 1, tmp2, sizeof(tmp2) - 1);
	if (addressLength2 == 0) {
		Atomic::inc64(&SparrowStatus::get().dnsErrorsDecoding_);
		return false;
	}
	nameLength = decodeCName(ref, static_cast<uint32_t>(buffer - ref), maxOffset, tmp, sizeof(tmp) - 1);
	if (nameLength == 0) {
		Atomic::inc64(&SparrowStatus::get().dnsErrorsDecoding_);
		return false;
	}
	buffer += nameLength;
	assert(length > nameLength);
	length -= nameLength;
	if (memcmp(tmp, tmp2, addressLength2) != 0) {
		// Address mismatch: this can occur in case of a late response
		// for a request id reused meanwhile.
		Atomic::inc64(&SparrowStatus::get().dnsDiscardedResponses4_);
		return false;
	}

	// Check answer is a PTR record with IN class.
	if (length < 4) {
		Atomic::inc64(&SparrowStatus::get().dnsErrorsDecoding_);
		return false;
	}
	length -= 4;
	type = *buffer++ << 8;
	type |= *buffer++;
	klass = *buffer++ << 8;
	klass |= *buffer++;
	if (type != 0x0c || klass != 1) {
		Atomic::inc64(&SparrowStatus::get().dnsErrorsDecoding_);
		return false;
	}

	// Get TTL.
	if (length < 4) {
		Atomic::inc64(&SparrowStatus::get().dnsErrorsDecoding_);
		return false;
	}
	length -= 4;
	uint32_t ttl = *buffer++ << 24;
	ttl |= *buffer++ << 16;
	ttl |= *buffer++ << 8;
	ttl |= *buffer++;

	// Skip data length.
	if (length < 2) {
		Atomic::inc64(&SparrowStatus::get().dnsErrorsDecoding_);
		return false;
	}
	buffer += 2;
	length -= 2;

	// Get name.
	nameLength = decodeCName(ref, static_cast<uint32_t>(buffer - ref), maxOffset, tmp, sizeof(tmp) - 1);
	if (nameLength == 0) {
		Atomic::inc64(&SparrowStatus::get().dnsErrorsDecoding_);
		return false;
	}
	const char* name = reinterpret_cast<const char*>(tmp);
	entry.resolve(now, name, static_cast<int>(strlen(name)), ttl);
	return true;
}

// STATIC
uint32_t DnsNet::decodeCName(const uint8_t* ref, uint32_t sourceOffset, const uint32_t maxSourceOffset, uint8_t* dest, const uint32_t maxDestOffset) {
	uint32_t initial = sourceOffset;
	uint32_t saved = 0;
	uint32_t destOffset = 0;
	uint32_t loop_counter = 0;		// debug - counter to avoid infinite loop
	uint32_t bckwd_ptr = 0;
	for (;;) {
		if (sourceOffset > maxSourceOffset) {
			return 0;
		}
		if ( ++loop_counter > maxSourceOffset ) {
			spw_print_information("[sparrow] DNS packet may be wrong: looped %d times, packet size %d, bckwd_ptr %d, sourceOffset %d, saved %d, initial %d", 
				loop_counter, maxSourceOffset, bckwd_ptr, sourceOffset, saved, initial);
			return 0;
		}
		const uint8_t b = ref[sourceOffset++];
		if (b == 0) {
			if (destOffset > maxDestOffset) {
				return 0;
			}
			dest[destOffset++] = 0;
			return (saved == 0 ? sourceOffset : saved) - initial;
		}
		const int check = b & 0xc0;
		if (check == 0) {
			const int length = (b & ~0xc0);
			if (destOffset != 0) {
				if (destOffset > maxDestOffset) {
					return 0;
				}
				dest[destOffset++] = '.';
			}
			for (int i = 0; i < length; ++i) {
				if (sourceOffset > maxSourceOffset || destOffset > maxDestOffset) {
					return 0;
				}
				dest[destOffset++] = ref[sourceOffset++];
			}
		} else if (check == 0xc0) {	// Pointer.
			if (sourceOffset + 1 > maxSourceOffset) {
				return 0;
			}
			const uint32_t newOffset = ref[sourceOffset++] + ((b & ~0xc0) << 8);
			if ( newOffset < sourceOffset ) {
				bckwd_ptr++;
			}
			if ( saved != 0 ) {
				spw_print_information("[sparrow] DNS packet may be wrong: at %d pointer to offset %d, but we were already dereferencing a pointer located at %d", 
					sourceOffset, newOffset, saved);
			}
			saved = sourceOffset;
			sourceOffset = newOffset;
		} else {
			return 0;
		}
	}
}

// The buffer must have room for 80 bytes (max encoded length of an IPv6 address).
// STATIC
uint32_t DnsNet::encodeCName(uint8_t* buffer, const uint8_t* address, const bool v6) {
	const uint8_t* ref = buffer;
	if (v6) {
		for (int i = 15; i >= 0; i--) {
			uint8_t b = address[i];
			*buffer++ = static_cast<uint8_t>(1);
			*buffer++ = nibbles_[b & 0xf];
			*buffer++ = static_cast<uint8_t>(1);
			*buffer++ = nibbles_[b >> 4];
		}
		memcpy(buffer, ipV6Suffix_, 10);
		buffer += 10;
	} else {
		int a = address[3];
		uint8_t x = format_[a];
		uint8_t y = format_[a + 256];
		uint8_t z = format_[a + 512];
		if (x != static_cast<uint8_t>('0')) {
			*buffer++ = static_cast<uint8_t>(3);
			*buffer++ = x;
			*buffer++ = y;
			*buffer++ = z;
		} else if (y !=static_cast<uint8_t>('0')) {
			*buffer++ = static_cast<uint8_t>(2);
			*buffer++ = y;
			*buffer++ = z;
		} else {
			*buffer++ = static_cast<uint8_t>(1);
			*buffer++ = z;
		}
		a = address[2];
		x = format_[a];
		y = format_[a + 256];
		z = format_[a + 512];
		if (x != static_cast<uint8_t>('0')) {
			*buffer++ = static_cast<uint8_t>(3);
			*buffer++ = x;
			*buffer++ = y;
			*buffer++ = z;
		} else if (y != static_cast<uint8_t>('0')) {
			*buffer++ = static_cast<uint8_t>(2);
			*buffer++ = y;
			*buffer++ = z;
		} else {
			*buffer++ = static_cast<uint8_t>(1);
			*buffer++ = z;
		}
		a = address[1];
		x = format_[a];
		y = format_[a + 256];
		z = format_[a + 512];
		if (x != static_cast<uint8_t>('0')) {
			*buffer++ = static_cast<uint8_t>(3);
			*buffer++ = x;
			*buffer++ = y;
			*buffer++ = z;
		} else if (y != static_cast<uint8_t>('0')) {
			*buffer++ = static_cast<uint8_t>(2);
			*buffer++ = y;
			*buffer++ = z;
		} else {
			*buffer++ = static_cast<uint8_t>(1);
			*buffer++ = z;
		}
		a = address[0];
		x = format_[a];
		y = format_[a + 256];
		z = format_[a + 512];
		if (x != static_cast<uint8_t>('0')) {
			*buffer++ = static_cast<uint8_t>(3);
			*buffer++ = x;
			*buffer++ = y;
			*buffer++ = z;
		} else if (y != static_cast<uint8_t>('0')) {
			*buffer++ = static_cast<uint8_t>(2);
			*buffer++ = y;
			*buffer++ = z;
		} else {
			*buffer++ = static_cast<uint8_t>(1);
			*buffer++ = z;
		}
		memcpy(buffer, ipV4Suffix_, 14);
		buffer += 14;
	}
	return static_cast<uint32_t>(buffer - ref);
}


}
