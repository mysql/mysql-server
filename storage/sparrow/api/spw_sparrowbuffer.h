#ifndef _spw_api_impl_sparrowbuffer_h_
#define _spw_api_impl_sparrowbuffer_h_

//#include "include/sparrowbuffer.h"
#include "include/exceptwrapper.h"
#include "spw_types.h"
#include "bufferlist.h"

namespace Sparrow
{

//////////////////////////////////////////////////////////////////////////////////////////////////////
// spw_SparrowBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

class spw_SparrowBuffer : public SparrowBuffer
{
private:
	BufferList	buffer_;
	Columns		columns_;
	uint32_t		rows_;
	uint64_t		timestamp_;			// Number of seconds since 01/01/1970
	const Table* table_;			// Only to pass column description to SparrowRow::decode()
	uint32_t		cursor_;

	void checkCompatibility(int column, ColumnType inType) _THROW_(SparrowException);

public:
	spw_SparrowBuffer(const Table* table, uint32_t capacity);
	~spw_SparrowBuffer();

	// Those methods pass along any SparrowException from BufferList methods
	int addNull(int column) override;
	int addBool(int column, bool value) override;
	int addByte(int column, uint8_t value) override;
	int addShort(int column, uint16_t value) override;
	int addInt(int column, uint32_t value) override;
	int addLong(int column, uint64_t value) override;
	int addDouble(int column, double value) override;
	int addString(int column, const char* value) override;
	int addBlob(int column, const uint8_t* value, uint32_t length) override;

	void clear() override;
	bool isEmpty() const override { return buffer_.getPosition() == 0; }
	uint32_t getSize() const override { return buffer_.getPosition(); }
	uint32_t getRows() const override { return rows_; }
	uint64_t getTimestamp() const override { return timestamp_; }
	bool hasExpired(uint32_t delay) const override {
		time_t	now = time(0);
		return !isEmpty() && timestamp_ + delay < static_cast<uint64_t>(now);
	}

	int addRow(const SparrowRow& row, void* dummy=NULL) override;

	const SYSvector<RefByteBuffer>& getBuffers() const { return buffer_.getBuffers(); }
};

}	// namespace Sparrow

#endif		// #define _spw_api_impl_sparrowbuffer_h_
