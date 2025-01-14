/*
	DNS listener and worker threads.
*/

#ifndef _dns_dns_h_
#define _dns_dns_h_

#include "dnsconfiguration.h"
#include "dnscache.h"

extern uint sparrow_idle_thread_timeout;

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsRequest
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsRequest {
private:

	DnsConfigurationGuard configuration_;
	DnsCacheEntry* entry_;

public:

	DnsRequest() : configuration_(0), entry_(0) {
	}

	DnsRequest(DnsConfiguration* configuration, DnsCacheEntry* entry)
		: configuration_(configuration), entry_(entry) {
	}

	DnsConfiguration* getConfiguration() {
		return configuration_.get();
	}

	DnsCacheEntry* getEntry() {
		return entry_;
	}

	bool isSet() const {
		return entry_ != 0;
	}

	void reset() {
		configuration_ = 0;
		entry_ = 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsRequests
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Outstanding requests.
class DnsRequests {
private:

	DnsRequest requests_[65536];
	Lock lock_;
	uint32_t n_;
	uint32_t free_;

private:

	uint32_t size() {
		return sizeof(requests_) / sizeof(requests_[0]);
	}

public:

	DnsRequests();

	void clear(uint32_t i);

	DnsConfiguration* getConfiguration(uint32_t i);

	DnsCacheEntry* checkEntry(uint32_t i, DnsConfiguration* configuration);

	void set(DnsConfiguration* configuration, DnsCacheEntry* entry);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsBuffer : public SYSidlink<DnsBuffer> {
private:

	DnsConfigurationGuard configuration_;
	SYSvector<uint8_t> data_;

public:

	DnsBuffer(uint32_t size) : data_(size) {
		data_.forceLength(size);
	}

	uint8_t* getData() {
		return const_cast<uint8_t*>(data_.data());
	}

	const uint8_t* getData() const {
		return data_.data();
	}

	uint32_t getLength() const {
		return data_.length();
	}

	uint32_t getCapacity() const {
		return data_.capacity();
	}

	void setLength(uint32_t length) {
		if (length > data_.capacity()) {
			data_.resize(length);
		}
		data_.forceLength(length);
	}

	DnsConfiguration& getConfiguration() {
		return *configuration_.get();
	}

	void setConfiguration(DnsConfiguration* configuration) {
		configuration_ = configuration;
	}

	uint32_t getId() const {
		const uint8_t* b = getData();
		return (b[0] << 8) | b[1];
	}
};

typedef SYSpSlist<DnsBuffer> DnsBuffers;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsWorker
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsWorker : public MessageThread<DnsBuffer> {
private:

	static ThreadPool<DnsBuffer>* threadPool_;

protected:

	bool process(SYSpSlist<DnsBuffer>* buffers) override;

public:

	DnsWorker(const char* name, Queue<DnsBuffer>& queue) : MessageThread<DnsBuffer>(name, queue, &sparrow_idle_thread_timeout) {
	}

	~DnsWorker() {
	}

	static void initialize() _THROW_(SparrowException);
	static void shutdown();
	static void sendBuffers(DnsBuffers& buffer);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsWorkerFactory
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsWorkerFactory : public MessageThreadFactory<DnsBuffer>, private ThreadNameGenerator {
public:

	static const DnsWorkerFactory factory_;

public:

	DnsWorkerFactory(const char* prefix) : ThreadNameGenerator(prefix) {
	}

	MessageThread<DnsBuffer>* createThread(Queue<DnsBuffer>& queue) const override {
		return new DnsWorker(getName().c_str(), queue);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Dns
//////////////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_DNS_BUFFERS 100

class Dns : public Thread {
private:

	Lock lock_;
	Cond cond_;
	volatile uint32_t serial_;	// Counter of modifications.
	volatile uint32_t last_;		// Value of counter when last checked.
	fd_set fdSet_;
	SYSvector<my_socket> socketIds_;
	my_socket maxSocketId_;
	DnsBuffers buffers_;
	Lock bufferLock_;
	DnsRequests requests_;

	char staticBuffer_[65536];
	SocketAddress from_;
#ifdef _WIN32
	WSABUF msgBuffers_[2];
#else
	struct msghdr msgheader_;
	struct iovec msgBuffers_[2];
#endif

	static Dns* dns_;

protected:

	bool process() override;

	bool notifyStop() override;

	bool deleteAfterExit() override {
		return false;
	}

public:

	static void initialize() _THROW_(SparrowException);

	static void shutdown() {
		if (dns_ != 0) {
			dns_->stop();
			delete dns_;
			dns_ = 0;
		}
	}

	Dns();

	~Dns() {
	}

	static void releaseBuffers(DnsBuffers& buffers) {
		Guard guard(dns_->bufferLock_);
		DnsBuffers& dnsBuffers = dns_->buffers_;
		while (!buffers.isEmpty()) {
			dnsBuffers.append(buffers.removeAt(0));
		}
		while (dnsBuffers.entries() > MAX_DNS_BUFFERS) {
			delete dnsBuffers.removeAt(0);
		}
	}

	static Lock& getLock() {
		return dns_->lock_;
	}

	static void incSerial() {
		dns_->serial_++;
	}

	static DnsCacheEntry* findEntry(uint32_t id, DnsConfiguration* configuration) {
		return dns_->requests_.checkEntry(id, configuration);
	}

	static void setRequest(DnsConfiguration* configuration, DnsCacheEntry* entry) {
		dns_->requests_.set(configuration, entry);
	}

	static void clearRequest(DnsCacheEntry* entry) {
		dns_->requests_.clear(entry->getRequestId());
	}
};

}

#endif /* #ifndef _dns_dns_h_ */
