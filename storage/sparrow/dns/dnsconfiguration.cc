/*
	DNS configuration and API.
*/

#include "dnsconfiguration.h"
#include "dnsnet.h"
#include "dns.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsConfiguration
//////////////////////////////////////////////////////////////////////////////////////////////////////

// This is the DNS configuration for one or several master files. For each listed DNS identifier, it gives a set
// of DNS servers.

SYSpHash<DnsConfiguration> DnsConfiguration::hash_(16);
Lock DnsConfiguration::hashLock_(true, "DnsConfiguration::hashLock_");
const DnsConfigId DnsConfigId::DEFAULT(-1);

DnsConfiguration::DnsConfiguration() : SYShash<DnsConfigId>(256), RefCounted(), lock_(false, DnsConfiguration::getName().c_str()), cache_(0), masterFiles_(0) {
}

DnsConfiguration::DnsConfiguration(const DnsConfiguration& right)
	: SYShash<DnsConfigId>(256), RefCounted(), lock_(false, DnsConfiguration::getName().c_str()), cache_(0), masterFiles_(0) {
	SYShash<DnsConfigId>::operator = (right);
}

// STATIC
Str DnsConfiguration::getName() {
	char tmp[128];
	static volatile uint32_t counter = 0;
	snprintf(tmp, sizeof(tmp), "DnsConfiguration(%u)::lock_", Atomic::inc32(&counter));
	return Str(tmp);
}

DnsConfiguration::~DnsConfiguration() {
	if (cache_ != 0) {
		delete cache_;
		Guard guard(Dns::getLock());
		Dns::incSerial();
	}
}

// Start DNS server connections.
void DnsConfiguration::start() _THROW_(SparrowException) {
	SPARROW_ENTER("DnsConfiguration::start");
	assert(cache_ == 0);
	SYShashIterator<DnsConfigId> iterator(*this);
	SYSslist<DnsConfigId> toRemove;
	while (++iterator) {
		DnsServers& servers = iterator.key().getServers();
		for (uint32_t i = 0; i < servers.length(); ++i) {
			try {
				servers[i].start();
			} catch(const SparrowException& e) {
				e.toLog();
				servers.removeAt(i--);
			}
		}
		if (servers.isEmpty()) {
			toRemove.append(iterator.key());
		}
	}
	while (!toRemove.isEmpty()) {
		remove(toRemove.first());
		toRemove.removeFirst();
	}
	cache_ = new DnsCache();
	tasks_.append(new DnsTimeoutTask(this));
	tasks_.append(new DnsPurgeTask(this));
	tasks_.append(new DnsSizeTask(this));
	for (uint32_t i = 0; i < tasks_.length(); ++i) {
		Scheduler::addTask(tasks_[i]);
	}
	Guard guard(Dns::getLock());
	Dns::incSerial();
}

DnsCacheEntry* DnsConfiguration::doResolve(const uint64_t now, const uint64_t mnow, const int id, const uint8_t* address, const uint32_t length) {
	SPARROW_ENTER("DnsConfiguration::doResolve");
	if (find(DnsConfigId(id)) == 0) {
		return cache_->doResolve(this, now, mnow, -1, address, length);
	} else {
		return cache_->doResolve(this, now, mnow, id, address, length);
	}
}

// Tries to send a DNS request for the given DNS cache entry. The uponError flag tells the method
// if we retry a request following an error in the response.
// Returns true if the request is now pending, false if we had to perform a default resolution.
bool DnsConfiguration::sendRequest(const uint64_t now, const uint64_t mnow, DnsCacheEntry* entry, const bool uponError) {
	SPARROW_ENTER("DnsConfiguration::sendRequest");

	// Actually send or retry the request if:
	// - The entry identifier exists.
	// - And we have not reached the maximum number of retries.
	// - And the uponError flag is false or the request as been sent less times than
	// the number of servers (i.e. there is a chance this error does not occur on another server).
	// Otherwise, use default resolution with a variable TTL.
	DnsConfigId* configId = find(DnsConfigId(entry->getId()));
	if (configId == 0) {
		configId = find(DnsConfigId::DEFAULT);
	}
	const uint32_t sent = entry->getSent();
	if (configId != 0 && sent <= sparrow_dns_retries
		&& (!uponError || sent < configId->getServers().length())) {
		DnsServers& servers = configId->getServers();
		const uint32_t serverId = (sent == 0 ? configId->getCurrent() : entry->getServerId() + 1) % servers.length();
		entry->setServerId(serverId);
		entry->setSent(sent + 1);
		entry->setTimestamp(mnow);
		try {
			if (sent == 1 || entry->isDelayed()) {
				// Set the outstanding request only if this is not a retry.
				Dns::setRequest(this, entry);
				if (entry->isDelayed()) {
					// No request id available: will send later.
					return true;
				}
			}
			uint8_t buffer[1024];
			const uint32_t length = DnsNet::forgeQuery(buffer, sizeof(buffer), entry->getRequestId(),
				true, entry->getAddress(), entry->isV6());
			assert(length <= sizeof(buffer));
			DnsServerConnection& connection = servers[serverId].getConnection();
			connection.send(buffer, length);
			Atomic::inc64(&SparrowStatus::get().dnsRequests_);
			if (sent > 1) {
				Atomic::inc64(&SparrowStatus::get().dnsRetries_);
			}
			return true;
		} catch(const SparrowException&) {
			entry->resolve(now, 3600);
			return false;
		}
	} else {
		if (uponError || configId == 0 || configId->getServers().isEmpty()) {
			// TTL = 1 hour in case of error: it is unlikely we get a good response soon.
			entry->resolve(now, 3600);
		} else {
			// TTL = 1 minute in case of timeout: the DNS server(s) is(are) probably momentarily
			// overloaded, so retry soon.
			entry->resolve(now, 60);
		}
		return false;
	}
}

// Decode the DNS response. If it succeeded, remove the DnsCacheEntry from the pending list and out it back in the main cache.
//	Otherwise, retry sending the DNS resolution request.
void DnsConfiguration::decodeResponse(uint64_t now, uint64_t mnow, const DnsBuffer& buffer, DnsCacheEntry* entry) {
	SPARROW_ENTER("DnsConfiguration::decodeResponse");
	// In some rare situations, the DnsCacheEntry may not be pending anymore: if the first request timed out and a new request was issued.
	//	But the response to the first request finally arrived and was processed, the DnsCacheEntry was removed from the pending list and put back in the main cache.
	if (!entry->isPending()) {
		Atomic::inc64(&SparrowStatus::get().dnsDiscardedResponses3_);
		return;
	}
	if (DnsNet::decodeResponse(now, buffer.getData(), buffer.getLength(), *entry)) {
		cache_->putBack(entry);
	} else {
		// We get an error: retry or terminate.
		cache_->doRetry(this, now, mnow, entry);
	}
}

// Called every time the DNS timeout expires.
void DnsConfiguration::processTimeout(const uint64_t now, const uint64_t mnow, const uint64_t timeout) {
	SPARROW_ENTER("DnsConfiguration::processTimeout");
	cache_->processTimeout(this, now, mnow, timeout);
}

void DnsConfiguration::processPurge(const uint64_t now, DnsCacheEntries& obsoleteEntries) {
	SPARROW_ENTER("DnsConfiguration::processPurge");
	cache_->processPurge(now, obsoleteEntries);
}

void DnsConfiguration::processControlSize(DnsCacheEntries& entries) {
	SPARROW_ENTER("DnsConfiguration::processControlSize");
	cache_->processControlSize(entries);
}

Str DnsConfiguration::print() {
	Str s;
	SYShashIterator<DnsConfigId> iterator(*this);
	while (++iterator) {
		const DnsConfigId& configId = iterator.key();
		char buffer[2048];
		snprintf(buffer, sizeof(buffer), "\n%d: ", configId.getId());
		s += Str(buffer);
		const DnsServers& servers = configId.getServers();
		for (uint32_t i = 0; i < servers.length(); ++i) {
			snprintf(buffer, sizeof(buffer), i == 0 ? "(%s)" : ", (%s)", servers[i].print().c_str());
			s += Str(buffer);
		}
	}
	return s;
}

// STATIC
DnsConfiguration* DnsConfiguration::acquire(const DnsConfiguration& key) {
	SPARROW_ENTER("DnsConfiguration::acquire");
	Guard guard(hashLock_);
	DnsConfiguration* dnsConfiguration = hash_.find(&key);
	if (dnsConfiguration == 0) {
		dnsConfiguration = new DnsConfiguration(key);
		dnsConfiguration->start();
		hash_.insert(dnsConfiguration);
	}
	dnsConfiguration->masterFiles_++;
	return dnsConfiguration;
}

// STATIC
void DnsConfiguration::release(DnsConfiguration* dnsConfiguration) {
	SPARROW_ENTER("DnsConfiguration::release");
	assert(dnsConfiguration->isStarted());
	Guard guard(hashLock_);
	if (--dnsConfiguration->masterFiles_ == 0) {
		DBUG_PRINT("sparrow_dns", ("DNS configuration has zero reference"));
		hash_.remove(dnsConfiguration);
		for (uint32_t i = 0; i < dnsConfiguration->tasks_.length(); ++i) {
			dnsConfiguration->tasks_[i]->stop();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsPurgeTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

void DnsPurgeTask::run(const uint64_t timestamp) _THROW_(SparrowException) {
	DnsCacheEntries obsoleteEntries;
	{
		Guard guard(dnsConfiguration_->getLock());
		dnsConfiguration_->processPurge(std::time(nullptr), obsoleteEntries);
	}
	Atomic::add64(&SparrowStatus::get().dnsCacheEntries_, -static_cast<int64_t>(obsoleteEntries.entries()));
	while (!obsoleteEntries.isEmpty()) {
		DnsCacheEntry* entry = obsoleteEntries.removeFirst();
		delete entry;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsSizeTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

void DnsSizeTask::run(const uint64_t timestamp) _THROW_(SparrowException) {
	DnsCacheEntries entries;
	{
		Guard guard(dnsConfiguration_->getLock());
		dnsConfiguration_->processControlSize(entries);
	}
	Atomic::add64(&SparrowStatus::get().dnsCacheEvictions_, static_cast<int64_t>(entries.entries()));
	Atomic::add64(&SparrowStatus::get().dnsCacheEntries_, -static_cast<int64_t>(entries.entries()));
	while (!entries.isEmpty()) {
		DnsCacheEntry* entry = entries.removeFirst();
		delete entry;
	}
}

}
