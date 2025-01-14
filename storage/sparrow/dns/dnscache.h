/*
	DNS cache.
*/

#ifndef _dns_cache_h_
#define _dns_cache_h_

#include "../engine/types.h"
#include "../engine/list.h"
#include "../handler/plugin.h"	// For SparrowStatus.

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsCacheEntry
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* IP address and host name.
*/

class DnsCacheEntry : public SYSidlink<DnsCacheEntry> {
private:

	// Identifier (of the list of DNS servers).
	int id_;

	// IP address.
	uint8_t address_[16];

	// Name.
	Str name_;

	// Is this an IPv6 address?
	uint32_t v6_:1;

	// Is this entry pending?
	// (The name has not yet been received from the DNS server.)
	uint32_t pending_:1;

	// Is this entry delayed?
	// (The request could not be sent because there was no available request id.)
	uint32_t delayed_:1;

	// If this entry is pending, how many times the request has been sent.
	uint32_t sent_:5;

	// If this entry is pending, on which DNS server was the request sent. This attribute
	// enables cycling through all the DNS servers defined for the entry id.
	uint32_t serverId_:8;

	// If this entry is pending, this gives the request id.
	uint16_t requestId_:16;

	// Timestamp: timeout if this entry is pending, or expiration if this entry is in the cache.
	//	Used in several ways: 
	//	 1) when the request has been sent, timestamp_ records the time at which it was sent. Used for requests timeout.
	//	 2) if a response has been received and correctly decoded, timestamp_ is the TTL as returned by the DNS server (time during which this name resolution will remain valid, probably several years). 
	//	 3) if the DNS resolution failed, timestamp_ is the expiration time: the time during which the entry stays in the cache. 
	//		Once it has been purged out, if this IP comes up again, a new DnsCacheEntry will be created and go through the whole cycle again.
	//		So, in other words, timestamp_ represents the delay to wait before trying again to resolve that IP.
	uint64_t timestamp_;

private:

	DnsCacheEntry(const DnsCacheEntry& right);
	DnsCacheEntry& operator = (const DnsCacheEntry& right);

public:

	DnsCacheEntry(const int id, const uint8_t* address, const uint32_t length)
		: id_(id), v6_(length == 16), pending_(false), delayed_(false),
		sent_(0), serverId_(0), requestId_(0), timestamp_(0) {
		assert(length <= sizeof(address_));
		memcpy(address_, address, length);
	}

	~DnsCacheEntry() {
	}

	void resolve(const uint64_t now, const char* name, const int length, const uint32_t ttl);

	void resolve(const uint64_t now, const uint32_t ttl);

	int getId() const {
		return id_;
	}

	bool isPending() const {
		return pending_;
	}

	void pending(const bool flag) {
		pending_ = flag;
	}

	bool isDelayed() const {
		return delayed_;
	}

	void delayed(const bool flag) {
		delayed_ = flag;
	}

	uint32_t getSent() const {
		return sent_;
	}

	void setSent(const uint32_t sent) {
		sent_ = sent;
	}

	uint32_t getServerId() const {
		return serverId_;
	}

	void setServerId(const uint32_t serverId) {
		serverId_ = serverId;
	}

	uint32_t getRequestId() const {
		return requestId_;
	}

	void setRequestId(const uint32_t requestId) {
		requestId_ = requestId;
	}

	void setTimestamp(const uint64_t timestamp) {
		timestamp_ = timestamp;
	}

	bool hasExpired(const uint64_t now) const {
		return now > timestamp_;
	}

	bool hasTimedOut(const uint64_t mnow, const uint64_t timeout) const {
		return mnow >= timestamp_ + timeout;
	}

	const Str& getName() const {
		return name_;
	}

	Str& getName() {
		return name_;
	}

	const uint8_t* getAddress() const {
		return address_;
	}

	bool isV6() const {
		return v6_;
	}

	bool operator == (const DnsCacheEntry& right) const {
		return id_ == right.id_ && v6_ == right.v6_ && memcmp(address_, right.address_, v6_ ? 16 : 4) == 0;
	}

	uint32_t hash() const {
		uint32_t h = 1;
		uint32_t i = v6_ ? 16 : 4;
		while (i-- > 0) {
			h = 31 * h + address_[i];
		}
		h = 31 * h + id_;
		return h;
	}

	uint32_t getSize() const {
		return sizeof(*this) + name_.length();
	}
};

typedef SYSidlist<DnsCacheEntry> DnsCacheEntries;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsCache
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* The SYSpHash<DnsCacheEntry> contains all cache entries. 
*/
class DnsConfiguration;
class DnsCache : private SYSpHash<DnsCacheEntry> {
private:

	// Entries waiting for a name resolution. Double linked list of pointers to the cache entries waiting for a reply from a DNS server 
	DnsCacheEntries pending_;

	// Cache LRU for eviction. Cache entries at the beginning have not been accessed for a long time and therefore are candidates for the purge. 
	//	Each time a cache entry is used, it's put back to the end of the list. New cache entries are also put at the end. 
	//	Entries in this list cannot also be in the pending_ list: pending requests cannot be purged out.
	DnsCacheEntries lru_;

private:

	void purge(DnsCacheEntries& entries);

public:

	DnsCache();

	~DnsCache();

	void processTimeout(DnsConfiguration* configuration, const uint64_t now, const uint64_t mnow, const uint64_t timeout);

	void processPurge(const uint64_t now, DnsCacheEntries& obsoleteEntries);

	void processControlSize(DnsCacheEntries& entries);

	void putBack(DnsCacheEntry* entry);

	void insert(DnsCacheEntry* entry);

	DnsCacheEntry* remove(const DnsCacheEntry* entry);

	DnsCacheEntry* doResolve(DnsConfiguration* configuration, const uint64_t now, const uint64_t mnow, const int id,
		const uint8_t* address, const uint32_t length);

	bool doRequest(DnsConfiguration* configuration, const uint64_t now, const uint64_t mnow,
		DnsCacheEntry* entry, const bool uponError);

	void doRetry(DnsConfiguration* configuration, const uint64_t now, const uint64_t mnow,
		DnsCacheEntry* entry);

	void clear();

	static void changeSize(const int64_t delta) {
		if (delta != 0) {
			Atomic::add64(&SparrowStatus::get().dnsCacheSize_, delta);
		}
	}
};

}

#endif /* #ifndef _dns_cache_h_ */
