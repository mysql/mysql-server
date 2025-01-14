/*
	Fields mapping MySQL fields.
*/

#ifndef _handler_field_h_
#define _handler_field_h_

#include "sql/field.h"
#include "../engine/fileutil.h"
#include "../engine/types.h"
#include "../engine/binbuffer.h"


// These warnings are triggered by the way we serialize / deserialize values to/from the 
//	data partition. This is work-as-designed, so these warnings should be ignored. 
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif 

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FieldBase
//////////////////////////////////////////////////////////////////////////////////////////////////////

/* Fields are used to read data from Sparrow partitions (persistent and transient), and offer some
	helper functions: compare field values, format a value to send back to MySQL. They mimic the 
	MySQL Field class. FieldBase is the base class, the interface. Each Sparrow type has its own
	implementation of the FieldBase interface.
*/
class FieldBase;
typedef SYSpVector<FieldBase, 0> TableFields;
class FieldBase {
protected:

	Column column_;

	// To fill MySQL buffers when not using key format.
	uint32_t nullOffset_;
	uint32_t nullBit_;
	uint32_t offset_;

#ifndef NDEBUG
	// If true, only key format is available.
	const bool onlyKeyFormat_;
#endif

private:

	static FieldType getFieldType(const uint32_t serial, const bool coalescing, const Column& column);
	static bool exists(const uint32_t serial, const Column& column);

public:

	static void createFields(const uint32_t serial, const bool coalescing, Field** myFields, const Columns& columns, TableFields& fields, const ColumnIds* skippedColumnIds = NULL);

	FieldBase(Field* myField, const Column& column);
	virtual ~FieldBase() {
	}
	const Column& getColumn() const {
		return column_;
	}
	bool isNullable() const {
		return column_.isFlagSet(COL_NULLABLE);
	}
	virtual bool isMapped() const = 0;
	virtual uint32_t getLength(const bool keyFormat) const = 0;	// Length in output buffer.
	virtual uint32_t getSize() const = 0;		// Size in data file.
	virtual uint32_t getBits() const = 0;
	virtual void skip(ByteBuffer& buffer) const = 0;
	virtual void readPersistent(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, uint8_t* buffer, const bool keyFormat) const = 0;
	virtual void readTransient(const uint64_t data, const bool isNull, uint8_t* buffer, const bool keyFormat) const = 0;
	virtual bool readMySqlTransient(const uint8_t* buffer, uint64_t& data) const = 0;
	virtual bool readMySqlPersistent(const uint8_t* buffer, ByteBuffer& output) const = 0;
	virtual void insertTransform(const uint8_t* buffer, ByteBuffer& output) const = 0;
	virtual int compare(const uint8_t* left, const uint8_t* right) const = 0;

	// Used by coalescing and index alteration.
	virtual void copy(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, ByteBuffer& buffer, BinBuffer* binBuffer) const = 0;
	virtual int compare(ByteBuffer& buffer1, const uint8_t bits1, PartitionReader& stringReader1,
		ByteBuffer& buffer2, const uint8_t bits2, PartitionReader& stringReader2, BinBuffer* binBuffer) const = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TableFieldsGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

class TableFieldsGuard {
private:

	TableFields fields_;
	bool owned_;

public:

	TableFieldsGuard() : owned_(true) {
	}

	void release() {
		owned_ = false;
	}

	~TableFieldsGuard() {
		if (owned_) {
			fields_.clearAndDestroy();
		}
	}

	TableFields& get() {
		return fields_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FieldTimestamp
//////////////////////////////////////////////////////////////////////////////////////////////////////

class FieldTimestamp : public FieldBase {
private:

	int	dec_;		// Number of decimals (0 to 6) if type is MYSQL_TYPE_TIMESTAMP2 (MySQL class Field_timestampf),
					// -1 if type is MYSQL_TYPE_TIMESTAMP (MySQL class Field_timestamp)

	uint64_t readMySql(const uint8_t* buffer, uint32_t offset=0) const;
	void writeMySql(uint64_t data, uint8_t* buffer, uint32_t offset=0) const;

public:

	FieldTimestamp(Field* myField, const Column& column)
		: FieldBase(myField, column) {
		if ( myField == 0 || myField->real_type() != MYSQL_TYPE_TIMESTAMP2 ) {
			dec_ = -1;
		} else {
			dec_ = myField->decimals();
		}
	}
	bool isMapped() const override {
		return true;
	}
	uint32_t getLength(const bool keyFormat) const override {		// Length in output buffer.
		// Non fractional part followed by fractional part.
		switch ( dec_ ) {
		case -1:
		case 0:	return 4;
		case 1:
		case 2:	return 4+1;
		case 3:
		case 4:	return 4+2;
		case 5:
		case 6:	return 4+3;
		}
		return 4;
	}
	uint32_t getSize() const override {		// Size in data file.
		return 8;
	}
	uint32_t getBits() const override {
		return column_.getBits();
	}
	void skip(ByteBuffer& buffer) const override {
		buffer.advance(column_.getDataSize());
	}
	void readPersistent(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, uint8_t* buffer, const bool keyFormat) const override;
	void readTransient(const uint64_t data, const bool isNull, uint8_t* buffer, const bool keyFormat) const override;
	bool readMySqlTransient(const uint8_t* buffer, uint64_t& data) const override;
	bool readMySqlPersistent(const uint8_t* buffer, ByteBuffer& output) const override;
	void insertTransform(const uint8_t* buffer, ByteBuffer& output) const override;
	int compare(const uint8_t* left, const uint8_t* right) const override;
	void copy(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, ByteBuffer& buffer, BinBuffer* binBuffer) const override;
	int compare(ByteBuffer& buffer1, const uint8_t bits1, PartitionReader& stringReader1,
		ByteBuffer& buffer2, const uint8_t bits2, PartitionReader& stringReader2, BinBuffer* buffer) const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FieldSimple
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T> class FieldSimple : public FieldBase {
private:

	static T convert(const uint64_t data);
	void marshall(uint8_t* buffer, const T v) const;
	uint64_t unmarshall(const uint8_t* buffer) const;

public:

	FieldSimple(Field* myField, const Column& column)
		: FieldBase(myField, column) {
	}
	bool isMapped() const override {
		return true;
	}
	uint32_t getLength(const bool keyFormat) const override {
		return sizeof(T);
	}
	uint32_t getSize() const override {
		return sizeof(T);
	}
	uint32_t getBits() const override {
		return column_.getBits();
	}
	void skip(ByteBuffer& buffer) const override {
		buffer.advance(column_.getDataSize());
	}
	void readPersistent(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, uint8_t* buffer, const bool keyFormat) const override;
	void readTransient(const uint64_t data, const bool isNull, uint8_t* buffer, const bool keyFormat) const override;
	bool readMySqlTransient(const uint8_t* buffer, uint64_t& data) const override;
	bool readMySqlPersistent(const uint8_t* buffer, ByteBuffer& output) const override;
	void insertTransform(const uint8_t* buffer, ByteBuffer& output) const override;
	static int compareSimple(const uint8_t* left, const uint8_t* right);
	int compare(const uint8_t* left, const uint8_t* right) const override {
		return compareSimple(left, right);
	}
	void copy(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, ByteBuffer& buffer, BinBuffer* binBuffer) const override;
	int compare(ByteBuffer& buffer1, const uint8_t bits1, PartitionReader& stringReader1,
		ByteBuffer& buffer2, const uint8_t bits2, PartitionReader& stringReader2, BinBuffer* buffer) const override;
};

template<typename T> inline T FieldSimple<T>::convert(const uint64_t data) {
	return static_cast<T>(data);
}

template<typename T> inline void FieldSimple<T>::readPersistent(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, uint8_t* buffer, const bool keyFormat) const {
	assert(keyFormat || !onlyKeyFormat_);
	T v;
	reader >> v;
	const bool isNull = (bits == 1);
	if (keyFormat) {
		if (isNull) {
			*buffer = 1;
		} else {
			if (isNullable()) {
				*buffer++ = 0;
			}
			marshall(buffer, v);
		}
	} else {
		if (isNull) {
			buffer[nullOffset_] |= nullBit_;
		} else {
			marshall(buffer + offset_, v);
		}
	}
}

template<typename T> inline void FieldSimple<T>::readTransient(const uint64_t data, bool isNull, uint8_t* buffer, bool keyFormat) const {
	assert(keyFormat || !onlyKeyFormat_);
	if (keyFormat) {
		if (isNull) {
			*buffer = 1;
		} else {
			if (isNullable()) {
				*buffer++ = 0;
			}
			marshall(buffer, convert(data));
		}
	} else {
		if (isNull) {
			buffer[nullOffset_] |= nullBit_;
		} else {
			marshall(buffer + offset_, convert(data));
		}
	}
}

template<typename T> inline bool FieldSimple<T>::readMySqlTransient(const uint8_t* buffer, uint64_t& data) const {
	assert(!onlyKeyFormat_);
	if (isNullable() && (buffer[nullOffset_] & nullBit_)) {
		return true;
	} else {
		data = unmarshall(buffer + offset_);
		return false;
	}
}

template<typename T> inline bool FieldSimple<T>::readMySqlPersistent(const uint8_t* buffer, ByteBuffer& output) const {
	assert(!onlyKeyFormat_);
	if (isNullable() && (buffer[nullOffset_] & nullBit_)) {
		output << static_cast<T>(0);
		return true;
	} else {
		const uint64_t v = unmarshall(buffer + offset_);
		if (sizeof(T) != sizeof(v)) {
			output << static_cast<T>(v);
		} else {
			output << v;
		}
		return false;
	}
}

template<typename T> inline void FieldSimple<T>::insertTransform(const uint8_t* buffer, ByteBuffer& output) const {
	assert(!onlyKeyFormat_);
	if (getColumn().isFlagSet(COL_AUTO_INC)) {
		return;
	}
	const bool nullable = isNullable();
	if (nullable && (buffer[nullOffset_] & nullBit_)) {
		output << static_cast<uint8_t>(1);
	} else {
		if (nullable) {
			output << static_cast<uint8_t>(0);
		}
		const uint64_t data = unmarshall(buffer + offset_);
		if (sizeof(T) != sizeof(data)) {
			output << static_cast<T>(data);
		} else {
			output << data;
		}
	}
}

template<typename T> inline void FieldSimple<T>::copy(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, ByteBuffer& buffer, BinBuffer* binBuffer) const {
	T v;
	reader >> v;
	buffer << v;
}

template<typename T> inline int FieldSimple<T>::compare(ByteBuffer& buffer1, const uint8_t bits1, PartitionReader& stringReader1,
	ByteBuffer& buffer2, const uint8_t bits2, PartitionReader& stringReader2, BinBuffer* binBuffer) const {
	const bool isNull1 = (bits1 == 1);
	const bool isNull2 = (bits2 == 1);
	if (isNull1) {
		buffer1.advance(sizeof(T));
		buffer2.advance(sizeof(T));
		if (isNull2) {
			return 0;
		} else {
			return -1;
		}
	} else if (isNull2) {
		buffer1.advance(sizeof(T));
		buffer2.advance(sizeof(T));
		return 1;
	}
	T l;
	buffer1 >> l;
	T r;
	buffer2 >> r;
	return (l < r) ? -1 : (l > r) ? 1 : 0;
}

// Implementations for different simple types.
template<> inline void FieldSimple<uint8_t>::marshall(uint8_t* buffer, const uint8_t v) const {
	*buffer = v;
}

template<> inline uint64_t FieldSimple<uint8_t>::unmarshall(const uint8_t* buffer) const {
	return static_cast<uint64_t>(*buffer);
}

// STATIC
template<> inline int FieldSimple<uint8_t>::compareSimple(const uint8_t* left, const uint8_t* right) {
	const uint8_t l = *left;
	const uint8_t r = *right;
	return (l < r) ? -1 : (l > r) ? 1 : 0;
}

template<> inline void FieldSimple<int8_t>::marshall(uint8_t* buffer, const int8_t v) const {
	*buffer = static_cast<uint8_t>(v);
}

template<> inline uint64_t FieldSimple<int8_t>::unmarshall(const uint8_t* buffer) const {
	return static_cast<uint64_t>(*buffer);
}

// STATIC
template<> inline int FieldSimple<int8_t>::compareSimple(const uint8_t* left, const uint8_t* right) {
	const int8_t l = static_cast<int8_t>(*left);
	const int8_t r = static_cast<int8_t>(*right);
	return (l < r) ? -1 : (l > r) ? 1 : 0;
}

template<> inline void FieldSimple<uint16_t>::marshall(uint8_t* buffer, const uint16_t v) const {
	FAST_STORE2_L(buffer, v);
}

template<> inline uint64_t FieldSimple<uint16_t>::unmarshall(const uint8_t* buffer) const {
	uint16_t v;
	FAST_LOAD2_L(buffer, v);
	return static_cast<uint64_t>(v);
}

// STATIC
template<> inline int FieldSimple<uint16_t>::compareSimple(const uint8_t* left, const uint8_t* right) {
	uint16_t l;
	FAST_LOAD2_L(left, l);
	uint16_t r;
	FAST_LOAD2_L(right, r);
	return (l < r) ? -1 : (l > r) ? 1 : 0;
}

template<> inline void FieldSimple<int16_t>::marshall(uint8_t* buffer, const int16_t v) const {
	FAST_STORE2_L(buffer, static_cast<uint16_t>(v));
}

template<> inline uint64_t FieldSimple<int16_t>::unmarshall(const uint8_t* buffer) const {
	uint16_t v;
	FAST_LOAD2_L(buffer, v);
	return static_cast<uint64_t>(v);
}

// STATIC
template<> inline int FieldSimple<int16_t>::compareSimple(const uint8_t* left, const uint8_t* right) {
	uint16_t l;
	FAST_LOAD2_L(left, l);
	uint16_t r;
	FAST_LOAD2_L(right, r);
	return (static_cast<int16_t>(l) < static_cast<int16_t>(r)) ? -1 : (static_cast<int16_t>(l) > static_cast<int16_t>(r)) ? 1 : 0;
}

template<> inline void FieldSimple<uint32_t>::marshall(uint8_t* buffer, const uint32_t v) const {
	FAST_STORE4_L(buffer, v);
}

template<> inline uint64_t FieldSimple<uint32_t>::unmarshall(const uint8_t* buffer) const {
	uint32_t v;
	FAST_LOAD4_L(buffer, v);
	return static_cast<uint64_t>(v);
}

// STATIC
template<> inline int FieldSimple<uint32_t>::compareSimple(const uint8_t* left, const uint8_t* right) {
	uint32_t l;
	FAST_LOAD4_L(left, l);
	uint32_t r;
	FAST_LOAD4_L(right, r);
	return (l < r) ? -1 : (l > r) ? 1 : 0;
}

template<> inline void FieldSimple<int32_t>::marshall(uint8_t* buffer, const int32_t v) const {
	FAST_STORE4_L(buffer, static_cast<uint32_t>(v));
}

template<> inline uint64_t FieldSimple<int32_t>::unmarshall(const uint8_t* buffer) const {
	uint32_t v;
	FAST_LOAD4_L(buffer, v);
	return static_cast<uint64_t>(v);
}

// STATIC
template<> inline int FieldSimple<int32_t>::compareSimple(const uint8_t* left, const uint8_t* right) {
	uint32_t l;
	FAST_LOAD4_L(left, l);
	uint32_t r;
	FAST_LOAD4_L(right, r);
	return (static_cast<int32_t>(l) < static_cast<int32_t>(r)) ? -1 : (static_cast<int32_t>(l) > static_cast<int32_t>(r)) ? 1 : 0;
}

template<> inline void FieldSimple<uint64_t>::marshall(uint8_t* buffer, const uint64_t v) const {
	FAST_STORE8_L(buffer, v);
}

template<> inline uint64_t FieldSimple<uint64_t>::unmarshall(const uint8_t* buffer) const {
	uint64_t v;
	FAST_LOAD8_L(buffer, v);
	return v;
}

// STATIC
template<> inline int FieldSimple<uint64_t>::compareSimple(const uint8_t* left, const uint8_t* right) {
	uint64_t l;
	FAST_LOAD8_L(left, l);
	uint64_t r;
	FAST_LOAD8_L(right, r);
	return (l < r) ? -1 : (l > r) ? 1 : 0;
}

template<> inline void FieldSimple<int64_t>::marshall(uint8_t* buffer, const int64_t v) const {
	FAST_STORE8_L(buffer, static_cast<uint64_t>(v));
}

template<> inline uint64_t FieldSimple<int64_t>::unmarshall(const uint8_t* buffer) const {
	uint64_t v;
	FAST_LOAD8_L(buffer, v);
	return v;
}

// STATIC
template<> inline int FieldSimple<int64_t>::compareSimple(const uint8_t* left, const uint8_t* right) {
	uint64_t l;
	FAST_LOAD8_L(left, l);
	uint64_t r;
	FAST_LOAD8_L(right, r);
	return (static_cast<int64_t>(l) < static_cast<int64_t>(r)) ? -1 : (static_cast<int64_t>(l) > static_cast<int64_t>(r)) ? 1 : 0;
}

template<> inline void FieldSimple<double>::marshall(uint8_t* buffer, const double v) const {
	const uint64_t l = *reinterpret_cast<const uint64_t*>(&v);
	FAST_STORE8_L(buffer, l);
}

template<> inline uint64_t FieldSimple<double>::unmarshall(const uint8_t* buffer) const {
	uint64_t v;
	FAST_LOAD8_L(buffer, v);
	return v;
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
// gcc 8.5.0 emits the following warning on variable dl, but i haven't been able to figure out why.
// Running the same code in another test program does not generate this warning. Is this warning
//	a gcc  bug ?
// warning: 'l' is used uninitialized [-Wuninitialized]
//  const double dl = *reinterpret_cast<double*>(&l);
#endif 

// STATIC
template<> inline int FieldSimple<double>::compareSimple(const uint8_t* left, const uint8_t* right) {
	uint64_t l = 0;
	FAST_LOAD8_L(left, l);
	uint64_t r(0);
	FAST_LOAD8_L(right, r);
	const double dl = *reinterpret_cast<double*>(&l);
	const double dr = *reinterpret_cast<double*>(&r);
	return (dl < dr) ? -1 : (dl > dr ? 1 : 0);
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif 


template<> inline double FieldSimple<double>::convert(const uint64_t data) {
	return *reinterpret_cast<const double*>(&data);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FieldString
//////////////////////////////////////////////////////////////////////////////////////////////////////

class FieldString : public FieldBase {
private:

	CHARSET_INFO* cs_;
	uint32_t lengthBytes_;
	uint32_t fieldLength_;

private:

	void store(uint8_t* buffer, const bool keyFormat, const uint8_t* data, const uint16_t length) const;

public:

	FieldString(Field* myField, const Column& column)
		: FieldBase(myField, column) {
		if (myField == 0) {
			cs_ = get_charset_by_name(column.getCharset().c_str(), MYF(MY_WME));
			lengthBytes_ = 0;
			fieldLength_ = 0;
		} else {
			cs_ = const_cast<CHARSET_INFO*>(myField->charset());
			lengthBytes_ = static_cast<Field_varstring*>(myField)->get_length_bytes();
			fieldLength_ = myField->field_length;
		}
	}
	bool isMapped() const override {
		return true;
	}
	uint32_t getLength(const bool keyFormat) const override {
		return (keyFormat ? 2 : lengthBytes_) + fieldLength_;
	}
	uint32_t getSize() const override {
		return 16;
	}
	uint32_t getBits() const override {
		return column_.getBits();
	}
	void skip(ByteBuffer& buffer) const override {
		buffer.advance(column_.getDataSize());
	}
	void readPersistent(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, uint8_t* buffer, const bool keyFormat) const override;
	void readTransient(const uint64_t data, const bool isNull, uint8_t* buffer, const bool keyFormat) const override;
	bool readMySqlTransient(const uint8_t* buffer, uint64_t& data) const override {
		assert(0);
		return false;
	}
	bool readMySqlPersistent(const uint8_t* buffer, ByteBuffer& output) const override {
		assert(0);
		return false;
	}
	void insertTransform(const uint8_t* buffer, ByteBuffer& output) const override;
	int compare(const uint8_t* left, const uint8_t* right) const override;
	void copy(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, ByteBuffer& buffer, BinBuffer* binBuffer) const override;
	int compare(ByteBuffer& buffer1, const uint8_t bits1, PartitionReader& stringReader1,
		ByteBuffer& buffer2, const uint8_t bits2, PartitionReader& stringReader2, BinBuffer* buffer) const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FieldSkip
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<uint32_t S> class FieldSkip : public FieldBase {
public:

	FieldSkip(const Column& column)
		: FieldBase(0, column) {
	}
	bool isMapped() const override {
		return false;
	}
	uint32_t getLength(const bool keyFormat) const override {
		return S;
	}
	uint32_t getSize() const override {
		return S;
	}
	uint32_t getBits() const override {
		return column_.getBits();
	}
	void skip(ByteBuffer& buffer) const override {
	}
	void readPersistent(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, uint8_t* buffer, const bool keyFormat) const override;
	void readTransient(const uint64_t data, const bool isNull, uint8_t* buffer, const bool keyFormat) const override;
	bool readMySqlTransient(const uint8_t* buffer, uint64_t& data) const override;
	bool readMySqlPersistent(const uint8_t* buffer, ByteBuffer& output) const override;
	void insertTransform(const uint8_t* buffer, ByteBuffer& output) const override;
	int compare(const uint8_t* left, const uint8_t* right) const override;
	void copy(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, ByteBuffer& buffer, BinBuffer* binBuffer) const override;
	int compare(ByteBuffer& buffer1, const uint8_t bits1, PartitionReader& stringReader1,
		ByteBuffer& buffer2, const uint8_t bits2, PartitionReader& stringReader2, BinBuffer* buffer) const override;
};

template<uint32_t S> inline void FieldSkip<S>::readPersistent(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, uint8_t* buffer, const bool keyFormat) const {
	reader.advance(S);
}

template<uint32_t S> inline void FieldSkip<S>::readTransient(const uint64_t data, bool isNull, uint8_t* buffer, bool keyFormat) const {
}

template<uint32_t S> inline bool FieldSkip<S>::readMySqlTransient(const uint8_t* buffer, uint64_t& data) const {
	assert(0);
	return false;
}

template<uint32_t S> inline bool FieldSkip<S>::readMySqlPersistent(const uint8_t* buffer, ByteBuffer& output) const {
	output.advance(sizeof(S));
	return false;
}

template<uint32_t S> inline void FieldSkip<S>::insertTransform(const uint8_t* buffer, ByteBuffer& output) const {
	assert(0);
}

template<uint32_t S> inline int FieldSkip<S>::compare(const uint8_t* left, const uint8_t* right) const {
	return 0;
}

template<uint32_t S> inline void FieldSkip<S>::copy(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, ByteBuffer& buffer, BinBuffer* binBuffer) const {
}

template<uint32_t S> inline int FieldSkip<S>::compare(ByteBuffer& buffer1, const uint8_t bits1, PartitionReader& stringReader1,
	ByteBuffer& buffer2, const uint8_t bits2, PartitionReader& stringReader2, BinBuffer* binBuffer) const {
	assert(0);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FieldDefault
//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef SYSarray<uint8_t> BinValue;
template<typename T> class FieldDefault : public FieldBase {
private:
	BinValue	value_;

private:

	void computeBinDefaultValue(Field* myField);
	void marshall(uint8_t* buffer, const bool keyFormat) const;

public:

	FieldDefault(Field* myField, const Column& column, const bool forceNull)
		: FieldBase(myField, column) {
		if (!forceNull) {
			computeBinDefaultValue(myField);
		}
	}
	bool isMapped() const override {
		return true;
	}
	uint32_t getLength(const bool keyFormat) const override {
		return sizeof(T);
	}
	uint32_t getSize() const override {
		return 0;
	}
	uint32_t getBits() const override {
		return 0;
	}
	void skip(ByteBuffer& buffer) const override {
	}
	void readPersistent(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, uint8_t* buffer, const bool keyFormat) const override;
	void readTransient(const uint64_t data, const bool isNull, uint8_t* buffer, const bool keyFormat) const override;
	bool readMySqlTransient(const uint8_t* buffer, uint64_t& data) const override;
	bool readMySqlPersistent(const uint8_t* buffer, ByteBuffer& output) const override;
	void insertTransform(const uint8_t* buffer, ByteBuffer& output) const override;
	int compare(const uint8_t* left, const uint8_t* right) const override;
	void copy(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, ByteBuffer& buffer, BinBuffer* binBuffer) const override;
	int compare(ByteBuffer& buffer1, const uint8_t bits1, PartitionReader& stringReader1,
		ByteBuffer& buffer2, const uint8_t bits2, PartitionReader& stringReader2, BinBuffer* buffer) const override;
};

template<typename T> inline void FieldDefault<T>::marshall(uint8_t* buffer, const bool keyFormat) const {
	const uint8_t* p = value_.data();
	const uint32_t length = value_.length();
	const bool isNull = length == 0;
	if (keyFormat) {
		if (isNull) {
			*buffer = 1;
		} else {
			if (isNullable()) {
				*buffer++ = 0;
			}
			memcpy(buffer, p, length);
		}
	} else {
		if (isNull) {
			buffer[nullOffset_] |= nullBit_;
		} else {
			buffer += offset_;
			memcpy(buffer, p, length);
		}
	}
}

template<typename T> inline void FieldDefault<T>::readPersistent(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, uint8_t* buffer, const bool keyFormat) const {
	assert(keyFormat || !onlyKeyFormat_);
	assert(bits == 0);
	marshall(buffer, keyFormat);
}

template<typename T> inline void FieldDefault<T>::readTransient(const uint64_t data, bool isNull, uint8_t* buffer, bool keyFormat) const {
	assert(keyFormat || !onlyKeyFormat_);
	marshall(buffer, keyFormat);
}

template<typename T> inline bool FieldDefault<T>::readMySqlTransient(const uint8_t* buffer, uint64_t& data) const {
	assert(0);
	return false;
}

template<typename T> inline bool FieldDefault<T>::readMySqlPersistent(const uint8_t* buffer, ByteBuffer& output) const {
	assert(0);
	return false;
}

template<typename T> inline void FieldDefault<T>::insertTransform(const uint8_t* buffer, ByteBuffer& output) const {
	assert(0);
}

template<typename T> inline void FieldDefault<T>::copy(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, ByteBuffer& buffer, BinBuffer* binBuffer) const {
}

template<typename T> inline int FieldDefault<T>::compare(ByteBuffer& buffer1, const uint8_t bits1, PartitionReader& stringReader1,
	ByteBuffer& buffer2, const uint8_t bits2, PartitionReader& stringReader2, BinBuffer* binBuffer) const {
	assert(0);
	return 0;
}

template<typename T> inline int FieldDefault<T>::compare(const uint8_t* left, const uint8_t* right) const {
	return FieldSimple<T>::compareSimple(left, right);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FieldDefaultString
//////////////////////////////////////////////////////////////////////////////////////////////////////

class FieldDefaultString : public FieldBase {
private:

	CHARSET_INFO* cs_;
	uint32_t lengthBytes_;
	uint32_t fieldLength_;
	BinValue value_;

private:

	void computeBinDefaultValue(Field* myField);
	void marshall(uint8_t* buffer, const bool keyFormat) const;

public:

	FieldDefaultString(Field* myField, const Column& column, const bool forceNull)
		: FieldBase(myField, column) {
		if (myField == 0) {
			cs_ = get_charset_by_name(column.getCharset().c_str(), MYF(MY_WME));
			lengthBytes_ = 0;
			fieldLength_ = 0;
		} else {
			cs_ = const_cast<CHARSET_INFO*>(myField->charset());
			lengthBytes_ = static_cast<Field_varstring*>(myField)->get_length_bytes();
			fieldLength_ = myField->field_length;
		}
		if (!forceNull) {
			computeBinDefaultValue(myField);
		}
	}
	bool isMapped() const override {
		return true;
	}
	uint32_t getLength(const bool keyFormat) const override {
		return (keyFormat ? 2 : lengthBytes_) + fieldLength_;
	}
	uint32_t getSize() const override {
		return 0;
	}
	uint32_t getBits() const override {
		return 0;
	}
	void skip(ByteBuffer& buffer) const override {
	}
	void readPersistent(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, uint8_t* buffer, const bool keyFormat) const override;
	void readTransient(const uint64_t data, const bool isNull, uint8_t* buffer, const bool keyFormat) const override;
	bool readMySqlTransient(const uint8_t* buffer, uint64_t& data) const override;
	bool readMySqlPersistent(const uint8_t* buffer, ByteBuffer& output) const override;
	void insertTransform(const uint8_t* buffer, ByteBuffer& output) const override;
	int compare(const uint8_t* left, const uint8_t* right) const override;
	void copy(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, ByteBuffer& buffer, BinBuffer* binBuffer) const override;
	int compare(ByteBuffer& buffer1, const uint8_t bits1, PartitionReader& stringReader1,
		ByteBuffer& buffer2, const uint8_t bits2, PartitionReader& stringReader2, BinBuffer* buffer) const override;
};

inline void FieldDefaultString::marshall(uint8_t* buffer, const bool keyFormat) const {
	const uint8_t* p = value_.data();
	const uint32_t length = value_.length();
	const bool isNull = length == 0;
	if (keyFormat) {
		if (isNull) {
			*buffer = 1;
		} else {
			if (isNullable()) {
				*buffer++ = 0;
			}
			memcpy(buffer, p, length);
		}
	} else {
		if (isNull) {
			buffer[nullOffset_] |= nullBit_;
		} else {
			buffer += offset_;
			if (lengthBytes_ == 2) {
				// Binary value is built with length on 2 bytes.
				memcpy(buffer, p, length);
			} else {
				uint16_t n;
				FAST_LOAD2_L(p, n);
				*buffer++ = static_cast<uint8_t>(n);
				memcpy(buffer, p + 2, length - 2);
			}
		}
	}
}

inline void FieldDefaultString::readPersistent(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, uint8_t* buffer, const bool keyFormat) const {
	marshall(buffer, keyFormat);
}

inline void FieldDefaultString::readTransient(const uint64_t data, bool isNull, uint8_t* buffer, bool keyFormat) const {
	marshall(buffer, keyFormat);
}

inline bool FieldDefaultString::readMySqlTransient(const uint8_t* buffer, uint64_t& data) const {
	assert(0);
	return false;
}

inline bool FieldDefaultString::readMySqlPersistent(const uint8_t* buffer, ByteBuffer& output) const {
	assert(0);
	return false;
}

inline void FieldDefaultString::insertTransform(const uint8_t* buffer, ByteBuffer& output) const {
	assert(0);
}

inline int FieldDefaultString::compare(const uint8_t* left, const uint8_t* right) const {
	uint16_t leftLength;
	FAST_LOAD2_L(left, leftLength);
	left += 2;
	uint16_t rightLength;
	FAST_LOAD2_L(right, rightLength);
	right += 2;
	return cs_->coll->strnncollsp(cs_, left, leftLength, right, rightLength);
}

inline void FieldDefaultString::copy(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, ByteBuffer& buffer, BinBuffer* binBuffer) const {
}

inline int FieldDefaultString::compare(ByteBuffer& buffer1, const uint8_t bits1, PartitionReader& stringReader1,
	ByteBuffer& buffer2, const uint8_t bits2, PartitionReader& stringReader2, BinBuffer* binBuffer) const {
	assert(0);
	return 0;
}

}

#endif /* #ifndef _handler_field_h_ */
