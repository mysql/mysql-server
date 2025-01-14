#include "my_sys.h"
#include "serial.h"
#include "str.h"

#ifdef _WIN32
#pragma warning(disable:4355)
#else
#include <sys/mman.h>
#endif

#include "my_io.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ByteBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

// ByteBuffers require page-aligned memory to perform e.g. direct I/O.
// STATIC
uint8_t* ByteBuffer::mmap(const uint32_t size) {
#ifdef _WIN32
	uint8_t* buffer = static_cast<uint8_t*>(VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
	uint8_t* buffer = reinterpret_cast<uint8_t*>(::mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));
	if (buffer == MAP_FAILED) {
		buffer = 0;
	}
#endif
	if (buffer == 0) {
		SparrowException e = SparrowException::create(true, SPW_API_OUT_OF_MEMORY, "Cannot allocate %u bytes", size);
		e.toLog();
		exit(1);
	}
	return buffer;
}

// STATIC
bool ByteBuffer::munmap(uint8_t* buffer, const uint32_t size) {
#ifdef _WIN32
	const BOOL result = VirtualFree(buffer, 0, MEM_RELEASE);
#else
	const bool result = ::munmap(reinterpret_cast<char*>(buffer), size) == 0;
#endif
	if (!result) {
		SparrowException e = SparrowException::create(true, SPW_API_FAILED, "Cannot free %u bytes at address %p", size, buffer);
		e.toLog();
		exit(1);
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// HeapBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

/*HeapBuffer::HeapBuffer() : ByteBuffer(new uint8_t[1024], 1024, this) {
}*/

HeapBuffer::HeapBuffer(const uint32_t limit /* = 1024 */) 
	: ByteBuffer(new uint8_t[limit], limit, this) {
}

HeapBuffer& HeapBuffer::operator = ( const HeapBuffer& buffer ) {
	if ( &buffer == this ) return *this;

	delete [] data_;
	pos_ = 0;

	data_ = new uint8_t[buffer.limit_];
	memcpy(data_, buffer.data_, buffer.limit_);
	limit_ = buffer.limit_;
	return *this;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketReader
//////////////////////////////////////////////////////////////////////////////////////////////////////

SocketReader::SocketReader(my_socket socketId, ByteBuffer& buffer) _THROW_(SparrowException)
	: ByteBuffer(buffer, this), socket_(socketId), bytesToRead_(buffer.limit()), bytesRead_(0) {
	overflow();
}

void SocketReader::overflow() _THROW_(SparrowException) {
	const int length = static_cast<int>(bytesToRead_) - bytesRead_;
	if (length == 0) {
		return;
	} else if (length < 0) {
		throw SparrowException("Reached end of stream");
	}
	int received = recv(socket_, reinterpret_cast<char*>(getData() + bytesRead_), length, 0);
#ifdef _WIN32
	if (GetLastError() == WSAECONNRESET) {
		// Treat connection reset as a normal close.
		received = 0;
	}
#endif
	if (received == -1) {
		throw SparrowException::create(true, SPW_API_SOCKET_READ_ERR, "Error while reading data from socket");
	} else if (received == 0) {
		throw SparrowException("Connection closed", false, SPW_API_SOCKET_CONN_CLOSED);
	}
	bytesRead_ += received;
	limit(bytesRead_);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketWriter
//////////////////////////////////////////////////////////////////////////////////////////////////////

const uint32_t SocketWriter::size_ = 16 * 1024 * 1024;

SocketWriter::SocketWriter(my_socket socketId)
	: ByteBuffer(ByteBuffer::mmap(SocketWriter::size_), SocketWriter::size_, this), socket_(socketId)
{
	if ( socketId == INVALID_SOCKET )
		throw SparrowException( "Invalid socket. Can't use it to send data.", true, SPW_API_SOCKET_CONN_CLOSED );
}

SocketWriter::~SocketWriter() {
	flush();
	ByteBuffer::munmap(getData(), SocketWriter::size_);
}

void SocketWriter::send(const ByteBuffer& v) _THROW_(SparrowException) {

	if ( socket_ == INVALID_SOCKET )
		throw SparrowException( "Invalid socket. Can't send data.", true, SPW_API_SOCKET_CONN_CLOSED );

	// Sends currently buffered data
	flush();
	position(0);

	// Sends the content of ByteBuffer v directly (does not make a copy into our internal buffer before).
	const uint32_t limit = v.limit();
	// If empty buffer, no need to make another send call
	if ( limit > 0 ) {
		const int sent = ::send(socket_, reinterpret_cast<const char*>(v.getData()), static_cast<int>(limit), 0);
		if (sent == -1) {
			PRINT_DBUG("[spw_Connection::process] send failed");
			throw SparrowException::create(true, SPW_API_SOCKET_WRITE_ERR, "Error while writing data to socket");
		}
	}
}

void SocketWriter::flush() _THROW_(SparrowException)
{
	if ( socket_ == INVALID_SOCKET )
		throw SparrowException( "Invalid socket. Can't flush data buffer.", true, SPW_API_SOCKET_CONN_CLOSED );

	uint32_t length = 0;
	while (length < position()) {
		const int sent = ::send(socket_, reinterpret_cast<const char*>(getData() + length), static_cast<int>(position() - length), 0);
		if (sent == -1) {
			PRINT_DBUG("[spw_Connection::process] send failed");
			throw SparrowException::create(true, SPW_API_SOCKET_WRITE_ERR, "Error while writing data to socket");
		}
		length += sent;
	}
}

}
