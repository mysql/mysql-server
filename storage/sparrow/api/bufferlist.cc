#include "str.h"
#include "bufferlist.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BufferList
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor for writable BufferList
BufferList::BufferList( uint32_t capacity, uint32_t size )
	: temp_(new uint8_t[8], 8)
{
	if ( capacity < size ) 
		throw SparrowException::create( false, SPW_API_BUFFER_FULL, "BufferList: illegal argument: capacity %u < size %u.", capacity, size );

	capacity_	= capacity;
	position_	= 0;
	size_		= size;
	buffer_		= -1;
}

BufferList::~BufferList()
{
	uint8_t*	temp = temp_.getData();
	if ( temp != NULL ) {
		delete [] temp;
	}
}

void BufferList::clear()
{
	position_		= 0;
	buffer_			= -1;
	currentBuffer_.reset();
	buffers_.clear();
}

void BufferList::mark()
{
	savedPosition_	= position_;
	savedBuffer_	= buffer_;
	if ( buffer_ >= 0 ) {
		savedBufferPosition_ = buffers_[buffer_]->position();
	} else {
		savedBufferPosition_ = 0;
	}
}

void BufferList::reset()
{
	position_	= savedPosition_;
	buffer_		= savedBuffer_;
	while ( static_cast<int>(buffers_.length()) > buffer_ + 1  ) {
		buffers_.removeLast();
	}
	if ( buffer_ >= 0 ) {
		currentBuffer_ = buffers_[buffer_];
		currentBuffer_->position(savedBufferPosition_);
	} else {
		currentBuffer_.reset();
	}
}

ByteBuffer& BufferList::makeRoom( uint32_t n ) _THROW_(SparrowException)
{
	if ( position_ + n > capacity_ )
		throw SparrowException::create( false, SPW_API_BUFFER_FULL, 
			"BufferList: Buffer overflow. Can't allocate %u more bytes."
			"Current position %u, capacity %u", n, position_, capacity_ );

	// Allocate one or more small buffers if necessary
	uint32_t	remaining = (currentBuffer_.get() == NULL ? 0 : currentBuffer_->remaining()); 
	while ( n > remaining ) {
		RefByteBuffer	b( new IOBuffer(size_) );
		buffers_.append( b );
		if ( n > size_ ) n-= size_;
		else n = 0;
	}
	if ( remaining == 0 && (buffer_ + 1) < static_cast<int>(buffers_.length()) ) {
		buffer_++;
		currentBuffer_ = buffers_[buffer_];
	}
	
	SPW_dbgASSERT(currentBuffer_.get() != NULL);
	return *currentBuffer_;
}

void BufferList::put( const ByteBuffer& value, bool extend )
{
	uint32_t	n = value.remaining();
	uint32_t	saved = n;
	ByteBuffer& buffer = (extend ? makeRoom(n) : *buffers_[buffer_]);
	if ( buffer.remaining() >= n ) {
		buffer << value;
	} else {
		uint32_t	offset = 0;
		while (true) {
			ByteBuffer& b = *buffers_[buffer_];
			uint32_t	length = std::min(b.remaining(), n);
			b << ByteBuffer( value.getData() + offset, length );
			offset += length;
			n -= length;
			if ( n == 0 ) {
				break;
			}
			buffer_++;
		}
	}
	if ( extend ) {
		position_ += saved;
	}
}

void BufferList::put(uint8_t value)
{
	ByteBuffer& buffer = makeRoom(1);
	buffer << value;
	position_ += 1;
}

void BufferList::putShort(uint16_t value)
{
	ByteBuffer& buffer = makeRoom(2);
	if ( buffer.remaining() >= 2 ) {
		buffer << value;
	} else {
		ByteBuffer&	temp = getTemp();
		uint32_t	limit = temp.limit();
		temp << value;
		temp.flip();
		put( temp, false );
		temp.limit( limit );
	}
	position_ += 2;
}

void BufferList::putInt(uint32_t value)
{
	ByteBuffer& buffer = makeRoom(4);
	if ( buffer.remaining() >= 4 ) {
		buffer << value;
	} else {
		ByteBuffer&		temp = getTemp();
		uint32_t	limit = temp.limit();
		temp << value;
		temp.flip();
		put( temp, false );
		temp.limit( limit );
	}
	position_ += 4;
}

void BufferList::putLong(uint64_t value)
{
	ByteBuffer& buffer = makeRoom(8);
	if ( buffer.remaining() >= 8 ) {
		buffer << value;
	} else {
		ByteBuffer&		temp = getTemp();
		uint32_t	limit = temp.limit();
		temp << value;
		temp.flip();
		put( temp, false );
		temp.limit( limit );
	}
	position_ += 8;
}

void BufferList::putDouble(double value)
{
	ByteBuffer& buffer = makeRoom(8);
	if ( buffer.remaining() >= 8 ) {
		buffer << value;
	} else {
		ByteBuffer&		temp = getTemp();
		uint32_t	limit = temp.limit();
		temp << value;
		temp.flip();
		put( temp, false );
		temp.limit( limit );
	}
	position_ += 8;
}


void BufferList::put(const ByteBuffer& value)
{
	put( value, true );
}

void BufferList::put(const uint8_t* value, uint32_t length)
{
	put( ByteBuffer(value, length), true );
}

void BufferList::put(const char* value)
{
	if ( !value ) return;
	uint32_t	length = static_cast<uint32_t>(strlen(value));
	putInt( length );
	put( reinterpret_cast<const uint8_t*>(value), length );
}

void BufferList::append( RefByteBuffer value )
{
	buffers_.append( value );
	position_ += value->position();
}

void BufferList::append(const SYSvector<RefByteBuffer>& value)
{
	for ( uint32_t i=0; i<value.length(); ++i ) {
		append( value[i] );
	}
}



//////////////////////////////////////////////////////////////////////////////////////////////////////
// ReadOnlyBufferList
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor for read-only ReadOnlyBufferList
ReadOnlyBufferList::ReadOnlyBufferList( const SYSvector<ByteBuffer>& buffers ) _THROW_(SparrowException)
{
	if ( buffers.length() == 0 )
		throw SparrowException::create( false, SPW_API_FAILED, "ReadOnlyBufferList: illegal argument: empty buffers" );

	position_		= 0;
	buffer_			= -1;
	currentData_	= NULL;
	currentStart_	= UINT_MAX32;
	currentEnd_		= 0;

	uint32_t capacity	= 0;
	uint32_t offset	= 0;
	for ( uint32_t i=0; i<buffers.length(); ++i ) {
		capacity += buffers[i].position();
		buffers_.append( buffers[i] );
		offsets_.append( offset );
		offset += buffers[i].position();
	}
	capacity_ = capacity;
}


uint8_t ReadOnlyBufferList::get( uint32_t pos ) _THROW_(SparrowException)
{
	if ( buffer_ >= 0 ) {
		if ( pos >= currentStart_ && pos < currentEnd_ ) {
			SPW_dbgASSERT(currentData_);
			return currentData_[pos];
		}
	}

	for ( buffer_=0; buffer_<static_cast<int>(buffers_.length()); ++buffer_ ) {
		const ByteBuffer&	buffer	= buffers_[buffer_];
		uint32_t				start	= offsets_[buffer_];
		uint32_t				end		= start + buffer.position();
		if ( pos >= start && pos < end ) {
			currentData_	= buffer.getData();
			currentStart_	= start;
			currentEnd_		= end;
			return currentData_[pos - start];
		}
	}
	throw SparrowException::create( false, SPW_API_FAILED, "ReadOnlyBufferList: Get(%u) out of range.", pos );
}


}		// namespace Sparrow
