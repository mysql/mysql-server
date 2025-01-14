/*
	IO helpers.
*/

#include "io.h"
#include "fileutil.h"
#include "purge.h"

#include "../engine/log.h"
#include "mysys/mysys_priv.h"

#ifdef _WIN32
#include <direct.h>
//extern "C" File my_open_osfhandle(HANDLE handle, int oflag);
//extern "C" struct st_my_file_info* my_file_info;
#endif

namespace Sparrow {


//////////////////////////////////////////////////////////////////////////////////////////////////////
// IO
//////////////////////////////////////////////////////////////////////////////////////////////////////

//pthread_key(IOContext*, IOContext::threadKey_);
thread_local IOContext*	IOContext::threadKey_{nullptr};

uint8_t* IO::trashBuffer_ = 0;

// STATIC
void IO::initialize() _THROW_(SparrowException) {
	IOContext::initialize();
	trashBuffer_ = ByteBuffer::mmap(FileUtil::getPageSize());
}

// STATIC
void IOContext::initialize() {
	IOContext::threadKey_ = nullptr;
}

// STATIC
IOContext& IOContext::getContext() {
	if (IOContext::threadKey_ == nullptr) {
		IOContext::threadKey_ = new IOContext(0);
	}
	return *IOContext::threadKey_;
}

// STATIC
IOContext& IOContext::get(const int nbEvents) _THROW_(SparrowException) {
	if (IOContext::threadKey_->getNbEvents() < nbEvents)
	{
		IOContext::threadKey_->destroyEvents();
		IOContext::threadKey_->initEvents(nbEvents);
	}
	return *IOContext::threadKey_;
}

// STATIC
void IOContext::destroy() {
	if (IOContext::threadKey_ != nullptr) {
		delete IOContext::threadKey_;
		IOContext::threadKey_ = nullptr;
	}
}

// Opens a file.
// STATIC
int IO::open(const char* name, const FileMode mode) _THROW_(SparrowException) {
	SPARROW_ENTER("IO::initialize");
	int file = -1;
	uint32_t retries = 0;
	const uint32_t maxRetries = 5;
	while (true) {
		bool notFound = false;
#ifdef _WIN32
		HANDLE handle;
		switch (mode) {
			case FILE_MODE_CREATE: {
				handle = CreateFile(name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS,
					FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED, 0);
				break;
			}
			case FILE_MODE_READ: {
				handle = CreateFile(name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
					FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, 0);
				break;
			}
			case FILE_MODE_UPDATE: {
				handle = CreateFile(name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
					FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED, 0);
				break;
			}
			default: assert(0);
		}
		if (handle == INVALID_HANDLE_VALUE) {
			notFound = GetLastError() == ERROR_PATH_NOT_FOUND;
		} else {
			file = my_get_filedescr(handle, 0);
			//file = RegisterHandle(handle, 0);
			//file = _open_osfhandle((intptr_t)handle, 0);
			//file = my_win_open(handle, 0);
			if (file == -1) {
				CloseHandle(handle);
				handle = INVALID_HANDLE_VALUE;		// Just to be clean
			}
		}
#else
		switch (mode) {
			case FILE_MODE_CREATE: {
				file = ::open(name, O_RDWR | O_CREAT | O_TRUNC, my_umask);
				break;
			}
			case FILE_MODE_READ: {
				file = ::open(name, O_RDONLY, my_umask);
				break;
			}
			case FILE_MODE_UPDATE: {
				file = ::open(name, O_RDWR, my_umask);
				break;
			}
			default: assert(0);
		}
		if (file == -1) {
			notFound = errno == ENOENT;
		}
#endif
		if (notFound && (mode == FILE_MODE_CREATE || mode == FILE_MODE_UPDATE) && ++retries <= maxRetries) {
			FileUtil::createDirectories(name);
		} else {
			break;
		}
	}
	if (file == -1) {
		switch (mode) {
			case FILE_MODE_CREATE: throw SparrowException::create(true, "Cannot create file %s", name);
			case FILE_MODE_READ: throw SparrowException::create(true, "Cannot open file %s for reading", name);
			case FILE_MODE_UPDATE: throw SparrowException::create(true, "Cannot open file %s for updating", name);
			default: assert(0);
		}
	} else {
		auto	cnv_mode = [mode]() {
			file_info::OpenType		open_mode{file_info::OpenType::UNOPEN};
			switch (mode) {
				case FILE_MODE_CREATE:	open_mode = file_info::OpenType::FILE_BY_CREATE; break;
				case FILE_MODE_READ:
				case FILE_MODE_UPDATE:	open_mode = file_info::OpenType::FILE_BY_OPEN; break;
				default: assert(0);
			}
			return open_mode;
		};
		file_info::RegisterFilename(file, name, cnv_mode());
	}
	// Not required anymore. File counting is done in methods CountFileOpen() in my_static.cc which is called 
	// file_info::RegisterFilename
	//thread_safe_increment(my_file_opened, &THR_LOCK_open);


	// Try to bypass buffering on Unix.
#ifdef O_DIRECT			// Linux or MacOS
	static bool directioSupport = true;
	if (fcntl(file, F_SETFL, O_DIRECT) == -1) {
		if (directioSupport) {
			directioSupport = false;
			spw_print_warning("Call to fcntl(O_DIRECT) failed: maybe the database file system does not support direct I/O");
		}
	}
#elif defined(__SunOS)	// Solaris
	static bool directioSupport = true;
	if (directio(file, DIRECTIO_ON) == -1) {
		if (directioSupport) {
			directioSupport = false;
			spw_print_warning("Call to directio() failed: maybe the database file system does not support direct I/O");
		}
	}
#endif
	return file;
}


// STATIC
void IO::close(File file)
{
	if (file == -1) return;

	// my_close calls file_info::UnregisterFilename() so we don't need to call it here. 
	my_close(file, MYF(0));
}

// Reads a single data block from a file.
// STATIC
uint32_t IO::read(const int file, const char* name, const uint64_t offset, uint8_t* data, const uint32_t size) _THROW_(SparrowException) {
	SPARROW_ENTER("IO::read");
	assert(size % FileUtil::getSectorSize() == 0);
	if (offset % sparrow_cache_block_size != 0) {
		throw SparrowException::create(false, "Seek at invalid offset %llu in file %s", static_cast<ulonglong>(offset), name);
	}
	bool ok = true;
	if (size <= sparrow_small_read_block_size) Atomic::inc64(&SparrowStatus::get().ioNbSmall_);
	else if (size <= sparrow_medium_read_block_size) Atomic::inc64(&SparrowStatus::get().ioNbMedium_);
	else Atomic::inc64(&SparrowStatus::get().ioNbLarge_);
#ifdef _WIN32
	OVERLAPPED* overlapped = IOContext::getOverlapped();
	overlapped->Offset = (DWORD)offset;
	overlapped->OffsetHigh = (DWORD)(offset >> 32);
	HANDLE handle = my_get_osfhandle(file);
	DWORD readBytes;
	if (ReadFile(handle, data, size, &readBytes, overlapped) == 0) {
		if (GetLastError() != ERROR_IO_PENDING
			|| GetOverlappedResult(handle, overlapped, &readBytes, true) == 0) {
			ok = false;
		}
	}
#elif defined(__MACH__)
	ssize_t readBytes = ::pread(file, data, size, offset);
	ok = readBytes != static_cast<ssize_t>(-1);
#elif defined(__linux__)
	ssize_t readBytes;
	if (sparrow_async_io) {
		IOContext& ctx = IOContext::get(1);
		struct iocb** iocb = ctx.getIocb();
		io_prep_pread(iocb[0], file, data, size, offset);
		int result = io_submit(ctx.get(), 1, iocb);
		if (result == 1) {
			ok = true;
		} else {
			ok = false;
			errno = -result;
		}
		while (ok) {
			struct io_event* events = ctx.getEvents();
			result = io_getevents(ctx.get(), 1, 1, events, 0);
			if (result == 1) {
				if (events->res2 == 0) {
					readBytes = events->res;
					break;
				} else {
					errno = -events->res2;
					ok = false;
				}
			} else if (result != -EAGAIN && result != -EINTR) {
				errno = -result;
				ok = false;
			}
		}
	} else {
		readBytes = ::pread64(file, data, size, offset);
		ok = readBytes != static_cast<ssize_t>(-1);
	}
#elif defined(__SunOS)
	ssize_t readBytes;
	if (sparrow_async_io) {
		IOContext& ctx = IOContext::get(1);
		aiocb64_t* iocb = ctx.getIocb();
		iocb->aio_reqprio = 0;
		iocb->aio_lio_opcode = LIO_READ;
		iocb->aio_fildes = file;
		iocb->aio_nbytes = size;
		iocb->aio_buf = data;
		iocb->aio_offset = offset;
		ok = aio_read64(iocb) == 0;
		port_event_t* events = ctx.getEvents();
		if (ok) {
			ok = port_get(ctx.getPort(), events, 0) == 0;
		}
		if (ok ){
			iocb = (aiocb64_t*)events->portev_object;
			const int check = iocb->aio_resultp.aio_errno;
			if (check == 0) {
				readBytes = aio_return64(iocb);
			} else {
				errno = check;
				ok = false;
			}
		}
	} else {
		readBytes = ::pread64(file, data, size, offset);
		ok = readBytes != static_cast<ssize_t>(-1);
	}
#else
#error Platform not supported
#endif
	if (!ok) {
		throw SparrowException::create(true, "Cannot read file %s at offset %llu", name, static_cast<ulonglong>(offset));
	}
	return static_cast<uint32_t>(readBytes);
}

// Reads multiple data blocks from a file.
// STATIC
uint32_t IO::readMultiple(const int file, Lock* lock, const char* name, const uint64_t offset, uint8_t** data, const uint32_t size) _THROW_(SparrowException) {
	SPARROW_ENTER("IO::readMultiple");
	bool ok = true;
	if (size <= sparrow_small_read_block_size) Atomic::inc64(&SparrowStatus::get().ioNbSmall_);
	else if (size <= sparrow_medium_read_block_size) Atomic::inc64(&SparrowStatus::get().ioNbMedium_);
	else Atomic::inc64(&SparrowStatus::get().ioNbLarge_);
#ifdef _WIN32
	OVERLAPPED* overlapped = IOContext::getOverlapped();
	overlapped->Offset = (DWORD)offset;
	overlapped->OffsetHigh = (DWORD)(offset >> 32);

	// Each segment is a page.
	const uint32_t pageSize = FileUtil::getPageSize();
	uint32_t pages = (size + pageSize - 1) / pageSize;
	uint32_t pagesPerBlock = sparrow_cache_block_size / pageSize;
	FILE_SEGMENT_ELEMENT* segments = static_cast<FILE_SEGMENT_ELEMENT*>(IOContext::getTempBuffer2((pages + 1) * sizeof(FILE_SEGMENT_ELEMENT)));
	memset(&segments[pages], 0, sizeof(FILE_SEGMENT_ELEMENT));
	uint32_t block = 0;
	for (uint32_t i = 0; i < pages; ++i) {
		uint32_t page = i % pagesPerBlock;
		if (i != 0 && page == 0) {
			block++;
		}
		uint8_t* d = data[block];
		uint8_t* p = d == 0 ? trashBuffer_ : d + page * pageSize;
		segments[i].Buffer = PtrToPtr64(p);
	}
	HANDLE handle = my_get_osfhandle(file);
	DWORD readBytes;
	if (ReadFileScatter(handle, segments, size, 0, overlapped) == 0) {
		if (GetLastError() != ERROR_IO_PENDING
			|| GetOverlappedResult(handle, overlapped, &readBytes, true) == 0) {
			ok = false;
		}
	} else {
		readBytes = size;
	}
#elif defined(__MACH__)
	const uint32_t blocks = (size + sparrow_cache_block_size - 1) / sparrow_cache_block_size;
	ssize_t readBytes = 0;
	if (blocks <= IOV_MAX) {
		struct iovec segments[IOV_MAX];
		uint32_t length = size;
		for (uint32_t i = 0; i < blocks; ++i) {
			struct iovec& v = segments[i];
			uint8_t* d = data[i];
			v.iov_base = (char*)(d == 0 ? trashBuffer_ : d);
			v.iov_len = std::min(length, sparrow_cache_block_size);
			length -= sparrow_cache_block_size;
		}

		// TODO use preadv when it is supported by redhat
		// Linux RedHat does not support preadv so we have to lock the file to avoid concurrent seek and read operations.
		Guard guard(*lock);
		ok = lseek(file, offset, SEEK_SET) == static_cast<off_t>(offset);
		if (ok) {
			readBytes = ::readv(file, segments, blocks);
		}
	} else {
		// Cannot use vectored I/O because there are too many blocks: read into a temporary
		// buffer, and copy blocks.
		uint8_t* buffer = ByteBuffer::mmap(size);
		readBytes = ::pread(file, buffer, size, offset);
		if (readBytes  > 0) {
			uint32_t length = size;
			for (uint32_t i = 0; i < blocks; ++i) {
				uint8_t* d = data[i];
				if (d != 0) {
					const uint32_t blockSize = std::min(length, sparrow_cache_block_size);
					memcpy(d, buffer + i * sparrow_cache_block_size, blockSize);
				}
				length -= sparrow_cache_block_size;
			}
		}
		ByteBuffer::munmap(buffer, size);
	}
	ok = readBytes != static_cast<ssize_t>(-1);
#elif defined(__linux__)
	const uint32_t blocks = (size + sparrow_cache_block_size - 1) / sparrow_cache_block_size;
	ssize_t readBytes = 0;
	if (sparrow_async_io) {
		IOContext& ctx = IOContext::get(blocks);
		struct iocb** iocb = ctx.getIocb();
		uint32_t length = size;
		uint64_t o = offset;
		int actual = 0;
		for (uint32_t i = 0; i < blocks; ++i) {
			uint8_t* d = data[i];
			const uint32_t l = std::min(length, sparrow_cache_block_size);
			if (d == 0) {
				readBytes += l;
			} else {
				io_prep_pread(iocb[actual], file, static_cast<void*>(d), l, o);
				++actual;
			}
			length -= sparrow_cache_block_size;
			o += sparrow_cache_block_size;
		}
		int result = io_submit(ctx.get(), actual, iocb);
		if (result == actual) {
			ok = true;
		} else {
			ok = false;
			errno = -result;
		}
		int count = 0;
		while (ok && count < actual) {
			struct io_event* events = ctx.getEvents();
			result = io_getevents(ctx.get(), 1, actual - count, events, 0);
			if (result >= 1) {
				count += result;
				for (int i = 0; i < result; ++i) {
					if (events[i].res2 == 0) {
						readBytes += events[i].res;
					} else {
						errno = -events[i].res2;
						ok = false;
					}
				}
			} else if (result != -EAGAIN && result != -EINTR) {
				errno = -result;
				ok = false;
			}
		}
	} else {
		if (blocks <= IOV_MAX) {
			struct iovec segments[IOV_MAX];
			uint32_t length = size;
			for (uint32_t i = 0; i < blocks; ++i) {
				struct iovec& v = segments[i];
				uint8_t* d = data[i];
				v.iov_base = (char*)(d == 0 ? trashBuffer_ : d);
				v.iov_len = std::min(length, sparrow_cache_block_size);
				length -= sparrow_cache_block_size;
			}

			// TODO use preadv when it is supported by redhat
			// Linux RedHat does not support preadv so we have to lock the file to avoid concurrent seek and read operations.
			Guard guard(*lock);
			ok = lseek(file, offset, SEEK_SET) == static_cast<off_t>(offset);
			if (ok) {
				readBytes = ::readv(file, segments, blocks);
			}
		} else {
			// Cannot use vectored I/O because there are too many blocks: read into a temporary
			// buffer, and copy blocks.
			uint8_t* buffer = ByteBuffer::mmap(size);
			readBytes = ::pread64(file, buffer, size, offset);
			if (readBytes  > 0) {
				uint32_t length = size;
				for (uint32_t i = 0; i < blocks; ++i) {
					uint8_t* d = data[i];
					if (d != 0) {
						const uint32_t blockSize = std::min(length, sparrow_cache_block_size);
						memcpy(d, buffer + i * sparrow_cache_block_size, blockSize);
					}
					length -= sparrow_cache_block_size;
				}
			}
			ByteBuffer::munmap(buffer, size);
		}
		ok = readBytes != static_cast<ssize_t>(-1);
	}
#elif defined(__SunOS)
	const uint32_t blocks = (size + sparrow_cache_block_size - 1) / sparrow_cache_block_size;
	ssize_t readBytes = 0;
	if (sparrow_async_io) {
		IOContext& ctx = IOContext::get(blocks);
		aiocb64_t* iocb = ctx.getIocb();
		uint32_t length = size;
		uint64_t o = offset;
		int actual = 0;
		for (uint32_t i = 0; i < blocks; ++i) {
			uint8_t* d = data[i];
			const uint32_t l = std::min(length, sparrow_cache_block_size);
			if (d == 0) {
				readBytes += l;
			} else {
				iocb[actual].aio_reqprio = 0;
				iocb[actual].aio_lio_opcode = LIO_READ;
				iocb[actual].aio_fildes = file;
				iocb[actual].aio_nbytes = l;
				iocb[actual].aio_buf = d;
				iocb[actual].aio_offset = o;
				ok = aio_read64(iocb + actual) == 0;
				if (!ok) {
					break;
				}
				++actual;
			}
			length -= sparrow_cache_block_size;
			o += sparrow_cache_block_size;
		}
		port_event_t* events = ctx.getEvents();
		if (ok) {
			uint_t n = actual;
			ok = port_getn(ctx.getPort(), events, actual, &n, 0) == 0;
		}
		if (ok) {
			for (int i = 0; i < actual; ++i) {
				iocb = (aiocb64_t*)events[i].portev_object;
				const int check = iocb->aio_resultp.aio_errno;
				if (check == 0) {
					readBytes += aio_return64(iocb);
				} else {
					errno = check;
					ok = false;
					break;
				}
			}
		}
	} else {
		if (blocks <= IOV_MAX) {
			struct iovec segments[IOV_MAX];
			uint32_t length = size;
			for (uint32_t i = 0; i < blocks; ++i) {
				struct iovec& v = segments[i];
				uint8_t* d = data[i];
				v.iov_base = (char*)(d == 0 ? trashBuffer_ : d);
				v.iov_len = std::min(length, sparrow_cache_block_size);
				length -= sparrow_cache_block_size;
			}

			// Solaris does not support preadv (see http://bugs.opensolaris.org/bugdatabase/view_bug.do?bug_id=1167819),
			// so we have to lock the file to avoid concurrent seek and read operations.
			Guard guard(*lock);
			ok = lseek(file, offset, SEEK_SET) == static_cast<off_t>(offset);
			if (ok) {
				readBytes = ::readv(file, segments, blocks);
			}
		} else {
			// Cannot use vectored I/O because there are too many blocks: read into a temporary
			// buffer, and copy blocks.
			uint8_t* buffer = ByteBuffer::mmap(size);
			readBytes = ::pread64(file, buffer, size, offset);
			if (readBytes  > 0) {
				uint32_t length = size;
				for (uint32_t i = 0; i < blocks; ++i) {
					uint8_t* d = data[i];
					if (d != 0) {
						const uint32_t blockSize = std::min(length, sparrow_cache_block_size);
						memcpy(d, buffer + i * sparrow_cache_block_size, blockSize);
					}
					length -= sparrow_cache_block_size;
				}
			}
			ByteBuffer::munmap(buffer, size);
		}
		ok = readBytes != static_cast<ssize_t>(-1);
	}
#else
#error Platform not supported
#endif
	if (!ok) {
		throw SparrowException::create(true, "Cannot read file %s at offset %llu", name, static_cast<ulonglong>(offset));
	}
	return static_cast<uint32_t>(readBytes);
}

// Write a data block to a file.
// STATIC
uint32_t IO::write(const int file, const char* name, const uint64_t offset, uint8_t* data, const uint32_t size) _THROW_(SparrowException) {
	SPARROW_ENTER("IO::write");

	// On Windows, we can only write a number of bytes multiple of the sector size,
	// so adjust size to sector size and zero extra bytes.
	// Note 1: this happens only when writing the last block of the file.
	// Note 2: the size is adjusted even on Unix, where this is maybe not necessary,
	// to keep the files platform-independent.
	const uint32_t adjustedSize = static_cast<uint32_t>(FileUtil::adjustSizeToSectorSize(size));
	memset(data + size, 0, adjustedSize - size);
	int retries = 0;
	while (true) {
		bool ok = true;
		bool retry = false;
#ifdef _WIN32
		OVERLAPPED* overlapped = IOContext::getOverlapped();
		overlapped->Offset = (DWORD)offset;
		overlapped->OffsetHigh = (DWORD)(offset >> 32);
		HANDLE handle = my_get_osfhandle(file);
		DWORD writtenBytes = 0;
		if (WriteFile(handle, data, adjustedSize, &writtenBytes, overlapped) == 0) {
			if (GetLastError() != ERROR_IO_PENDING
				|| GetOverlappedResult(handle, overlapped, &writtenBytes, true) == 0) {
				ok = false;
				retry = (GetLastError() == ERROR_DISK_FULL || (writtenBytes < adjustedSize && writtenBytes > 0));
			}
		}
#elif defined(__MACH__)
		ssize_t writtenBytes = ::pwrite(file, data, adjustedSize, offset);
		ok = writtenBytes != static_cast<ssize_t>(adjustedSize);
		retry = errno == ENOSPC || (writtenBytes < adjustedSize && writtenBytes > 0);
#elif defined(__linux__)
		ssize_t writtenBytes = 0;
		if (sparrow_async_io) {
			IOContext& ctx = IOContext::get(1);
			struct iocb** iocb = ctx.getIocb();
			io_prep_pwrite(iocb[0], file, data, adjustedSize, offset);
			int result = io_submit(ctx.get(), 1, iocb);
			if (result == 1) {
				ok = true;
			} else {
				ok = false;
				errno = -result;
			}
			while (ok) {
				struct io_event* events = ctx.getEvents();
				result = io_getevents(ctx.get(), 1, 1, events, 0);
				if (result == 1) {
					int		result = (int)events->res;
					if (result < 0) {
						errno = -result;
						ok = false;
						retry = errno == ENOSPC;
					} else if (events->res2 == 0) {
						writtenBytes = events->res;
						if (writtenBytes < adjustedSize && writtenBytes > 0) {
							retry = true;
						}
						break;
					} else {
						errno = -events->res2;
						ok = false;
						retry = errno == ENOSPC;
					}
					break;
				} else if (result != -EAGAIN && result != -EINTR) {
					errno = -result;
					ok = false;
				}
			}
		} else {
			writtenBytes = ::pwrite(file, data, adjustedSize, offset);
			ok = writtenBytes == static_cast<ssize_t>(adjustedSize);
			retry = errno == ENOSPC || (writtenBytes < adjustedSize && writtenBytes > 0);
		}
#elif defined(__SunOS)
		ssize_t writtenBytes = 0;
		if (sparrow_async_io) {
			IOContext& ctx = IOContext::get(1);
			aiocb64_t* iocb = ctx.getIocb();
			iocb->aio_reqprio = 0;
			iocb->aio_lio_opcode = LIO_WRITE;
			iocb->aio_fildes = file;
			iocb->aio_nbytes = adjustedSize;
			iocb->aio_buf = data;
			iocb->aio_offset = offset;
			ok = aio_write64(iocb) == 0;
			port_event_t* events = ctx.getEvents();
			if (ok) {
				ok = port_get(ctx.getPort(), events, 0) == 0;
			}
			if (ok) {
				iocb = (aiocb64_t*)events->portev_object;
				const int check = iocb->aio_resultp.aio_errno;
				if (check == 0) {
					writtenBytes = aio_return64(iocb);
					if (writtenBytes < adjustedSize && writtenBytes > 0) {
						retry = true;
					}
				} else {
					errno = check;
					ok = false;
					retry = errno == ENOSPC;
				}
			}
		} else {
			writtenBytes = ::pwrite(file, data, adjustedSize, offset);
			ok = writtenBytes == static_cast<ssize_t>(adjustedSize);
			retry = errno == ENOSPC || (writtenBytes < adjustedSize && writtenBytes > 0);
		}
#else
#error Platform not supported
#endif
		if (retry && retries++ < 10) {
			if (retries == 1) {
				spw_print_information("Writing %u bytes in file %s failed. Only %u bytes were written (%s). Triggering purge.",
					adjustedSize, name, (uint)writtenBytes, strerror(errno));
			}
			Purge::wakeUp(true);
			my_sleep(1000000);
		} else {
			if (!ok) {
				throw SparrowException::create(true, "Cannot write %u bytes to file %s at offset %llu", adjustedSize, name, static_cast<ulonglong>(offset));
			}
			if (writtenBytes != adjustedSize) {
				throw SparrowException::create(false, "Incomplete write file %s (%ld bytes written instead of %u)", name, writtenBytes, adjustedSize);
			}
			break;
		}
	}
	return adjustedSize;
}

// STATIC
void IO::flush(const int file, const char* name) _THROW_(SparrowException) {
#ifdef _WIN32
	HANDLE handle = my_get_osfhandle(file);
	const bool ok = FlushFileBuffers(handle) != 0;
#else
	const bool ok = fsync(file) == 0;
#endif
	if (!ok) {
		throw SparrowException::create(true, "Cannot flush file %s", name);
	}
}

}

