/*
	DNS listener and worker threads.
*/

#include "../handler/plugin.h"	// For configuration parameters.
#include "dns.h"
#include "dnsnet.h"
#include "dnsconfiguration.h"


namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsWorker
//////////////////////////////////////////////////////////////////////////////////////////////////////

const DnsWorkerFactory DnsWorkerFactory::factory_("DnsWorker");

ThreadPool<DnsBuffer>* DnsWorker::threadPool_ = 0;

// STATIC
void DnsWorker::initialize() _THROW_(SparrowException) {
	threadPool_ = new ThreadPool<DnsBuffer>(DnsWorkerFactory::factory_, &sparrow_max_dns_worker_threads,
		&SparrowStatus::get().dnsWorkerThreads_, "DnsWorker::Queue", true);
}

// STATIC
void DnsWorker::shutdown() {
	threadPool_->stop();
	delete threadPool_;
	threadPool_ = 0;
}

// STATIC
void DnsWorker::sendBuffers(DnsBuffers& buffers) {
	threadPool_->send(buffers);
}

// Worker's processing method: it handles incoming DNS responses.
bool DnsWorker::process(SYSpSlist<DnsBuffer>* buffers)
{
	const uint64_t now = std::time(nullptr);
	const uint64_t mnow = my_micro_time();

	// Received a response: add entry to cache and terminate related pending requests.
	DnsBuffers processedBuffers;
	{
		while (!buffers->isEmpty()) {
			DnsBuffer& buffer = *buffers->removeAt(0);
			uint32_t requestId = buffer.getId();
			DnsConfiguration& configuration = buffer.getConfiguration();
			{
				Guard guard(configuration.getLock());
				DnsCacheEntry* entry = Dns::findEntry(requestId, &configuration);
				if (entry == 0) {
					Atomic::inc64(&SparrowStatus::get().dnsDiscardedResponses2_);
				} else {
					configuration.decodeResponse(now, mnow, buffer, entry);
				}
			}
			buffer.setConfiguration(0);
			processedBuffers.append(&buffer);
		}
	}

	// Put back buffers.
	Dns::releaseBuffers(processedBuffers);
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsRequests
//////////////////////////////////////////////////////////////////////////////////////////////////////

// This is an array of outstanding DNS requests. This way, we can easily find the request when a
// response arrives, just by extracting the reponse identifier from the packet.
// Each request is made of a pending DNS cache entry and a DNS worker thread that will take care
// of processing the response when it arrives.

DnsRequests::DnsRequests() : lock_(false, "DnsRequests::lock_"), n_(0), free_(0) {
}

void DnsRequests::clear(uint32_t i) {
	Guard guard(lock_);
	assert(i < size());
	if (requests_[i].isSet()) {
		requests_[i].reset();
		if (i < free_) {
			free_ = i;
		}
		assert(n_ > 0);
		n_--;
	}
}

DnsConfiguration* DnsRequests::getConfiguration(uint32_t i) {
	Guard guard(lock_);
	assert(i < size());
	return requests_[i].getConfiguration();
}

DnsCacheEntry* DnsRequests::checkEntry(uint32_t i, DnsConfiguration* configuration) {
	Guard guard(lock_);
	assert(i < size());
	DnsCacheEntry* entry = requests_[i].getEntry();
	if (entry != 0 && entry->getRequestId() == i && requests_[i].getConfiguration() == configuration) {
		return entry;
	} else {
		return 0;
	}
}

void DnsRequests::set(DnsConfiguration* configuration, DnsCacheEntry* entry) {
	Guard guard(lock_);
	if (n_ == size()) {
		entry->delayed(true);
	} else {
		entry->delayed(false);
		n_++;
		assert(free_ < size());
		requests_[free_] = DnsRequest(configuration, entry);
		entry->setRequestId(free_);
		uint32_t i;
		for (i = free_ + 1; i < size(); ++i) {
			if (!requests_[i].isSet()) {
				break;
			}
		}
		assert(i <= size());
		free_ = i;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Dns
//////////////////////////////////////////////////////////////////////////////////////////////////////

// This is the DNS listener thread.

Dns* Dns::dns_ = 0;

// Creates and starts the DNS listener.
// STATIC
void Dns::initialize() _THROW_(SparrowException) {
	dns_ = new Dns();
	if (!dns_->start()) {
		throw SparrowException::create(false, "Cannot start DNS thread");
	}
}

Dns::Dns() : Thread("Dns::dns_"), lock_(false, "Dns::lock_"), cond_(false, lock_, "Dns::cond_"),
	serial_(0), last_(UINT_MAX), maxSocketId_(INVALID_SOCKET), bufferLock_(false, "Dns::bufferLock_") {
	FD_ZERO(&fdSet_);
	from_ = SocketUtil::getAddress(0, 0);
#ifdef _WIN32
	msgBuffers_[0].buf = 0;
	msgBuffers_[0].len = 0;
	msgBuffers_[1].buf = staticBuffer_;
	msgBuffers_[1].len = sizeof(staticBuffer_);
#else
	memset(&msgheader_, 0, sizeof (msgheader_));
	msgheader_.msg_name = reinterpret_cast<caddr_t>(from_.getSockAddr());
	msgheader_.msg_namelen = from_.getSockAddrLength();
	msgBuffers_[0].iov_base = 0;
	msgBuffers_[0].iov_len = 0;
	msgBuffers_[1].iov_base = static_cast<caddr_t>(staticBuffer_);
	msgBuffers_[1].iov_len = static_cast<int>(sizeof(staticBuffer_));
	msgheader_.msg_iov = reinterpret_cast<struct iovec*>(&msgBuffers_);
	msgheader_.msg_iovlen = 2;
#endif
}

// DNS listener's processing method.
bool Dns::process() {
	// Check if configuration changed.
	{
		Guard guard(lock_);
		if (last_ != serial_) {
			// Build fd_set.
			maxSocketId_ = DnsSocket::fillFdSet(&fdSet_, socketIds_);
			last_ = serial_;
		}
	}
	if (socketIds_.isEmpty()) {
		// Nothing to do; just sleep.
		cond_.wait(1000, false);
	} else {
		DnsBuffers emptyBuffers;
		DnsBuffers sentBuffers;

		// Wait for a network event or timeout.
		fd_set fdSet = fdSet_;
		struct timeval tv;
		tv.tv_sec = sparrow_dns_timeout / 1000;
		tv.tv_usec = (sparrow_dns_timeout * 1000) % 1000000;
		int rc = select(static_cast<int>(maxSocketId_), &fdSet, 0, 0, &tv);
		if (rc > 0) {
			// Prepare buffers: one buffer per selected socket.
			{
				Guard bufferGuard(bufferLock_);
				for (int i = 0; i < rc; ++i) {
					emptyBuffers.append(buffers_.isEmpty() ? new DnsBuffer(256) : buffers_.removeAt(0));
				}
			}

			// Read incoming packets.
			for (uint32_t i = 0; i < socketIds_.length(); ++i) {
				my_socket socketId = socketIds_[i];
				if (FD_ISSET(socketId, &fdSet)) {
					DnsBuffer& buffer = *emptyBuffers.removeAt(0);
#ifdef _WIN32
					msgBuffers_[0].buf = reinterpret_cast<char*>(buffer.getData());
					msgBuffers_[0].len = buffer.getCapacity();
					unsigned long packetLength = 0;
					DWORD flags = 0;
					int fromLength = from_.getSockAddrLength();
					if (WSARecvFrom(socketId, (LPWSABUF)&msgBuffers_, 2, &packetLength, &flags,
						from_.getSockAddr(), &fromLength, 0, 0) == SOCKET_ERROR) {
						packetLength = 0;
					}
#else
					msgBuffers_[0].iov_base = reinterpret_cast<caddr_t>(buffer.getData());
					msgBuffers_[0].iov_len = static_cast<int>(buffer.getCapacity());
					ssize_t packetLength = recvmsg(socketId, &msgheader_, 0);
#endif
					SparrowStatus::get().dnsResponses_++;

					// We are only interested in packets likely to contain a DNS answer.
					bool putBack = false;
					if (packetLength > 12) {	// DNS header is 12 byte long.
						uint32_t capacity = buffer.getCapacity();
						buffer.setLength(packetLength);
						if (packetLength > capacity) {
							memcpy(buffer.getData() + capacity, staticBuffer_, packetLength - capacity);
						}

						// Find request id.
						DnsConfiguration* configuration = requests_.getConfiguration(buffer.getId());
						if (configuration == 0) {
							putBack = true;
						} else {
							buffer.setConfiguration(configuration);
							sentBuffers.append(&buffer);
						}
					} else {
						putBack = true;
					}
					if (putBack) {
						// Put back buffer.
						emptyBuffers.append(&buffer);
						Atomic::inc64(&SparrowStatus::get().dnsDiscardedResponses1_);
					}
				}
			}

			// Send all received buffers to worker threads.
			DnsWorker::sendBuffers(sentBuffers);
		}

		if (!emptyBuffers.isEmpty()) {
			// Put back buffers.
			releaseBuffers(emptyBuffers);
		}
	}
	return true;
}

bool Dns::notifyStop() {
	cond_.signal();
	return SocketUtil::notifyStopSocket();
}

}
