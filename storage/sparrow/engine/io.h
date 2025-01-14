/*
	IO helpers.
*/

#ifndef _engine_io_h_
#define _engine_io_h_

#include "exception.h"
#include "types.h"

#if defined(__linux__)
#include <libaio.h>
#include <sys/uio.h>
#elif defined(__SunOS)
#include <aio.h>
#include <port.h>
#elif defined(__MACH__)
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

// We probably don't need these additionnal thread local services, but gthey could be usefull. 
// #include "my_thread_local.h"

extern mysql_mutex_t THR_LOCK_open;

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// IOBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

class IOBuffer : public ByteBuffer {
private:

	const uint32_t capacity_;

public:

	IOBuffer(const uint32_t capacity) : ByteBuffer(ByteBuffer::mmap(capacity), capacity), capacity_(capacity) {
		Atomic::inc32(&SparrowStatus::get().ioBuffers_);
		Atomic::add64(&SparrowStatus::get().ioBufferSize_, static_cast<int64_t>(capacity_));
	}

	~IOBuffer() {
		ByteBuffer::munmap(getData(), capacity_);
		Atomic::dec32(&SparrowStatus::get().ioBuffers_);
		Atomic::add64(&SparrowStatus::get().ioBufferSize_, -static_cast<int64_t>(capacity_));
	}

	uint32_t capacity() const {
		return capacity_;
	}
};

typedef SYSarray<uint8_t> TempBuffer;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// IOContext
//////////////////////////////////////////////////////////////////////////////////////////////////////

class IOContext {
private:

	static thread_local IOContext* threadKey_;

	int nbEvents_;

	IOBuffer* buffer_;

	TempBuffer* tempBuffer1_;
	TempBuffer* tempBuffer2_;
	TempBuffer* tempBuffer3_;

#ifdef _WIN32
	OVERLAPPED overlapped_;
#elif defined(__linux__)
	io_context_t context_;
	SYSpVector<struct iocb, 0> iocb_;
	struct io_event* events_;
#elif defined(__SunOS)
	int port_;
	port_notify_t portNotify_;
	aiocb64_t* iocb_;
	port_event_t* events_;
#endif

private:

	static IOContext& getContext();

	void destroyBuffers() {
		if (buffer_ != 0) { delete buffer_; buffer_ = 0; }
		if (tempBuffer1_ != 0) { delete tempBuffer1_; tempBuffer1_ = 0; }
		if (tempBuffer2_ != 0) { delete tempBuffer2_; tempBuffer2_ = 0; }
		if (tempBuffer3_ != 0)  { delete tempBuffer3_; tempBuffer3_ = 0; }
	}

public:

	static void initialize();

	int getNbEvents() const {
		return nbEvents_;
	}

	static IOContext& get(const int nbEvents) _THROW_(SparrowException);

	static void destroy();

#ifdef _WIN32

	IOContext(const int nbEvents) _THROW_(SparrowException) : nbEvents_(nbEvents), buffer_(0), tempBuffer1_(0), tempBuffer2_(0), tempBuffer3_(0) {
		memset(&overlapped_, 0, sizeof(overlapped_));
		HANDLE handle = CreateEvent(0, false, false, 0);
		if (handle == 0) {
			throw SparrowException::create(true, "Cannot create event for OVERLAPPED structure");
		}
		overlapped_.hEvent = handle;
	}

	~IOContext() {
		CloseHandle(overlapped_.hEvent);
		destroyBuffers();
	}

	void initEvents([[maybe_unused]] const int nbEvents){
	}

	void destroyEvents() {}

	static OVERLAPPED* getOverlapped() _THROW_(SparrowException) {
		return &getContext().overlapped_;
	}

#elif defined(__MACH__)

	IOContext(const int nbEvents) _THROW_(SparrowException) :  nbEvents_(nbEvents), buffer_(0), tempBuffer1_(0), tempBuffer2_(0), tempBuffer3_(0) {
	}

	~IOContext() {
		destroyBuffers();
	}

#elif defined(__linux__)
	
	IOContext(const int nbEvents) _THROW_(SparrowException) :  nbEvents_(0), buffer_(0), tempBuffer1_(0), tempBuffer2_(0), tempBuffer3_(0), events_(0) {
		initEvents(nbEvents);
	}

	~IOContext() {
		destroyEvents();
		destroyBuffers();
	}

	void initEvents(const int nbEvents) 
	{
		destroyEvents();

		int attempts = 0;
		while (nbEvents > 0) {
			memset(&context_, 0, sizeof(context_));
			const int test = io_setup(nbEvents, &context_);
			if (test == 0) {
				break;
			} else if (++attempts == 10 || test != -EAGAIN) {
				errno = -test;
				throw SparrowException::create(true, "Cannot initialize IO context for %u events", nbEvents);
			}
		}
		iocb_.resize(nbEvents);
		for (int i = 0; i < nbEvents; ++i) {
			iocb_.append(new struct iocb);
		}
		events_ = new struct io_event[nbEvents];

		nbEvents_ = nbEvents;
	}

	void destroyEvents() {
		if (nbEvents_ > 0) {
			io_destroy(context_);
			nbEvents_ = 0;
		}
		iocb_.clearAndDestroy();
		if ( events_ != NULL ) {
			delete [] events_;
			events_ = NULL;
		}
	}

	io_context_t get() {
		return context_;
	}

	struct iocb** getIocb() {
		return const_cast<struct iocb**>(iocb_.data());
	}

	struct io_event* getEvents() {
		memset(events_, 0, sizeof(events_[0]) * nbEvents_);
		return events_;
	}

#elif defined(__SunOS)

	IOContext(const int nbEvents) _THROW_(SparrowException) :  nbEvents_(nbEvents), buffer_(0), tempBuffer1_(0), tempBuffer2_(0), tempBuffer3_(0),
		iocb_(0), events_(0) {
		port_ = port_create();
		if (port_ < 0) {
			throw SparrowException::create(true, "Cannot create completion port");
		}
		memset(&portNotify_, 0, sizeof(portNotify_));
		portNotify_.portnfy_port = port_;
		iocb_ = new aiocb64_t[nbEvents_];
		events_ = new port_event_t[nbEvents_];
	}

	~IOContext() {
		close(port_);
		if ( iocb_ != NULL )
			delete [] iocb_;
		if ( events_ != NULL  )
			delete [] events_;
		destroyBuffers();
	}

	int getPort() const {
		return port_;
	}

	aiocb64_t* getIocb() {
		for (int i = 0; i < nbEvents_; ++i) {
			iocb_[i].aio_sigevent.sigev_notify = SIGEV_PORT;
			iocb_[i].aio_sigevent.sigev_value.sival_ptr = &portNotify_;
		}
		return iocb_;
	}

	port_event_t* getEvents() {
		return events_;
	}

#endif

	static ByteBuffer& getBuffer(const uint32_t size) {
		IOContext& ctx = getContext();
		if (ctx.buffer_ == 0 || ctx.buffer_->capacity() < size) {
			if (ctx.buffer_ != 0) delete ctx.buffer_;
			ctx.buffer_ = new IOBuffer(size);
		}
		ctx.buffer_->position(0);
		ctx.buffer_->limit(size);
		return *ctx.buffer_;
	}

	static void* getTempBuffer1(const uint64_t size) {
		IOContext& ctx = getContext();
		if (ctx.tempBuffer1_ == 0 || ctx.tempBuffer1_->length() < size) {
			if (ctx.tempBuffer1_ != 0) delete ctx.tempBuffer1_;
			ctx.tempBuffer1_ = new TempBuffer(static_cast<uint32_t>(size));
		}
		return (void*)ctx.tempBuffer1_->data();
	}

	static void* getTempBuffer2(const uint64_t size) {
		IOContext& ctx = getContext();
		if (ctx.tempBuffer2_ == 0 || ctx.tempBuffer2_->length() < size) {
			if (ctx.tempBuffer2_ != 0) delete ctx.tempBuffer2_;
			ctx.tempBuffer2_ = new TempBuffer(static_cast<uint32_t>(size));
		}
		return (void*)ctx.tempBuffer2_->data();
	}

	static void* getTempBuffer3(const uint64_t size) {
		IOContext& ctx = getContext();
		if (ctx.tempBuffer3_ == 0 || ctx.tempBuffer3_->length() < size) {
			if (ctx.tempBuffer3_ != 0) delete ctx.tempBuffer3_;
			ctx.tempBuffer3_ = new TempBuffer(static_cast<uint32_t>(size));
		}
		return (void*)ctx.tempBuffer3_->data();
	}

private:

	IOContext(const IOContext& right);
	IOContext& operator = (const IOContext& right);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// IO
//////////////////////////////////////////////////////////////////////////////////////////////////////

enum FileMode {
	FILE_MODE_CREATE = 0,
	FILE_MODE_READ = 1,
	FILE_MODE_UPDATE = 2
};

class IO {
private:

	static uint8_t* trashBuffer_;

public:

	static void initialize() _THROW_(SparrowException);

	static int open(const char* name, const FileMode mode) _THROW_(SparrowException);

	static void close(File file);

	static uint32_t read(const int file, const char* name, const uint64_t offset, uint8_t* data, const uint32_t size) _THROW_(SparrowException);

	static uint32_t readMultiple(const int file, Lock* lock, const char* name, const uint64_t offset, uint8_t** data, const uint32_t size) _THROW_(SparrowException);

	static uint32_t write(const int file, const char* name, const uint64_t offset, uint8_t* data, const uint32_t size) _THROW_(SparrowException);

	static void flush(const int file, const char* name) _THROW_(SparrowException);
};

}

#endif /* #ifndef _engine_io_h_ */

