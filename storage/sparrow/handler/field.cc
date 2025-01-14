/*
	Fields mapping MySQL fields.
*/

#include "field.h"
#include "../engine/transient.h"
#include "../engine/fileutil.h"
//#include <sql_time.h>
//#include <tztime.h>

#include "sql/current_thd.h"
#include "sql/sql_class.h"
#include "sql/tztime.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FieldBase
//////////////////////////////////////////////////////////////////////////////////////////////////////

// STATIC
void FieldBase::createFields(const uint32_t serial, const bool coalescing, Field** myFields, const Columns& columns, 
	TableFields& fields, const ColumnIds* skippedColumnIds /* = NULL */) {
	fields.clearAndDestroy();
	const uint32_t nbColumns = columns.length();
	fields.resize(nbColumns);
	uint32_t	j = 0, k = 0;
	for (uint32_t i = 0; i < nbColumns; ++i) {
		const Column& column = columns[i];
		const ColumnType type = column.getType();
		const bool isUnsigned = column.isFlagSet(COL_UNSIGNED);
		FieldType fieldType = FieldBase::getFieldType(serial, coalescing, column);
		bool	forceNull = false;
		if (skippedColumnIds != NULL && (fieldType == FIELD_NORMAL || fieldType == FIELD_SKIP)) {
			// If this column was optimized out of this partition because all its values were NULL,
			for (uint l=0; l<skippedColumnIds->length(); ++l) {
				if ((*skippedColumnIds)[l] == k) {
					if (fieldType == FIELD_NORMAL) {
						// If the column is expected to exist in the partition, switch to FIELD_DEFAULT instead and force the value to NULL. 
						fieldType = FIELD_DEFAULT;
						forceNull = true;
					} else if (fieldType == FIELD_SKIP) {
						// If the column has been dropped, switch to FIELD_NONE
						fieldType = FIELD_NONE;
					}
					break;
				}
			}
		}
		FieldBase* field = 0;
		switch (fieldType) {
			case FIELD_NONE:
				break;
			case FIELD_NORMAL: {
				Field* myField = myFields == 0 ? 0 : myFields[j++];
				switch (type) {
					case COL_BYTE: {
						if (isUnsigned) {
							field = new FieldSimple<uint8_t>(myField, column);
						} else {
							field = new FieldSimple<int8_t>(myField, column);
						}
						break;
					}
					case COL_SHORT: {
						if (isUnsigned) {
							field = new FieldSimple<uint16_t>(myField, column);
						} else {
							field = new FieldSimple<int16_t>(myField, column);
						}
						break;
					}
					case COL_INT: {
						if (isUnsigned) {
							field = new FieldSimple<uint32_t>(myField, column);
						} else {
							field = new FieldSimple<int32_t>(myField, column);
						}
						break;
					}
					case COL_DOUBLE: 
						field = new FieldSimple<double>(myField, column);
						break;
					case COL_TIMESTAMP:
						field = new FieldTimestamp(myField, column);
						break;
					case COL_LONG: {
						if (isUnsigned) {
							field = new FieldSimple<uint64_t>(myField, column);
						} else {
							field = new FieldSimple<int64_t>(myField, column);
						}
						break;
					}
					case COL_STRING:
						field = new FieldString(myField, column);
						break;
					case COL_BLOB:
						field = new FieldString(myField, column);
						break;
					default: assert(0); break;
				}
				break;
			}
			case FIELD_DEFAULT: {
				Field* myField = myFields == 0 ? 0 : myFields[j++];
				switch (type) {
					case COL_BYTE:
						if (isUnsigned) {
							field = new FieldDefault<uint8_t>(myField, column, forceNull);
						} else {
							field = new FieldDefault<int8_t>(myField, column, forceNull);
						}
						break;
					case COL_SHORT:
						if (isUnsigned) {
							field = new FieldDefault<uint16_t>(myField, column, forceNull);
						} else {
							field = new FieldDefault<int16_t>(myField, column, forceNull);
						}
						break;
					case COL_INT:
						if (isUnsigned) {
							field = new FieldDefault<uint32_t>(myField, column, forceNull);
						} else {
							field = new FieldDefault<int32_t>(myField, column, forceNull);
						}
						break;
					case COL_DOUBLE:
						field = new FieldDefault<double>(myField, column, forceNull);
						break;
					case COL_TIMESTAMP:
					case COL_LONG:
						if (isUnsigned) {
							field = new FieldDefault<uint64_t>(myField, column, forceNull);
						} else {
							field = new FieldDefault<int64_t>(myField, column, forceNull);
						}
						break;
					case COL_STRING:
					case COL_BLOB:
						field = new FieldDefaultString(myField, column, forceNull);
						break;
					default: assert(0); break;
				}
				break;
			}
			case FIELD_SKIP: {
				switch (column.getType()) {
					case COL_BYTE:
						field = new FieldSkip<1>(column);
						break;
					case COL_SHORT:
						field = new FieldSkip<2>(column);
						break;
					case COL_INT:
						field = new FieldSkip<4>(column);
						break;
					case COL_DOUBLE:
					case COL_TIMESTAMP:
					case COL_LONG:
						field = new FieldSkip<8>(column);
						break;
					case COL_STRING:
					case COL_BLOB:
						field = new FieldSkip<16>(column);
						break;
					default: assert(0); break;
				}
				break;
			}
			default: assert(0); break;
		}
		fields.append(field);		
		if (FieldBase::exists(serial, column)) {
			k++; 
		}
	}
}

FieldBase::FieldBase(Field* myField, const Column& column)
	: column_(column)
#ifndef NDEBUG
	, onlyKeyFormat_(myField == 0)
#endif
{
	if (myField == 0) {
		nullOffset_ = 0;
		nullBit_ = 0;
		offset_ = 0;
	} else {
		uchar* tableBuffer = myField->table->record[0];
		nullOffset_ = myField->is_nullable() ? myField->null_offset() : UINT_MAX;
		nullBit_ = myField->null_bit;
		offset_ = myField->offset(tableBuffer);
	}
}

// STATIC
FieldType FieldBase::getFieldType(const uint32_t serial, const bool coalescing, const Column& column) {
	const uint32_t dropSerial = column.getDropSerial();
	if (serial >= column.getSerial()) {
		if (dropSerial == 0) {
			// This is an existing column for which we have data.
			return FIELD_NORMAL;
		} else {
			if (serial >= dropSerial) {
				// This is a deleted column for which we have no data.
				return FIELD_NONE;
			} else {
				// This is a deleted column for which we have data.
				return coalescing ? FIELD_NORMAL : FIELD_SKIP;
			}
		}
	} else {
		if (dropSerial == 0) {
			// This is a new column for which we have no data.
			return coalescing ? FIELD_NONE : FIELD_DEFAULT;
		} else {
			// This is a deleted column for which we have no data.
			return FIELD_NONE;
		}
	}
}

// STATIC
bool FieldBase::exists(const uint32_t serial, const Column& column) {
	const uint32_t dropSerial = column.getDropSerial();
	if (serial >= column.getSerial()) {
		// This column was created before this partition and it was not dropped or it was dropped after this partition was created,
		//	therefore the column exists in this partition.
		return (dropSerial == 0 || serial < dropSerial);
	} else {
		// This column was created after this partition, therefore this column does not exist in this partition
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FieldString
//////////////////////////////////////////////////////////////////////////////////////////////////////

void FieldString::readPersistent(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, uint8_t* buffer, const bool keyFormat) const {
	assert(keyFormat || !onlyKeyFormat_);
	const bool isNull = isNullable() && (bits & 1) != 0;
	if (isNull) {
		reader.advance(16);
		if (keyFormat) {
			*buffer = 1;
		} else {
			buffer[nullOffset_] |= nullBit_;
		}
	} else {
		const uint32_t l = isNullable() ? (bits >> 1) : bits;
		if (l == 0) {
			// Long string.
			uint64_t position;
			uint64_t length;
			reader >> position >> length;
			if (length == 0) {
				store(buffer, keyFormat, reader.getData(), 0);
			} else if ( length > 1000000 ) {
				throw SparrowException::create(false, "internal error: string field length outrageously too big");
			} else {
				stringReader.seekBin(position);
				if (stringReader.position() + length <= stringReader.limit()) {
					// The string is entirely contained in the buffer.
					store(buffer, keyFormat, stringReader.getCurrentData(), static_cast<uint32_t>(length));
				} else {
					// Need to read it across several buffers; use a temporary buffer.
					ByteBuffer stringBuffer(static_cast<uint8_t*>(IOContext::getTempBuffer1(length)), length);
					stringReader >> stringBuffer;
					store(buffer, keyFormat, stringBuffer.getData(), static_cast<uint32_t>(length));
				}
			}
		} else {
			// Small string.
			uint8_t v[16];
			ByteBuffer b(v, static_cast<uint32_t>(sizeof(v)));
			reader >> b;
			store(buffer, keyFormat, v, l);
		}
	}
}

void FieldString::readTransient(const uint64_t data, const bool isNull, uint8_t* buffer, const bool keyFormat) const {
	assert(keyFormat || !onlyKeyFormat_);
	const BinString& string = *(const BinString*)data;
	if (keyFormat) {
		if (isNull) {
			*buffer = 1;
		} else {
			store(buffer, keyFormat, string.getData(), string.getLength());
		}
	} else {
		if (isNull) {
			buffer[nullOffset_] |= nullBit_;
		} else {
			store(buffer, keyFormat, string.getData(), string.getLength());
		}
	}
}

void FieldString::insertTransform(const uint8_t* buffer, ByteBuffer& output) const {
	assert(!onlyKeyFormat_);
	if (getColumn().isFlagSet(COL_IP_LOOKUP)) {
		return;
	}
	const bool nullable = isNullable();
	if (nullable && (buffer[nullOffset_] & nullBit_)) {
		output << static_cast<uint8_t>(1);
	} else {
		if (nullable) {
			output << static_cast<uint8_t>(0);
		}
		uint16_t length;
		const uint8_t* data = buffer + offset_;
		if (lengthBytes_ == 1) {
			length = *data++;
		} else {
			FAST_LOAD2_L(data, length);
			data += 2;
		}
		output << static_cast<uint32_t>(length);
		output << ByteBuffer(data, length);
	}
}

void FieldString::store(uint8_t* buffer, const bool keyFormat, const uint8_t* data, const uint16_t length) const {
	assert(keyFormat || !onlyKeyFormat_);
	uint8_t* p;
	uint32_t lengthBytes;
	if (keyFormat) {
		if (isNullable()) {
			*buffer++ = 0;
		}
		p = buffer;
		lengthBytes = 2;
	} else {
		p = buffer + offset_;
		lengthBytes = lengthBytes_;
	}
	int wellFormedError;
	const uint16_t wlength = (cs_ == &my_charset_bin || fieldLength_ == 0 || length <= fieldLength_) ? length
		: static_cast<uint16_t>(cs_->cset->well_formed_len(cs_, reinterpret_cast<const char*>(data),
		reinterpret_cast<const char*>(data) + fieldLength_, UINT_MAX, &wellFormedError));
	if (lengthBytes == 1) {
		*p++ = static_cast<uint8_t>(wlength);
	} else {
		FAST_STORE2_L(p, wlength);
		p += 2;
	}
	memcpy(p, data, wlength);
}

// Important note:
// Both strings must be in key format, where the length is stored on exactly two bytes.
int FieldString::compare(const uint8_t* left, const uint8_t* right) const {
	uint16_t leftLength;
	FAST_LOAD2_L(left, leftLength);
	left += 2;
	uint16_t rightLength;
	FAST_LOAD2_L(right, rightLength);
	right += 2;
	return cs_->coll->strnncollsp(cs_, left, leftLength, right, rightLength);
}

void FieldString::copy(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, ByteBuffer& buffer, BinBuffer* binBuffer) const {
	const bool isNull = isNullable() && (bits & 1) != 0;
	if (isNull) {
		reader.advance(16);
		buffer << static_cast<uint64_t>(0) << static_cast<uint64_t>(0);
	} else {
		const uint32_t l = isNullable() ? (bits >> 1) : bits;
		if (l == 0) {
			// Long string.
			uint64_t position;
			uint64_t length;
			reader >> position >> length;
			if (length > 0 && binBuffer != 0) {
				stringReader.seekBin(position);
				BinString* s;
				if (stringReader.position() + length <= stringReader.limit()) {
					// The string is entirely contained in the buffer.
					s = binBuffer->insert(stringReader.getCurrentData(), static_cast<uint32_t>(length));
				} else {
					// Need to read it across several buffers; use a temporary buffer.
					ByteBuffer stringBuffer(static_cast<uint8_t*>(IOContext::getTempBuffer1(length)), length);
					stringReader >> stringBuffer;
					s = binBuffer->insert(stringBuffer.getData(), static_cast<uint32_t>(length));
				}
				position = s->getPosition(binBuffer->getData());
			}
			buffer << (length == 0 ? static_cast<uint64_t>(0) : position) << length;
		} else {
			// Small string.
			uint8_t v[16];
			ByteBuffer b(v, static_cast<uint32_t>(sizeof(v)));
			reader >> b;
			buffer << ByteBuffer(v, static_cast<uint32_t>(sizeof(v)));
		}
	}
}

int FieldString::compare(ByteBuffer& buffer1, const uint8_t bits1, PartitionReader& stringReader1,
		ByteBuffer& buffer2, const uint8_t bits2, PartitionReader& stringReader2, BinBuffer* binBuffer) const {
	const bool nullable = isNullable();
	const bool isNull1 = nullable && (bits1 & 1) != 0;
	const bool isNull2 = nullable && (bits2 & 1) != 0;
	if (isNull1) {
		if (isNull2) {
			return 0;
		} else {
			return -1;
		}
	} else if (isNull2) {
		return 1;
	}
	uint64_t length1 = nullable ? (bits1 >> 1) : bits1;
	uint8_t* s1;
	uint8_t v1[16];
	if (length1 == 0) {
		uint64_t pos1;
		buffer1 >> pos1 >> length1;
		if (binBuffer == 0) {
			if (length1 > 0) {
				stringReader1.seekBin(pos1);
				if (stringReader1.position() + length1 <= stringReader1.limit()) {
					// The string is entirely contained in the buffer.
					s1 = stringReader1.getCurrentData();
				} else {
					// Need to read it across several buffers; use a temporary buffer.
					ByteBuffer stringBuffer(static_cast<uint8_t*>(IOContext::getTempBuffer1(length1)), length1);
					stringReader1 >> stringBuffer;
					s1 = stringBuffer.getData();
				}
			} else {
				s1 = v1;
			}
		} else {
			s1 = const_cast<uint8_t*>(binBuffer->getData() + pos1);
		}
	} else {
		ByteBuffer b(v1, 16);
		buffer1 >> b;
		s1 = v1;
	}
	int wellFormedError;
	const uint16_t wlength1 = (cs_ == &my_charset_bin || fieldLength_ == 0 || length1 <= fieldLength_) ? static_cast<uint16_t>(length1)
		: static_cast<uint16_t>(cs_->cset->well_formed_len(cs_, reinterpret_cast<const char*>(s1),
		reinterpret_cast<const char*>(s1) + fieldLength_, UINT_MAX, &wellFormedError));
	uint64_t length2 = nullable ? (bits2 >> 1) : bits2;
	uint8_t* s2;
	uint8_t v2[16];
	if (length2 == 0) {
		uint64_t pos2;
		buffer2 >> pos2 >> length2;
		if (binBuffer == 0) {
			if (length2 > 0) {
				stringReader2.seekBin(pos2);
				if (stringReader2.position() + length2 <= stringReader2.limit()) {
					// The string is entirely contained in the buffer.
					s2 = stringReader2.getCurrentData();
				} else {
					// Need to read it across several buffers; use a temporary buffer.
					ByteBuffer stringBuffer(static_cast<uint8_t*>(IOContext::getTempBuffer2(length2)), length2);
					stringReader2 >> stringBuffer;
					s2 = stringBuffer.getData();
				}
			} else {
				s2 = v2;
			}
		} else {
			s2 = const_cast<uint8_t*>(binBuffer->getData() + pos2);
		}
	} else {
		ByteBuffer b(v2, 16);
		buffer2 >> b;
		s2 = v2;
	}
	const uint16_t wlength2 = (cs_ == &my_charset_bin || fieldLength_ == 0 || length2 <= fieldLength_) ? static_cast<uint16_t>(length2)
		: static_cast<uint16_t>(cs_->cset->well_formed_len(cs_, reinterpret_cast<const char*>(s2),
		reinterpret_cast<const char*>(s2) + fieldLength_, UINT_MAX, &wellFormedError));
	return cs_->coll->strnncollsp(cs_, s1, wlength1, s2, wlength2);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FieldTimestamp
//////////////////////////////////////////////////////////////////////////////////////////////////////

void FieldTimestamp::readPersistent(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, uint8_t* buffer, const bool keyFormat) const {
	assert(keyFormat || !onlyKeyFormat_);
	const bool isNull = (bits == 1);
	uint64_t v;
	if (isNull) {
		reader.advance(sizeof(v));
	} else {
		reader >> v;
	}
	if (keyFormat) {
		if (isNull) {
			*buffer = 1;
		} else {
			if (isNullable()) {
				*buffer++ = 0;
			}
			writeMySql( v, buffer );
		}
	} else {
		if (isNull) {
			buffer[nullOffset_] |= nullBit_;
		} else {
			writeMySql( v, buffer + offset_ );
		}
	}
}

void FieldTimestamp::readTransient(const uint64_t data, const bool isNull, uint8_t* buffer, const bool keyFormat) const {
	assert(keyFormat || !onlyKeyFormat_);

	if (keyFormat) {
		if (isNull) {
			*buffer = 1;
		} else {
			if (isNullable()) {
				*buffer++ = 0;
			}
			writeMySql( data, buffer );
		}
	} else {
		if (isNull) {
			buffer[nullOffset_] |= nullBit_;
		} else {
			writeMySql( data, buffer + offset_ );
		}
	}
}

bool FieldTimestamp::readMySqlTransient(const uint8_t* buffer, uint64_t& data) const {
	assert(!onlyKeyFormat_);
	if (isNullable() && (buffer[nullOffset_] & nullBit_)) {
		return true;
	} else {
		data = readMySql( buffer + offset_ );
		return false;
	}
}

bool FieldTimestamp::readMySqlPersistent(const uint8_t* buffer, ByteBuffer& output) const {
	assert(!onlyKeyFormat_);
	if (isNullable() && (buffer[nullOffset_] & nullBit_)) {
		output << static_cast<uint64_t>(0);
		return true;
	} else {
		uint64_t	data = readMySql( buffer + offset_ );
		output << data;
		return false;
	}
}

void FieldTimestamp::insertTransform(const uint8_t* buffer, ByteBuffer& output) const {
	assert(!onlyKeyFormat_);
	const bool nullable = isNullable();
	if (nullable && (buffer[nullOffset_] & nullBit_)) {
		output << static_cast<uint8_t>(1);
	} else {
		if (nullable) {
			output << static_cast<uint8_t>(0);
		}
		uint64_t	data = readMySql( buffer + offset_ );
		output << data;
	}
}

int FieldTimestamp::compare(const uint8_t* left, const uint8_t* right) const {
	uint64_t	l = readMySql( left );
	uint64_t	r = readMySql( right );
	return (l < r) ? -1 : (l > r) ? 1 : 0;
}

uint64_t FieldTimestamp::readMySql( const uint8_t* buffer, uint32_t offset ) const {
	uint32_t t = 0, tf = 0;
	if ( dec_ < 0 ) { 
		FAST_LOAD4_L(buffer + offset, t);
	} else {
		FAST_LOAD4_B(buffer + offset, t);
		switch (dec_)
		{
		case 1:	
		case 2:	tf = buffer[offset+4]; tf *= 10; break;
		case 3:	
		case 4:	FAST_LOAD2_B(buffer + offset + 4, tf); tf /= 10; break;
		case 5:	
		case 6:	FAST_LOAD3_B(buffer + offset + 4, tf); tf /= 1000; break;
		}
	}
	uint64_t		data = static_cast<uint64_t>(t) * 1000 + tf;
	return data;
}

void FieldTimestamp::writeMySql( uint64_t data, uint8_t* buffer, uint32_t offset ) const {
	const uint32_t t = static_cast<uint32_t>(data / 1000);		// Timestamp in seconds
	uint32_t tf = static_cast<uint32_t>(data % 1000);		// Fractional part of the timestamp in ms.
	uint8_t* p = buffer + offset;
	if ( dec_ < 0 ) {
		FAST_STORE4_L(p, t);
	} else {
		FAST_STORE4_B(p, t);
		p += 4;
		switch (dec_)
		{
		case 0:	break;
		case 1:	
		case 2:	tf /= 10; *p = static_cast<uint8_t>(tf); break;
		case 3:	
		case 4:	tf *= 10; FAST_STORE2_B(p, tf); break;
		case 5:	
		case 6:	tf *= 1000; FAST_STORE3_B(p, tf); break;
		}
	}
}

void FieldTimestamp::copy(PartitionReader& reader, PartitionReader& stringReader, const uint8_t bits, ByteBuffer& buffer, BinBuffer* binBuffer) const {
	uint64_t v;
	reader >> v;
	buffer << v;
}

int FieldTimestamp::compare(ByteBuffer& buffer1, const uint8_t bits1, PartitionReader& stringReader1,
	ByteBuffer& buffer2, const uint8_t bits2, PartitionReader& stringReader2, BinBuffer* binBuffer) const {
	uint64_t l;
	uint64_t r;
	buffer1 >> l;
	buffer2 >> r;
	const bool isNull1 = (bits1 == 1);
	const bool isNull2 = (bits2 == 1);
	if (isNull1) {
		if (isNull2) {
			return 0;
		} else {
			return -1;
		}
	} else if (isNull2) {
		return 1;
	}
	return (l < r) ? -1 : (l > r) ? 1 : 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FieldDefault
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T> void FieldDefault<T>::computeBinDefaultValue(Field* myField) {
	const bool isNullable = column_.isFlagSet(COL_NULLABLE);
	CHARSET_INFO* cs = myField == 0 ? get_charset_by_name(column_.getCharset().c_str(), MYF(MY_WME)) : const_cast<CHARSET_INFO*>(myField->charset());
	const Str& defaultValue = column_.getDefaultValue();
	const uint32_t length = defaultValue.length();
	if (length == 0 && isNullable) {
		// This is a NULL.
		value_ = BinValue();
	} else {
		const char* s = defaultValue.c_str();
		int error = 0;
		const char* end;
		const int isUnsigned = column_.isFlagSet(COL_UNSIGNED) ? 1 : 0;
		switch (column_.getType()) {
			case COL_BYTE: {
				value_ = BinValue(1);
				const uint64_t x = length == 0 ? 0 : cs->cset->strntoull10rnd(cs, s, length, isUnsigned, &end, &error);
				*value_.data() = static_cast<uint8_t>(x & 0xff);
				break;
			}
			case COL_SHORT: {
				value_ = BinValue(2);
				const uint64_t x = length == 0 ? 0 : cs->cset->strntoull10rnd(cs, s, length, isUnsigned, &end, &error);
				const uint16_t v = static_cast<uint16_t>(x & 0xffff);
				uint8_t* d = value_.data();
				FAST_STORE2_L(d, v);
				break;
			}
			case COL_INT: {
				value_ = BinValue(4);
				const uint64_t x = length == 0 ? 0 : cs->cset->strntoull10rnd(cs, s, length, isUnsigned, &end, &error);
				const uint32_t v = static_cast<uint32_t>(x & 0xffffffff);
				uint8_t* d = value_.data();
				FAST_STORE4_L(d, v);
				break;
			}
			case COL_DOUBLE: {
				value_ = BinValue(8);
				const double x = length == 0 ? 0 : cs->cset->strntod(cs, const_cast<char*>(s), length, &end, &error);
				const uint64_t v = *reinterpret_cast<const uint64_t*>(&x);
				uint8_t* d = value_.data();
				FAST_STORE8_L(d, v);
				break;
			}
			case COL_TIMESTAMP: {
				value_ = BinValue(4);
				MYSQL_TIME t;
				MYSQL_TIME_STATUS	status;
#ifndef NDEBUG
				const bool result =
#endif
				str_to_datetime(s, length, &t, TIME_DATETIME_ONLY, &status);
				assert(result == true);
				error = status.warnings;
				THD* thd = current_thd;
				Time_zone* tz = thd == 0 ? my_tz_SYSTEM : thd->variables.time_zone;
				bool dummy;
				my_time_t x = length == 0 ? 0 : tz->TIME_to_gmt_sec(&t, &dummy);
				const uint32_t v = static_cast<uint32_t>(x & 0xffffffff);
				uint8_t* d = value_.data();
				FAST_STORE4_L(d, v);
				break;
			}
			case COL_LONG: {
				value_ = BinValue(8);
				const uint64_t x = length == 0 ? 0 : cs->cset->strntoull10rnd(cs, s, length, isUnsigned, &end, &error);
				uint8_t* d = value_.data();
				FAST_STORE8_L(d, x);
				break;
			}
			default: break;
		}
		assert(error == 0);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FieldDefaultString
//////////////////////////////////////////////////////////////////////////////////////////////////////

void FieldDefaultString::computeBinDefaultValue(Field* myField) {
	const bool isNullable = column_.isFlagSet(COL_NULLABLE);
	const Str& defaultValue = column_.getDefaultValue();
	const uint32_t length = defaultValue.length();
	if (length == 0 && isNullable) {
		// This is a NULL.
		value_ = BinValue();
	} else {
		const char* s = defaultValue.c_str();
		value_ = BinValue(2 + length);
		uint8_t* p = value_.data();
		FAST_STORE2_L(p, length);
		memcpy(p + 2, s, length);
	}
}

}
