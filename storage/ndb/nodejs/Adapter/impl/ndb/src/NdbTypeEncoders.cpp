/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef WIN32
#include <float.h>
#endif

#include "adapter_global.h"
#include "NdbTypeEncoders.h"
#include "js_wrapper_macros.h"
#include "JsWrapper.h"

#include "node.h"
#include "node_buffer.h"

#include "ndb_util/CharsetMap.hpp"
#include "ndb_util/decimal_utils.hpp"
#include "EncoderCharset.h"

using namespace v8;

extern void freeBufferContentsFromJs(char *, void *);  // in BlobHandler.cpp

Handle<String>    /* keys of MySQLTime (Adapter/impl/common/MySQLTime.js) */
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

Handle<Value>   /* SQLState Error Codes */
  K_22000_DataError,
  K_22001_StringTooLong,
  K_22003_OutOfRange,
  K_22007_InvalidDatetime,
  K_0F001_Bad_BLOB,
  K_HY000;

#define ENCODER(A, B, C) NdbTypeEncoder A = { & B, & C, 0 }

#define DECLARE_ENCODER(TYPE) \
  EncoderReader TYPE##Reader; \
  EncoderWriter TYPE##Writer; \
  ENCODER(TYPE##Encoder, TYPE##Reader, TYPE##Writer)

#define DECLARE_ENCODER_TEMPLATES(TYPE) \
  template <typename T> Handle<Value> TYPE##Reader(const NdbDictionary::Column *,\
    char *, size_t); \
  template <typename T> Handle<Value> TYPE##Writer(const NdbDictionary::Column *, \
    Handle<Value>, char *, size_t);

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
Handle<Value> encoderRead(const Arguments & args) {
  HandleScope scope;
  const NdbDictionary::Column * col =
    unwrapPointer<const NdbDictionary::Column *>(args[0]->ToObject());
  const NdbTypeEncoder * encoder = getEncoderForColumn(col);
  char * buffer = node::Buffer::Data(args[1]->ToObject());
  
  return encoder->read(col, buffer, args[2]->Uint32Value());
}


/* write(col, value, buffer, offset) 
*/
Handle<Value> encoderWrite(const Arguments & args) {
  HandleScope scope;

  const NdbDictionary::Column * col =
    unwrapPointer<const NdbDictionary::Column *>(args[0]->ToObject());
  const NdbTypeEncoder * encoder = getEncoderForColumn(col);
  char * buffer = node::Buffer::Data(args[2]->ToObject());
  size_t offset = args[3]->Uint32Value();

  Handle<Value> error = encoder->write(col, args[1], buffer, offset);

  return scope.Close(error);
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
Handle<Value> GET_read_strings_externalized(Local<String>, const AccessorInfo &) {
  HandleScope scope;
  return scope.Close(Number::New(stats.read_strings_externalized));
}

Handle<Value> GET_read_strings_created(Local<String>, const AccessorInfo &) {
  HandleScope scope;
  return scope.Close(Number::New(stats.read_strings_created));
}

Handle<Value> GET_read_strings_recoded(Local<String>, const AccessorInfo &) {
  HandleScope scope;
  return scope.Close(Number::New(stats.read_strings_recoded));
}

Handle<Value> GET_externalized_text_writes(Local<String>, const AccessorInfo &){
  HandleScope scope;
  return scope.Close(Number::New(stats.externalized_text_writes));
}

Handle<Value> GET_direct_writes(Local<String>, const AccessorInfo &) {
  HandleScope scope;
  return scope.Close(Number::New(stats.direct_writes));
}

Handle<Value> GET_recode_writes(Local<String>, const AccessorInfo &) {
  HandleScope scope;
  return scope.Close(Number::New(stats.recode_writes));
}

Handle<Value> bufferForText(const Arguments &);
Handle<Value> textFromBuffer(const Arguments &);

void NdbTypeEncoders_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  DEFINE_JS_FUNCTION(target, "encoderRead", encoderRead);
  DEFINE_JS_FUNCTION(target, "encoderWrite", encoderWrite);
  DEFINE_JS_FUNCTION(target, "bufferForText", bufferForText);
  DEFINE_JS_FUNCTION(target, "textFromBuffer", textFromBuffer);
  K_sign = Persistent<String>::New(String::NewSymbol("sign"));
  K_year = Persistent<String>::New(String::NewSymbol("year"));
  K_month = Persistent<String>::New(String::NewSymbol("month"));
  K_day = Persistent<String>::New(String::NewSymbol("day"));
  K_hour = Persistent<String>::New(String::NewSymbol("hour"));
  K_minute = Persistent<String>::New(String::NewSymbol("minute"));
  K_second = Persistent<String>::New(String::NewSymbol("second"));
  K_microsec = Persistent<String>::New(String::NewSymbol("microsec"));
  K_fsp = Persistent<String>::New(String::NewSymbol("fsp"));
  K_valid = Persistent<String>::New(String::NewSymbol("valid"));
  K_22000_DataError = Persistent<String>::New(String::NewSymbol("22000"));
  K_22001_StringTooLong = Persistent<String>::New(String::NewSymbol("22001"));
  K_22003_OutOfRange = Persistent<String>::New(String::NewSymbol("22003"));
  K_22007_InvalidDatetime = Persistent<String>::New(String::NewSymbol("22007"));
  K_0F001_Bad_BLOB = Persistent<String>::New(String::NewSymbol("0F001"));
  K_HY000 = Persistent<String>::New(String::NewSymbol("HY000"));

  Persistent<Object> s = Persistent<Object>(Object::New());
  target->Set(Persistent<String>(String::NewSymbol("encoder_stats")), s);
  DEFINE_JS_ACCESSOR(s, "read_strings_externalized", 
                     GET_read_strings_externalized);
  DEFINE_JS_ACCESSOR(s, "read_strings_created", GET_read_strings_created);
  DEFINE_JS_ACCESSOR(s, "read_strings_recoded", GET_read_strings_recoded);
  DEFINE_JS_ACCESSOR(s, "externalized_text_writes", GET_externalized_text_writes);
  DEFINE_JS_ACCESSOR(s, "direct_writes", GET_direct_writes);
  DEFINE_JS_ACCESSOR(s, "recode_writes", GET_recode_writes);
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

/* File-scope global return from succesful write encoders: 
*/
Handle<Value> writerOK = Undefined();

template <typename INTSZ> Handle<Value> checkNumber(double);

template<> inline Handle<Value> checkNumber<int>(double d) {
  if(isfinite(d)) {
    return (d >= -2147483648.0 && d <= 2147483648.0) ? writerOK : K_22003_OutOfRange;
  }
  return K_HY000;
}

template<> inline Handle<Value> checkNumber<uint32_t>(double d) {
  if(isfinite(d)) {
    return (d >= 0 && d < 4294967296.0) ? writerOK : K_22003_OutOfRange;
  }
  return K_HY000;
}

template <typename INTSZ> bool checkIntValue(int);

template <typename INTSZ> inline Handle<Value> getStatusForValue(double d) {
  if(isfinite(d)) {
    return checkIntValue<INTSZ>(d) ? writerOK : K_22003_OutOfRange;
  }
  return K_HY000;
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

inline Handle<Value> checkMedium(int r) {
  return (r >= -8338608 && r < 8338608) ? writerOK :  K_22003_OutOfRange;
}

inline Handle<Value> getStatusForMedium(double dval) {
  if(isfinite(dval)) {
    return checkMedium(static_cast<int>(dval));
  }
  return K_HY000;
}

inline Handle<Value> checkUnsignedMedium(int r) {
  return (r >= 0 && r < 16277216) ? writerOK :  K_22003_OutOfRange;
}

inline Handle<Value> getStatusForUnsignedMedium(double dval) {
  if(isfinite(dval)) {
    return checkUnsignedMedium(static_cast<int>(dval));
  }
  return K_HY000;
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
   Derived from ndb/src/comon/util/NdbSqlUtil.cpp
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
Handle<Value> UnsupportedTypeReader(const NdbDictionary::Column *col, 
                                    char *buffer, size_t offset) {
  //TODO EXCEPTION
  return Undefined();
}

Handle<Value> UnsupportedTypeWriter(const NdbDictionary::Column * col,
                                    Handle<Value> value, 
                                    char *buffer, size_t offset) {
  //TODO EXCEPTION
  return Undefined();
}

// Int
Handle<Value> IntReader(const NdbDictionary::Column *col, 
                        char *buffer, size_t offset) {
  HandleScope scope;
  LOAD_ALIGNED_DATA(int, i, buffer+offset);
  return scope.Close(Integer::New(i));                        
}                        

Handle<Value> IntWriter(const NdbDictionary::Column * col,
                        Handle<Value> value, 
                        char *buffer, size_t offset) {
  int *ipos = (int *) (buffer+offset);
  Handle<Value> status;

  if(value->IsInt32()) {
    *ipos = value->Int32Value();
    status = writerOK;
  }
  else {
    double dval = value->ToNumber()->Value();
    *ipos = static_cast<int>(rint(dval));
    status = checkNumber<int>(dval);
  }
  return status;
}                        


// Unsigned Int
Handle<Value> UnsignedIntReader(const NdbDictionary::Column *col, 
                                char *buffer, size_t offset) {
  HandleScope scope;
  LOAD_ALIGNED_DATA(uint32_t, i, buffer+offset);
  return scope.Close(Integer::NewFromUnsigned(i));
}                        

Handle<Value> UnsignedIntWriter(const NdbDictionary::Column * col,
                                Handle<Value> value, 
                                char *buffer, size_t offset) {
  Handle<Value> status;
  uint32_t *ipos = (uint32_t *) (buffer+offset);
  if(value->IsUint32()) {
    *ipos = value->Uint32Value();
    status = writerOK;
  } else {
    double dval = value->ToNumber()->Value();
    *ipos = static_cast<uint32_t>(rint(dval));
    status = checkNumber<uint32_t>(dval);
  }
  return status;
}


// Templated encoder for TINY and SMALL int types
template <typename INTSZ>
Handle<Value> smallintReader(const NdbDictionary::Column *col, 
                             char *buffer, size_t offset) {
  HandleScope scope;
  LOAD_ALIGNED_DATA(INTSZ, i, buffer+offset);
  return scope.Close(Integer::New(i));
}


template <typename INTSZ> 
Handle<Value> smallintWriter(const NdbDictionary::Column * col,
                             Handle<Value> value, char *buffer, size_t offset) {
  INTSZ *ipos = (INTSZ *) (buffer+offset);
  Handle<Value> status;
  if(value->IsInt32()) {
    *ipos = value->Int32Value();
    status = checkIntValue<INTSZ>(*ipos) ? writerOK : K_22003_OutOfRange;
  } else {
    double dval = value->ToNumber()->Value();
    *ipos = static_cast<INTSZ>(dval);
    status = getStatusForValue<INTSZ>(dval);
  }
  return status;
}


// Medium signed & unsigned int types
Handle<Value> MediumReader(const NdbDictionary::Column *col, 
                           char *buffer, size_t offset) {
  HandleScope scope;
  char * cbuf = buffer+offset;
  int i = sint3korr(cbuf);
  return scope.Close(Integer::New(i));
}

Handle<Value> MediumWriter(const NdbDictionary::Column * col,
                           Handle<Value> value, char *buffer, size_t offset) {  
  int8_t *cbuf = (int8_t *) (buffer+offset);
  Handle<Value> status;
  double dval;
  int chkv;
  if(value->IsInt32()) {
    chkv = value->Int32Value();
    status = checkMedium(chkv);
  } else {
    dval = value->ToNumber()->Value();
    chkv = static_cast<int>(rint(dval));
    status = getStatusForMedium(dval);
  }
  writeSignedMedium(cbuf, chkv);

  return status;
}                        

Handle<Value> MediumUnsignedReader(const NdbDictionary::Column *col, 
                                   char *buffer, size_t offset) {
  HandleScope scope;
  char * cbuf = buffer+offset;
  int i = uint3korr(cbuf);
  return scope.Close(Integer::New(i));
}

Handle<Value> MediumUnsignedWriter(const NdbDictionary::Column * col,
                                   Handle<Value> value, 
                                   char *buffer, size_t offset) {
  uint8_t *cbuf = (uint8_t *) (buffer+offset);
  Handle<Value> status;
  double dval;
  int chkv;
  if(value->IsInt32()) {
    chkv = value->Int32Value();
    status = checkUnsignedMedium(chkv);
  } else {
    dval = value->ToNumber()->Value();
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

template<typename T> bool writeBigint(Handle<Value>, T *);

template<> inline bool writeBigint<int64_t>(Handle<Value> val, int64_t *ipos) {
  if(val->IsInt32()) {
    *ipos = val->Int32Value();
    return true;
  }
  return false;
}

template<> inline bool writeBigint<uint64_t>(Handle<Value> val, uint64_t *ipos) {
  if(val->IsUint32()) {
    *ipos = val->Uint32Value();
    return true;
  }
  return false;
}

template<typename T> void bigintToString(char *, T);

template<> inline void bigintToString<int64_t>(char * strbuf, int64_t bigint) {
  sprintf(strbuf, "%lld", bigint);
}

template<> inline void bigintToString<uint64_t>(char * strbuf, uint64_t bigint) {
  sprintf(strbuf, "%llu", bigint);
}

template <typename BIGT>
Handle<Value> bigintReader(const NdbDictionary::Column *col, 
                            char *buffer, size_t offset) {
  char strbuf[32];
  HandleScope scope;
  LOAD_ALIGNED_DATA(BIGT, bigint, buffer+offset);
  bigintToString(strbuf, bigint);
  return scope.Close(String::New(strbuf));
}

template <typename BIGT>
Handle<Value> bigintWriter(const NdbDictionary::Column *col, 
                           Handle<Value> value, char *buffer, size_t offset) {
  char strbuf[32];
  BIGT *ipos = (BIGT *) (buffer+offset);
  bool valid = writeBigint(value, ipos);  // try fast track
  if(! valid) {  // slow track
    value->ToString()->WriteAscii(strbuf, 0, 32);
    valid = stringToBigint(strbuf, ipos);
  } 
  return valid ? writerOK : K_22003_OutOfRange;
}

// Decimal.  JS Value to and from decimal types is treated as a string.
Handle<Value> DecimalReader(const NdbDictionary::Column *col,
                            char *buffer, size_t offset) {
  HandleScope scope;
  char strbuf[96];
  int scale = col->getScale();
  int prec  = col->getPrecision();
  int len = scale + prec + 3;
  decimal_bin2str(buffer + offset, col->getSizeInBytes(),
                  prec, scale, strbuf, len);
  return scope.Close(String::New(strbuf));
}

Handle<Value> DecimalWriter(const NdbDictionary::Column *col,
                            Handle<Value> value, char *buffer, size_t offset) {
  HandleScope scope;
  char strbuf[96];
  if(! (isfinite(value->NumberValue()))) {
    return K_HY000;
  } 
  int length = value->ToString()->WriteAscii(strbuf, 0, 96);
  int status = decimal_str2bin(strbuf, length, 
                               col->getPrecision(), col->getScale(), 
                               buffer + offset, col->getSizeInBytes());
  return status ? K_22003_OutOfRange : writerOK;
}


// Unsigned Decimal.  Writer adds boundary checking.
Handle<Value> UnsignedDecimalWriter(const NdbDictionary::Column *col,
                                    Handle<Value> value, char *buffer, 
                                    size_t offset) {
  HandleScope scope;
  return value->NumberValue() >= 0 ?
    DecimalWriter(col, value, buffer, offset) :
    K_22003_OutOfRange;
}


// Templated encoder for float and double
template<typename FPT> 
Handle<Value> fpReader(const NdbDictionary::Column *col, 
                       char *buffer, size_t offset) {
  HandleScope scope;
  LOAD_ALIGNED_DATA(FPT, value, buffer+offset);
  return scope.Close(Number::New(value)); 
}

template<typename FPT>
Handle<Value> fpWriter(const NdbDictionary::Column * col,
                       Handle<Value> value, 
                       char *buffer, size_t offset) {
  double dval = value->ToNumber()->NumberValue();
  bool valid = isfinite(dval);
  if(valid) {
    STORE_ALIGNED_DATA(FPT, dval, buffer+offset);
  }
  return valid ? writerOK : K_22003_OutOfRange;
}

/****** Binary & Varbinary *******/

Handle<Value> BinaryReader(const NdbDictionary::Column *col, 
                           char *buffer, size_t offset) {
  HandleScope scope;
  node::Buffer * b = node::Buffer::New(buffer + offset, col->getLength());
  return scope.Close(b->handle_);
}

Handle<Value> BinaryWriter(const NdbDictionary::Column * col,
                           Handle<Value> value, char *buffer, size_t offset) {
  bool valid = node::Buffer::HasInstance(value);
  if(valid) {
    Handle<Object> obj = value->ToObject();
    size_t col_len = col->getLength();
    size_t data_len = node::Buffer::Length(obj);
    size_t ncopied = col_len > data_len ? data_len : col_len;
    memmove(buffer+offset, node::Buffer::Data(obj), ncopied);
    if(ncopied < col_len) {
      memset(buffer+offset+ncopied, 0, col_len - ncopied); // padding
    }
  }
  return valid ? writerOK : K_22000_DataError;
}

template<typename LENGTHTYPE>
Handle<Value> varbinaryReader(const NdbDictionary::Column *col, 
                              char *buffer, size_t offset) {
  HandleScope scope;
  LOAD_ALIGNED_DATA(LENGTHTYPE, length, buffer+offset);
  char * data = buffer+offset+sizeof(length);
  node::Buffer *b = node::Buffer::New(data, length);
  return scope.Close(b->handle_);
}                            

template<typename LENGTHTYPE>
Handle<Value> varbinaryWriter(const NdbDictionary::Column * col,
                              Handle<Value> value, 
                              char *buffer, size_t offset) {
  bool valid = node::Buffer::HasInstance(value);
  if(valid) {
    size_t col_len = col->getLength();
    Handle<Object> obj = value->ToObject();
    LENGTHTYPE data_len = node::Buffer::Length(obj);
    if(data_len > col_len) data_len = col_len;  // truncate
    STORE_ALIGNED_DATA(LENGTHTYPE, data_len, buffer+offset);
    char * data = buffer+offset+sizeof(data_len);
    memmove(data, node::Buffer::Data(obj), data_len);
  }
  return valid ? writerOK : K_22000_DataError;
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

inline bool stringIsAscii(const unsigned char *str, size_t len) {
  for(unsigned int i = 0 ; i < len ; i++) 
    if(str[i] & 128) 
      return false;
  return true;
}

class ExternalizedAsciiString : public String::ExternalAsciiStringResource {
public:
  char * buffer;
  size_t len; 
  bool isAscii;
  Persistent<Value> ref;
  ExternalizedAsciiString(char *_buffer, size_t _len) : 
    buffer(_buffer), len(_len), isAscii(true)
  {
    ref.Clear();
  };
  ~ExternalizedAsciiString() 
  {
    if(! ref.IsEmpty()) ref.Dispose();
  }
  const char* data() const       { return buffer; }
  size_t length() const          { return len; }
};

class ExternalizedUnicodeString : public String::ExternalStringResource { 
public:
  uint16_t * buffer;
  size_t len;  /* The number of two-byte characters in the string */
  bool isAscii;
  Persistent<Value> ref;
  ExternalizedUnicodeString(uint16_t *_buffer, size_t _len) : 
    buffer(_buffer), len(_len), isAscii(false)
  {
    ref.Clear();
  };
  ~ExternalizedUnicodeString() 
  {
    if(! ref.IsEmpty()) ref.Dispose();
  }
  const uint16_t * data() const  { return buffer; }
  size_t length() const          { return len; }
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
                          Handle<String>, char *, bool);

int writeUtf16le(const NdbDictionary::Column *, Handle<String>, char *, bool);
int writeAscii(const NdbDictionary::Column *, Handle<String>, char *, bool);
int writeUtf8(const NdbDictionary::Column *, Handle<String>, char *, bool);
int writeGeneric(const NdbDictionary::Column *, Handle<String>, char *, bool);
int writeRecode(const NdbDictionary::Column *, Handle<String>, char *, bool);


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
                 Handle<String> strval, char * buffer, bool pad) {
  stats.direct_writes++;
  size_t bufsz = column->getLength() / 2;  /* Work in 16-byte characters */
  uint16_t * str = (uint16_t *) buffer;
  if(pad) for(size_t i = 0; i < bufsz ; i ++) str[i] = ' ';  
  int charsWritten = strval->Write(str, 0, bufsz, String::NO_NULL_TERMINATION); 
  int sz = charsWritten * 2;
  return sz;
}                

int writeUtf8(const NdbDictionary::Column * column,
               Handle<String> strval, char * buffer, bool pad) {
  stats.direct_writes++;
  const size_t & bufsz = column->getLength();
  size_t sz = strval->WriteUtf8(buffer, bufsz, NULL, String::NO_NULL_TERMINATION);
  if(pad) 
    while(sz < bufsz) buffer[sz++] = ' ';
  return sz;
}

int writeAscii(const NdbDictionary::Column * column,
               Handle<String> strval, char * buffer, bool pad) {
  stats.direct_writes++;
  const size_t & bufsz = column->getLength();
  size_t sz = strval->WriteAscii(buffer, 0, bufsz, String::NO_NULL_TERMINATION);
  if(pad)
    while(sz < bufsz) buffer[sz++] = ' ';
  return sz;
}

int writeGeneric(const NdbDictionary::Column *col, 
                Handle<String> strval, char * buffer, bool pad) {
  /* In UTF-8 encoding, only characters less than 0x7F are encoded with
     one byte.  Length() returns the string length in characters.
     So: Length() == Utf8Length() implies a strict ASCII string.
  */
  return (strval->Utf8Length() == strval->Length()) ?
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
                Handle<String> strval, char * buffer, bool pad) {
  stats.recode_writes++;
  const EncoderCharset * csinfo = getEncoderCharsetForColumn(col);
  int columnSizeInBytes = col->getLength();
  int utf8bufferSize = getUtf8BufferSizeForColumn(columnSizeInBytes, csinfo);
 
#ifdef WIN32
  /* Write to the heap */
  char * recode_stack = new char[utf8bufferSize];
#else
  /* Write the JavaScript string onto the stack as UTF-8 */
  char recode_stack[utf8bufferSize];
#endif
  int recodeSz = strval->WriteUtf8(recode_stack, utf8bufferSize,
                                   NULL, String::NO_NULL_TERMINATION);
  if(pad) {
    /* Pad all the way to the end of the recode buffer */
    while(recodeSz < utf8bufferSize) recode_stack[recodeSz++] = ' ';
  }

  int bytesWritten = recodeFromUtf8(recode_stack, recodeSz, 
                                    buffer, columnSizeInBytes,
                                    col->getCharsetNumber());
#ifdef WIN32
  delete[] recode_stack;
#endif
  return bytesWritten; 
}

/* TEXT column writer: bufferForText(column, value). 
   The CHAR and VARCHAR writers refer to the column length, but this TEXT 
   writer assumes the string will fit into the column and lets Ndb truncate 
   the value if needed.  
*/
Handle<Value> bufferForText(const Arguments & args) {
  if(! args[1]->IsString()) return Null();

  const NdbDictionary::Column * col =
    unwrapPointer<const NdbDictionary::Column *>(args[0]->ToObject());
  return getBufferForText(col, args[1]->ToString());
}

Handle<Object> getBufferForText(const NdbDictionary::Column *col, 
                                Handle<String> str) {
  HandleScope scope;
  const EncoderCharset * csinfo = getEncoderCharsetForColumn(col);
  size_t length, utf8Length;
  node::Buffer * buffer;
  char * data;

  /* Fully Externalized Value; no copying.
  */
  if(   (str->IsExternalAscii() && ! csinfo->isMultibyte)
     || (str->IsExternal() && csinfo->isUtf16le))
  {
    DEBUG_PRINT("getBufferForText: fully externalized");
    stats.externalized_text_writes++;
    return scope.Close(node::Buffer::New(str));
  }

  length = str->Length();
  DEBUG_PRINT("getBufferForText: %s %d", col->getName(), length);
  utf8Length = str->Utf8Length();
  bool valueIsAscii = (utf8Length == length);
     
  if(csinfo->isAscii || (valueIsAscii && ! csinfo->isMultibyte)) {
    stats.direct_writes++;
    buffer = node::Buffer::New(length);
    data = node::Buffer::Data(buffer);
    str->WriteAscii(data, 0, length);
  } else if(csinfo->isUtf16le) {
    stats.direct_writes++;
    buffer = node::Buffer::New(length * 2);
    uint16_t * mbdata = (uint16_t*) node::Buffer::Data(buffer);
    str->Write(mbdata, 0, length);
  } else if(csinfo->isUtf8) {
    stats.direct_writes++;
    buffer = node::Buffer::New(utf8Length);
    data = node::Buffer::Data(buffer);
    str->WriteUtf8(data, utf8Length);
  } else {
    /* Recode */
    stats.recode_writes++;
    char * recode_buffer = new char[utf8Length];    
    str->WriteUtf8(recode_buffer, utf8Length, 0, String::NO_NULL_TERMINATION);
    size_t buflen = getRecodeBufferSize(length, utf8Length, csinfo);
    data = (char *) malloc(buflen);
    size_t result_len = recodeFromUtf8(recode_buffer, utf8Length,
                                       data, buflen, col->getCharsetNumber());
    buffer = node::Buffer::New(data, result_len, freeBufferContentsFromJs, 0);
    delete[] recode_buffer;
  }
  
  return scope.Close(buffer->handle_);
}


// TEXT column reader textFromBuffer(column, buffer) 
Handle<Value> textFromBuffer(const Arguments & args) {  
  if(! args[1]->IsObject()) return Null();
  const NdbDictionary::Column * col =
    unwrapPointer<const NdbDictionary::Column *>(args[0]->ToObject());
  return getTextFromBuffer(col, args[1]->ToObject());
}


Handle<String> getTextFromBuffer(const NdbDictionary::Column *col, 
                                 Handle<Object> bufferObj) {
  HandleScope scope;

  const EncoderCharset * csinfo = getEncoderCharsetForColumn(col);
  size_t len = node::Buffer::Length(bufferObj);
  char * str = node::Buffer::Data(bufferObj);

  Local<String> string;
  
  // We won't call stringIsAscii() on a whole big TEXT buffer...
  if(csinfo->isAscii) {
    stats.read_strings_externalized++;
    ExternalizedAsciiString *ext = new ExternalizedAsciiString(str, len);
    ext->ref = Persistent<Value>::New(bufferObj);
    string = String::NewExternal(ext);
  } else if (csinfo->isUtf16le) {
    stats.read_strings_externalized++;
    uint16_t * buf = (uint16_t *) str;
    ExternalizedUnicodeString * ext = new ExternalizedUnicodeString(buf, len/2);
    ext->ref = Persistent<Value>::New(bufferObj);
    string = String::NewExternal(ext);        
  } else {
    stats.read_strings_created++;
    if (csinfo->isUtf8) {
      DEBUG_PRINT("New from UTF8 [%d] %s", len, str);
      string = String::New(str, len);
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
      string = String::New(recode_buffer, lengths[1]);
      delete[] recode_buffer;
    }
  }
  return scope.Close(string);
}  

// CHAR

Handle<Value> CharReader(const NdbDictionary::Column *col, 
                         char *buffer, size_t offset) {
  HandleScope scope;
  char * str = buffer+offset;
  Local<String> string;
  size_t len = col->getLength();
  const EncoderCharset * csinfo = getEncoderCharsetForColumn(col);

  if(csinfo->isAscii || 
     (! csinfo->isMultibyte && stringIsAscii((const unsigned char *) str, len))) {
    stats.read_strings_externalized++;
    while(str[--len] == ' ') ;  // skip past space padding
    len++;  // undo 1 place
    ExternalizedAsciiString *ext = new ExternalizedAsciiString(str, len);
    string = String::NewExternal(ext);   
    //DEBUG_PRINT("(A): External ASCII");
  }
  else if(csinfo->isUtf16le) {
    len /= 2;
    stats.read_strings_externalized++;
    uint16_t * buf = (uint16_t *) str;
    while(buf[--len] == ' ') {}; len++;  // skip padding, then undo 1
    ExternalizedUnicodeString * ext = new ExternalizedUnicodeString(buf, len);
    string = String::NewExternal(ext);
    //DEBUG_PRINT("(B): External UTF-16-LE");
  }
  else if(csinfo->isUtf8) {
    stats.read_strings_created++;
    while(str[--len] == ' ') {}; len++; // skip padding, then undo 1
    string = String::New(str, len);
    //DEBUG_PRINT("(C): New From UTF-8");
  }
  else {
    stats.read_strings_created++;
    stats.read_strings_recoded++;
    CharsetMap csmap;
    size_t recode_size = getUtf8BufferSizeForColumn(len, csinfo);
#ifdef WIN32
    char * recode_buffer = new char[recode_size];
#else
    char recode_buffer[recode_size];
#endif

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
    string = String::New(recode_buffer, len);

#ifdef WIN32
    delete[] recode_buffer;
#endif

    //DEBUG_PRINT("(D.2): Recode to UTF-8 and create new");
  }

  return scope.Close(string);
}

Handle<Value> CharWriter(const NdbDictionary::Column * col,
                            Handle<Value> value, 
                            char *buffer, size_t offset) {
  HandleScope scope;  
  Handle<String> strval = value->ToString();
  CharsetWriter * writer = getWriterForColumn(col);
  writer(col, strval, buffer+offset, true);
  return writerOK;
}

// Templated encoder for Varchar and LongVarchar
template<typename LENGTHTYPE>
Handle<Value> varcharReader(const NdbDictionary::Column *col, 
                            char *buffer, size_t offset) {
  HandleScope scope;
  LOAD_ALIGNED_DATA(LENGTHTYPE, length, buffer+offset);
  char * str = buffer+offset+sizeof(length);
  Local<String> string;
  const EncoderCharset * csinfo = getEncoderCharsetForColumn(col);

  if(csinfo->isAscii || 
     (! csinfo->isMultibyte && stringIsAscii((const unsigned char *) str, length))) {
    stats.read_strings_externalized++;
    ExternalizedAsciiString *ext = new ExternalizedAsciiString(str, length);
    string = String::NewExternal(ext);   
    //DEBUG_PRINT("(A): External ASCII [size %d]", length);
  }
  else if(csinfo->isUtf16le) {
    stats.read_strings_externalized++;
    uint16_t * buf = (uint16_t *) str;
    ExternalizedUnicodeString * ext = new ExternalizedUnicodeString(buf, length/2);
    string = String::NewExternal(ext);
    //DEBUG_PRINT("(B): External UTF-16-LE [size %d]", length);
  }
  else if(csinfo->isUtf8) {
    stats.read_strings_created++;
    string = String::New(str, length);
    //DEBUG_PRINT("(C): New From UTF-8 [size %d]", length);
  }
  else {
    stats.read_strings_created++;
    stats.read_strings_recoded++;
    CharsetMap csmap;
    size_t recode_size = getUtf8BufferSizeForColumn(length, csinfo);
#ifdef WIN32
    char * recode_buffer = new char[recode_size];
#else
    char recode_buffer[recode_size];
#endif
    int32_t lengths[2];
    lengths[0] = length;
    lengths[1] = recode_size;
    csmap.recode(lengths, 
                 col->getCharsetNumber(), csmap.getUTF8CharsetNumber(),
                 str, recode_buffer);
    string = String::New(recode_buffer, lengths[1]);
#ifdef WIN32
    delete[] recode_buffer;
#endif
    //DEBUG_PRINT("(D.2): Recode to UTF-8 and create new [size %d]", length);
  }
  return scope.Close(string);
}

template<typename LENGTHTYPE>
Handle<Value> varcharWriter(const NdbDictionary::Column * col,
                            Handle<Value> value, 
                            char *buffer, size_t offset) {  
  HandleScope scope;
  Handle<String> strval = value->ToString();
  CharsetWriter * writer = getWriterForColumn(col);

  LENGTHTYPE len = writer(col, strval, buffer+offset+sizeof(len), false);
  STORE_ALIGNED_DATA(LENGTHTYPE, len, buffer+offset);

  return (strval->Length() > col->getLength()) ? K_22001_StringTooLong : writerOK;
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
    {};
  TimeHelper(Handle<Value>);

  /* methods */
  Handle<Value> toJs();
  void factor_HHMMSS(int int_time) {
    if(int_time < 0) { sign = -1; int_time = - int_time; }
    hour   = int_time/10000;
    minute = int_time/100 % 100;
    second = int_time % 100;
  };
  void factor_YYYYMMDD(int int_date) {
    year  = int_date/10000 % 10000;
    month = int_date/100 % 100;
    day   = int_date % 100;  
  };
  
  /* data */
  int sign;
  bool valid;
  unsigned int fsp, year, month, day, hour, minute, second, microsec;
};
 
Handle<Value> TimeHelper::toJs() {
  HandleScope scope;  
  Local<Object> obj = Object::New();
  obj->Set(K_sign,     Integer::New(sign));
  obj->Set(K_year,     Integer::New(year));
  obj->Set(K_month,    Integer::New(month));
  obj->Set(K_day,      Integer::New(day));
  obj->Set(K_hour,     Integer::New(hour));
  obj->Set(K_minute,   Integer::New(minute));
  obj->Set(K_second,   Integer::New(second));
  obj->Set(K_microsec, Integer::New(microsec));
  obj->Set(K_fsp,      Integer::New(fsp));
  return scope.Close(obj);
}

TimeHelper::TimeHelper(Handle<Value> mysqlTime) :
  sign(+1), valid(false), 
  year(0), month(0), day(0), hour(0), minute(0), second(0), microsec(0)
{
  HandleScope scope;
  int nkeys = 0;

  if(mysqlTime->IsObject()) {
    Local<Object> obj = mysqlTime->ToObject();
    if(obj->Has(K_valid) && ! (obj->Get(K_valid)->BooleanValue())) {
      return; // return with this.valid still set to false.
    }
    if(obj->Has(K_sign))  { sign  = obj->Get(K_sign)->Int32Value(); nkeys++; }
    if(obj->Has(K_year))  { year  = obj->Get(K_year)->Int32Value(); nkeys++; }
    if(obj->Has(K_month)) { month = obj->Get(K_month)->Int32Value(); nkeys++; }
    if(obj->Has(K_day))   { day   = obj->Get(K_day)->Int32Value(); nkeys++; }
    if(obj->Has(K_hour))  { hour  = obj->Get(K_hour)->Int32Value(); nkeys++; }
    if(obj->Has(K_minute)){ minute= obj->Get(K_minute)->Int32Value(); nkeys++; }
    if(obj->Has(K_second)){ second= obj->Get(K_second)->Int32Value(); nkeys++; }
    if(obj->Has(K_microsec)){ microsec = obj->Get(K_microsec)->Int32Value(); nkeys++; }
  }
  valid = (nkeys > 0);
}


/* readFraction() returns value in microseconds
*/
int readFraction(const NdbDictionary::Column *col, char *buf) {
  int prec  = col->getPrecision();
  int usec = 0;
  if(prec > 0) {
    register int bufsz = (1 + prec) / 2;
    usec = unpack_bigendian(buf, bufsz);
    while(prec < 5) usec *= 100, prec += 2;
  }
  return usec;
}

void writeFraction(const NdbDictionary::Column *col, int usec, char *buf) {
  int prec  = col->getPrecision();
  if(prec > 0) {
    register int bufsz = (1 + prec) / 2; // {1,1,2,2,3,3}
    while(prec < 5) usec /= 100, prec += 2;
    if(prec % 2) usec -= (usec % 10); // forced loss of precision
    pack_bigendian(usec, buf, bufsz);
  }
}


// Timstamp
Handle<Value> TimestampReader(const NdbDictionary::Column *col, 
                              char *buffer, size_t offset) {
  HandleScope scope;
  LOAD_ALIGNED_DATA(uint32_t, timestamp, buffer+offset);
  double jsdate = timestamp * 1000;  // unix seconds-> js milliseconds
  return scope.Close(Date::New(jsdate));
}                        

Handle<Value> TimestampWriter(const NdbDictionary::Column * col,
                              Handle<Value> value, 
                              char *buffer, size_t offset) {
  uint32_t *tpos = (uint32_t *) (buffer+offset);
  double dval;
  bool valid = value->IsDate();
  if(valid) {
    dval = Date::Cast(*value)->NumberValue() / 1000;
    valid = (dval >= 0);   // MySQL does not accept dates before 1970
    *tpos = static_cast<uint32_t>(dval);
  }
  return valid ? writerOK : K_22007_InvalidDatetime;
}


// Timestamp2
/* Timestamp2 is implemented to directly read and write Javascript Date.
   If col->getPrecision() > 3, some precision is lost.
*/
Handle<Value> Timestamp2Reader(const NdbDictionary::Column *col, 
                               char *buffer, size_t offset) {
  HandleScope scope;
  uint32_t timeSeconds = unpack_bigendian(buffer+offset, 4);
  int timeMilliseconds = readFraction(col, buffer+offset+4) / 1000;
  double jsdate = ((double) timeSeconds * 1000) + timeMilliseconds;
  return scope.Close(Date::New(jsdate));
}
 
Handle<Value> Timestamp2Writer(const NdbDictionary::Column * col,
                               Handle<Value> value, 
                               char *buffer, size_t offset) {
  bool valid = value->IsDate();
  if(valid) {
    double jsdate = Date::Cast(*value)->NumberValue();
    int64_t timeMilliseconds = (int64_t) jsdate;
    int64_t timeSeconds = timeMilliseconds / 1000;
    timeMilliseconds %= 1000;
    pack_bigendian(timeSeconds, buffer+offset, 4);
    writeFraction(col, timeMilliseconds * 1000, buffer+offset+4);
    valid = (timeSeconds >= 0);   // MySQL does not accept dates before 1970
  }
  return valid ? writerOK : K_22007_InvalidDatetime;
}

/* Datetime 
   Interfaces with JavaScript via TimeHelper
*/
Handle<Value> DatetimeReader(const NdbDictionary::Column *col, 
                             char *buffer, size_t offset) {
  HandleScope scope;
  TimeHelper tm;
  LOAD_ALIGNED_DATA(uint64_t, int_datetime, buffer+offset);
  int int_date = int_datetime / 1000000;
  tm.factor_YYYYMMDD(int_date);
  tm.factor_HHMMSS(int_datetime - (uint64_t) int_date * 1000000);
  return scope.Close(tm.toJs());
}

Handle<Value> DatetimeWriter(const NdbDictionary::Column * col,
                              Handle<Value> value, 
                              char *buffer, size_t offset) {
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
  return tm.valid ? writerOK : K_22007_InvalidDatetime;  
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
Handle<Value> Datetime2Reader(const NdbDictionary::Column *col, 
                              char *buffer, size_t offset) {
  HandleScope scope;
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
  return scope.Close(tm.toJs());
}

Handle<Value> Datetime2Writer(const NdbDictionary::Column * col,
                              Handle<Value> value, 
                              char *buffer, size_t offset) {
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
  return tm.valid ? writerOK : K_22007_InvalidDatetime;  
}


// Year
Handle<Value> YearReader(const NdbDictionary::Column *col, 
                         char *buffer, size_t offset) {
  HandleScope scope;
  LOAD_ALIGNED_DATA(uint8_t, myr, buffer+offset);
  int year = 1900 + myr;
  return scope.Close(Number::New(year));
}

Handle<Value> YearWriter(const NdbDictionary::Column * col,
                         Handle<Value> value, char *buffer, size_t offset) {
  bool valid = value->IsInt32();
  if(valid) {
    int chkv = value->Int32Value() - 1900;
    valid = checkIntValue<uint8_t>(chkv);
    if(valid) STORE_ALIGNED_DATA(uint8_t, chkv, buffer+offset);
  }
  return valid ? writerOK : K_22007_InvalidDatetime;
}


// Time.  Uses TimeHelper.
Handle<Value> TimeReader(const NdbDictionary::Column *col, 
                         char *buffer, size_t offset) {
  HandleScope scope;
  TimeHelper tm; 
  char * cbuf = buffer+offset;
  int sqlTime = sint3korr(cbuf);
  tm.factor_HHMMSS(sqlTime);
  return scope.Close(tm.toJs());
}

Handle<Value> TimeWriter(const NdbDictionary::Column * col,
                         Handle<Value> value, char *buffer, size_t offset) {
  TimeHelper tm(value);
  int dtval = 0;
  if(tm.valid) {
    dtval += tm.hour;      dtval *= 100;
    dtval += tm.minute;    dtval *= 100;
    dtval += tm.second;  
    dtval *= tm.sign;
    writeSignedMedium((int8_t *) buffer+offset, dtval);
  }  
  
  return tm.valid ? writerOK : K_22007_InvalidDatetime;  
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
Handle<Value> Time2Reader(const NdbDictionary::Column *col, 
                          char *buffer, size_t offset) {
  HandleScope scope;
  TimeHelper tm;
  int prec = col->getPrecision();
  int fsp_size = (1 + prec) / 2;
  int buf_size = 3 + fsp_size;
  int fsp_bits = fsp_size * 8;
  int fsp_mask = (1UL << fsp_bits) - 1;
  int sign_pos = fsp_bits + 23;
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
  tm.microsec = (packedValue & fsp_mask);   packedValue >>= fsp_bits;
  tm.second   = (packedValue & 0x3F);       packedValue >>= 6;
  tm.minute   = (packedValue & 0x3F);       packedValue >>= 6;
  tm.hour     = (packedValue & 0x03FF);     packedValue >>= 10;

  while(prec < 5) tm.microsec *= 100, prec += 2;

  return scope.Close(tm.toJs());
}

Handle<Value> Time2Writer(const NdbDictionary::Column * col,
                          Handle<Value> value, char *buffer, size_t offset) {
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
  
  return tm.valid ? writerOK : K_22007_InvalidDatetime;  
}


// Date
Handle<Value> DateReader(const NdbDictionary::Column *col, 
                         char *buffer, size_t offset) {
  HandleScope scope;
  TimeHelper tm; 
  char * cbuf = buffer+offset;
  int encodedDate = uint3korr(cbuf);
  tm.day   = (encodedDate & 31);  // five bits
  tm.month = (encodedDate >> 5 & 15); // four bits
  tm.year  = (encodedDate >> 9);
  return scope.Close(tm.toJs());
}

Handle<Value> DateWriter(const NdbDictionary::Column * col,
                         Handle<Value> value, char *buffer, size_t offset) {
  TimeHelper tm(value);
  int encodedDate = 0;
  if(tm.valid) {
    encodedDate = (tm.year << 9) | (tm.month << 5) | tm.day;
    writeUnsignedMedium((uint8_t *) buffer+offset, encodedDate);
  }  
  
  return tm.valid ? writerOK : K_22007_InvalidDatetime;  
}


// BLOB
// BlobReader is a no-op
Handle<Value> BlobReader(const NdbDictionary::Column *, char *, size_t) {
  HandleScope scope;
  return Undefined();
}

// The BlobWriter does write anything, but it does verify that the 
// intended value is a Node Buffer.
Handle<Value> BlobWriter(const NdbDictionary::Column *, Handle<Value> value,
                        char *, size_t) {
  return node::Buffer::HasInstance(value) ? writerOK : K_0F001_Bad_BLOB;  
}
