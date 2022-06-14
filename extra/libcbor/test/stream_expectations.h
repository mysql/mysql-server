/*
 * This file provides testing tools for the streaming decoder. The intended
 * usage is as follows: 1) SE API wrapper is initialized 2) Client builds
 * (ordered) series of expectations 3) The decoder is executed 4) SE checks all
 * assertions 5) Go to 2) if desired
 */

#ifndef STREAM_EXPECTATIONS_H_
#define STREAM_EXPECTATIONS_H_

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include <stdint.h>
#include "cbor.h"

#define MAX_QUEUE_ITEMS 30

enum test_expectation {
  UINT8_EQ,
  UINT16_EQ,
  UINT32_EQ,
  UINT64_EQ,

  NEGINT8_EQ,
  NEGINT16_EQ,
  NEGINT32_EQ,
  NEGINT64_EQ,

  BSTRING_MEM_EQ, /* Matches length and memory address for definite byte strings
                   */
  BSTRING_INDEF_START,

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

/* Tested function */
typedef struct cbor_decoder_result decoder_t(cbor_data, size_t,
                                             const struct cbor_callbacks *,
                                             void *);

void set_decoder(decoder_t *);
struct cbor_decoder_result decode(cbor_data, size_t);

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
void assert_bstring_indef_start();

void assert_array_start(size_t);
void assert_indef_array_start();

void assert_map_start(size_t);
void assert_indef_map_start();

void assert_tag_eq(uint64_t);

void assert_half(float);
void assert_float(float);
void assert_double(double);

void assert_bool(bool);
void assert_nil(); /* assert_null already exists */
void assert_undef();

void assert_indef_break();

/* Assertions verifying callbacks */
void uint8_callback(void *, uint8_t);
void uint16_callback(void *, uint16_t);
void uint32_callback(void *, uint32_t);
void uint64_callback(void *, uint64_t);

void negint8_callback(void *, uint8_t);
void negint16_callback(void *, uint16_t);
void negint32_callback(void *, uint32_t);
void negint64_callback(void *, uint64_t);

void byte_string_callback(void *, cbor_data, size_t);
void byte_string_start_callback(void *);

void array_start_callback(void *, size_t);
void indef_array_start_callback(void *);

void map_start_callback(void *, size_t);
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
