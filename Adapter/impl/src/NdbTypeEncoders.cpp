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

#include "adapter_global.h"
#include "unified_debug.h"
#include "NdbTypeEncoders.h"
#include "js_wrapper_macros.h"
#include "JsWrapper.h"

using namespace v8;

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

DECLARE_ENCODER(Char);
DECLARE_ENCODER_TEMPLATES(varchar);
ENCODER(VarcharEncoder, varcharReader<uint8_t>, varcharWriter<uint8_t>);
ENCODER(LongVarcharEncoder, varcharReader<uint16_t>, varcharWriter<uint16_t>);


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
  & UnsupportedTypeEncoder,               // 16 BINARY
  & UnsupportedTypeEncoder,               // 17 VARBINARY
  & UnsupportedTypeEncoder,               // 18 DATETIME
  & UnsupportedTypeEncoder,               // 19 DATE
  & UnsupportedTypeEncoder,               // 20 BLOB
  & UnsupportedTypeEncoder,               // 21 TEXT
  & UnsupportedTypeEncoder,               // 22 BIT
  & LongVarcharEncoder,                   // 23 LONGVARCHAR
  & UnsupportedTypeEncoder,               // 24 LONGVARBINARY
  & UnsupportedTypeEncoder,               // 25 TIME
  & UnsupportedTypeEncoder,               // 26 YEAR
  & UnsupportedTypeEncoder,               // 27 TIMESTAMP
  & UnsupportedTypeEncoder,               // 28 OLDDECIMAL UNSIGNED
  & UnsupportedTypeEncoder,               // 29 DECIMAL
  & UnsupportedTypeEncoder                // 30 DECIMAL UNSIGNED
#if NDB_TYPE_MAX > 31
  ,
  & UnsupportedTypeEncoder,               // 31 TIME2
  & UnsupportedTypeEncoder,               // 32 DATETIME2
  & UnsupportedTypeEncoder,               // 33 TIMESTAMP2
#endif
};


inline const NdbTypeEncoder * getEncoderForColumn(const NdbDictionary::Column *col) {
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

  if(! error->IsUndefined()) ThrowException(error);

  return scope.Close(error);
}


/* Exports to JavaScript 
*/
void NdbTypeEncoders_initOnLoad(Handle<Object> target) {
  DEFINE_JS_FUNCTION(target, "encoderRead", encoderRead);
  DEFINE_JS_FUNCTION(target, "encoderWrite", encoderWrite);
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

template <typename INTSZ> bool checkValue(int);

template<> inline bool checkValue<int8_t>(int r) {
  return (r >= -128 && r < 128);
}

template<> inline bool checkValue<uint8_t>(int r) {
  return (r >= 0 && r < 256);
}

template<> inline bool checkValue<int16_t>(int r) {
  return (r >= -32768 && r < 32768);
}

template<> inline bool checkValue<uint16_t>(int r) {
  return (r >= 0 && r < 65536);
}

inline bool checkMedium(int r) {
  return (r >= -8338608 && r < 8338608);
}

inline bool checkUnsignedMedium(int r) {
  return (r >= 0 && r < 16277216);
}

Handle<Value> outOfRange(const char * column) { 
  HandleScope scope;
  Local<String> message = 
    String::Concat(String::New("Invalid value for column"),
                   String::New(column));
  Local<Value> error = Exception::RangeError(message);
  return scope.Close(error);
}

/* File-scope global return from succesful write encoders: 
*/
Handle<Value> writerOK = Undefined();


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
  bool valid = value->IsInt32();
  if(valid) {
    *ipos = value->Int32Value();
  }
  return valid ? writerOK : outOfRange(col->getName());
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
  uint32_t *ipos = (uint32_t *) (buffer+offset);
  bool valid = value->IsUint32();
  if(valid) {
    *ipos = value->Uint32Value();
  }
  return valid ? writerOK : outOfRange(col->getName());
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
  bool valid = value->IsInt32();
  if(valid) {
    int chkv = value->Int32Value();
    valid = checkValue<INTSZ>(chkv);
    if(valid) *ipos = chkv;
  }
  return valid ? writerOK : outOfRange(col->getName());
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
  bool valid = value->IsInt32();
  if(valid) {
    int chkv = value->Int32Value();
    valid = checkMedium(chkv);
    if(valid) {
      cbuf[0] = (int8_t) (chkv);
      cbuf[1] = (int8_t) (chkv >> 8);
      cbuf[2] = (int8_t) (chkv >> 16);        
    }
  }
  return valid ? writerOK : outOfRange(col->getName());
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
  bool valid = value->IsInt32();
  if(valid) {
    int chkv = value->Int32Value();
    valid = checkUnsignedMedium(chkv);
    if(valid) {
      cbuf[0] = (uint8_t) (chkv);
      cbuf[1] = (uint8_t) (chkv >> 8);
      cbuf[2] = (uint8_t) (chkv >> 16);        
    }
  }
  return valid ? writerOK : outOfRange(col->getName());
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
  return valid ? writerOK : outOfRange(col->getName());
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
  bool valid = value->IsNumber();
  if(valid) {
    STORE_ALIGNED_DATA(FPT, value->NumberValue(), buffer+offset);
  }
  return valid ? writerOK : outOfRange(col->getName());
}


/****** String types ********/
// TODO: Provide routines to determine if a string can be externalized
//   Resolve linker issues with charset map before attempting this

// CHAR
Handle<Value> CharReader(const NdbDictionary::Column *col, 
                         char *buffer, size_t offset) {
  HandleScope scope;
  const char * str = buffer+offset;
  //TODO CHARSET CONVERSION
  //TODO SOME STRINGS CAN BE EXTERNALIZED: strict ascii, UTF16, UTF8
  Local<String> string = String::New(str, col->getLength());
  return scope.Close(string);
}

Handle<Value> CharWriter(const NdbDictionary::Column * col,
                            Handle<Value> value, 
                            char *buffer, size_t offset) {
  HandleScope scope;  
  Handle<String> strval = value->ToString();
  int len = strval->Length();
  bool valid = (len <= col->getLength());
  if(valid) {
    /* copy string into buffer */
    strval->WriteAscii(buffer+offset, 0, len);
    /* right-pad with spaces */
    for(char *s = buffer+offset+len ; len < col->getLength(); len++) {
      *(s++) = ' ';
    }
  }
  return valid ? writerOK : outOfRange(col->getName());
}

// Templated encoder for Varchar and LongVarchar
template<typename LENGTHTYPE>
Handle<Value> varcharReader(const NdbDictionary::Column *col, 
                            char *buffer, size_t offset) {
  HandleScope scope;
  LOAD_ALIGNED_DATA(LENGTHTYPE, length, buffer+offset);
  const char * str = buffer+offset+sizeof(length);
  //TODO CHARSET CONVERSION
  //TODO SOME STRINGS CAN BE EXTERNALIZED: strict ascii, UTF16, UTF8
  Local<String> string = String::New(str, length);

  return scope.Close(string);
}

template<typename LENGTHTYPE>
Handle<Value> varcharWriter(const NdbDictionary::Column * col,
                            Handle<Value> value, 
                            char *buffer, size_t offset) {  
  HandleScope scope;  
  Handle<String> strval = value->ToString();
  LENGTHTYPE len = strval->Length();
  bool valid = (len <= col->getLength());
  if(valid) {
    STORE_ALIGNED_DATA(LENGTHTYPE, len, buffer+offset);
    // TODO: switch(col->getCharsetNumber()) { ... 
    strval->WriteAscii(buffer+offset+sizeof(len), 0, len);
  }
  return valid ? writerOK : outOfRange(col->getName());
}


/******  Temporal types ********/
typedef struct {
  unsigned int year, month, day, hour, minute, second, msec;
  bool is_negative;
} time_helper;

inline void factor_HHMMSS(time_helper *tm, int32_t int_time) {
  if(int_time < 0) {
    tm->is_negative = true; int_time = - int_time;
  }
  tm->hour   = int_time/10000;
  tm->minute = int_time/100 % 100;
  tm->second = int_time % 100;  
}

inline void factor_YYYYMMDD(time_helper *tm, int32_t int_date) {
  tm->year  = int_date/10000 % 10000;
  tm->month = int_date/100 % 100;
  tm->day   = int_date % 100;  
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
  bool valid = value->IsDate();
  if(valid) {
    *tpos = Date::Cast(*value)->NumberValue() / 1000;
  }
  return valid ? writerOK : outOfRange(col->getName());
}

