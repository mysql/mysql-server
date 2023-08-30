#ifndef STREAM_EXPECTATIONS_H_
#define STREAM_EXPECTATIONS_H_

#include "assertions.h"
#include "cbor.h"

#define MAX_QUEUE_ITEMS 30

// Utilities to test `cbor_stream_decode`.  See `cbor_stream_decode_test.cc`.
//
// Usage:
// - The `assert_` helpers build a queue of `test_assertion`s
//   (`assertions_queue` in the implementation file), specifying
//  - Which callback is expected (`test_expectation`)
//  - And what is the expected argument value (if applicable,
//    `test_expectation_data`)
// - `decode` will invoke `cbor_stream_decode` (test subject)
// - `cbor_stream_decode` will invoke one of the `_callback` functions, which
//   will check the passed data against the `assertions_queue`

enum test_expectation {
  UINT8_EQ,
  UINT16_EQ,
  UINT32_EQ,
  UINT64_EQ,

  NEGINT8_EQ,
  NEGINT16_EQ,
  NEGINT32_EQ,
  NEGINT64_EQ,

  // Matches length and memory address for definite strings
  BSTRING_MEM_EQ,
  BSTRING_INDEF_START,

  STRING_MEM_EQ,
  STRING_INDEF_START,

  ARRAY_START, /* Definite arrays only */
  ARRAY_INDEF_START,

  MAP_START, /* Definite maps only */
  MAP_INDEF_START,

  TAG_EQ,

  HALF_EQ,
  FLOAT_EQ,
  DOUBLE_EQ,
  BOOL_EQ,
  NIL,
  UNDEF,
  INDEF_BREAK /* Expect "Break" */
};

union test_expectation_data {
  uint8_t int8;
  uint16_t int16;
  uint32_t int32;
  uint64_t int64;
  struct string {
    cbor_data address;
    size_t length;
  } string;
  size_t length;
  float float2;
  float float4;
  double float8;
  bool boolean;
};

struct test_assertion {
  enum test_expectation expectation;
  union test_expectation_data data;
};

/* Test harness -- calls `cbor_stream_decode` and checks assertions */
struct cbor_decoder_result decode(cbor_data, size_t);

/* Verify all assertions were applied and clean up */
int clean_up_stream_assertions(void **);

/* Assertions builders */
void assert_uint8_eq(uint8_t);
void assert_uint16_eq(uint16_t);
void assert_uint32_eq(uint32_t);
void assert_uint64_eq(uint64_t);

void assert_negint8_eq(uint8_t);
void assert_negint16_eq(uint16_t);
void assert_negint32_eq(uint32_t);
void assert_negint64_eq(uint64_t);

void assert_bstring_mem_eq(cbor_data, size_t);
void assert_bstring_indef_start(void);

void assert_string_mem_eq(cbor_data, size_t);
void assert_string_indef_start(void);

void assert_array_start(size_t);
void assert_indef_array_start(void);

void assert_map_start(size_t);
void assert_indef_map_start(void);

void assert_tag_eq(uint64_t);

void assert_half(float);
void assert_float(float);
void assert_double(double);

void assert_bool(bool);
void assert_nil(void); /* assert_null already exists */
void assert_undef(void);

void assert_indef_break(void);

/* Assertions verifying callbacks */
void uint8_callback(void *, uint8_t);
void uint16_callback(void *, uint16_t);
void uint32_callback(void *, uint32_t);
void uint64_callback(void *, uint64_t);

void negint8_callback(void *, uint8_t);
void negint16_callback(void *, uint16_t);
void negint32_callback(void *, uint32_t);
void negint64_callback(void *, uint64_t);

void byte_string_callback(void *, cbor_data, uint64_t);
void byte_string_start_callback(void *);

void string_callback(void *, cbor_data, uint64_t);
void string_start_callback(void *);

void array_start_callback(void *, uint64_t);
void indef_array_start_callback(void *);

void map_start_callback(void *, uint64_t);
void indef_map_start_callback(void *);

void tag_callback(void *, uint64_t);

void half_callback(void *, float);
void float_callback(void *, float);
void double_callback(void *, double);
void indef_break_callback(void *);

void bool_callback(void *, bool);
void null_callback(void *);
void undef_callback(void *);

#endif
