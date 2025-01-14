/*
	Serialization.
*/

#ifndef _engine_serial_h_
#define _engine_serial_h_

#include "exception.h"
#include "interval.h"
#include "vec.h"
#include "lock.h"
#include "hash.h"

// Use little endian to get better performances on x86 and x64 (around 15% faster).
// If you change endianness, make sure the Java side is updated too (see SparrowBuffer.getByteOrder()).
// Of course, changing endianness breaks existing files.
#define SPARROW_LITTLE_ENDIAN 1

// Marshalling/unmarshalling macros. SLOW macros are used when memory crosses the buffer limit,
// FAST macros are used otherwise.

// Macros for little endian ordering.
//-----------------------------------

#define SLOW_STORE2_L(V) do {	put((uint8_t)(V)); put((uint8_t)((V) >> 8)); } while(0)
#define SLOW_STORE4_L(V) do {	put((uint8_t)(V)); put((uint8_t)((V) >> 8)); \
	put((uint8_t)((V) >> 16)); put((uint8_t)((V) >> 24)); } while(0)
#define SLOW_STORE8_L(V) do {	put((uint8_t)(V)); put((uint8_t)((V) >> 8)); \
	put((uint8_t)((V) >> 16)); put((uint8_t)((V) >> 24)); put((uint8_t)((V) >> 32)); \
	put((uint8_t)((V) >> 40)); put((uint8_t)((V) >> 48)); put((uint8_t)((V) >> 56)); } while(0)
#define SLOW_LOAD2_L(V) do { (V) = get(); V |= (get() << 8); } while(0)
#define SLOW_LOAD4_L(V) do { (V) = get(); V |= (get() << 8);  V |= (get() << 16); V |= (get() << 24); } while(0)
#define SLOW_LOAD8_L(V) do { (V) = (uint64_t)get(); V |= ((uint64_t)get() << 8); V |= ((uint64_t)get() << 16); V |= ((uint64_t)get() << 24); \
	V |= ((uint64_t)get() << 32); V |= ((uint64_t)get() << 40); V |= ((uint64_t)get() << 48); V |= ((uint64_t)get() << 56); } while(0)
#ifdef WORDS_BIGENDIAN		// Macros for OS that use Big Endian
#define FAST_STORE2_L(P,V) do { *(P) = (uint8_t)(V); *((P) + 1) = (uint8_t)((V) >> 8); } while(0)
#define FAST_STORE4_L(P,V) do { *(P) = (uint8_t)(V); *((P) + 1) = (uint8_t)((V) >> 8); \
	*((P) + 2) = (uint8_t)((V) >> 16); *((P) + 3) = (uint8_t)((V) >> 24); } while(0)
#define FAST_STORE8_L(P,V) do { *(P) = (uint8_t)(V); *((P) + 1) = (uint8_t)((V) >> 8);  \
	*((P) + 2) = (uint8_t)((V) >> 16); *((P) + 3) = (uint8_t)((V) >> 24); *((P) + 4) = (uint8_t)((V) >> 32); \
	*((P) + 5) = (uint8_t)((V) >> 40); *((P) + 6) = (uint8_t)((V) >> 48); *((P) + 7) = (uint8_t)((V) >> 56); } while(0)
#define FAST_LOAD2_L(P, V) do { uint16_t vtemp; uint8_t* ptemp = (uint8_t*)&vtemp; \
	*(ptemp) = *((P) + 1); *(ptemp + 1) = *(P); (V) = vtemp; } while(0)
#define FAST_LOAD4_L(P, V) do { uint32_t vtemp; uint8_t* ptemp = (uint8_t*)&vtemp; \
	*(ptemp) = *((P) + 3); *(ptemp + 1) = *((P) + 2); *(ptemp + 2) = *((P) + 1); *(ptemp + 3) = *(P); \
	(V) = vtemp; } while(0)
#define FAST_LOAD8_L(P, V) do { uint64_t vtemp; uint8_t* ptemp = (uint8_t*)&vtemp; \
	*(ptemp) = *((P) + 7); *(ptemp + 1) = *((P) + 6); *(ptemp + 2) = *((P) + 5); *(ptemp + 3) = *((P) + 4); \
	*(ptemp + 4) = *((P) + 3); *(ptemp + 5) = *((P) + 2); *(ptemp + 6) = *((P) + 1); *(ptemp + 7) = *(P); \
	(V) = vtemp; } while(0)
#else		// Macros for OS that use Little Endian
#define FAST_STORE2_L(P,V) do { *((uint16_t*)(P)) = (uint16_t)(V); } while(0)
#define FAST_STORE4_L(P,V) do { *((uint32_t*)(P)) = (uint32_t)(V); } while(0)
#define FAST_STORE8_L(P,V) do { *((uint64_t*)(P)) = (uint64_t)(V); } while(0)
#define FAST_LOAD2_L(P,V) do { V = *((uint16_t*)(P)); } while(0)
#define FAST_LOAD4_L(P,V) do { V = *((uint32_t*)(P)); } while(0)
#define FAST_LOAD8_L(P,V) do { V = *((uint64_t*)(P)); } while(0)
#endif

// Macros for big endian ordering.
//--------------------------------

#define SLOW_STORE2_B(V) do {	put((uint8_t)((V) >> 8)); put((uint8_t)(V)); } while(0)
#define SLOW_STORE4_B(V) do {	put((uint8_t)((V) >> 24)); put((uint8_t)((V) >> 16)); \
	put((uint8_t)((V) >> 8)); put((uint8_t)(V)); } while(0)
#define SLOW_STORE8_B(V) do {	put((uint8_t)((V) >> 56)); put((uint8_t)((V) >> 48)); \
	put((uint8_t)((V) >> 40)); put((uint8_t)((V) >> 32)); put((uint8_t)((V) >> 24)); \
	put((uint8_t)((V) >> 16)); put((uint8_t)((V) >> 8)); put((uint8_t)(V)); } while(0)
#define SLOW_LOAD2_B(V) do { (V) = (get() << 8); V |= get(); } while(0)
#define SLOW_LOAD4_B(V) do { (V) = (get() << 24); V |= (get() << 16); V |= (get() << 8); V |= get(); } while(0)
#define SLOW_LOAD8_B(V) do { (V) = ((uint64_t)get() << 56); V |= ((uint64_t)get() << 48); V |= ((uint64_t)get() << 40); \
	V |= ((uint64_t)get() << 32); V |= ((uint64_t)get() << 24); V |= ((uint64_t)get() << 16); V |= ((uint64_t)get() << 8); V |= (uint64_t)get(); } while(0)
#ifdef WORDS_BIGENDIAN		// Macros for OS that use Big Endian
#define FAST_STORE2_B(P,V) do { memcpy((uint8_t*)(P), (uint8_t*)(&V), 2); } while(0)
#define FAST_STORE3_B(P,V) do { memcpy((uint8_t*)(P), (uint8_t*)(&V), 3); } while(0)
#define FAST_STORE4_B(P,V) do { memcpy((uint8_t*)(P), (uint8_t*)(&V), 4); } while(0)
#define FAST_STORE8_B(P,V) do { memcpy((uint8_t*)(P), (uint8_t*)(&V), 8); } while(0)
#define FAST_LOAD2_B(P, V) do { memcpy((uint8_t*)(&V), (uint8_t*)(P), 2); } while(0)
#define FAST_LOAD3_B(P, V) do { (V) = 0; memcpy((uint8_t*)(&V), (uint8_t*)(P), 3); } while(0)
#define FAST_LOAD4_B(P, V) do { memcpy((uint8_t*)(&V), (uint8_t*)(P), 4); } while(0)
#define FAST_LOAD8_B(P, V) do { memcpy((uint8_t*)(&V), (uint8_t*)(P), 8); } while(0)
#else		// Macros for OS that use Little Endian
#define FAST_STORE2_B(P,V) do { *(P) = (uint8_t)((V) >> 8); *((P) + 1) = (uint8_t)(V); } while(0)
#define FAST_STORE3_B(P,V) do { *(P) = (uint8_t)((V) >> 16); *((P) + 1) = (uint8_t)((V) >> 8); \
	*((P) + 2) = (uint8_t)(V); } while(0)
#define FAST_STORE4_B(P,V) do { *(P) = (uint8_t)((V) >> 24); *((P) + 1) = (uint8_t)((V) >> 16); \
	*((P) + 2) = (uint8_t)((V) >> 8); *((P) + 3) = (uint8_t)(V); } while(0)
#define FAST_STORE8_B(P,V) do { *(P) = (uint8_t)((V) >> 56); *((P) + 1) = (uint8_t)((V) >> 48);  \
	*((P) + 2) = (uint8_t)((V) >> 40); *((P) + 3) = (uint8_t)((V) >> 32); *((P) + 4) = (uint8_t)((V) >> 24); \
	*((P) + 5) = (uint8_t)((V) >> 16); *((P) + 6) = (uint8_t)((V) >> 8); *((P) + 7) = (uint8_t)(V); } while(0)
#define FAST_LOAD2_B(P, V) do { uint16_t vtemp; uint8_t* ptemp = (uint8_t*)&vtemp; \
	*(ptemp) = *((P) + 1); *(ptemp + 1) = *(P); (V) = vtemp; } while(0)
#define FAST_LOAD3_B(P, V) do { uint32_t vtemp = 0; uint8_t* ptemp = (uint8_t*)&vtemp; \
	*(ptemp) = *((P) + 2); *(ptemp + 1) = *((P) + 1); *(ptemp + 2) = *(P); \
	(V) = vtemp; } while(0)
#define FAST_LOAD4_B(P, V) do { uint32_t vtemp; uint8_t* ptemp = (uint8_t*)&vtemp; \
	*(ptemp) = *((P) + 3); *(ptemp + 1) = *((P) + 2); *(ptemp + 2) = *((P) + 1); *(ptemp + 3) = *(P); \
	(V) = vtemp; } while(0)
#define FAST_LOAD8_B(P, V) do { uint64_t vtemp; uint8_t* ptemp = (uint8_t*)&vtemp; \
	*(ptemp) = *((P) + 7); *(ptemp + 1) = *((P) + 6); *(ptemp + 2) = *((P) + 5); *(ptemp + 3) = *((P) + 4); \
	*(ptemp + 4) = *((P) + 3); *(ptemp + 5) = *((P) + 2); *(ptemp + 6) = *((P) + 1); *(ptemp + 7) = *(P); \
	(V) = vtemp; } while(0)
#endif

// Macros to encode Sparrow data.

#ifdef SPARROW_LITTLE_ENDIAN
#define SLOW_STORE2(V) SLOW_STORE2_L(V)
#define SLOW_STORE4(V) SLOW_STORE4_L(V)
#define SLOW_STORE8(V) SLOW_STORE8_L(V)
#define SLOW_LOAD2(V) SLOW_LOAD2_L(V)
#define SLOW_LOAD4(V) SLOW_LOAD4_L(V)
#define SLOW_LOAD8(V) SLOW_LOAD8_L(V)
#define FAST_STORE2(P,V) FAST_STORE2_L(P,V)
#define FAST_STORE4(P,V) FAST_STORE4_L(P,V)
#define FAST_STORE8(P,V) FAST_STORE8_L(P,V)
#define FAST_LOAD2(P, V) FAST_LOAD2_L(P, V)
#define FAST_LOAD4(P, V) FAST_LOAD4_L(P, V)
#define FAST_LOAD8(P, V) FAST_LOAD8_L(P, V)
#else
#define SLOW_STORE2(V) SLOW_STORE2_B(V)
#define SLOW_STORE4(V) SLOW_STORE4_B(V)
#define SLOW_STORE8(V) SLOW_STORE8_B(V)
#define SLOW_LOAD2(V) SLOW_LOAD2_B(V)
#define SLOW_LOAD4(V) SLOW_LOAD4_B(V)
#define SLOW_LOAD8(V) SLOW_LOAD8_B(V)
#define FAST_STORE2(P,V) FAST_STORE2_B(P,V)
#define FAST_STORE4(P,V) FAST_STORE4_B(P,V)
#define FAST_STORE8(P,V) FAST_STORE8_B(P,V)
#define FAST_LOAD2(P, V) FAST_LOAD2_B(P, V)
#define FAST_LOAD4(P, V) FAST_LOAD4_B(P, V)
#define FAST_LOAD8(P, V) FAST_LOAD8_B(P, V)
#endif

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// ByteBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Abstract class to handle buffer overflows.
class ByteBufferOverflow {
public:

	virtual ~ByteBufferOverflow() {
	}
	virtual void overflow() _THROW_(SparrowException) = 0;
	virtual bool end() const = 0;
};

class ByteBuffer {
protected:

	uint8_t* data_;
	uint64_t limit_;
	uint64_t pos_;
	ByteBufferOverflow* overflow_;
	uint32_t version_;

#ifdef _WIN32
	static volatile bool lockError_;
	static bool canLock_;
#endif

private:

	void overflow() _THROW_(SparrowException);
	uint8_t get();
	void put(const uint8_t v);

public:

	ByteBuffer(const uint8_t* data, const uint64_t limit);
	ByteBuffer(uint8_t* data, const uint64_t limit, ByteBufferOverflow* overflow = 0, const uint32_t version = UINT_MAX);
	ByteBuffer(const ByteBuffer& buffer, ByteBufferOverflow* overflow);

	uint8_t* getData();
	uint8_t* getCurrentData();
	const uint8_t* getData() const;
	const uint8_t* getCurrentData() const;
	void position(const uint64_t pos);
	uint64_t position() const;
	uint64_t limit() const;
	void limit(const uint64_t newLimit);
	bool end() const;
	void advance(uint64_t offset);
	uint32_t getVersion() const;
	void setVersion(const uint32_t version);

	ByteBuffer& operator << (const uint8_t v);
	ByteBuffer& operator << (const int8_t v);
	ByteBuffer& operator << (const uint16_t v);
	ByteBuffer& operator << (const int16_t v);
	ByteBuffer& operator << (const uint32_t v);
	ByteBuffer& operator << (const int32_t v);
	ByteBuffer& operator << (const uint64_t v);
	ByteBuffer& operator << (const int64_t v);
	ByteBuffer& operator << (const double v);
	ByteBuffer& operator << (const bool v);
	ByteBuffer& operator << (const ByteBuffer& v);
	ByteBuffer& operator << (const char* v);

	ByteBuffer& operator >> (uint8_t& v);
	ByteBuffer& operator >> (int8_t& v);
	ByteBuffer& operator >> (uint16_t& v);
	ByteBuffer& operator >> (int16_t& v);
	ByteBuffer& operator >> (uint32_t& v);
	ByteBuffer& operator >> (int32_t& v);
	ByteBuffer& operator >> (uint64_t& v);
	ByteBuffer& operator >> (int64_t& v);
	ByteBuffer& operator >> (double& v);
	ByteBuffer& operator >> (bool& v);
	ByteBuffer& operator >> (ByteBuffer& v);

	static void initialize();
	static uint8_t* mmap(const uint32_t size);
	static bool munmap(uint8_t* buffer, const uint32_t size);
};

inline void ByteBuffer::overflow() _THROW_(SparrowException) {
	if (overflow_ == 0) {
		throw SparrowException::create(false, "Buffer overflow (limit=%llu bytes)", static_cast<ulonglong>(limit_));
	} else {
		overflow_->overflow();
	}
}

inline uint8_t ByteBuffer::get() {
	if (pos_ >= limit_) {
		overflow();
	}
	return data_[pos_++];
}

inline void ByteBuffer::put(const uint8_t v) {
	if (pos_ >= limit_) {
		overflow();
	}
	data_[pos_++] = v;
}

inline ByteBuffer::ByteBuffer(const uint8_t* data, const uint64_t limit)
	: data_(const_cast<uint8_t*>(data)), limit_(limit), pos_(0), overflow_(0), version_(UINT_MAX) {
}

inline ByteBuffer::ByteBuffer(uint8_t* data, const uint64_t limit, ByteBufferOverflow* overflow /* = 0 */, const uint32_t version /* = UINT_MAX */)
	: data_(data), limit_(limit), pos_(0), overflow_(overflow), version_(version) {
}

inline ByteBuffer::ByteBuffer(const ByteBuffer& buffer, ByteBufferOverflow* overflow) {
	data_ = buffer.data_;
	limit_ = buffer.limit_;
	pos_ = buffer.pos_;
	version_ = buffer.version_;
	overflow_ = overflow;
}

inline uint8_t* ByteBuffer::getData() {
	return data_;
}

inline uint8_t* ByteBuffer::getCurrentData() {
	return data_ + pos_;
}

inline const uint8_t* ByteBuffer::getData() const {
	return data_;
}

inline const uint8_t* ByteBuffer::getCurrentData() const {
	return data_ + pos_;
}

inline void ByteBuffer::position(const uint64_t pos) {
	pos_ = pos;
}

inline uint64_t ByteBuffer::position() const {
	return pos_;
}

inline uint64_t ByteBuffer::limit() const {
	return limit_;
}

inline void ByteBuffer::limit(const uint64_t newLimit) {
	limit_ = newLimit;
}

inline bool ByteBuffer::end() const {
	return overflow_ == 0 ? (pos_ == limit_) : overflow_->end();
}

inline void ByteBuffer::advance(uint64_t offset) {
	while (offset > 0) {
		if (pos_ >= limit_) {
			overflow();
		}
		const uint64_t length = std::min(limit_ - pos_, offset);
		pos_ += length;
		offset -= length;
	}
}

inline uint32_t ByteBuffer::getVersion() const {
	return version_;
}

inline void ByteBuffer::setVersion(const uint32_t version) {
	version_ = version;
}

inline ByteBuffer& ByteBuffer::operator << (const uint8_t v) {
	put(v);
	return *this;
}

inline ByteBuffer& ByteBuffer::operator << (const int8_t v) {
	return *this << static_cast<uint8_t>(v);
}

inline ByteBuffer& ByteBuffer::operator << (const uint16_t v) {
	const uint64_t npos = pos_ + 2;
	if (npos > limit_) {
		SLOW_STORE2(v);
	} else {
		uint8_t* p = data_ + pos_;
		FAST_STORE2(p, v);
		pos_ = npos;
	}
	return *this;
}

inline ByteBuffer& ByteBuffer::operator << (const int16_t v) {
	return *this << static_cast<uint16_t>(v);
}

inline ByteBuffer& ByteBuffer::operator << (const uint32_t v) {
	const uint64_t npos = pos_ + 4;
	if (npos > limit_) {
		SLOW_STORE4(v);
	} else {
		uint8_t* p = data_ + pos_;
		FAST_STORE4(p, v);
		pos_ = npos;
	}
	return *this;
}

inline ByteBuffer& ByteBuffer::operator << (const int32_t v) {
	return *this << static_cast<uint32_t>(v);
}

inline ByteBuffer& ByteBuffer::operator << (const uint64_t v) {
	const uint64_t npos = pos_ + 8;
	if (npos > limit_) {
		SLOW_STORE8(v);
	} else {
		uint8_t* p = data_ + pos_;
		FAST_STORE8(p, v);
		pos_ = npos;
	}
	return *this;
}

inline ByteBuffer& ByteBuffer::operator << (const int64_t v) {
	return *this << static_cast<uint64_t>(v);
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif 

inline ByteBuffer& ByteBuffer::operator << (const double v) {
	return *this << *reinterpret_cast<const uint64_t*>(&v);
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif 

inline ByteBuffer& ByteBuffer::operator << (const bool v) {
	return *this << static_cast<uint8_t>(v ? 1 : 0);
}

inline ByteBuffer& ByteBuffer::operator << (const ByteBuffer& v) {
	const uint64_t limit = v.limit_;
	uint64_t pos = v.pos_;
	while (pos < limit) {
		if (pos_ >= limit_) {
			overflow();
		}
		const uint64_t length = std::min(limit_ - pos_, limit - pos);
		memcpy(data_ + pos_, v.data_ + pos, length);
		pos_ += length;
		pos += length;
	}
	return *this;
}

inline ByteBuffer& ByteBuffer::operator << (const char* v) {
	return *this << ByteBuffer(reinterpret_cast<const uint8_t*>(v), static_cast<uint32_t>(strlen(v)));
}

inline ByteBuffer& ByteBuffer::operator >> (uint8_t& v) {
	v = get();
	return *this;
}

inline ByteBuffer& ByteBuffer::operator >> (int8_t& v) {
	return *this >> *(reinterpret_cast<uint8_t*>(&v));
}

inline ByteBuffer& ByteBuffer::operator >> (uint16_t& v) {
	const uint64_t npos = pos_ + 2;
	if (npos > limit_) {
		SLOW_LOAD2(v);
	} else {
		uint8_t* p = data_ + pos_;
		FAST_LOAD2(p, v);
		pos_ = npos;
	}
	return *this;
}

inline ByteBuffer& ByteBuffer::operator >> (int16_t& v) {
	return *this >> *(reinterpret_cast<uint16_t*>(&v));
}

inline ByteBuffer& ByteBuffer::operator >> (uint32_t& v) {
	const uint64_t npos = pos_ + 4;
	if (npos > limit_) {
		SLOW_LOAD4(v);
	} else {
		uint8_t* p = data_ + pos_;
		FAST_LOAD4(p, v);
		pos_ = npos;
	}
	return *this;
}

inline ByteBuffer& ByteBuffer::operator >> (int32_t& v) {
	return *this >> *(reinterpret_cast<uint32_t*>(&v));
}

inline ByteBuffer& ByteBuffer::operator >> (uint64_t& v) {
	const uint64_t npos = pos_ + 8;
	if (npos > limit_) {
		SLOW_LOAD8(v);
	} else {
		uint8_t* p = data_ + pos_;
		FAST_LOAD8(p, v);
		pos_ = npos;
	}
	return *this;
}

inline ByteBuffer& ByteBuffer::operator >> (int64_t& v) {
	return *this >> *(reinterpret_cast<uint64_t*>(&v));
}

inline ByteBuffer& ByteBuffer::operator >> (double& v) {
	return *this >> *(reinterpret_cast<uint64_t*>(&v));
}

inline ByteBuffer& ByteBuffer::operator >> (bool& v) {
	uint8_t b;
	*this >> b;
	v = (b != 0);
	return *this;
}

inline ByteBuffer& ByteBuffer::operator >> (ByteBuffer& v) {
	while (v.pos_ < v.limit_) {
		if (pos_ >= limit_) {
			overflow();
		}
		const uint64_t length = std::min(limit_ - pos_, v.limit_ - v.pos_);
		memcpy(v.data_ + v.pos_, data_ + pos_, length);
		pos_ += length;
		v.pos_ += length;
	}
	return *this;
}

template<typename T> inline ByteBuffer& operator >> (ByteBuffer& buffer, Interval<T>& interval) {
	T low;
	bool lowerSet;
	buffer >> lowerSet;
	if (lowerSet) {
		buffer >> low;
	}
	T up;
	bool upperSet;
	buffer >> upperSet;
	if (upperSet) {
		buffer >> up;
	}
	bool lowerIncluded, upperIncluded;
	buffer >> lowerIncluded >> upperIncluded;
	interval = Interval<T>(lowerSet ? &low : 0, upperSet ? &up : 0, lowerIncluded, upperIncluded);
	return buffer;
}

template<typename T> inline ByteBuffer& operator << (ByteBuffer& buffer, const Interval<T>& interval) {
	const T* low = interval.getLow();
	buffer << (low != 0);
	if (low != 0) {
		buffer << *low;
	}
	const T* up = interval.getUp();
	buffer << (up != 0);
	if (up != 0) {
		buffer << *up;
	}
	buffer << interval.isLowerIncluded() << interval.isUpperIncluded();
	return buffer;
}

template<typename T> inline ByteBuffer& operator << (ByteBuffer& buffer, const SYSarray<T>& v) {
	buffer << static_cast<uint32_t>(v.length());
	for (uint32_t i = 0; i < v.length(); ++i) {
		buffer << v[i];
	}
	return buffer;
}

template<typename T> inline ByteBuffer& operator >> (ByteBuffer& buffer, SYSarray<T>& v) {
	uint32_t length;
	buffer >> length;
	v = SYSarray<T>(length);
	for (uint32_t i = 0; i < length; ++i) {
		buffer >> v[i];
	}
	return buffer;
}

template<typename T, uint32_t G> inline ByteBuffer& operator << (ByteBuffer& buffer, const SYSvector<T, G>& v) {
	buffer << static_cast<uint32_t>(v.length());
	for (uint32_t i = 0; i < v.length(); ++i) {
		buffer << v[i];
	}
	return buffer;
}

template<typename T, uint32_t G> inline ByteBuffer& operator >> (ByteBuffer& buffer, SYSvector<T, G>& v) {
	uint32_t length;
	buffer >> length;
	v.resize(length);
	for (uint32_t i = 0; i < length; ++i) {
		T t;
		buffer >> t;
		v.append(t);
	}
	return buffer;
}

template<typename T, uint32_t G> inline ByteBuffer& operator << (ByteBuffer& buffer, const SYSpVector<T, G>& v) {
	buffer << static_cast<uint32_t>(v.length());
	for (uint32_t i = 0; i < v.length(); ++i) {
		buffer << *(v[i]);
	}
	return buffer;
}

template<typename T, uint32_t G> inline ByteBuffer& operator >> (ByteBuffer& buffer, SYSpVector<T, G>& v) {
	uint32_t length;
	buffer >> length;
	v.resize(length);
	for (uint32_t i = 0; i < length; ++i) {
		T* t = new T();
		buffer >> *t;
		v.append(t);
	}
	return buffer;
}

template<typename T, uint32_t G> inline ByteBuffer& operator << (ByteBuffer& buffer, const SYSpSortedVector<T, G>& v) {
	buffer << static_cast<uint32_t>(v.length());
	for (uint32_t i = 0; i < v.length(); ++i) {
		buffer << *(v[i]);
	}
	return buffer;
}

template<typename T, uint32_t G> inline ByteBuffer& operator >> (ByteBuffer& buffer, SYSpSortedVector<T, G>& v) {
	uint32_t length;
	buffer >> length;
	v.resize(length);
	for (uint32_t i = 0; i < length; ++i) {
		T* t = new T();
		buffer >> *t;
		v.append(t);
	}
	return buffer;
}

template<class T, class A> inline ByteBuffer& operator << (ByteBuffer& buffer, const SYShash<T,A>& h) {
	buffer << h.entries();
	SYShashIterator<T,A> iterator(h);
	while (++iterator) {
		buffer << iterator.key();
	}
	return buffer;
}

template<class T, class A> inline ByteBuffer& operator >> (ByteBuffer& buffer, SYShash<T,A>& h) {
	uint32_t entries;
	buffer >> entries;
	for (uint32_t i = 0; i < entries; ++i) {
		T entry;
		buffer >> entry;
		h.insert(entry);
	}
	return buffer;
}

template<class T, class A> inline ByteBuffer& operator << (ByteBuffer& buffer, const SYSpHash<T,A>& h) {
	buffer << h.entries();
	SYSpHashIterator<T,A> iterator(h);
	while (++iterator) {
		buffer << *iterator.key();
	}
	return buffer;
}

template<class T, class A> inline ByteBuffer& operator >> (ByteBuffer& buffer, SYSpHash<T,A>& h) {
	uint32_t entries;
	buffer >> entries;
	for (uint32_t i = 0; i < entries; ++i) {
		T* entry = new T();
		buffer >> *entry;
		h.insert(entry);
	}
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PrintBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

class PrintBuffer : public ByteBuffer, public ByteBufferOverflow {
public:

	PrintBuffer();

	virtual ~PrintBuffer() {
		delete [] data_;
	}

	void overflow() override _THROW_(SparrowException) {
		uint8_t* save = data_;
		data_ = new uint8_t[limit_ * 2];
		memcpy(data_, save, limit_);
		limit_ *= 2;
		delete [] save;
	}

	bool end() const override {
		return false;
	}
};

PrintBuffer& operator << (PrintBuffer& buffer, const char* v);
class Str;
PrintBuffer& operator << (PrintBuffer& buffer, const Str& v);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketReader
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Connection;
class SocketReader : public ByteBuffer, public ByteBufferOverflow {
private:

	Connection& connection_;
	const uint32_t bytesToRead_;
	int bytesRead_;

public:

	SocketReader(Connection& connection, ByteBuffer& buffer) _THROW_(SparrowException);

	virtual ~SocketReader() {
	}

	void overflow() override _THROW_(SparrowException);

	bool end() const override {
		return bytesRead_ == static_cast<int>(bytesToRead_);
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SocketWriter
//////////////////////////////////////////////////////////////////////////////////////////////////////

class SocketWriter : public ByteBuffer, public ByteBufferOverflow {
private:

	Connection& connection_;

public:

	SocketWriter(Connection& connection);

	virtual ~SocketWriter();

	void overflow() override _THROW_(SparrowException) {
		flush();
		position(0);
	}

	void flush() _THROW_(SparrowException);

	bool end() const override {
		return false;	// No EOF when writing.
	}
};

}

#endif /* #ifndef _engine_serial_h_ */
