/*
 Copyright (c) 2013, 2022, Oracle and/or its affiliates.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cstddef>

#ifdef WIN32
#include <float.h>
#endif

/**
 * Include NDB file CharsetMap.hpp first to avoid clash between NDB
 * define Int32 and v8::Int32 in declaration of CharsetMap::recode().
 */
#include "CharsetMap.hpp"

#include "adapter_global.h"
#include "NdbTypeEncoders.h"
#include "js_wrapper_macros.h"
#include "JsWrapper.h"
#include "JsValueAccess.h"

#include "node.h"

#include "decimal_utils.hpp"
#include "EncoderCharset.h"

extern void freeBufferContentsFromJs(char *, void *);  // in BlobHandler.cpp

Eternal<String>    /* keys of MySQLTime (Adapter/impl/common/MySQLTime.js) */
  K_sign, 
  K_year, 
  K_month, 
  K_day, 
  K_hour, 
  K_minute, 
  K_second, 
  K_microsec, 
  K_fsp,
  K_valid;

Eternal<Value>   /* SQLState Error Codes */
  K_22000_DataError,
  K_22001_StringTooLong,
  K_22003_OutOfRange,
  K_22007_InvalidDatetime,
  K_0F001_Bad_BLOB,
  K_HY000;

#define writerOK Local<Value>::New(isolate, Undefined(isolate))

Isolate * isolate;

#define ENCODER(A, B, C) NdbTypeEncoder A = { & B, & C, 0 }

#define DECLARE_ENCODER(TYPE) \
  EncoderReader TYPE##Reader; \
  EncoderWriter TYPE##Writer; \
  ENCODER(TYPE##Encoder, TYPE##Reader, TYPE##Writer)

#define DECLARE_ENCODER_TEMPLATES(TYPE) \
  template <typename T> Local<Value> TYPE##Reader(const NdbDictionary::Column *,\
    char *, uint32_t); \
  template <typename T> Local<Value> TYPE##Writer(const NdbDictionary::Column *, \
    Local<Value>, char *, uint32_t)

DECLARE_ENCODER(UnsupportedType);

DECLARE_ENCODER(Int);
DECLARE_ENCODER(UnsignedInt);
DECLARE_ENCODER_TEMPLATES(smallint);
ENCODER(TinyIntEncoder, smallintReader<int8_t>, smallintWriter<int8_t>);
ENCODER(TinyUnsignedEncoder, smallintReader<uint8_t>, smallintWriter<uint8_t>);
ENCODER(SmallIntEncoder, smallintReader<int16_t>, smallintWriter<int16_t>);
ENCODER(SmallUnsignedEncoder, smallintReader<uint16_t>, smallintWriter<uint16_t>);
DECLARE_ENCODER(Medium);
DECLARE_ENCODER(MediumUnsigned);
DECLARE_ENCODER_TEMPLATES(bigint);
ENCODER(BigintEncoder, bigintReader<int64_t>, bigintWriter<int64_t>);
ENCODER(BigintUnsignedEncoder, bigintReader<uint64_t>, bigintWriter<uint64_t>);

DECLARE_ENCODER_TEMPLATES(fp);
ENCODER(FloatEncoder, fpReader<float>, fpWriter<float>);
ENCODER(DoubleEncoder, fpReader<double>, fpWriter<double>);

DECLARE_ENCODER(Binary);
DECLARE_ENCODER_TEMPLATES(varbinary);
ENCODER(VarbinaryEncoder, varbinaryReader<uint8_t>, varbinaryWriter<uint8_t>);
ENCODER(LongVarbinaryEncoder, varbinaryReader<uint16_t>, varbinaryWriter<uint16_t>);

DECLARE_ENCODER(Char);
DECLARE_ENCODER_TEMPLATES(varchar);
ENCODER(VarcharEncoder, varcharReader<uint8_t>, varcharWriter<uint8_t>);
ENCODER(LongVarcharEncoder, varcharReader<uint16_t>, varcharWriter<uint16_t>);

DECLARE_ENCODER(Year);
DECLARE_ENCODER(Timestamp);
DECLARE_ENCODER(Datetime);
DECLARE_ENCODER(Timestamp2);
DECLARE_ENCODER(Datetime2);
DECLARE_ENCODER(Time);
DECLARE_ENCODER(Time2);
DECLARE_ENCODER(Date);
DECLARE_ENCODER(Blob);

DECLARE_ENCODER(Decimal);
EncoderWriter UnsignedDecimalWriter;
ENCODER(UnsignedDecimalEncoder, DecimalReader, UnsignedDecimalWriter);

const NdbTypeEncoder * AllEncoders[NDB_TYPE_MAX] = {
  & UnsupportedTypeEncoder,               // 0
  & TinyIntEncoder,                       // 1  TINY INT
  & TinyUnsignedEncoder,                  // 2  TINY UNSIGNED
  & SmallIntEncoder,                      // 3  SMALL INT
  & SmallUnsignedEncoder,                 // 4  SMALL UNSIGNED
  & MediumEncoder,                        // 5  MEDIUM INT
  & MediumUnsignedEncoder,                // 6  MEDIUM UNSIGNED
  & IntEncoder,                           // 7  INT
  & UnsignedIntEncoder,                   // 8  UNSIGNED
  & BigintEncoder,                        // 9  BIGINT
  & BigintUnsignedEncoder,                // 10 BIG UNSIGNED
  & FloatEncoder,                         // 11 FLOAT
  & DoubleEncoder,                        // 12 DOUBLE
  & UnsupportedTypeEncoder,               // 13 OLDDECIMAL
  & CharEncoder,                          // 14 CHAR
  & VarcharEncoder,                       // 15 VARCHAR
  & BinaryEncoder,                        // 16 BINARY
  & VarbinaryEncoder,                     // 17 VARBINARY
  & DatetimeEncoder,                      // 18 DATETIME
  & DateEncoder,                          // 19 DATE
  & BlobEncoder,                          // 20 BLOB
  & UnsupportedTypeEncoder,               // 21 TEXT
  & UnsupportedTypeEncoder,               // 22 BIT
  & LongVarcharEncoder,                   // 23 LONGVARCHAR
  & LongVarbinaryEncoder,                 // 24 LONGVARBINARY
  & TimeEncoder,                          // 25 TIME
  & YearEncoder,                          // 26 YEAR
  & TimestampEncoder,                     // 27 TIMESTAMP
  & UnsupportedTypeEncoder,               // 28 OLDDECIMAL UNSIGNED
  & DecimalEncoder,                       // 29 DECIMAL
  & UnsignedDecimalEncoder                // 30 DECIMAL UNSIGNED
#if NDB_TYPE_MAX > 31
  ,
  & Time2Encoder,                         // 31 TIME2
  & Datetime2Encoder,                     // 32 DATETIME2
  & Timestamp2Encoder,                    // 33 TIMESTAMP2
#endif
};


const NdbTypeEncoder * getEncoderForColumn(const NdbDictionary::Column *col) {
  return AllEncoders[col->getType()];
}


/* read(col, buffer, offset)
*/
void encoderRead(const Arguments & args) {
  isolate = args.GetIsolate();
  EscapableHandleScope scope(isolate);

  const NdbDictionary::Column * col =
    unwrapPointer<const NdbDictionary::Column *>(ArgToObject(args, 0));
  const NdbTypeEncoder * encoder = getEncoderForColumn(col);
  char * buffer = GetBufferData(ArgToObject(args, 1));

  args.GetReturnValue().Set(
    scope.Escape(encoder->read(col, buffer, GetInt32Arg(args, 2))));
}


/* write(col, value, buffer, offset) 
*/
void encoderWrite(const Arguments & args) {
  isolate = args.GetIsolate();
  EscapableHandleScope scope(isolate);

  const NdbDictionary::Column * col =
    unwrapPointer<const NdbDictionary::Column *>(ArgToObject(args, 0));
  const NdbTypeEncoder * encoder = getEncoderForColumn(col);
  char * buffer = GetBufferData(ArgToObject(args, 2));
  uint32_t offset = GetUint32Arg(args, 3);

  args.GetReturnValue().Set(
    scope.Escape(encoder->write(col, args[1], buffer, offset)));
}


/* String Encoder Statistics */
struct encoder_stats_t {
  unsigned read_strings_externalized; // JS Strings that reference ASCII or UTF16LE buffers
  unsigned read_strings_created; // JS Strings created from UTF-8 representation
  unsigned read_strings_recoded; // Reads recoded from MySQL Charset to UTF-8
  unsigned externalized_text_writes;  // String reused as TEXT buffer (no copying)
  unsigned direct_writes;  // ASCII/UTF16LE/UTF8 written directly to DB buffer
  unsigned recode_writes;  // Writes recoded from UTF8 to MySQL Charset
} stats;


/* Exports to JavaScript 
*/
void GET_read_strings_externalized(Local<Name>, const AccessorInfo & info) {
  info.GetReturnValue().Set(stats.read_strings_externalized);
}

void GET_read_strings_created(Local<Name>, const AccessorInfo & info) {
  info.GetReturnValue().Set(stats.read_strings_created);
}

void GET_read_strings_recoded(Local<Name>, const AccessorInfo & info) {
  info.GetReturnValue().Set(stats.read_strings_recoded);
}

void GET_externalized_text_writes(Local<Name>, const AccessorInfo & info){
  info.GetReturnValue().Set(stats.externalized_text_writes);
}

void GET_direct_writes(Local<Name>, const AccessorInfo & info) {
  info.GetReturnValue().Set(stats.direct_writes);
}

void GET_recode_writes(Local<Name>, const AccessorInfo & info) {
  info.GetReturnValue().Set(stats.recode_writes);
}

void bufferForText(const Arguments &);
void textFromBuffer(const Arguments &);

#define SET_KEY(X,Y) X.Set(isolate, NewUtf8String(isolate, Y))

void NdbTypeEncoders_initOnLoad(Local<Object> target) {
  isolate = Isolate::GetCurrent();

  DEFINE_JS_FUNCTION(target, "encoderRead", encoderRead);
  DEFINE_JS_FUNCTION(target, "encoderWrite", encoderWrite);
  DEFINE_JS_FUNCTION(target, "bufferForText", bufferForText);
  DEFINE_JS_FUNCTION(target, "textFromBuffer", textFromBuffer);
  SET_KEY(K_sign, "sign");
  SET_KEY(K_year, "year");
  SET_KEY(K_month, "month");
  SET_KEY(K_day, "day");
  SET_KEY(K_hour, "hour");
  SET_KEY(K_minute, "minute");
  SET_KEY(K_second, "second");
  SET_KEY(K_microsec, "microsec");
  SET_KEY(K_fsp, "fsp");
  SET_KEY(K_valid, "valid");
  SET_KEY(K_22000_DataError, "22000");
  SET_KEY(K_22001_StringTooLong, "22001");
  SET_KEY(K_22003_OutOfRange, "22003");
  SET_KEY(K_22007_InvalidDatetime, "22007");
  SET_KEY(K_0F001_Bad_BLOB, "0F001");
  SET_KEY(K_HY000, "HY000");

  Local<Object> s = Object::New(isolate);
  SetProp(target, "encoder_stats", s);
  DEFINE_JS_ACCESSOR(isolate, s, "read_strings_externalized",
                     GET_read_strings_externalized);
  DEFINE_JS_ACCESSOR(isolate, s, "read_strings_created",
                     GET_read_strings_created);
  DEFINE_JS_ACCESSOR(isolate, s, "read_strings_recoded",
                     GET_read_strings_recoded);
  DEFINE_JS_ACCESSOR(isolate, s, "externalized_text_writes",
                     GET_externalized_text_writes);
  DEFINE_JS_ACCESSOR(isolate, s, "direct_writes", GET_direct_writes);
  DEFINE_JS_ACCESSOR(isolate, s, "recode_writes", GET_recode_writes);
}



/*************************************************************
   ******                                              *****
      *****                                         *****
         *****              Macros               *****/


/* FOR INTEGER TYPES, x86 allows unaligned access, but most other machines do not.
   FOR FLOATING POINT TYPES: access must be aligned on all architectures.
   v8 supports ARM and MIPS along with x86 and x86_64.
   Wherever in the code there is a LOAD_ALIGNED_DATA macro, we assume the record  
   has been laid out with necessary padding for alignment. 
*/

/* Assign x := *buf, assuming correct alignment */
#define LOAD_ALIGNED_DATA(Type, x, buf) \
Type x = *((Type *) (buf));

/* Assign *buf := x, assuming correct alignment */
#define STORE_ALIGNED_DATA(Type, x, buf) \
*((Type *) (buf)) = (Type) x;

/* Assign x := *buf */
#define ALIGN_AND_LOAD(Type, x, buf) \
Type x; \
memcpy(&x, buf, sizeof(x));

/* Assign *buf := x */
#define ALIGN_AND_STORE(Type, x, buf) \
Type tmp_value = (Type) x; \
memcpy(buf, &tmp_value, sizeof(tmp_value));

#define sint3korr(A)  ((int32_t) ((((uint8_t) (A)[2]) & 128) ? \
                                  (((uint32_t) 255L << 24) | \
                                  (((uint32_t) (uint8_t) (A)[2]) << 16) |\
                                  (((uint32_t) (uint8_t) (A)[1]) << 8) | \
                                   ((uint32_t) (uint8_t) (A)[0])) : \
                                 (((uint32_t) (uint8_t) (A)[2]) << 16) |\
                                 (((uint32_t) (uint8_t) (A)[1]) << 8) | \
                                  ((uint32_t) (uint8_t) (A)[0])))

#define uint3korr(A)  (uint32_t) (((uint32_t) ((uint8_t) (A)[0])) +\
                                  (((uint32_t) ((uint8_t) (A)[1])) << 8) +\
                                  (((uint32_t) ((uint8_t) (A)[2])) << 16))


/*************************************************************
   ******                                              *****
      *****                                         *****
         *****              Utilities            *****/

/* File-scope global return from successful write encoders: 
*/
template <typename INTSZ> Local<Value> checkNumber(double);

template<> inline Local<Value> checkNumber<int>(double d) {
  if(isfinite(d)) {
    return (d >= -2147483648.0 && d <= 2147483648.0) ? writerOK : K_22003_OutOfRange.Get(isolate);
  }
  return K_HY000.Get(isolate);
}

template<> inline Local<Value> checkNumber<uint32_t>(double d) {
  if(isfinite(d)) {
    return (d >= 0 && d < 4294967296.0) ? writerOK : K_22003_OutOfRange.Get(isolate);
  }
  return K_HY000.Get(isolate);
}

template <typename INTSZ> bool checkIntValue(int);

template <typename INTSZ> inline Local<Value> getStatusForValue(double d) {
  if(isfinite(d)) {
    return checkIntValue<INTSZ>(static_cast<INTSZ>(d)) ? writerOK : K_22003_OutOfRange.Get(isolate);
  }
  return K_HY000.Get(isolate);
}

template<> inline bool checkIntValue<int8_t>(int r) {
  return (r >= -128 && r < 128);
}

template<> inline bool checkIntValue<uint8_t>(int r) {
  return (r >= 0 && r < 256);
}

template<> inline bool checkIntValue<int16_t>(int r) {
  return (r >= -32768 && r < 32768);
}

template<> inline bool checkIntValue<uint16_t>(int r) {
  return (r >= 0 && r < 65536);
}

inline Local<Value> checkMedium(int r) {
  return (r >= -8338608 && r < 8338608) ? writerOK :  K_22003_OutOfRange.Get(isolate);
}

inline Local<Value> getStatusForMedium(double dval) {
  if(isfinite(dval)) {
    return checkMedium(static_cast<int>(dval));
  }
  return K_HY000.Get(isolate);
}

inline Local<Value> checkUnsignedMedium(int r) {
  return (r >= 0 && r < 16277216) ? writerOK :  K_22003_OutOfRange.Get(isolate);
}

inline Local<Value> getStatusForUnsignedMedium(double dval) {
  if(isfinite(dval)) {
    return checkUnsignedMedium(static_cast<int>(dval));
  }
  return K_HY000.Get(isolate);
}

inline void writeSignedMedium(int8_t * cbuf, int mval) {
  cbuf[0] = (int8_t) (mval);
  cbuf[1] = (int8_t) (mval >> 8);
  cbuf[2] = (int8_t) (mval >> 16);
}

inline void writeUnsignedMedium(uint8_t * cbuf, uint32_t mval) {
  cbuf[0] = (uint8_t) (mval);
  cbuf[1] = (uint8_t) (mval >> 8);
  cbuf[2] = (uint8_t) (mval >> 16);
}

/* bigendian utilities, used with the wl#946 temporal types.
   Derived from ndb/src/common/util/NdbSqlUtil.cpp
*/
static uint64_t unpack_bigendian(const char * buf, unsigned int len) {
  uint64_t val = 0;
  unsigned int i = len;
  int shift = 0;
  while (i != 0) {
    i--;
    uint64_t v = (uint8_t) buf[i];
    val += (v << shift);
    shift += 8;
  }
  return val;
}

void pack_bigendian(uint64_t val, char * buf, unsigned int len) {
  uint8_t b[8];
  unsigned int i = 0, j = 0;
  while (i  < len) {
    b[i] = (uint8_t)(val & 255);
    val >>= 8;
    i++;
  }
  while (i != 0) {
    buf[--i] = b[j++];
  }
}


/*************************************************************
   ******                                              *****
      *****                                         *****
         *****         Implementations           *****/



// UnsupportedType
Local<Value> UnsupportedTypeReader(const NdbDictionary::Column *col, 
                                    char *buffer, uint32_t offset) {
  //TODO EXCEPTION
  return writerOK;
}

Local<Value> UnsupportedTypeWriter(const NdbDictionary::Column * col,
                                    Local<Value> value,
                                    char *buffer, uint32_t offset) {
  //TODO EXCEPTION
  return writerOK;
}

// Int
Local<Value> IntReader(const NdbDictionary::Column *col, 
                       char *buffer, uint32_t offset) {
  LOAD_ALIGNED_DATA(int, i, buffer+offset);
  return Integer::New(isolate, i);
}                        

Local<Value> IntWriter(const NdbDictionary::Column * col,
                       Local<Value> value,
                       char *buffer, uint32_t offset) {
  int *ipos = (int *) (buffer+offset);
  Local<Value> status;

  if(value->IsInt32()) {
    *ipos = GetInt32Value(isolate, value);
    status = writerOK;
  }
  else {
    double dval = ToNumber(isolate, value);
    *ipos = static_cast<int>(rint(dval));
    status = checkNumber<int>(dval);
  }
  return status;
}                        


// Unsigned Int
Local<Value> UnsignedIntReader(const NdbDictionary::Column *col, 
                                char *buffer, uint32_t offset) {
  LOAD_ALIGNED_DATA(uint32_t, i, buffer+offset);
  return Integer::NewFromUnsigned(isolate, i);
}                        

Local<Value> UnsignedIntWriter(const NdbDictionary::Column * col,
                                Local<Value> value,
                                char *buffer, uint32_t offset) {
  Local<Value> status;
  uint32_t *ipos = (uint32_t *) (buffer+offset);
  if(value->IsUint32()) {
    *ipos = GetUint32Value(isolate, value);
    status = writerOK;
  } else {
    double dval = ToNumber(isolate, value);
    *ipos = static_cast<uint32_t>(rint(dval));
    status = checkNumber<uint32_t>(dval);
  }
  return status;
}


// Templated encoder for TINY and SMALL int types
template <typename INTSZ>
Local<Value> smallintReader(const NdbDictionary::Column *col, 
                             char *buffer, uint32_t offset) {
  LOAD_ALIGNED_DATA(INTSZ, i, buffer+offset);
  return Integer::New(isolate, i);
}


template <typename INTSZ> 
Local<Value> smallintWriter(const NdbDictionary::Column * col,
                             Local<Value> value, char *buffer, uint32_t offset) {
  INTSZ *ipos = (INTSZ *) (buffer+offset);
  Local<Value> status;
  if(value->IsInt32()) {
    int32_t ival = GetInt32Value(isolate, value);
    *ipos = static_cast<INTSZ>(ival);
    status = checkIntValue<INTSZ>(ival) ? writerOK : K_22003_OutOfRange.Get(isolate);
  } else {
    double dval = ToNumber(isolate, value);
    *ipos = static_cast<INTSZ>(dval);
    status = getStatusForValue<INTSZ>(dval);
  }
  return status;
}


// Medium signed & unsigned int types
Local<Value> MediumReader(const NdbDictionary::Column *col, 
                           char *buffer, uint32_t offset) {
  char * cbuf = buffer+offset;
  int i = sint3korr(cbuf);
  return Integer::New(isolate, i);
}

Local<Value> MediumWriter(const NdbDictionary::Column * col,
                           Local<Value> value, char *buffer, uint32_t offset) {
  int8_t *cbuf = (int8_t *) (buffer+offset);
  Local<Value> status;
  double dval;
  int chkv;
  if(value->IsInt32()) {
    chkv = GetInt32Value(isolate, value);
    status = checkMedium(chkv);
  } else {
    dval = ToNumber(isolate, value);
    chkv = static_cast<int>(rint(dval));
    status = getStatusForMedium(dval);
  }
  writeSignedMedium(cbuf, chkv);

  return status;
}                        

Local<Value> MediumUnsignedReader(const NdbDictionary::Column *col, 
                                   char *buffer, uint32_t offset) {
  char * cbuf = buffer+offset;
  int i = uint3korr(cbuf);
  return Integer::New(isolate, i);
}

Local<Value> MediumUnsignedWriter(const NdbDictionary::Column * col,
                                   Local<Value> value,
                                   char *buffer, uint32_t offset) {
  uint8_t *cbuf = (uint8_t *) (buffer+offset);
  Local<Value> status;
  double dval;
  int chkv;
  if(value->IsInt32()) {
    chkv = GetInt32Value(isolate, value);
    status = checkUnsignedMedium(chkv);
  } else {
    dval = ToNumber(isolate, value);
    chkv = static_cast<int>(rint(dval));
    status = getStatusForUnsignedMedium(dval);
  }
  writeUnsignedMedium(cbuf, chkv);
  return status;
}                        


// Bigint encoders
template<typename T> bool stringToBigint(const char *, T *);

template<> bool stringToBigint<int64_t>(const char *str, int64_t *out) {
  char *endptr;
  bool rval = false;
  errno = 0;
  long long ll = strtoll(str, &endptr, 10);
  if (errno != ERANGE && 
      ( (isspace((unsigned char) *endptr) || 
        (*endptr == '\0' && endptr != str)
      ))) {
     *out = ll;
     rval = true;
  }
  return rval;
}

template<> bool stringToBigint<uint64_t>(const char *str, uint64_t *out) {
  char *endptr;
  errno = 0;
  unsigned long long ull = strtoull(str, &endptr, 10);
  if(errno == ERANGE) return false;
  if(((long long) ull < 0) && (strchr(str, '-') != NULL)) return false;
  if(isspace((unsigned char) *endptr) || (*endptr == '\0' && endptr != str)) {
    *out = ull;
    return true;
  }
  return false;
}

template<typename T> bool writeBigint(Local<Value>, T *);

template<> inline bool writeBigint<int64_t>(Local<Value> val, int64_t *ipos) {
  if(val->IsInt32()) {
    *ipos = GetInt32Value(isolate, val);
    return true;
  }
  return false;
}

template<> inline bool writeBigint<uint64_t>(Local<Value> val, uint64_t *ipos) {
  if(val->IsUint32()) {
    *ipos = GetInt32Value(isolate, val);
    return true;
  }
  return false;
}

template<typename T> void bigintToString(char *, T);

template<> inline void bigintToString<int64_t>(char * strbuf, int64_t bigint) {
  sprintf(strbuf, "%lld", (long long int) bigint);
}

template<> inline void bigintToString<uint64_t>(char * strbuf, uint64_t bigint) {
  sprintf(strbuf, "%llu", (long long unsigned int) bigint);
}

template <typename BIGT>
Local<Value> bigintReader(const NdbDictionary::Column *col, 
                            char *buffer, uint32_t offset) {
  char strbuf[32];
  LOAD_ALIGNED_DATA(BIGT, bigint, buffer+offset);
  bigintToString(strbuf, bigint);
  return NewUtf8String(isolate, strbuf);
}

template <typename BIGT>
Local<Value> bigintWriter(const NdbDictionary::Column *col, 
                           Local<Value> value, char *buffer, uint32_t offset) {
  unsigned char strbuf[32];
  BIGT *ipos = (BIGT *) (buffer+offset);
  bool valid = writeBigint(value, ipos);  // try fast track
  if(! valid) {  // slow track
    ToString(isolate, value)->WriteOneByte(isolate, strbuf, 0, 32);
    valid = stringToBigint((char *)strbuf, ipos);
  } 
  return valid ? writerOK : K_22003_OutOfRange.Get(isolate);
}

// Decimal.  JS Value to and from decimal types is treated as a string.
Local<Value> DecimalReader(const NdbDictionary::Column *col,
                            char *buffer, uint32_t offset) {
  char strbuf[96];
  int scale = col->getScale();
  int prec  = col->getPrecision();
  int len = scale + prec + 3;
  decimal_bin2str(buffer + offset, col->getSizeInBytes(),
                  prec, scale, strbuf, len);
  return NewUtf8String(isolate, strbuf);
}

Local<Value> DecimalWriter(const NdbDictionary::Column *col,
                            Local<Value> value, char *buffer, uint32_t offset) {
  unsigned char strbuf[96];
  if(! (isfinite(ToNumber(isolate, value)))) {
    return K_HY000.Get(isolate);
  } 
  int length = ToString(isolate, value)->WriteOneByte(isolate, strbuf, 0, 96);
  int status = decimal_str2bin((const char *) strbuf, length,
                               col->getPrecision(), col->getScale(), 
                               buffer + offset, col->getSizeInBytes());
  return status ? K_22003_OutOfRange.Get(isolate) : writerOK;
}


// Unsigned Decimal.  Writer adds boundary checking.
Local<Value> UnsignedDecimalWriter(const NdbDictionary::Column *col,
                                    Local<Value> value, char *buffer,
                                    uint32_t offset) {
  return ToNumber(isolate, value) >= 0 ?
    DecimalWriter(col, value, buffer, offset) :
    K_22003_OutOfRange.Get(isolate);
}


// Templated encoder for float and double
template<typename FPT> 
Local<Value> fpReader(const NdbDictionary::Column *col, 
                       char *buffer, uint32_t offset) {
  LOAD_ALIGNED_DATA(FPT, value, buffer+offset);
  return Number::New(isolate, value);
}

template<typename FPT>
Local<Value> fpWriter(const NdbDictionary::Column * col,
                       Local<Value> value,
                       char *buffer, uint32_t offset) {
  double dval = ToNumber(isolate, value);
  bool valid = isfinite(dval);
  if(valid) {
    STORE_ALIGNED_DATA(FPT, dval, buffer+offset);
  }
  return valid ? writerOK : K_22003_OutOfRange.Get(isolate);
}

/****** Binary & Varbinary *******/

Local<Value> BinaryReader(const NdbDictionary::Column *col, 
                           char *buffer, uint32_t offset) {
  return NewJsBuffer(isolate, buffer + offset, col->getLength());
}

Local<Value> BinaryWriter(const NdbDictionary::Column * col,
                          Local<Value> value, char *buffer, uint32_t offset) {
  bool valid = IsJsBuffer(value);
  if(valid) {
    Local<Object> obj = ToObject(isolate, value);
    uint32_t col_len = col->getLength();
    uint32_t data_len = GetBufferLength(obj);
    uint32_t ncopied = col_len > data_len ? data_len : col_len;
    memmove(buffer+offset, GetBufferData(obj), ncopied);
    if(ncopied < col_len) {
      memset(buffer+offset+ncopied, 0, col_len - ncopied); // padding
    }
  }
  return valid ? writerOK : K_0F001_Bad_BLOB.Get(isolate);
}

template<typename LENGTHTYPE>
Local<Value> varbinaryReader(const NdbDictionary::Column *col, 
                              char *buffer, uint32_t offset) {
  LOAD_ALIGNED_DATA(LENGTHTYPE, length, buffer+offset);
  char * data = buffer+offset+sizeof(length);
  return NewJsBuffer(isolate, data, length);
}

template<typename LENGTHTYPE>
Local<Value> varbinaryWriter(const NdbDictionary::Column * col,
                              Local<Value> value,
                              char *buffer, uint32_t offset) {
  bool valid = IsJsBuffer(value);
  if(valid) {
    LENGTHTYPE col_len = static_cast<LENGTHTYPE>(col->getLength());
    Local<Object> obj = ToObject(isolate, value);
    LENGTHTYPE data_len = static_cast<LENGTHTYPE>(GetBufferLength(obj));
    if(data_len > col_len) data_len = col_len;  // truncate
    STORE_ALIGNED_DATA(LENGTHTYPE, data_len, buffer+offset);
    char * data = buffer+offset+sizeof(data_len);
    memmove(data, GetBufferData(obj), data_len);
  }
  return valid ? writerOK : K_22000_DataError.Get(isolate);
}

/****** String types ********/

/** 
 * V8 can work with two kinds of external strings: Strict ASCII and UTF-16.
 * But working with external UTF-16 depends on MySQL's UTF-16-LE charset, 
 * which is available only in MySQL 5.6 and higher. 
 * 
 * (A) For any strict ASCII string, even if its character set is latin1 or
 *     UTF-8 (i.e. it could have non-ascii characters, but it doesn't), we 
 *     present it to V8 as external ASCII.
 * (B) If a string is UTF16LE, we present it as external UTF-16.
 * (C) If a string is UTF-8, we create a new JS string (one copy operation)
 * (D) All others must be recoded.  There are two possibilities:
 *   (D.1) Recode to UTF16LE and present as external string (one copy operation)
 *   (D.2) Recode to UTF8 and create a new JS string (two copy operations)
 *
 * For all string operations, we basically have four code paths:
 * (A), (B), (C), and (D.2). 
 * (D.1) is skipped because Cluster < 7.3 does not have UTF16LE and because
 * it requires some new interfaces from ColumnProxy to TypeEncoder 
 */

inline bool stringIsAscii(const unsigned char *str, uint32_t len) {
  for(unsigned int i = 0 ; i < len ; i++) 
    if(str[i] & 128) 
      return false;
  return true;
}

class ExternalizedAsciiString : public String::ExternalOneByteStringResource {
public:
  char * buffer;
  uint32_t len;
  bool isAscii;
  Persistent<Value> ref;
  ExternalizedAsciiString(char *_buffer, uint32_t _len) :
    buffer(_buffer), len(_len), isAscii(true)
  {
    ref.Reset();
  }
  const char* data() const override  { return buffer; }
  size_t length() const override     { return len; }
};

class ExternalizedUnicodeString : public String::ExternalStringResource { 
public:
  uint16_t * buffer;
  uint32_t len;  /* The number of two-byte characters in the string */
  bool isAscii;
  Persistent<Value> ref;
  ExternalizedUnicodeString(uint16_t *_buffer, uint32_t _len) :
    buffer(_buffer), len(_len), isAscii(false)
  {
    ref.Reset();
  }
  const uint16_t * data() const override { return buffer; }
  size_t length() const override         { return len; }
};

inline int getUtf8BufferSizeForColumn(int columnSizeInBytes,
                                      const EncoderCharset * csinfo) {
  int columnSizeInCharacters = columnSizeInBytes / csinfo->minlen;
  int utf8MaxChar = csinfo->maxlen < 3 ? csinfo->maxlen + 1 : 4;
  return (columnSizeInCharacters * utf8MaxChar);  
}

inline int getRecodeBufferSize(int length, int utf8Length,
                               const EncoderCharset * csinfo) {
  int result = csinfo->minlen * length; 
  result += (utf8Length - length) * (csinfo->maxlen - csinfo->minlen);
  return result;
}                                            


typedef int CharsetWriter(const NdbDictionary::Column *, 
                          Local<String>, char *, bool);

int writeUtf16le(const NdbDictionary::Column *, Local<String>, char *, bool);
int writeAscii(const NdbDictionary::Column *, Local<String>, char *, bool);
int writeUtf8(const NdbDictionary::Column *, Local<String>, char *, bool);
int writeGeneric(const NdbDictionary::Column *, Local<String>, char *, bool);
int writeRecode(const NdbDictionary::Column *, Local<String>, char *, bool);


inline CharsetWriter * getWriterForColumn(const NdbDictionary::Column *col) {
  const EncoderCharset * csinfo = getEncoderCharsetForColumn(col);
  CharsetWriter * writer = & writeGeneric;
  if(csinfo->isUtf8) writer = & writeUtf8;
  else if(csinfo->isUtf16le) writer = & writeUtf16le;
  else if(csinfo->isAscii) writer = & writeAscii;
  else if(csinfo->isMultibyte) writer = & writeRecode;
  return writer;
}

/* String writers.
   For CHAR, bufsz will be bigger than string size, so value will be padded.
*/
int writeUtf16le(const NdbDictionary::Column * column,
                 Local<String> strval, char * buffer, bool pad) {
  stats.direct_writes++;
  int bufsz = column->getLength() / 2;  /* Work in 16-byte characters */
  uint16_t * str = (uint16_t *) buffer;
  if(pad) for(int i = 0; i < bufsz ; i ++) str[i] = ' ';
  int charsWritten = strval->Write(isolate, str, 0, bufsz, String::NO_NULL_TERMINATION);
  int sz = charsWritten * 2;
  return sz;
}                

int writeUtf8(const NdbDictionary::Column * column,
               Local<String> strval, char * buffer, bool pad) {
  stats.direct_writes++;
  const int & bufsz = column->getLength();
  int sz = strval->WriteUtf8(isolate, buffer, bufsz, NULL, String::NO_NULL_TERMINATION);
  if(pad) 
    while(sz < bufsz) buffer[sz++] = ' ';
  return sz;
}

int writeAscii(const NdbDictionary::Column * column,
               Local<String> strval, char * buffer, bool pad) {
  stats.direct_writes++;
  const int & bufsz = column->getLength();
  int sz = strval->WriteOneByte(isolate, (uint8_t*) buffer, 0, bufsz, String::NO_NULL_TERMINATION);
  if(pad)
    while(sz < bufsz) buffer[sz++] = ' ';
  return sz;
}

int writeGeneric(const NdbDictionary::Column *col, 
                Local<String> strval, char * buffer, bool pad) {
  /* In UTF-8 encoding, only characters less than 0x7F are encoded with
     one byte.  Length() returns the string length in characters.
     So: Length() == Utf8Length() implies a strict ASCII string.
  */
  return (strval->Utf8Length(isolate) == strval->Length()) ?
    writeAscii(col, strval, buffer, pad) :
    writeRecode(col, strval, buffer, pad);
}

inline int recodeFromUtf8(const char * src, int srcLen, 
                   char * dest, int destLen, int destCharsetNumber) {
  CharsetMap csmap;
  int32_t lengths[2] = { srcLen, destLen };
  csmap.recode(lengths, csmap.getUTF8CharsetNumber(),
               destCharsetNumber, src, dest);
  return lengths[1];
}


/* We have two versions of writeRecode().
   One for non-Microsoft that recodes onto the stack.
   One for Microsoft where "char recode_stack[localInt]" is illegal.
*/
int writeRecode(const NdbDictionary::Column *col, 
                Local<String> strval, char * buffer, bool pad) {
  stats.recode_writes++;
  const EncoderCharset * csinfo = getEncoderCharsetForColumn(col);
  int columnSizeInBytes = col->getLength();
  int utf8bufferSize = getUtf8BufferSizeForColumn(columnSizeInBytes, csinfo);
 
  /* Write to the heap */
  char * recode_stack = new char[utf8bufferSize];
  int recodeSz = strval->WriteUtf8(isolate, recode_stack, utf8bufferSize,
                                   NULL, String::NO_NULL_TERMINATION);
  if(pad) {
    /* Pad all the way to the end of the recode buffer */
    while(recodeSz < utf8bufferSize) recode_stack[recodeSz++] = ' ';
  }

  int bytesWritten = recodeFromUtf8(recode_stack, recodeSz, 
                                    buffer, columnSizeInBytes,
                                    col->getCharsetNumber());
  delete[] recode_stack;
  return bytesWritten; 
}

/* TEXT column writer: bufferForText(column, value). 
   The CHAR and VARCHAR writers refer to the column length, but this TEXT 
   writer assumes the string will fit into the column and lets Ndb truncate 
   the value if needed.  
*/
void bufferForText(const Arguments & args) {
  isolate = args.GetIsolate();
  EscapableHandleScope scope(isolate);
  if(! args[1]->IsString()) {
    args.GetReturnValue().SetNull();
    return;
  }
  const NdbDictionary::Column * col =
    unwrapPointer<const NdbDictionary::Column *>(ArgToObject(args, 0));
  args.GetReturnValue().Set(
    scope.Escape(getBufferForText(col, ArgToString(args, 1))));
}

Local<Object> getBufferForText(const NdbDictionary::Column *col,
                               Local<String> str) {
  const EncoderCharset * csinfo = getEncoderCharsetForColumn(col);
  int length, utf8Length;
  Local<Object> buffer;
  char * data;

  /* Fully Externalized Value; no copying.
  */
  if(   (str->IsExternalOneByte() && ! csinfo->isMultibyte)
     || (str->IsExternal() && csinfo->isUtf16le))
  {
    DEBUG_PRINT("getBufferForText: fully externalized");
    stats.externalized_text_writes++;
    return NewJsBuffer(isolate, str);
  }

  length = str->Length();
  DEBUG_PRINT("getBufferForText: %s %d", col->getName(), length);
  utf8Length = str->Utf8Length(isolate);
  bool valueIsAscii = (utf8Length == length);
     
  if(csinfo->isAscii || (valueIsAscii && ! csinfo->isMultibyte)) {
    stats.direct_writes++;
    buffer = NewJsBuffer(isolate, length);
    data = GetBufferData(buffer);
    str->WriteOneByte(isolate, (uint8_t*) data, 0, length);
  } else if(csinfo->isUtf16le) {
    stats.direct_writes++;
    buffer = NewJsBuffer(isolate, length * 2);
    uint16_t * mbdata = (uint16_t*) GetBufferData(buffer);
    str->Write(isolate, mbdata, 0, length);
  } else if(csinfo->isUtf8) {
    stats.direct_writes++;
    buffer = NewJsBuffer(isolate, utf8Length);
    data = GetBufferData(buffer);
    str->WriteUtf8(isolate, data, utf8Length);
  } else {
    /* Recode */
    stats.recode_writes++;
    char * recode_buffer = new char[utf8Length];    
    str->WriteUtf8(isolate, recode_buffer, utf8Length, 0, String::NO_NULL_TERMINATION);
    int buflen = getRecodeBufferSize(length, utf8Length, csinfo);
    data = (char *) malloc(buflen);
    int result_len = recodeFromUtf8(recode_buffer, utf8Length,
                                    data, buflen, col->getCharsetNumber());
    buffer = NewJsBuffer(isolate, data, result_len, freeBufferContentsFromJs);
    delete[] recode_buffer;
  }
  
  return buffer;
}


// TEXT column reader textFromBuffer(column, buffer) 
void textFromBuffer(const Arguments & args) {
  isolate = args.GetIsolate();
  EscapableHandleScope scope(isolate);
  if(! args[1]->IsObject()) {
    args.GetReturnValue().SetNull();
    return;
  }
  const NdbDictionary::Column * col =
    unwrapPointer<const NdbDictionary::Column *>(ArgToObject(args, 0));
  args.GetReturnValue().Set(
    scope.Escape(getTextFromBuffer(col, ArgToObject(args, 1))));
}


Local<String> getTextFromBuffer(const NdbDictionary::Column *col,
                                 Local<Object> bufferObj) {
  const EncoderCharset * csinfo = getEncoderCharsetForColumn(col);
  uint32_t len = GetBufferLength(bufferObj);
  char * str = GetBufferData(bufferObj);

  Local<String> string;
  
  // We won't call stringIsAscii() on a whole big TEXT buffer...
  if(csinfo->isAscii) {
    stats.read_strings_externalized++;
    ExternalizedAsciiString *ext = new ExternalizedAsciiString(str, len);
    ext->ref.Reset(isolate, bufferObj);
    string = NewExternalOneByteString(isolate, ext);
  } else if (csinfo->isUtf16le) {
    stats.read_strings_externalized++;
    uint16_t * buf = (uint16_t *) str;
    ExternalizedUnicodeString * ext = new ExternalizedUnicodeString(buf, len/2);
    ext->ref.Reset(isolate, bufferObj);
    string = NewExternalTwoByteString(isolate, ext);
  } else {
    stats.read_strings_created++;
    if (csinfo->isUtf8) {
      DEBUG_PRINT("New from UTF8 [%d] %s", len, str);
      string = NewUtf8String(isolate, str, len);
    } else { // Recode
      stats.read_strings_recoded++;
      CharsetMap csmap;
      int32_t lengths[2];
      lengths[0] = len;
      lengths[1] = getUtf8BufferSizeForColumn(len, csinfo);
      DEBUG_PRINT("Recode [%d / %d]", lengths[0], lengths[1]);
      char * recode_buffer = new char[lengths[1]];
      csmap.recode(lengths, 
                   col->getCharsetNumber(),
                   csmap.getUTF8CharsetNumber(),
                   str, recode_buffer);
      DEBUG_PRINT("New from Recode [%d] %s", lengths[1], recode_buffer);
      string = NewUtf8String(isolate, recode_buffer, lengths[1]);
      delete[] recode_buffer;
    }
  }
  return string;
}  

// CHAR

Local<Value> CharReader(const NdbDictionary::Column *col, 
                         char *buffer, uint32_t offset) {
  char * str = buffer+offset;
  Local<String> string;
  int len = col->getLength();
  const EncoderCharset * csinfo = getEncoderCharsetForColumn(col);

  if(csinfo->isAscii || 
     (! csinfo->isMultibyte && stringIsAscii((const unsigned char *) str, len))) {
    stats.read_strings_externalized++;
    while(str[--len] == ' ') ;  // skip past space padding
    len++;  // undo 1 place
    ExternalizedAsciiString *ext = new ExternalizedAsciiString(str, len);
    string = NewExternalOneByteString(isolate, ext);
    //DEBUG_PRINT("(A): External ASCII");
  }
  else if(csinfo->isUtf16le) {
    len /= 2;
    stats.read_strings_externalized++;
    uint16_t * buf = (uint16_t *) str;
    while(buf[--len] == ' ') {}; len++;  // skip padding, then undo 1
    ExternalizedUnicodeString * ext = new ExternalizedUnicodeString(buf, len);
    string = NewExternalTwoByteString(isolate, ext);
    //DEBUG_PRINT("(B): External UTF-16-LE");
  }
  else if(csinfo->isUtf8) {
    stats.read_strings_created++;
    while(str[--len] == ' ') {}; len++; // skip padding, then undo 1
    string = NewUtf8String(isolate, str, len);
    //DEBUG_PRINT("(C): New From UTF-8");
  }
  else {
    stats.read_strings_created++;
    stats.read_strings_recoded++;
    CharsetMap csmap;
    int recode_size = getUtf8BufferSizeForColumn(len, csinfo);
    char * recode_buffer = new char[recode_size];

    /* Recode from the buffer into the UTF8 stack */
    int32_t lengths[2];
    lengths[0] = len;
    lengths[1] = recode_size;
    csmap.recode(lengths, 
                 col->getCharsetNumber(),
                 csmap.getUTF8CharsetNumber(),
                 str, recode_buffer);
    len = lengths[1];
    while(recode_buffer[--len] == ' ') {}; len++; // skip padding, then undo 1

    /* Create a new JS String from the UTF-8 recode buffer */
    string = NewUtf8String(isolate, recode_buffer, len);

    delete[] recode_buffer;

    //DEBUG_PRINT("(D.2): Recode to UTF-8 and create new");
  }

  return string;
}

Local<Value> CharWriter(const NdbDictionary::Column * col,
                            Local<Value> value,
                            char *buffer, uint32_t offset) {
  Local<String> strval = ToString(isolate, value);
  CharsetWriter * writer = getWriterForColumn(col);
  writer(col, strval, buffer+offset, true);
  return writerOK;
}

// Templated encoder for Varchar and LongVarchar
template<typename LENGTHTYPE>
Local<Value> varcharReader(const NdbDictionary::Column *col, 
                            char *buffer, uint32_t offset) {
  LOAD_ALIGNED_DATA(LENGTHTYPE, length, buffer+offset);
  char * str = buffer+offset+sizeof(length);
  Local<String> string;
  const EncoderCharset * csinfo = getEncoderCharsetForColumn(col);

  if(csinfo->isAscii || 
     (! csinfo->isMultibyte && stringIsAscii((const unsigned char *) str, length))) {
    stats.read_strings_externalized++;
    ExternalizedAsciiString *ext = new ExternalizedAsciiString(str, length);
    string = NewExternalOneByteString(isolate, ext);
    //DEBUG_PRINT("(A): External ASCII [size %d]", length);
  }
  else if(csinfo->isUtf16le) {
    stats.read_strings_externalized++;
    uint16_t * buf = (uint16_t *) str;
    ExternalizedUnicodeString * ext = new ExternalizedUnicodeString(buf, length/2);
    string = NewExternalTwoByteString(isolate, ext);
    //DEBUG_PRINT("(B): External UTF-16-LE [size %d]", length);
  }
  else if(csinfo->isUtf8) {
    stats.read_strings_created++;
    string = NewUtf8String(isolate, str, length);
    //DEBUG_PRINT("(C): New From UTF-8 [size %d]", length);
  }
  else {
    stats.read_strings_created++;
    stats.read_strings_recoded++;
    CharsetMap csmap;
    int recode_size = getUtf8BufferSizeForColumn(length, csinfo);
    char * recode_buffer = new char[recode_size];
    int32_t lengths[2];
    lengths[0] = length;
    lengths[1] = recode_size;
    csmap.recode(lengths, 
                 col->getCharsetNumber(), csmap.getUTF8CharsetNumber(),
                 str, recode_buffer);
    string = NewUtf8String(isolate, recode_buffer, lengths[1]);
    delete[] recode_buffer;
    //DEBUG_PRINT("(D.2): Recode to UTF-8 and create new [size %d]", length);
  }
  return string;
}

template<typename LENGTHTYPE>
Local<Value> varcharWriter(const NdbDictionary::Column * col,
                            Local<Value> value,
                            char *buffer, unsigned int offset) {
  Local<String> strval = ToString(isolate, value);
  CharsetWriter * writer = getWriterForColumn(col);

  LENGTHTYPE len = static_cast<LENGTHTYPE>
                    (writer(col, strval, buffer+offset+sizeof(len), false));
  STORE_ALIGNED_DATA(LENGTHTYPE, len, buffer+offset);

  return (strval->Length() > col->getLength()) ? K_22001_StringTooLong.Get(isolate) : writerOK;
}


/******  Temporal types ********/

/* TimeHelper defines a C structure for managing parts of a MySQL temporal type
   and is able to read and write a JavaScript object that handles that date
   with no loss of precision.
*/
class TimeHelper { 
public:
  TimeHelper() : 
    sign(+1), valid(true), fsp(0), 
    year(0), month(0), day(0), hour(0), minute(0), second(0), microsec(0)
    {}
  TimeHelper(Local<Value>);

  /* methods */
  Local<Value> toJs();
  void factor_HHMMSS(int int_time) {
    if(int_time < 0) { sign = -1; int_time = - int_time; }
    hour   = int_time/10000;
    minute = int_time/100 % 100;
    second = int_time % 100;
  }
  void factor_YYYYMMDD(int int_date) {
    year  = int_date/10000 % 10000;
    month = int_date/100 % 100;
    day   = int_date % 100;  
  }
  
  /* data */
  int sign;
  bool valid;
  unsigned int fsp, year, month, day, hour, minute, second, microsec;
};
 
Local<Value> TimeHelper::toJs() {
  Local<Object> obj = Object::New(isolate);
  SetProp(obj, K_sign.Get(isolate),     Integer::New(isolate, sign));
  SetProp(obj, K_year.Get(isolate),     Integer::New(isolate, year));
  SetProp(obj, K_month.Get(isolate),    Integer::New(isolate, month));
  SetProp(obj, K_day.Get(isolate),      Integer::New(isolate, day));
  SetProp(obj, K_hour.Get(isolate),     Integer::New(isolate, hour));
  SetProp(obj, K_minute.Get(isolate),   Integer::New(isolate, minute));
  SetProp(obj, K_second.Get(isolate),   Integer::New(isolate, second));
  SetProp(obj, K_microsec.Get(isolate), Integer::New(isolate, microsec));
  SetProp(obj, K_fsp.Get(isolate),      Integer::New(isolate, fsp));
  return obj;
}

inline int GetInt32(Local<Object> obj, Local<Value> key) {
  return GetInt32Property(isolate, obj, key);
}

inline int Has(Local<Object> obj, Local<Value> key) {
  return HasProperty(isolate, obj, key);
}

TimeHelper::TimeHelper(Local<Value> mysqlTime) :
  sign(+1), valid(false), 
  year(0), month(0), day(0), hour(0), minute(0), second(0), microsec(0)
{
  int nkeys = 0;


  if(mysqlTime->IsObject()) {

    Local<Value> _sign = K_sign.Get(isolate),
                 _year = K_year.Get(isolate),
                 _month = K_month.Get(isolate),
                 _day = K_day.Get(isolate),
                 _hour = K_hour.Get(isolate),
                 _minute = K_minute.Get(isolate),
                 _second = K_second.Get(isolate),
                 _microsec = K_microsec.Get(isolate),
                 _valid = K_valid.Get(isolate);

    Local<Object> obj = ToObject(isolate, mysqlTime);
    if(Has(obj, _valid) && ! GetBoolProperty(isolate, obj, _valid)) {
      return; // return with this.valid still set to false.
    }
    if(Has(obj, _sign))     { sign  = GetInt32(obj, _sign);        nkeys++; }
    if(Has(obj, _year))     { year  = GetInt32(obj, _year);        nkeys++; }
    if(Has(obj, _month))    { month = GetInt32(obj, _month);       nkeys++; }
    if(Has(obj, _day))      { day   = GetInt32(obj, _day);         nkeys++; }
    if(Has(obj, _hour))     { hour  = GetInt32(obj, _hour);        nkeys++; }
    if(Has(obj, _minute))   { minute= GetInt32(obj, _minute);      nkeys++; }
    if(Has(obj, _second))   { second= GetInt32(obj, _second);      nkeys++; }
    if(Has(obj, _microsec)) { microsec = GetInt32(obj, _microsec); nkeys++; }
  }
  valid = (nkeys > 0);
}


/* readFraction() returns value in microseconds
*/
unsigned int readFraction(const NdbDictionary::Column *col, char *buf) {
  int prec  = col->getPrecision();
  unsigned int usec = 0;
  if(prec > 0) {
    int bufsz = (1 + prec) / 2;
    usec = (unsigned int) unpack_bigendian(buf, bufsz);
    while(prec < 5) usec *= 100, prec += 2;
  }
  return usec;
}

void writeFraction(const NdbDictionary::Column *col, int usec, char *buf) {
  int prec  = col->getPrecision();
  if(prec > 0) {
    int bufsz = (1 + prec) / 2; // {1,1,2,2,3,3}
    while(prec < 5) usec /= 100, prec += 2;
    if(prec % 2) usec -= (usec % 10); // forced loss of precision
    pack_bigendian(usec, buf, bufsz);
  }
}


// Timstamp
Local<Value> TimestampReader(const NdbDictionary::Column *col, 
                              char *buffer, uint32_t offset) {
  LOAD_ALIGNED_DATA(uint32_t, timestamp, buffer+offset);
  double jsdate = timestamp * 1000;  // unix seconds-> js milliseconds
  return Date::New(isolate->GetCurrentContext(), jsdate).ToLocalChecked();
}                        

Local<Value> TimestampWriter(const NdbDictionary::Column * col,
                              Local<Value> value,
                              char *buffer, uint32_t offset) {
  uint32_t *tpos = (uint32_t *) (buffer+offset);
  double dval;
  bool valid = value->IsDate();
  if(valid) {
    dval = Date::Cast(*value)->ValueOf() / 1000;
    valid = (dval >= 0);   // MySQL does not accept dates before 1970
    *tpos = static_cast<uint32_t>(dval);
  }
  return valid ? writerOK : K_22007_InvalidDatetime.Get(isolate);
}


// Timestamp2
/* Timestamp2 is implemented to directly read and write Javascript Date.
   If col->getPrecision() > 3, some precision is lost.
*/
Local<Value> Timestamp2Reader(const NdbDictionary::Column *col, 
                               char *buffer, uint32_t offset) {
  uint32_t timeSeconds = (uint32_t) unpack_bigendian(buffer+offset, 4);
  int timeMilliseconds = readFraction(col, buffer+offset+4) / 1000;
  double jsdate = ((double) timeSeconds * 1000) + timeMilliseconds;
  return Date::New(isolate->GetCurrentContext(), jsdate).ToLocalChecked();
}
 
Local<Value> Timestamp2Writer(const NdbDictionary::Column * col,
                               Local<Value> value,
                               char *buffer, uint32_t offset) {
  bool valid = value->IsDate();
  if(valid) {
    double jsdate = Date::Cast(*value)->ValueOf();
    int64_t timeMilliseconds = (int64_t) jsdate;
    int64_t timeSeconds = timeMilliseconds / 1000;
    timeMilliseconds %= 1000;
    pack_bigendian(timeSeconds, buffer+offset, 4);
    writeFraction(col, static_cast<int>(timeMilliseconds * 1000), buffer+offset+4);
    valid = (timeSeconds >= 0);   // MySQL does not accept dates before 1970
  }
  return valid ? writerOK : K_22007_InvalidDatetime.Get(isolate);
}

/* Datetime 
   Interfaces with JavaScript via TimeHelper
*/
Local<Value> DatetimeReader(const NdbDictionary::Column *col, 
                             char *buffer, uint32_t offset) {
  TimeHelper tm;
  LOAD_ALIGNED_DATA(uint64_t, int_datetime, buffer+offset);
  int int_date = static_cast<int>(int_datetime / 1000000);
  tm.factor_YYYYMMDD(int_date);
  int int_time = static_cast<int>(int_datetime - (int_date * 1000000));
  // The time part of a stored datetime is non-negative
  assert(int_time >= 0);
  tm.factor_HHMMSS(int_time);
  return tm.toJs();
}

Local<Value> DatetimeWriter(const NdbDictionary::Column * col,
                              Local<Value> value,
                              char *buffer, uint32_t offset) {
  TimeHelper tm(value);
  uint64_t dtval = 0;
  if(tm.valid) {
    dtval += tm.year;      dtval *= 100;
    dtval += tm.month;     dtval *= 100;
    dtval += tm.day;       dtval *= 100;
    dtval += tm.hour;      dtval *= 100;
    dtval += tm.minute;    dtval *= 100;
    dtval += tm.second;
    STORE_ALIGNED_DATA(uint64_t, dtval, buffer+offset);
  }
  return tm.valid ? writerOK : K_22007_InvalidDatetime.Get(isolate);
}    


/* Datetime2
   Interfaces with JavaScript via TimeHelper

  The packed datetime2 integer part is:
   
  1 bit  sign (1= non-negative, 0= negative)     [ALWAYS POSITIVE IN MYSQL 5.6]
 17 bits year*13+month  (year 0-9999, month 1-12)
  5 bits day            (0-31)
  5 bits hour           (0-23)
  6 bits minute         (0-59)
  6 bits second         (0-59)
  ---------------------------
  40 bits = 5 bytes
*/
Local<Value> Datetime2Reader(const NdbDictionary::Column *col, 
                              char *buffer, uint32_t offset) {
  TimeHelper tm;
  uint64_t packedValue = unpack_bigendian(buffer+offset, 5);
  tm.microsec = readFraction(col, buffer+offset+5);
  tm.fsp = col->getPrecision();
  tm.second = (packedValue & 0x3F);       packedValue >>= 6;
  tm.minute = (packedValue & 0x3F);       packedValue >>= 6;
  tm.hour   = (packedValue & 0x1F);       packedValue >>= 5;
  tm.day    = (packedValue & 0x1F);       packedValue >>= 5;
  int yrMo  = (packedValue & 0x01FFFF);  
  tm.year = yrMo / 13;
  tm.month = yrMo % 13;
  return tm.toJs();
}

Local<Value> Datetime2Writer(const NdbDictionary::Column * col,
                              Local<Value> value,
                              char *buffer, uint32_t offset) {
  TimeHelper tm(value);
  uint64_t packedValue = 0;
  if(tm.valid) {
    packedValue = 1;                            packedValue <<= 17;
    packedValue |= (tm.year * 13 + tm.month);   packedValue <<= 5;
    packedValue |= tm.day;                      packedValue <<= 5;
    packedValue |= tm.hour;                     packedValue <<= 6;
    packedValue |= tm.minute;                   packedValue <<= 6;
    packedValue |= tm.second;                   
    pack_bigendian(packedValue, buffer+offset, 5);
    writeFraction(col, tm.microsec, buffer+offset+5);
  }
  return tm.valid ? writerOK : K_22007_InvalidDatetime.Get(isolate);
}


// Year
Local<Value> YearReader(const NdbDictionary::Column *col, 
                         char *buffer, uint32_t offset) {
  LOAD_ALIGNED_DATA(uint8_t, myr, buffer+offset);
  int year = 1900 + myr;
  return Number::New(isolate, year);
}

Local<Value> YearWriter(const NdbDictionary::Column * col,
                         Local<Value> value, char *buffer, uint32_t offset) {
  int chkv;
  if(value->IsInt32()) {
    chkv = GetInt32Value(isolate, value);
  } else {
    double dval = ToNumber(isolate, value);
    chkv = static_cast<int>(rint(dval));
  }

  chkv -= 1900;

  if(checkIntValue<uint8_t>(chkv)) {
    STORE_ALIGNED_DATA(uint8_t, chkv, buffer+offset);
    return writerOK;
  }
  return K_22007_InvalidDatetime.Get(isolate);
}


// Time.  Uses TimeHelper.
Local<Value> TimeReader(const NdbDictionary::Column *col, 
                         char *buffer, uint32_t offset) {
  TimeHelper tm;
  char * cbuf = buffer+offset;
  int sqlTime = sint3korr(cbuf);
  tm.factor_HHMMSS(sqlTime);
  return tm.toJs();
}

Local<Value> TimeWriter(const NdbDictionary::Column * col,
                         Local<Value> value, char *buffer, uint32_t offset) {
  TimeHelper tm(value);
  int dtval = 0;
  if(tm.valid) {
    dtval += tm.hour;      dtval *= 100;
    dtval += tm.minute;    dtval *= 100;
    dtval += tm.second;  
    dtval *= tm.sign;
    writeSignedMedium((int8_t *) buffer+offset, dtval);
  }  
  
  return tm.valid ? writerOK : K_22007_InvalidDatetime.Get(isolate);
}


/* Time2.  Uses TimeHelper.
  1 bit sign   (1= non-negative, 0= negative)
  1 bit unused  (reserved for INTERVAL type)
 10 bits hour   (0-838)
  6 bits minute (0-59) 
  6 bits second (0-59) 
  --------------------
  24 bits = 3 bytes whole-number part, + fractional part.
  If time is negative, then the entire value (including fractional part) 
  is converted to its two's complement.  readFraction() and writeFraction()
  cannot be used.
*/
Local<Value> Time2Reader(const NdbDictionary::Column *col, 
                          char *buffer, uint32_t offset) {
  TimeHelper tm;
  int prec = col->getPrecision();
  int fsp_size = (1 + prec) / 2;
  int buf_size = 3 + fsp_size;
  int fsp_bits = fsp_size * 8;
  int sign_pos = fsp_bits + 23;
  uint64_t fsp_mask = (1ULL << fsp_bits) - 1;
  uint64_t sign_val = 1ULL << sign_pos;
  uint64_t packedValue = unpack_bigendian(buffer+offset, buf_size);

  if((packedValue & sign_val) == sign_val) {
    tm.sign = 1;
  }
  else {
    tm.sign = -1;
    packedValue = sign_val - packedValue;   // two's complement
  }
  tm.fsp      = prec;
  tm.microsec = (int) (packedValue & fsp_mask);   packedValue >>= fsp_bits;
  tm.second   =       (packedValue & 0x3F);       packedValue >>= 6;
  tm.minute   =       (packedValue & 0x3F);       packedValue >>= 6;
  tm.hour     =       (packedValue & 0x03FF);     packedValue >>= 10;

  while(prec < 5) tm.microsec *= 100, prec += 2;

  return tm.toJs();
}

Local<Value> Time2Writer(const NdbDictionary::Column * col,
                          Local<Value> value, char *buffer, uint32_t offset) {
  TimeHelper tm(value);
  int prec = col->getPrecision();
  int fsp_size = (1 + prec) / 2;
  int buf_size = 3 + fsp_size;
  int fsp_bits = fsp_size * 8;
  uint64_t sign_val = 1ULL << (23 + fsp_bits);
  int fsec = tm.microsec;
  bool is_neg = (tm.sign < 0);
  uint64_t packedValue = 0;

  if(fsec) {
    while(prec < 5) fsec /= 100, prec += 2;
    if(prec % 2) fsec -= (fsec % 10); // forced loss of precision
  }

  if(tm.valid) {
    packedValue = (is_neg ? 0 : 1);         packedValue <<= 11;
    packedValue |= tm.hour;                 packedValue <<= 6;
    packedValue |= tm.minute;               packedValue <<= 6;
    packedValue |= tm.second;               packedValue <<= fsp_bits;
    packedValue |= fsec;
    if (is_neg) {
      packedValue = sign_val - packedValue;    // two's complement
    }
    pack_bigendian(packedValue, buffer+offset, buf_size);
  }
  
  return tm.valid ? writerOK : K_22007_InvalidDatetime.Get(isolate);
}


// Date
Local<Value> DateReader(const NdbDictionary::Column *col, 
                         char *buffer, uint32_t offset) {
  TimeHelper tm;
  char * cbuf = buffer+offset;
  int encodedDate = uint3korr(cbuf);
  tm.day   = (encodedDate & 31);  // five bits
  tm.month = (encodedDate >> 5 & 15); // four bits
  tm.year  = (encodedDate >> 9);
  return tm.toJs();
}

Local<Value> DateWriter(const NdbDictionary::Column * col,
                         Local<Value> value, char *buffer, uint32_t offset) {
  TimeHelper tm(value);
  int encodedDate = 0;
  if(tm.valid) {
    encodedDate = (tm.year << 9) | (tm.month << 5) | tm.day;
    writeUnsignedMedium((uint8_t *) buffer+offset, encodedDate);
  }  
  
  return tm.valid ? writerOK : K_22007_InvalidDatetime.Get(isolate);
}


// BLOB
// BlobReader is a no-op
Local<Value> BlobReader(const NdbDictionary::Column *, char *, uint32_t) {
  return writerOK;
}

// The BlobWriter does write anything, but it does verify that the 
// intended value is a Node Buffer.
Local<Value> BlobWriter(const NdbDictionary::Column *, Local<Value> value,
                        char *, uint32_t) {
  return IsJsBuffer(value) ? writerOK : K_0F001_Bad_BLOB.Get(isolate);
}
