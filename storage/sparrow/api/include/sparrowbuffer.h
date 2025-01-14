#ifndef _spw_api_sparrowbuffer_h_
#define _spw_api_sparrowbuffer_h_

#include "global.h"
#include "table.h"

namespace Sparrow
{

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SparrowRow
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* All row data that are inserted through the Sparrow API must inherit from the SparrowRow interface 
	and implement the decode() method. That method uses the addXXX methods from SparrowBuffer to
	format row data for insertion into Sparrow. There should be one addXXX call per column in the table, 
	in the same order as the column were created.
*/

class SparrowBuffer;
class SparrowRow
{
public:
	virtual ~SparrowRow() {}

	// Use dummy to pass whatever optional client context object necessary for storing the 
	//	row into the buffer
	virtual int decode(SparrowBuffer* buffer, void* dummy) const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SparrowBuffer
//////////////////////////////////////////////////////////////////////////////////////////////////////

class SparrowBuffer
{
public:
	virtual ~SparrowBuffer() {}

	// Called by the client application's implementation of SparrowRow::decode()
	virtual int addNull(int column) = 0;
	virtual int addBool(int column, bool value) = 0;
	virtual int addByte(int column, uint8_t value) = 0;
	virtual int addShort(int column, uint16_t value) = 0;
	virtual int addInt(int column, uint32_t value) = 0;
	virtual int addLong(int column, uint64_t value) = 0;
	virtual int addDouble(int column, double value) = 0;
	virtual int addString(int column, const char* value) = 0;
	virtual int addBlob(int column, const uint8_t* value, uint32_t length) = 0;

	// Called by the client application
	virtual int addRow(const SparrowRow& row, void* dummy=NULL) = 0;

	// Frees all buffers
	virtual void clear() = 0;

	// Returns true if empty
	virtual bool isEmpty() const = 0;

	// Returns size in bytes
	virtual uint32_t getSize() const = 0;

	// Returns number of rows
	virtual uint32_t getRows() const = 0;

	// Returns time of insertion of the first row
	virtual uint64_t getTimestamp() const = 0;

	// Returns true if the data has been stored in buffer for more than 'delay' (in seconds)
	virtual bool hasExpired(uint32_t delay) const = 0;
};


}	// namespace Sparrow

#endif		// #define _spw_api_sparrowbuffer_h_
