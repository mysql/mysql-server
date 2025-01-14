#ifndef _spw_api_bufferlist_h_
#define _spw_api_bufferlist_h_

#include "include/global.h"
#include "vec.h"
#include "serial.h"
#include "misc.h"


namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BufferList
//////////////////////////////////////////////////////////////////////////////////////////////////////

// List of byte buffers of fixed size (no overflow mechanism to automatically grow)
class BufferList
{
private:
	SYSvector<RefByteBuffer>	buffers_;
	SYSvector<uint32_t>			offsets_;

	uint32_t	capacity_;			// Total size of all buffers in the list
	uint32_t	position_;
	uint32_t	size_;				// Size of each buffer in the list

	int					buffer_;			// index of current buffer
	RefByteBuffer		currentBuffer_;

	int		savedBuffer_;
	uint32_t	savedPosition_;
	uint32_t	savedBufferPosition_;

	ByteBuffer	temp_;		

	ByteBuffer& getTemp() {
		temp_.position(0);
		return temp_;
	}

	void put(const ByteBuffer& value, bool extend);

public:
	BufferList(uint32_t capacity, uint32_t size);
	~BufferList();

	void clear();
	ByteBuffer& makeRoom(uint32_t n) _THROW_(SparrowException);

	// Copies data in our buffer list, allocating new buffers as required
	void put(uint8_t value);
	void putShort(uint16_t value);
	void putInt(uint32_t value);
	void putLong(uint64_t value);
	void putDouble(double value);
	void put(const ByteBuffer& value);
	void put(const char* value);
	void put(const uint8_t* value, uint32_t length);

	// Appends ByteBuffer references to the end of our buffer list - no memory copy
	void append(RefByteBuffer value);
	void append(const SYSvector<RefByteBuffer>& value);

	void mark();
	void reset();

	uint32_t getPosition() const { return position_; }

	const SYSvector<RefByteBuffer>& getBuffers() const { return buffers_; }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ReadOnlyBufferList
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Makes a read-only copy of a list of buffers
class ReadOnlyBufferList
{
private:
	SYSvector<ByteBuffer>	buffers_;
	SYSvector<uint32_t>		offsets_;

	uint32_t	capacity_;
	uint32_t	position_;

	int				buffer_;			// Current buffer
	const uint8_t*	currentData_;
	uint32_t			currentStart_;
	uint32_t			currentEnd_;

public:
	ReadOnlyBufferList(const SYSvector<ByteBuffer>& buffers) _THROW_(SparrowException);

	uint8_t get(uint32_t pos) _THROW_(SparrowException);

	uint32_t getPosition() const { return position_; }
};

}		// namespace Sparrow

#endif	// #define _spw_api_bufferlist_h_
