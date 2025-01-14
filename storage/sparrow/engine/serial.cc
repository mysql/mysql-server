/*
	Serialization.
*/

#include "types.h"
#include "serial.h"
#include "listener.h"
#include "../handler/plugin.h"		// For configuration parameters.
#include "io.h"
#include "../engine/log.h"
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#pragma warning(disable:4355)
#else
#include <sys/mman.h>
#endif

#include "sql/mysqld.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ByteBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
volatile bool ByteBuffer::lockError_ = false;
bool ByteBuffer::canLock_ = false;
#endif

// Initialize memory locking if --memlock option is enabled.
// STATIC
void ByteBuffer::initialize() {
	SPARROW_ENTER("ByteBuffer::initialize");
	if (!locked_in_memory) {
		spw_print_information("Sparrow is not using memory locked pages because memlock option is not enabled");
		return;
	}
	[[maybe_unused]] const char* warning = "Sparrow cannot use memory locked pages: %s";
#ifdef _WIN32
	try {
		// Set max working set size to the largest possible value.
		MEMORYSTATUSEX memStat;
		memStat.dwLength = sizeof (memStat);
		if (!GlobalMemoryStatusEx(&memStat)) {
			throw SparrowException::create(true, "Unable to get memory status");
		}
		uint64_t mb = static_cast<uint32_t>(memStat.ullTotalPhys / 1024 / 1024);
		HANDLE h = GetCurrentProcess();
		while (true) {
			mb -= 512;
			if (SetProcessWorkingSetSize(h, static_cast<SIZE_T>(mb * 1024 * 1024), static_cast<SIZE_T>(mb * 1024 * 1024))) {
#ifndef NDEBUG
				SIZE_T minWorkingSet;
				SIZE_T maxWorkingSet;
				GetProcessWorkingSetSize(h, &minWorkingSet, &maxWorkingSet);
				DBUG_PRINT("sparrow_memory", ("Set process WS size; min=%u KB, max=%u KB", static_cast<uint32_t>(minWorkingSet / 1024),
					 static_cast<uint32_t>(maxWorkingSet / 1024)));
#endif
				break;
			} else if (mb <= 512) {
				throw SparrowException::create(true, "Unable to set working set size");
			}
		}
	} catch(const SparrowException& e) {
		spw_print_warning(warning, e.getText());
		return;
	}
	HANDLE hToken;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
		return;
	}
	const TCHAR* privName = TEXT("SeLockMemoryPrivilege");
	TOKEN_PRIVILEGES tp;
	if (!LookupPrivilegeValue(0, privName, &tp.Privileges[0].Luid)) {
		return;
	}
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	const bool status = AdjustTokenPrivileges(hToken, false, &tp, 0, 0, 0);

	// It is possible for AdjustTokenPrivileges to return true and still not succeed.  
	// So always check for the last error value.
	if (!status || GetLastError() != ERROR_SUCCESS) {
		SparrowException e = SparrowException::create(true, "Unable to set privilege SeLockMemoryPrivilege");
		spw_print_warning(warning, e.getText());
		return;
	}
	if (!CloseHandle(hToken)) {
		return;
	}
	canLock_ = true;
#else
	if (::mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
		[[maybe_unused]] SparrowException e = SparrowException::create(true, "mlockall error");
		spw_print_warning(warning, e.getText());
		return;
	}
#endif
	spw_print_information("Sparrow is using memory locked pages");
}

// ByteBuffers require page-aligned memory to perform e.g. direct I/O.
// STATIC
uint8_t* ByteBuffer::mmap(const uint32_t size) {
#ifdef _WIN32
	uint8_t* buffer = static_cast<uint8_t*>(VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

	// Lock pages in memory if possible.
	if (canLock_ && buffer != 0) {
		try {
			if (!VirtualLock(buffer, size)) {
				throw SparrowException::create(true, "VirtualLock error");
			}
		} catch(const SparrowException& e) {
			if (!lockError_) {
				lockError_ = true;
				spw_print_warning("Sparrow: Memory locking failed: %s", e.getText());
			}
		}
	}
#else
	uint8_t* buffer = reinterpret_cast<uint8_t*>(::mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));
	if (buffer == MAP_FAILED) {
		buffer = 0;
	}
#endif
	if (buffer == 0) {
		SparrowException e = SparrowException::create(true, "Cannot allocate %u bytes", size);
		e.toLog();
		exit(1);
	}
	return buffer;
}

// STATIC
bool ByteBuffer::munmap(uint8_t* buffer, const uint32_t size) {
#ifdef _WIN32
	const bool result = VirtualFree(buffer, 0, MEM_RELEASE);
#else
	const bool result = ::munmap(reinterpret_cast<char*>(buffer), size) == 0;
#endif
	if (!result) {
		SparrowException e = SparrowException::create(true, "Cannot free %u bytes at address %p", size, buffer);
		e.toLog();
		my_abort();
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PrintBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

PrintBuffer::PrintBuffer() : ByteBuffer(new uint8_t[1024], 1024, this) {
}

PrintBuffer& operator << (PrintBuffer& buffer, const char* v) {
	static_cast<ByteBuffer&>(buffer) << v;
	return buffer;
}

PrintBuffer& operator << (PrintBuffer& buffer, const Str& v) {
	return buffer << v.c_str();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketReader
//////////////////////////////////////////////////////////////////////////////////////////////////////

SocketReader::SocketReader(Connection& connection, ByteBuffer& buffer) _THROW_(SparrowException)
	: ByteBuffer(buffer, this), connection_(connection), bytesToRead_(static_cast<uint32_t>(buffer.limit())), bytesRead_(0) {
	overflow();
}

void SocketReader::overflow() _THROW_(SparrowException) {
	const int length = static_cast<int>(bytesToRead_) - bytesRead_;
	if (length == 0) {
		return;
	} else if (length < 0) {
		throw SparrowException("Reached end of stream", false);
	}
	int received = 0;
	if (!connection_.isClosed()) {
		received = recv(connection_.getSocket(), reinterpret_cast<char*>(getData() + bytesRead_), length, 0);
#ifdef _WIN32
		if (GetLastError() == WSAECONNRESET) {
			// Treat connection reset as a normal close.
			received = 0;
		}
#endif
	}
	if (received == -1) {
		throw SparrowException::create(true, "Error while reading data from socket");
	} else if (received == 0) {
		throw SparrowException("Connection closed", false, 0);
	}
	bytesRead_ += received;
	limit(bytesRead_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketWriter
//////////////////////////////////////////////////////////////////////////////////////////////////////

SocketWriter::SocketWriter(Connection& connection)
	: ByteBuffer(IOContext::getBuffer(sparrow_transfer_block_size), this), connection_(connection) {
}

SocketWriter::~SocketWriter() {
	flush();
}

void SocketWriter::flush() _THROW_(SparrowException) {
	uint32_t length = 0;
	while (length < position()) {
		bool closed = false;
		if (connection_.isClosed()) {
			closed = true;
		} else {
			const int sent = send(connection_.getSocket(), reinterpret_cast<const char*>(getData() + length), static_cast<int>(position() - length), 0);
			if (sent == -1) {
#ifdef _WIN32
				if (GetLastError() == WSAECONNRESET) {
					closed = true;
				} else
#endif
				throw SparrowException::create(true, "Error while writing data to socket");
			}
			length += sent;
		}
		if (closed) {
			throw SparrowException("Connection closed", false, 0);
		}
	}
}

}

