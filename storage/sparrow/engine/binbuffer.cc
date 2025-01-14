/*
	Buffer for binary strings.
	May be stored on disk when becoming too large to be kept in memory.
*/

#include "binbuffer.h"
#include "fileutil.h"

#include "../engine/log.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BinString
//////////////////////////////////////////////////////////////////////////////////////////////////////

void BinString::read(FileReader& reader, ByteBuffer& buffer) {
	length_ = 0;
	for (uint32_t offset = 0; ; offset += 7) {
		uint8_t v;
		reader >> v;
		length_ |= (v & 0x7f) << offset;
		if ((v & 0x80) == 0) {
			break;
		}
	}
	offset_ = reader.getFileOffset();
	// Ensure there is at least length_ bytes available in buffer
	buffer.advance(length_);
	data_ = buffer.getCurrentData() - length_;
	ByteBuffer tmp(data_, static_cast<uint64_t>(length_));
	reader >> tmp;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BinBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

void BinBuffer::optimize(FileReader& reader, const uint64_t size) {
	GrowingByteBuffer buffer;
	const uint64_t limit = reader.getFileOffset() + size;
	while (reader.getFileOffset() < limit) {
		buffer.position(0);
		BinString s;
		s.read(reader, buffer);
		BinString* found = hash_.find(s);
		if (found != 0) {
			found->setOffset(s.getOffset());
		}
	}
}

uint64_t BinBuffer::flush(FileWriter& writer) {
	SYShashIterator<BinString, SYShPoolAllocator<BinString> > iterator(hash_);
	const uint64_t save = writer.getFileOffset();
	while (++iterator) {
		BinString& s = iterator.key();
		if (!s.isSmall() && s.getOffset() == TRANSIENT) {
			const uint64_t offset = writer.getFileOffset();
			const uint32_t length = s.write(writer);
			s.setOffset(offset + length);
		}
	}
	return writer.getFileOffset() - save;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// GrowingByteBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

const uint32_t GrowingByteBuffer::initialSize_ = 1024;

// Increase buffer size (X2). 
void GrowingByteBuffer::overflow() _THROW_(SparrowException) {
	const uint8_t* oldData = data_;
	const bool initialized = limit_ != 0;
	const uint64_t newLimit = initialized ? limit_ * 2 : GrowingByteBuffer::initialSize_;
	uint8_t* newData = new uint8_t[newLimit];
	if (newData == 0) {
		spw_print_error("GrowingByteBuffer::overflow: cannot allocate %llu bytes of memory", static_cast<ulonglong>(newLimit));
	}
	if (initialized) {
		memcpy(newData, oldData, limit_);
	}
	limit_ = newLimit;
	data_ = newData;
	if (initialized) {
		// Virtual method to notify derived classes of the buffer relocation. For example, if a derived class
		//	held pointers into this buffer, it needs to relocate them. 
		extended(oldData, newData);
		delete [] oldData;
	}
}

}

