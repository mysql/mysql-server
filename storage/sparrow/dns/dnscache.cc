/*
	DNS cache.
*/

#include "../handler/plugin.h"	// For configuration parameters.
#include "dnscache.h"
#include "dns.h"
#include "dnsnet.h"
#include "../functions/ipaddress.h"

namespace Sparrow {

using namespace IvFunctions;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsCache
//////////////////////////////////////////////////////////////////////////////////////////////////////

DnsCache::DnsCache() : SYSpHash<DnsCacheEntry>(16384) {
	Atomic::inc32(&SparrowStatus::get().dnsCaches_);
}

DnsCache::~DnsCache() {
	clear();
	Atomic::dec32(&SparrowStatus::get().dnsCaches_);
}

void DnsCache::clear() {
	int64_t size = getSize();
	SYSpHashIterator<DnsCacheEntry> iterator(*this);
	while (++iterator) {
		size += iterator.key()->getSize();
	}
	clearAndDestroy();
	changeSize(-size);
}

void DnsCache::insert(DnsCacheEntry* entry) {
	const int64_t before = getSize();
	SYSpHash<DnsCacheEntry>::insert(entry);
	changeSize(getSize() + entry->getSize() - before);
}

DnsCacheEntry* DnsCache::remove(const DnsCacheEntry* entry) {
	const int64_t before = getSize();
	DnsCacheEntry* removedEntry = SYSpHash<DnsCacheEntry>::remove(entry);
	changeSize(getSize() - before - entry->getSize());
	return removedEntry;
}

// Remove from the DnsCacheEntry from the pending list, either because the DNS resolution succeeded or because it failed (decoding error or a timeout) 
//	and put the DnsCacheEntry back in the main cache.
void DnsCache::putBack(DnsCacheEntry* entry) {
	if (entry->isPending()) {
		pending_.remove(entry);
		entry->pending(false);
		entry->setSent(0);
		Dns::clearRequest(entry);
		Atomic::dec64(&SparrowStatus::get().dnsCachePendingEntries_);
		lru_.append(entry);		// Put back in lru_ as it had been removed when it became pending.
	}
}

DnsCacheEntry* DnsCache::doResolve(DnsConfiguration* configuration, const uint64_t now, const uint64_t mnow, const int id,
	const uint8_t* address, const uint32_t length) {
	if (length != 4 && length != 16) {
		return 0;
	}
	Atomic::inc64(&SparrowStatus::get().dnsCacheAcquires_);
	DnsCacheEntry key(id, address, length);
	DnsCacheEntry* found = find(&key);
	if (found != 0) {
		Atomic::inc64(&SparrowStatus::get().dnsCacheHits_);
	}
	if (found == 0) {
		DnsCacheEntry* entry = new DnsCacheEntry(id, address, length);
		insert(entry);
		Atomic::inc64(&SparrowStatus::get().dnsCacheEntries_);
		lru_.append(entry);
		if (!doRequest(configuration, now, mnow, entry, false)) {
			found = entry;
		}
	} else if (found->isPending()) {
		found = 0;
	} else if (found->hasExpired(now)) {
		if (doRequest(configuration, now, mnow, found, false)) {
			found = 0;
		}
	} else {
		// Valid entry found: put it to the LRU top.
		lru_.remove(found);
		lru_.append(found);
	}
	return found;
}

// Send the DNS resolution request. Returns true if the given entry is pending.
bool DnsCache::doRequest(DnsConfiguration* configuration, const uint64_t now, const uint64_t mnow,
	DnsCacheEntry* entry, const bool uponError) {
	// If it's a re-try because the previous response could not be decoded correctly
	//	and the request cannot be sent again (for any reason), then remove this request from the pending list.
	if (uponError) {
		if (!configuration->sendRequest(now, mnow, entry, true)) {
			putBack(entry);
			return false;
		}
	} else {
		assert(!entry->isPending());
		if (configuration->sendRequest(now, mnow, entry, false)) {
			lru_.remove(entry);		// Remove from lru_ as a DnsCacheEntry cannot be in both lists at once
			entry->pending(true);
			pending_.append(entry);
			Atomic::inc64(&SparrowStatus::get().dnsCachePendingEntries_);
		} else {
			return false;
		}
	}
	return true;
}

void DnsCache::doRetry(DnsConfiguration* configuration, const uint64_t now, const uint64_t mnow,
	DnsCacheEntry* entry) {
	doRequest(configuration, now, mnow, entry, true);
}

// Called regularly to handle timeouts.
void DnsCache::processTimeout(DnsConfiguration* configuration, const uint64_t now, const uint64_t mnow, const uint64_t timeout) {
	// Check if we can retry or terminate pending requests, or send delayed requests.
	DnsCacheEntry* entry = pending_.first();
	while (entry != 0) {
		assert(entry->isPending());
		DnsCacheEntry* next = entry->next_;
		if (entry->hasTimedOut(mnow, timeout) && !configuration->sendRequest(now, mnow, entry, false)) {
			putBack(entry);
		}
		entry = next;
	}
}

// Called regularly to purge obsolete entries.
void DnsCache::processPurge(const uint64_t now, DnsCacheEntries& obsoleteEntries) {
	DnsCacheEntry* entry = lru_.first();
	while (entry != 0) {
		assert(!entry->isPending());
		DnsCacheEntry* next = entry->next_;
		if (entry->hasExpired(now)) {
			lru_.remove(entry);
			remove(entry);
			obsoleteEntries.append(entry);
		} 
		entry = next;
	}
}

// Called regularly to control DNS cache size.
void DnsCache::processControlSize(DnsCacheEntries& entries) {
	if (sparrow_max_dns_cache_size == 0) {
		// No size limit.
		return;
	}
	const uint64_t total = Atomic::get64(&SparrowStatus::get().dnsCacheSize_);
	if (total == 0) {
		return;
	}
	const double ratio = static_cast<double>(sparrow_max_dns_cache_size) / total;
	const uint32_t maxEntries = static_cast<uint32_t>(ratio * this->entries());
	while (lru_.entries() > maxEntries) {
		DnsCacheEntry* entry = lru_.removeFirst();
		remove(entry);
		entries.append(entry);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsCacheEntry
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Resolve entry: set name and TTL.
void DnsCacheEntry::resolve(const uint64_t now, const char* name, const int length, const uint32_t ttl) {
	int64_t delta = -static_cast<int64_t>(getSize());
	name_ = Str(name, length);
	DnsCache::changeSize(delta + getSize());
	timestamp_ = now + ttl;
}

// Name not found, or no response from DNS server: the name is the address as a string.
void DnsCacheEntry::resolve(const uint64_t now, const uint32_t ttl) {
	char buffer[128];
	IpAddress address(address_, v6_ ? 16 : 4);
	const uint32_t length = address.print(buffer);
	resolve(now, buffer, static_cast<int>(length), ttl);	// TTL is one hour.
}

}
