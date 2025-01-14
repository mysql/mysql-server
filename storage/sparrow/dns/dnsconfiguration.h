/*
	DNS configuration.
*/

#ifndef _dns_dnsconfiguration_h_
#define _dns_dnsconfiguration_h_

#include "dnsserver.h"
#include "../handler/plugin.h"	// For configuration parameters.

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsConfigId
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* A DNS configuration is a group of DNS servers to use for IP resolution.
*/

class DnsConfigId {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, DnsConfigId& id);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const DnsConfigId& id);

private:

	int id_;						// DNS identifier.
	bool key_;						// Is this configId a search key?
	DnsServers servers_;			// DNS servers.
	mutable RefCounter current_;	// Current DNS server (round robin).

public:

	static const DnsConfigId DEFAULT;

public:

	DnsConfigId() : id_(-1), key_(false) {
	}

	DnsConfigId(int id) : id_(id), key_(true) {
	}

	DnsConfigId(int id, const DnsServers& servers) : id_(id), key_(false), servers_(servers) {
	}

	int getId() const {
		return id_;
	}

	bool operator == (const DnsConfigId& right) const {
		return id_ == right.id_ && (key_ || right.key_ || servers_ == right.servers_);
	}

	const DnsServers& getServers() const {
		return servers_;
	}

	DnsServers& getServers() {
		return servers_;
	}

	uint32_t getCurrent() const {
		return current_++;
	}

	uint32_t hash() const {
		return id_;
	}
};

inline ByteBuffer& operator << (ByteBuffer& buffer, const DnsConfigId& id) {
	buffer << id.id_ << id.servers_;
	return buffer;
}

inline ByteBuffer& operator >> (ByteBuffer& buffer, DnsConfigId& id) {
	buffer >> id.id_ >> id.servers_;

	// Replace "@default" servers by the list of default DNS servers.
	DnsServers extendedServers;
	for (uint32_t i = 0; i < id.servers_.length(); ++i) {
		const DnsServer& server = id.servers_[i];
		if (strcmp(server.getHost(), "@default") == 0) {
			for (int j = 0; j < DnsDefault::getNumber(); ++j) {
				extendedServers.append(DnsServer(DnsDefault::getServer(j), server.getPort(),
					server.getSourceAddress(), server.getSourcePort()));
			}
		} else {
			extendedServers.append(server);
		}
	}
	id.servers_ = extendedServers;
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsConfiguration
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsBuffer;
class DnsTimeoutTask;
class DnsPurgeTask;
class DnsCache;
class DnsCacheEntry;
typedef SYSidlist<DnsCacheEntry> DnsCacheEntries;
class DnsConfiguration : public SYShash<DnsConfigId>, public RefCounted {

	friend class DnsTimeoutTask;

private:

	Lock lock_;
	DnsCache* cache_;

	// Number of master files currently referencing this configuration.
	uint32_t masterFiles_;

	// Tasks.
	SYSpVector<Task, 8> tasks_;

	static SYSpHash<DnsConfiguration> hash_;
	static Lock hashLock_;

private:

	static Str getName();

public:

	DnsConfiguration();

	~DnsConfiguration();

	DnsConfiguration(const DnsConfiguration& right);

	Lock& getLock() {
		return lock_;
	}

	void start() _THROW_(SparrowException);

	bool isStarted() const {
		return cache_ != 0;
	}

	DnsCacheEntry* doResolve(const uint64_t now, const uint64_t mnow, const int id, const uint8_t* address, const uint32_t length);

	bool sendRequest(const uint64_t now, const uint64_t mnow, DnsCacheEntry* entry, const bool uponError);

	void decodeResponse(uint64_t now, uint64_t mnow, const DnsBuffer& buffer, DnsCacheEntry* entry);

	void processTimeout(const uint64_t now, const uint64_t mnow, const uint64_t timeout);

	void processPurge(const uint64_t now, DnsCacheEntries& obsoleteEntries);

	void processControlSize(DnsCacheEntries& entries);

	bool operator == (const DnsConfiguration& right) const {
		return SYShash<DnsConfigId>::operator == (right);
	}

	uint32_t hash() const {
		uint32_t h = 0;
		SYShashIterator<DnsConfigId> iterator(*this);
		while (++iterator) {
			h = h + iterator.key().hash();
		}
		return h;
	}

	Str print();

	static DnsConfiguration* acquire(const DnsConfiguration& key);

	static void release(DnsConfiguration* dnsConfiguration);

private:

	DnsConfiguration& operator = (const DnsConfiguration& right);
};

typedef RefPtr<DnsConfiguration> DnsConfigurationGuard;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsTimeoutTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsTimeoutTask : public Task {
private:

	DnsConfigurationGuard dnsConfiguration_;

public:

	DnsTimeoutTask(DnsConfiguration* dnsConfiguration)
		: Task(Worker::getQueue()), dnsConfiguration_(dnsConfiguration) {
	}

	virtual bool operator == (const DnsTimeoutTask& right) const {
		return *dnsConfiguration_ == *right.dnsConfiguration_;
	}

	virtual bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override {
		return sparrow_dns_timeout;
	}

	void run(const uint64_t timestamp) override _THROW_(SparrowException) {
		Guard guard(dnsConfiguration_->getLock());
		dnsConfiguration_->processTimeout(std::time(nullptr), my_micro_time(), sparrow_dns_timeout * 1000);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsPurgeTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsPurgeTask : public Task {
private:

	DnsConfigurationGuard dnsConfiguration_;

public:

	DnsPurgeTask(DnsConfiguration* dnsConfiguration)
		: Task(Worker::getQueue()), dnsConfiguration_(dnsConfiguration) {
	}

	virtual bool operator == (const DnsPurgeTask& right) const {
		return *dnsConfiguration_ == *right.dnsConfiguration_;
	}

	virtual bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override {
		return 300000;
	}

	void run(const uint64_t timestamp) override _THROW_(SparrowException);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsSizeTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsSizeTask : public Task {
private:

	DnsConfigurationGuard dnsConfiguration_;

public:

	DnsSizeTask(DnsConfiguration* dnsConfiguration)
		: Task(Worker::getQueue()), dnsConfiguration_(dnsConfiguration) {
	}

	virtual bool operator == (const DnsSizeTask& right) const {
		return *dnsConfiguration_ == *right.dnsConfiguration_;
	}

	virtual bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override {
		return 5000;
	}

	void run(const uint64_t timestamp) override _THROW_(SparrowException);
};

}

#endif /* #ifndef _dns_dnsconfiguration_h_ */
