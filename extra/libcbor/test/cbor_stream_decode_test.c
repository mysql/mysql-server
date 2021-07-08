/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "assertions.h"
#include "cbor.h"
#include "stream_expectations.h"

unsigned char embedded_uint8_data[] = {0x00, 0x01, 0x05, 0x17};
static void test_uint8_embedded_decoding(void **state) {
  assert_uint8_eq(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(embedded_uint8_data, 1));

  assert_uint8_eq(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(embedded_uint8_data + 1, 1));

  assert_uint8_eq(5);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(embedded_uint8_data + 2, 1));

  assert_uint8_eq(23);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(embedded_uint8_data + 3, 1));
}

unsigned char uint8_data[] = {0x18, 0x83, 0x18, 0xFF};
static void test_uint8_decoding(void **state) {
  assert_uint8_eq(0x83);
  assert_decoder_result(2, CBOR_DECODER_FINISHED, decode(uint8_data, 2));

  assert_uint8_eq(0xFF);
  assert_decoder_result(2, CBOR_DECODER_FINISHED, decode(uint8_data + 2, 2));
}

unsigned char uint16_data[] = {0x19, 0x01, 0xf4};
static void test_uint16_decoding(void **state) {
  assert_uint16_eq(500);
  assert_decoder_result(3, CBOR_DECODER_FINISHED, decode(uint16_data, 3));
}

unsigned char uint32_data[] = {0x1a, 0xa5, 0xf7, 0x02, 0xb3};
static void test_uint32_decoding(void **state) {
  assert_uint32_eq((uint32_t)2784428723UL);
  assert_decoder_result(5, CBOR_DECODER_FINISHED, decode(uint32_data, 5));
}

unsigned char uint64_data[] = {0x1b, 0xa5, 0xf7, 0x02, 0xb3,
                               0xa5, 0xf7, 0x02, 0xb3};
static void test_uint64_decoding(void **state) {
  assert_uint64_eq(11959030306112471731ULL);
  assert_decoder_result(9, CBOR_DECODER_FINISHED, decode(uint64_data, 9));
}

unsigned char embedded_negint8_data[] = {0x20, 0x21, 0x25, 0x37};
static void test_negint8_embedded_decoding(void **state) {
  assert_negint8_eq(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(embedded_negint8_data, 1));

  assert_negint8_eq(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(embedded_negint8_data + 1, 1));

  assert_negint8_eq(5);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(embedded_negint8_data + 2, 1));

  assert_negint8_eq(23);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(embedded_negint8_data + 3, 1));
}

unsigned char negint8_data[] = {0x38, 0x83, 0x38, 0xFF};
static void test_negint8_decoding(void **state) {
  assert_negint8_eq(0x83);
  assert_decoder_result(2, CBOR_DECODER_FINISHED, decode(negint8_data, 2));

  assert_negint8_eq(0xFF);
  assert_decoder_result(2, CBOR_DECODER_FINISHED, decode(negint8_data + 2, 2));
}

unsigned char negint16_data[] = {0x39, 0x01, 0xf4};
static void test_negint16_decoding(void **state) {
  assert_negint16_eq(500);
  assert_decoder_result(3, CBOR_DECODER_FINISHED, decode(negint16_data, 3));
}

unsigned char negint32_data[] = {0x3a, 0xa5, 0xf7, 0x02, 0xb3};
static void test_negint32_decoding(void **state) {
  assert_negint32_eq((uint32_t)2784428723UL);
  assert_decoder_result(5, CBOR_DECODER_FINISHED, decode(negint32_data, 5));
}

unsigned char negint64_data[] = {0x3b, 0xa5, 0xf7, 0x02, 0xb3,
                                 0xa5, 0xf7, 0x02, 0xb3};
static void test_negint64_decoding(void **state) {
  assert_negint64_eq(11959030306112471731ULL);
  assert_decoder_result(9, CBOR_DECODER_FINISHED, decode(negint64_data, 9));
}

unsigned char bstring_embedded_int8_data[] = {0x41, 0xFF};
static void test_bstring_embedded_int8_decoding(void **state) {
  assert_bstring_mem_eq(bstring_embedded_int8_data + 1, 1);
  assert_decoder_result(2, CBOR_DECODER_FINISHED,
                        decode(bstring_embedded_int8_data, 2));
}

unsigned char bstring_int8_data[] = {0x58, 0x00};
static void test_bstring_int8_decoding(void **state) {
  assert_bstring_mem_eq(bstring_int8_data + 2, 0);
  assert_decoder_result(2, CBOR_DECODER_FINISHED, decode(bstring_int8_data, 2));
}

unsigned char bstring_int16_data[] = {0x59, 0x01, 0x5C /*, [348 bytes] */};
static void test_bstring_int16_decoding(void **state) {
  assert_bstring_mem_eq(bstring_int16_data + 3, 348);
  assert_decoder_result(3 + 348, CBOR_DECODER_FINISHED,
                        decode(bstring_int16_data, 3 + 348));
}

unsigned char bstring_int32_data[] = {0x5A, 0x00, 0x10, 0x10,
                                      0x10 /*, [1052688 bytes] */};
static void test_bstring_int32_decoding(void **state) {
  assert_bstring_mem_eq(bstring_int32_data + 5, 1052688);
  assert_decoder_result(5 + 1052688, CBOR_DECODER_FINISHED,
                        decode(bstring_int32_data, 5 + 1052688));
}

#ifdef EIGHT_BYTE_SIZE_T
unsigned char bstring_int64_data[] = {
    0x5B, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00 /*, [4294967296 bytes] */};
static void test_bstring_int64_decoding(void **state) {
  assert_bstring_mem_eq(bstring_int64_data + 9, 4294967296);
  assert_decoder_result(9 + 4294967296, CBOR_DECODER_FINISHED,
                        decode(bstring_int64_data, 9 + 4294967296));
}
#endif

unsigned char bstring_indef_1_data[] = {0x5F, 0x40 /* Empty byte string */,
                                        0xFF};
static void test_bstring_indef_decoding_1(void **state) {
  assert_bstring_indef_start();
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(bstring_indef_1_data, 3));

  assert_bstring_mem_eq(bstring_indef_1_data + 2, 0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(bstring_indef_1_data + 1, 2));

  assert_indef_break();
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(bstring_indef_1_data + 2, 1));
}

unsigned char bstring_indef_2_data[] = {0x5F, 0xFF};
static void test_bstring_indef_decoding_2(void **state) {
  assert_bstring_indef_start();
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(bstring_indef_2_data, 2));

  assert_indef_break();
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(bstring_indef_2_data + 1, 1));
}

unsigned char bstring_indef_3_data[] = {
    0x5F, 0x40 /* Empty byte string */,      0x58,
    0x01, 0xFF /* 1B 1 char bytes string */, 0xFF};
static void test_bstring_indef_decoding_3(void **state) {
  assert_bstring_indef_start();
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(bstring_indef_3_data, 6));

  assert_bstring_mem_eq(bstring_indef_3_data + 2, 0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(bstring_indef_3_data + 1, 5));

  assert_bstring_mem_eq(bstring_indef_3_data + 4, 1);
  assert_decoder_result(3, CBOR_DECODER_FINISHED,
                        decode(bstring_indef_3_data + 2, 4));

  assert_indef_break();
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(bstring_indef_3_data + 5, 1));
}

unsigned char array_embedded_int8_data[] = {0x80};
static void test_array_embedded_int8_decoding(void **state) {
  assert_array_start(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(array_embedded_int8_data, 1));
}

unsigned char array_int8_data[] = {0x98, 0x02, 0x00, 0x01};
static void test_array_int8_decoding(void **state) {
  assert_array_start(2);
  assert_decoder_result(2, CBOR_DECODER_FINISHED, decode(array_int8_data, 4));

  assert_uint8_eq(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(array_int8_data + 2, 2));

  assert_uint8_eq(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(array_int8_data + 3, 1));
}

unsigned char array_int16_data[] = {0x99, 0x00, 0x02, 0x00, 0x01};
static void test_array_int16_decoding(void **state) {
  assert_array_start(2);
  assert_decoder_result(3, CBOR_DECODER_FINISHED, decode(array_int16_data, 5));

  assert_uint8_eq(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(array_int16_data + 3, 2));

  assert_uint8_eq(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(array_int16_data + 4, 1));
}

unsigned char array_int32_data[] = {0x9A, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01};
static void test_array_int32_decoding(void **state) {
  assert_array_start(2);
  assert_decoder_result(5, CBOR_DECODER_FINISHED, decode(array_int32_data, 7));

  assert_uint8_eq(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(array_int32_data + 5, 2));

  assert_uint8_eq(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(array_int32_data + 6, 1));
}

unsigned char array_int64_data[] = {0x9B, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x02, 0x00, 0x01};
static void test_array_int64_decoding(void **state) {
  assert_array_start(2);
  assert_decoder_result(9, CBOR_DECODER_FINISHED, decode(array_int64_data, 11));

  assert_uint8_eq(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(array_int64_data + 9, 2));

  assert_uint8_eq(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(array_int64_data + 10, 1));
}

unsigned char array_of_arrays_data[] = {0x82, 0x80, 0x80};
static void test_array_of_arrays_decoding(void **state) {
  assert_array_start(2);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(array_of_arrays_data, 3));

  assert_array_start(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(array_of_arrays_data + 1, 2));

  assert_array_start(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(array_of_arrays_data + 2, 1));
}

unsigned char indef_array_data_1[] = {0x9F, 0x00, 0x18, 0xFF, 0x9F, 0xFF, 0xFF};
static void test_indef_array_decoding_1(void **state) {
  assert_indef_array_start();
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(indef_array_data_1, 7));

  assert_uint8_eq(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(indef_array_data_1 + 1, 6));

  assert_uint8_eq(255);
  assert_decoder_result(2, CBOR_DECODER_FINISHED,
                        decode(indef_array_data_1 + 2, 4));

  assert_indef_array_start();
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(indef_array_data_1 + 4, 3));

  assert_indef_break();
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(indef_array_data_1 + 5, 2));

  assert_indef_break();
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(indef_array_data_1 + 6, 1));
}

unsigned char map_embedded_int8_data[] = {0xa1, 0x01, 0x00};
static void test_map_embedded_int8_decoding(void **state) {
  assert_map_start(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(map_embedded_int8_data, 3));

  assert_uint8_eq(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(map_embedded_int8_data + 1, 2));

  assert_uint8_eq(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(map_embedded_int8_data + 2, 1));
}

unsigned char map_int8_data[] = {0xB8, 0x01, 0x00, 0x01};
static void test_map_int8_decoding(void **state) {
  assert_map_start(1);
  assert_decoder_result(2, CBOR_DECODER_FINISHED, decode(map_int8_data, 4));

  assert_uint8_eq(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED, decode(map_int8_data + 2, 2));

  assert_uint8_eq(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED, decode(map_int8_data + 3, 1));
}

unsigned char map_int16_data[] = {0xB9, 0x00, 0x01, 0x00, 0x01};
static void test_map_int16_decoding(void **state) {
  assert_map_start(1);
  assert_decoder_result(3, CBOR_DECODER_FINISHED, decode(map_int16_data, 5));

  assert_uint8_eq(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(map_int16_data + 3, 2));

  assert_uint8_eq(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(map_int16_data + 4, 1));
}

unsigned char map_int32_data[] = {0xBA, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01};
static void test_map_int32_decoding(void **state) {
  assert_map_start(1);
  assert_decoder_result(5, CBOR_DECODER_FINISHED, decode(map_int32_data, 7));

  assert_uint8_eq(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(map_int32_data + 5, 2));

  assert_uint8_eq(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(map_int32_data + 6, 1));
}

unsigned char map_int64_data[] = {0xBB, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x01, 0x00, 0x01};
static void test_map_int64_decoding(void **state) {
  assert_map_start(1);
  assert_decoder_result(9, CBOR_DECODER_FINISHED, decode(map_int64_data, 11));

  assert_uint8_eq(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(map_int64_data + 9, 2));

  assert_uint8_eq(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(map_int64_data + 10, 1));
}

unsigned char indef_map_data_1[] = {0xBF, 0x00, 0x18, 0xFF, 0xFF};
static void test_indef_map_decoding_1(void **state) {
  assert_indef_map_start();
  assert_decoder_result(1, CBOR_DECODER_FINISHED, decode(indef_map_data_1, 5));

  assert_uint8_eq(0);
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(indef_map_data_1 + 1, 4));

  assert_uint8_eq(255);
  assert_decoder_result(2, CBOR_DECODER_FINISHED,
                        decode(indef_map_data_1 + 2, 3));

  assert_indef_break();
  assert_decoder_result(1, CBOR_DECODER_FINISHED,
                        decode(indef_map_data_1 + 4, 1));
}

unsigned char map_nedata[] = {0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x01, 0x19, 0x01, 0xf4, 0x01};
static void test_nedata_map_decoding(void **state) {
  assert_decoder_result_nedata(8, decode(map_nedata, 1));

  assert_map_start(1);
  assert_decoder_result(9, CBOR_DECODER_FINISHED, decode(map_nedata, 9));

  assert_decoder_result_nedata(2, decode(map_nedata + 9, 1));

  assert_uint16_eq(500);
  assert_decoder_result(3, CBOR_DECODER_FINISHED, decode(map_nedata + 9, 3));

  assert_uint8_eq(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED, decode(map_nedata + 12, 1));
}

unsigned char embedded_tag_data[] = {0xC1};
static void test_embedded_tag_decoding(void **state) {
  assert_tag_eq(1);
  assert_decoder_result(1, CBOR_DECODER_FINISHED, decode(embedded_tag_data, 1));
}

unsigned char int8_tag_data[] = {0xD8, 0xFE};
static void test_int8_tag_decoding(void **state) {
  assert_tag_eq(254);
  assert_decoder_result(2, CBOR_DECODER_FINISHED, decode(int8_tag_data, 2));
}

unsigned char int16_tag_data[] = {0xD9, 0xFE, 0xFD};
static void test_int16_tag_decoding(void **state) {
  assert_tag_eq(65277);
  assert_decoder_result(3, CBOR_DECODER_FINISHED, decode(int16_tag_data, 3));
}

unsigned char int32_tag_data[] = {0xDA, 0xFE, 0xFD, 0xFC, 0xFB};
static void test_int32_tag_decoding(void **state) {
  assert_tag_eq(4278058235ULL);
  assert_decoder_result(5, CBOR_DECODER_FINISHED, decode(int32_tag_data, 5));
}

unsigned char int64_tag_data[] = {0xDB, 0xFE, 0xFD, 0xFC, 0xFB,
                                  0xFA, 0xF9, 0xF8, 0xF7};
static void test_int64_tag_decoding(void **state) {
  assert_tag_eq(18374120213919168759ULL);
  assert_decoder_result(9, CBOR_DECODER_FINISHED, decode(int64_tag_data, 9));
}

unsigned char bad_tag_data[] = {0xC6};
static void test_bad_tag_decoding(void **state) {
  assert_decoder_result(0, CBOR_DECODER_ERROR, decode(bad_tag_data, 1));
}

unsigned char float2_data[] = {0xF9, 0x7B, 0xFF};
static void test_float2_decoding(void **state) {
  assert_half(65504.0);
  assert_decoder_result(3, CBOR_DECODER_FINISHED, decode(float2_data, 3));
}

unsigned char float4_data[] = {0xFA, 0x47, 0xC3, 0x50, 0x00};
static void test_float4_decoding(void **state) {
  assert_float(100000.0);
  assert_decoder_result(5, CBOR_DECODER_FINISHED, decode(float4_data, 5));
}

unsigned char float8_data[] = {0xFB, 0xC0, 0x10, 0x66, 0x66,
                               0x66, 0x66, 0x66, 0x66};
static void test_float8_decoding(void **state) {
  assert_double(-4.1);
  assert_decoder_result(9, CBOR_DECODER_FINISHED, decode(float8_data, 9));
}

unsigned char false_data[] = {0xF4};
static void test_false_decoding(void **state) {
  assert_bool(false);
  assert_decoder_result(1, CBOR_DECODER_FINISHED, decode(false_data, 1));
}

unsigned char true_data[] = {0xF5};
static void test_true_decoding(void **state) {
  assert_bool(true);
  assert_decoder_result(1, CBOR_DECODER_FINISHED, decode(true_data, 1));
}

unsigned char null_data[] = {0xF6};
static void test_null_decoding(void **state) {
  assert_nil();
  assert_decoder_result(1, CBOR_DECODER_FINISHED, decode(null_data, 1));
}

unsigned char undef_data[] = {0xF7};
static void test_undef_decoding(void **state) {
  assert_undef();
  assert_decoder_result(1, CBOR_DECODER_FINISHED, decode(undef_data, 1));
}

int main(void) {
  set_decoder(&cbor_stream_decode);
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_uint8_embedded_decoding),
      cmocka_unit_test(test_uint8_decoding),
      cmocka_unit_test(test_uint16_decoding),
      cmocka_unit_test(test_uint32_decoding),
      cmocka_unit_test(test_uint64_decoding),

      cmocka_unit_test(test_negint8_embedded_decoding),
      cmocka_unit_test(test_negint8_decoding),
      cmocka_unit_test(test_negint16_decoding),
      cmocka_unit_test(test_negint32_decoding),
      cmocka_unit_test(test_negint64_decoding),

      cmocka_unit_test(test_bstring_embedded_int8_decoding),
      cmocka_unit_test(test_bstring_int8_decoding),
      cmocka_unit_test(test_bstring_int16_decoding),
      cmocka_unit_test(test_bstring_int32_decoding),
#ifdef EIGHT_BYTE_SIZE_T
      cmocka_unit_test(test_bstring_int64_decoding),
#endif
      cmocka_unit_test(test_bstring_indef_decoding_1),
      cmocka_unit_test(test_bstring_indef_decoding_2),
      cmocka_unit_test(test_bstring_indef_decoding_3),

      cmocka_unit_test(test_array_embedded_int8_decoding),
      cmocka_unit_test(test_array_int8_decoding),
      cmocka_unit_test(test_array_int16_decoding),
      cmocka_unit_test(test_array_int32_decoding),
      cmocka_unit_test(test_array_int64_decoding),
      cmocka_unit_test(test_array_of_arrays_decoding),
      cmocka_unit_test(test_indef_array_decoding_1),

      cmocka_unit_test(test_map_embedded_int8_decoding),
      cmocka_unit_test(test_map_int8_decoding),
      cmocka_unit_test(test_map_int16_decoding),
      cmocka_unit_test(test_map_int32_decoding),
      cmocka_unit_test(test_map_int64_decoding),
      cmocka_unit_test(test_indef_map_decoding_1),
      cmocka_unit_test(test_nedata_map_decoding),

      cmocka_unit_test(test_embedded_tag_decoding),
      cmocka_unit_test(test_int8_tag_decoding),
      cmocka_unit_test(test_int16_tag_decoding),
      cmocka_unit_test(test_int32_tag_decoding),
      cmocka_unit_test(test_int64_tag_decoding),
      cmocka_unit_test(test_bad_tag_decoding),

      cmocka_unit_test(test_float2_decoding),
      cmocka_unit_test(test_float4_decoding),
      cmocka_unit_test(test_float8_decoding),

      cmocka_unit_test(test_false_decoding),
      cmocka_unit_test(test_true_decoding),
      cmocka_unit_test(test_null_decoding),
      cmocka_unit_test(test_undef_decoding)};
  return cmocka_run_group_tests(tests, NULL, NULL);
}
