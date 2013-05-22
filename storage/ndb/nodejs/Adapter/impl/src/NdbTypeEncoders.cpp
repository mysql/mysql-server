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
#include "NdbTypeEncoders.h"
#include "js_wrapper_macros.h"
#include "JsWrapper.h"
#include "ndb_util/CharsetMap.hpp"
#include "EncoderCharset.h"

#include "node_buffer.h"

using namespace v8;

Handle<String> 
  K_sign, K_year, K_month, K_day, K_hour, K_minute, K_second, K_microsec;

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

DECLARE_ENCODER(Year);
DECLARE_ENCODER(Timestamp);
DECLARE_ENCODER(Datetime);
DECLARE_ENCODER(Timestamp2);
DECLARE_ENCODER(Datetime2);
DECLARE_ENCODER(Time);
DECLARE_ENCODER(Time2);
DECLARE_ENCODER(Date);

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
  & DatetimeEncoder,                      // 18 DATETIME
  & DateEncoder,                          // 19 DATE
  & UnsupportedTypeEncoder,               // 20 BLOB
  & UnsupportedTypeEncoder,               // 21 TEXT
  & UnsupportedTypeEncoder,               // 22 BIT
  & LongVarcharEncoder,                   // 23 LONGVARCHAR
  & UnsupportedTypeEncoder,               // 24 LONGVARBINARY
  & TimeEncoder,                          // 25 TIME
  & YearEncoder,                          // 26 YEAR
  & TimestampEncoder,                     // 27 TIMESTAMP
  & UnsupportedTypeEncoder,               // 28 OLDDECIMAL UNSIGNED
  & UnsupportedTypeEncoder,               // 29 DECIMAL
  & UnsupportedTypeEncoder                // 30 DECIMAL UNSIGNED
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


/* Exports to JavaScript 
*/
void NdbTypeEncoders_initOnLoad(Handle<Object> target) {
  HandleScope scope;
  DEFINE_JS_FUNCTION(target, "encoderRead", encoderRead);
  DEFINE_JS_FUNCTION(target, "encoderWrite", encoderWrite);
  K_sign = Persistent<String>::New(String::NewSymbol("sign"));
  K_year = Persistent<String>::New(String::NewSymbol("year"));
  K_month = Persistent<String>::New(String::NewSymbol("month"));
  K_day = Persistent<String>::New(String::NewSymbol("day"));
  K_hour = Persistent<String>::New(String::NewSymbol("hour"));
  K_minute = Persistent<String>::New(String::NewSymbol("minute"));
  K_second = Persistent<String>::New(String::NewSymbol("second"));
  K_microsec = Persistent<String>::New(String::NewSymbol("microsec"));
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

Handle<Value> outOfRange(const char * column) { 
  HandleScope scope;
  Local<String> message = 
    String::Concat(String::New("Invalid value for column "),
                   String::New(column));
  Local<Value> error = message;
  return scope.Close(error);
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
    if(valid) writeSignedMedium(cbuf, chkv);
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
    if(valid) writeUnsignedMedium(cbuf, chkv);
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

// TODO: make these stats visible from JavaScript
struct encoder_stats_t {
  unsigned read_strings_externalized;
  unsigned read_strings_created;
  unsigned read_strings_recoded;
  unsigned direct_writes;
  unsigned recode_writes;
} stats;

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
  ExternalizedAsciiString(char *_buffer, size_t _len) : 
    buffer(_buffer), len(_len), isAscii(true) 
  {};
  const char* data() const       { return buffer; }
  size_t length() const          { return len; }
};

class ExternalizedUnicodeString : public String::ExternalStringResource { 
public:
  uint16_t * buffer;
  size_t len;  /* The number of two-byte characters in the string */
  bool isAscii;
  ExternalizedUnicodeString(uint16_t *_buffer, size_t _len) : 
    buffer(_buffer), len(_len), isAscii(false)
  {};
  const uint16_t * data() const  { return buffer; }
  size_t length() const          { return len; }
};

inline int getUtf8BufferSizeForColumn(int columnSizeInBytes,
                                      const EncoderCharset * csinfo) {
  int columnSizeInCharacters = columnSizeInBytes / csinfo->maxlen;
  int utf8MaxChar = csinfo->maxlen < 3 ? csinfo->maxlen + 1 : 4;
  return (columnSizeInCharacters * utf8MaxChar);  
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

/* We have two versions of writeRecode().
   One for non-Microsoft that recodes onto the stack.
   One for Microsoft where "char recode_stack[localInt]" is illegal.
*/
int writeRecode(const NdbDictionary::Column *col, 
                Handle<String> strval, char * buffer, bool pad) {
  stats.recode_writes++;
  CharsetMap csmap;
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

  /* Recode from UTF-8 on stack to column's charset in record buffer */
  int32_t lengths[2] = { recodeSz, columnSizeInBytes };
  csmap.recode(lengths, csmap.getUTF8CharsetNumber(),
               col->getCharsetNumber(), recode_stack, buffer);
  int bytesWritten = lengths[1];
#ifdef WIN32
  delete[] recode_stack;
#endif
  return bytesWritten; 
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
    while(buf[--len] == ' ') ; len++;  // skip padding, then undo 1
    ExternalizedUnicodeString * ext = new ExternalizedUnicodeString(buf, len);
    string = String::NewExternal(ext);
    //DEBUG_PRINT("(B): External UTF-16-LE");
  }
  else if(csinfo->isUtf8) {
    stats.read_strings_created++;
    while(str[--len] == ' ') ; len++; // skip padding, then undo 1
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
    int32_t lengths[2] = { len, recode_size } ;
    csmap.recode(lengths, 
                 col->getCharsetNumber(),
                 csmap.getUTF8CharsetNumber(),
                 str, recode_buffer);
    len = lengths[1];
    while(recode_buffer[--len] == ' ') ; len++; // skip padding, then undo 1

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
  bool valid = true;
  Handle<String> strval = value->ToString();
  CharsetWriter * writer = getWriterForColumn(col);
  writer(col, strval, buffer+offset, true);

  return valid ? writerOK : outOfRange(col->getName());
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
    int32_t lengths[2] = { length, recode_size } ;
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
  bool valid = true;
  Handle<String> strval = value->ToString();
  CharsetWriter * writer = getWriterForColumn(col);

  LENGTHTYPE len = writer(col, strval, buffer+offset+sizeof(len), false);
  STORE_ALIGNED_DATA(LENGTHTYPE, len, buffer+offset);
  return valid ? writerOK : outOfRange(col->getName());
}


/******  Temporal types ********/


/* TODO:
   All temporal types should be liberal in what they accept:
    number, string, TimeHelper, JS Date
   This will allow us to set TypeConverter.toDB := null
   and (in the case of NdbRecordObject) to avoid the trip out to JS and back
   on every setter call.
*/

/* TimeHelper defines a C structure for managing parts of a MySQL temporal type
   and is able to read and write a JavaScript object that handles that date
   with no loss of precision.
*/
class TimeHelper { 
public:
  TimeHelper() : 
    sign(+1), valid(true), 
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
  unsigned int year, month, day, hour, minute, second, microsec;
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
    register int bufsz = (1 + prec) / 2;
    while(prec < 5) usec /= 100, prec += 2;
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
  bool valid = value->IsDate();
  if(valid) {
    *tpos = Date::Cast(*value)->NumberValue() / 1000;
  }
  return valid ? writerOK : outOfRange(col->getName());
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
  double jsdate = (timeSeconds * 1000) + timeMilliseconds;
  return scope.Close(Date::New(jsdate));
}
 
Handle<Value> Timestamp2Writer(const NdbDictionary::Column * col,
                               Handle<Value> value, 
                               char *buffer, size_t offset) {
  bool valid = value->IsDate();
  if(valid) {
    double jsdate = Date::Cast(*value)->NumberValue();
    uint32_t timeSeconds = jsdate / 1000;
    jsdate -= (timeSeconds * 1000);
    int timeMilliseconds = jsdate;
    pack_bigendian(timeSeconds, buffer+offset, 4);
    writeFraction(col, timeMilliseconds * 1000, buffer+offset+4);
  }
  return valid ? writerOK : outOfRange(col->getName());
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
  return tm.valid ? writerOK : outOfRange(col->getName());  
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
  return tm.valid ? writerOK : outOfRange(col->getName());  
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
    valid = checkValue<uint8_t>(chkv);
    if(valid) STORE_ALIGNED_DATA(uint8_t, chkv, buffer+offset);
  }
  return valid ? writerOK : outOfRange(col->getName());
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
  
  return tm.valid ? writerOK : outOfRange(col->getName());  
}


/* Time2.  Uses TimeHelper.
  1 bit sign   (1= non-negative, 0= negative)
  1 bit unused  (reserved for INTERVAL type)
 10 bits hour   (0-838)
  6 bits minute (0-59) 
  6 bits second (0-59) 
  --------------------
  24 bits = 3 bytes
*/
Handle<Value> Time2Reader(const NdbDictionary::Column *col, 
                          char *buffer, size_t offset) {
  HandleScope scope;
  TimeHelper tm; 
  int packedValue = (int) unpack_bigendian(buffer+offset, 3);
  tm.second = (packedValue & 0x3F);       packedValue >>= 6;
  tm.minute = (packedValue & 0x3F);       packedValue >>= 6;
  tm.hour   = (packedValue & 0x03FF);     packedValue >>= 10;
  tm.sign   = (packedValue > 0 ? 1 : -1);
  tm.microsec = readFraction(col, buffer+offset+3);
  return scope.Close(tm.toJs());
}

Handle<Value> Time2Writer(const NdbDictionary::Column * col,
                          Handle<Value> value, char *buffer, size_t offset) {
  TimeHelper tm(value);
  int packedValue = 0;
  if(tm.valid) {
    packedValue = (tm.sign > 0 ? 1 : 0);         packedValue <<= 11;
    packedValue |= tm.hour;                      packedValue <<= 6;
    packedValue |= tm.minute;                    packedValue <<= 6;
    packedValue |= tm.second;
    pack_bigendian(packedValue, buffer+offset, 3);
    writeFraction(col, tm.microsec, buffer+offset+3);
  }  
  
  return tm.valid ? writerOK : outOfRange(col->getName());  
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
  
  return tm.valid ? writerOK : outOfRange(col->getName());  
}

