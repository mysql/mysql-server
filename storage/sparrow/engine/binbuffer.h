/*
	Buffer for binary strings.
	May be stored on disk when becoming too large to be kept in memory.
*/

#ifndef _engine_binbuffer_h_
#define _engine_binbuffer_h_

#include "fileutil.h"
#include "types.h"
#include "serial.h"
#include "list.h"
#include "hash.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// GrowingByteBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Byte buffer that can grow in memory.
class GrowingByteBuffer : public ByteBuffer, public ByteBufferOverflow {
private:

	static const uint32_t initialSize_;

public:

	GrowingByteBuffer() : ByteBuffer(0, 0, this) {
	}

	// Clears this buffer.
	void clear() {
		position(0);
		limit(0);
		delete [] data_;
		data_ = 0;
	}

	~GrowingByteBuffer() {
		clear();
	}

	void overflow()  override _THROW_(SparrowException);

	bool end() const override {
		return position() >= limit();
	}

	virtual void extended(const uint8_t* oldData, const uint8_t* newData) {
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BinString
//////////////////////////////////////////////////////////////////////////////////////////////////////

#define TRANSIENT ULLONG_MAX

/* References a string. The object does not own the string: it is not allocated or freed here.
	If it is used to reference a string from a PERSISTENT partition, offset_ gives the offset into the 
	SPS file of the start of the string and length_ gives the string length. data_ is unused.
	If it is used to reference a string from a TRANSIENT partition, data_ points to the start of the string
	and length_ gives the string length. offset_ is unused. 
*/

class BinString {
private:

	const uint8_t* data_;
	uint32_t length_;
	uint64_t offset_;

public:

	BinString() : data_(0), length_(0), offset_(TRANSIENT) {
	}

	BinString(const uint8_t* data, const uint32_t length) : data_(data), length_(length), offset_(TRANSIENT) {
	}

	uint64_t getPosition(const uint8_t* base) const {
		return offset_ == TRANSIENT ? static_cast<uint64_t>(data_ - base) : offset_;
	}

	const uint8_t* getData() const {
		return data_;
	}

	uint32_t getLength() const {
		return length_;
	}

	bool isSmall() const {
		return length_ <= 16;
	}

	uint64_t getOffset() const {
		return offset_;
	}

	void setOffset(const uint64_t offset) {
		offset_ = offset;
	}

	// In case the string moves in memory.
	void rebase(const uint8_t* oldBase, const uint8_t* newBase) {
		const uint64_t position = getPosition(oldBase);
		data_ = newBase + position;
	}

	// Compare from right to left.
	bool operator == (const BinString& right) const {
		if (this == &right) {
			return true;
		}
		if (length_ == right.length_) {
			uint32_t i = length_;
			while (i-- > 0) {
				if (data_[i] != right.data_[i]) {
					return false;
				}
			}
			return true;
		}
		return false;
	}

	// Java-like hash code.
	uint32_t hash() const {
		uint32_t h = 1;
		for (uint32_t i = 0; i < length_; ++i) {
			h = 31 * h + data_[i];
		}
		return h;
	}

	void read(FileReader& reader, ByteBuffer& buffer);

	uint32_t write(ByteBuffer& buffer) const;
};

typedef SYSxvector<BinString*> BinStrings;

inline uint32_t BinString::write(ByteBuffer& buffer) const {
	uint32_t length = getLength();
	uint32_t count = 0;
	for (;;) {
		uint8_t v = length & 0x7f;
		length >>= 7;
		const bool end = length == 0;
		buffer << static_cast<uint8_t>(end ? v : (v | 0x80));
		++count;
		if (end) {
			break;
		}
	}
	buffer << ByteBuffer(getData(), static_cast<uint64_t>(getLength()));
	return count;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BinBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* Buffer containing unique byte strings. The strings are all stored, one after the other, in the 
	GrowingByteBuffer. The SYShash<BinString> hash_ allows for a quick lookup of the strings the buffer contains.
	Each BinString object references a string: 
	. If the buffer comes from a TRANSIENT partition, the BinString objects contain a pointer into this buffer 
		of the start of each string and the string length.
	. If the buffer was read from a PERSISTENT partition, the BinString objects contain the offset into the file
		of the start of each string and the string length.
*/
class FileWriter;
class BinBuffer : public GrowingByteBuffer {
	friend ByteBuffer& operator << (ByteBuffer& buffer, const BinBuffer& binBuffer);

private:

	// Hash of byte strings.
	SYShash<BinString, SYShPoolAllocator<BinString> > hash_;

public:

	BinBuffer() : hash_(65536) {
	}

	void clear() {
		hash_.clear();
		GrowingByteBuffer::clear();
	}

	void extended(const uint8_t* oldData, const uint8_t* newData) override {
		// Resize has changed buffer location: need to update all pointers in hash.
		SYShashIterator<BinString, SYShPoolAllocator<BinString> > iterator(hash_);
		while (++iterator) {
			iterator.key().rebase(oldData, newData);
		}
	}

	bool end() const override {
		return false;
	}

	// Inserts a new byte string.
	BinString* insert(const uint8_t* data, const uint32_t length) {
		if (data_ == 0) {	// Initial size.
			limit_ = 1024;
			data_ = new uint8_t[limit_];
		}
		const BinString key(data, length);
		// If it's not already referenced in our buffer, add it and add a reference to it into hash_. 
		BinString* binString = hash_.find(key);
		if (binString == 0) {
			*this << ByteBuffer(data, length);
			binString = hash_.insertAndReturn(BinString(data_ + pos_ - length, length));
		}
		return binString;
	}

	void optimize(FileReader& reader, const uint64_t size);

	uint64_t flush(FileWriter& writer);

	// Gets the memory used by this buffer.
	int64_t getSize() const {
		return limit() + hash_.getSize();
	}
};

inline ByteBuffer& operator << (ByteBuffer& buffer, const BinBuffer& binBuffer) {
	buffer << ByteBuffer(binBuffer.getData(), binBuffer.position());
	return buffer;
}

}

#endif /* #ifndef _engine_binbuffer_h_ */
