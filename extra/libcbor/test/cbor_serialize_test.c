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

#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include "cbor.h"

unsigned char buffer[512];

static void test_serialize_uint8(void **state) {
  cbor_item_t *item = cbor_new_int8();
  cbor_set_uint8(item, 0);
  assert_int_equal(1, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, (unsigned char[]){0x00}, 1);
  cbor_decref(&item);
}

static void test_serialize_uint16(void **state) {
  cbor_item_t *item = cbor_new_int16();
  cbor_set_uint16(item, 1000);
  assert_int_equal(3, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x19, 0x03, 0xE8}), 3);
  cbor_decref(&item);
}

static void test_serialize_uint32(void **state) {
  cbor_item_t *item = cbor_new_int32();
  cbor_set_uint32(item, 1000000);
  assert_int_equal(5, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x1A, 0x00, 0x0F, 0x42, 0x40}),
                      5);
  cbor_decref(&item);
}

static void test_serialize_uint64(void **state) {
  cbor_item_t *item = cbor_new_int64();
  cbor_set_uint64(item, 1000000000000);
  assert_int_equal(9, cbor_serialize(item, buffer, 512));
  assert_memory_equal(
      buffer,
      ((unsigned char[]){0x1B, 0x00, 0x00, 0x00, 0xE8, 0xD4, 0xA5, 0x10, 0x00}),
      9);
  cbor_decref(&item);
}

static void test_serialize_negint8(void **state) {
  cbor_item_t *item = cbor_new_int8();
  cbor_set_uint8(item, 0);
  cbor_mark_negint(item);
  assert_int_equal(1, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, (unsigned char[]){0x20}, 1);
  cbor_decref(&item);
}

static void test_serialize_negint16(void **state) {
  cbor_item_t *item = cbor_new_int16();
  cbor_set_uint16(item, 1000);
  cbor_mark_negint(item);
  assert_int_equal(3, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x39, 0x03, 0xE8}), 3);
  cbor_decref(&item);
}

static void test_serialize_negint32(void **state) {
  cbor_item_t *item = cbor_new_int32();
  cbor_set_uint32(item, 1000000);
  cbor_mark_negint(item);
  assert_int_equal(5, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x3A, 0x00, 0x0F, 0x42, 0x40}),
                      5);
  cbor_decref(&item);
}

static void test_serialize_negint64(void **state) {
  cbor_item_t *item = cbor_new_int64();
  cbor_set_uint64(item, 1000000000000);
  cbor_mark_negint(item);
  assert_int_equal(9, cbor_serialize(item, buffer, 512));
  assert_memory_equal(
      buffer,
      ((unsigned char[]){0x3B, 0x00, 0x00, 0x00, 0xE8, 0xD4, 0xA5, 0x10, 0x00}),
      9);
  cbor_decref(&item);
}

static void test_serialize_definite_bytestring(void **state) {
  cbor_item_t *item = cbor_new_definite_bytestring();
  unsigned char *data = malloc(256);
  bzero(data, 256); /* Prevent undefined behavior in comparison */
  cbor_bytestring_set_handle(item, data, 256);
  assert_int_equal(256 + 3, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x59, 0x01, 0x00}), 3);
  assert_memory_equal(buffer + 3, data, 256);
  cbor_decref(&item);
}

static void test_serialize_indefinite_bytestring(void **state) {
  cbor_item_t *item = cbor_new_indefinite_bytestring();

  cbor_item_t *chunk = cbor_new_definite_bytestring();
  unsigned char *data = malloc(256);
  bzero(data, 256); /* Prevent undefined behavior in comparison */
  cbor_bytestring_set_handle(chunk, data, 256);

  cbor_bytestring_add_chunk(item, cbor_move(chunk));
  assert_int_equal(cbor_bytestring_chunk_count(item), 1);

  assert_int_equal(1 + 3 + 256 + 1, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x5F, 0x59, 0x01, 0x00}), 4);
  assert_memory_equal(buffer + 4, data, 256);
  assert_memory_equal(buffer + 4 + 256, ((unsigned char[]){0xFF}), 1);
  cbor_decref(&item);
}

static void test_serialize_definite_string(void **state) {
  cbor_item_t *item = cbor_new_definite_string();
  unsigned char *data = malloc(12);
  strncpy((char *)data, "Hello world!", 12);
  cbor_string_set_handle(item, data, 12);
  assert_int_equal(1 + 12, cbor_serialize(item, buffer, 512));
  assert_memory_equal(
      buffer,
      ((unsigned char[]){0x6C, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77, 0x6F,
                         0x72, 0x6C, 0x64, 0x21}),
      13);
  cbor_decref(&item);
}

static void test_serialize_indefinite_string(void **state) {
  cbor_item_t *item = cbor_new_indefinite_string();
  cbor_item_t *chunk = cbor_new_definite_string();

  unsigned char *data = malloc(12);
  strncpy((char *)data, "Hello world!", 12);
  cbor_string_set_handle(chunk, data, 12);

  assert_true(cbor_string_add_chunk(item, cbor_move(chunk)));
  assert_int_equal(cbor_string_chunk_count(item), 1);

  assert_int_equal(15, cbor_serialize(item, buffer, 512));
  assert_memory_equal(
      buffer,
      ((unsigned char[]){0x7F, 0x6C, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77,
                         0x6F, 0x72, 0x6C, 0x64, 0x21, 0xFF}),
      15);
  cbor_decref(&item);
}

static void test_serialize_definite_array(void **state) {
  cbor_item_t *item = cbor_new_definite_array(2);
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_item_t *two = cbor_build_uint8(2);

  cbor_array_push(item, one);
  cbor_array_set(item, 1, two);
  cbor_array_replace(item, 0, one);

  assert_int_equal(3, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x82, 0x01, 0x02}), 3);
  cbor_decref(&item);
  cbor_decref(&one);
  cbor_decref(&two);
}

static void test_serialize_indefinite_array(void **state) {
  cbor_item_t *item = cbor_new_indefinite_array();
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_item_t *two = cbor_build_uint8(2);

  cbor_array_push(item, one);
  cbor_array_push(item, two);

  assert_int_equal(4, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x9F, 0x01, 0x02, 0xFF}), 4);
  cbor_decref(&item);
  cbor_decref(&one);
  cbor_decref(&two);
}

static void test_serialize_definite_map(void **state) {
  cbor_item_t *item = cbor_new_definite_map(2);
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_item_t *two = cbor_build_uint8(2);

  cbor_map_add(item, (struct cbor_pair){.key = one, .value = two});
  cbor_map_add(item, (struct cbor_pair){.key = two, .value = one});

  assert_int_equal(5, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xA2, 0x01, 0x02, 0x02, 0x01}),
                      5);
  cbor_decref(&item);
  cbor_decref(&one);
  cbor_decref(&two);
}

static void test_serialize_indefinite_map(void **state) {
  cbor_item_t *item = cbor_new_indefinite_map(2);
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_item_t *two = cbor_build_uint8(2);

  assert_true(cbor_map_add(item, (struct cbor_pair){.key = one, .value = two}));
  assert_true(cbor_map_add(item, (struct cbor_pair){.key = two, .value = one}));

  assert_int_equal(6, cbor_serialize(item, buffer, 512));
  assert_memory_equal(
      buffer, ((unsigned char[]){0xBF, 0x01, 0x02, 0x02, 0x01, 0xFF}), 6);
  cbor_decref(&item);
  cbor_decref(&one);
  cbor_decref(&two);
}

static void test_serialize_tags(void **state) {
  cbor_item_t *item = cbor_new_tag(21);
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_tag_set_item(item, one);

  assert_int_equal(2, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xD5, 0x01}), 2);
  cbor_decref(&item);
  cbor_decref(&one);
}

static void test_serialize_half(void **state) {
  cbor_item_t *item = cbor_new_float2();
  cbor_set_float2(item, NAN);

  assert_int_equal(3, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xF9, 0x7E, 0x00}), 3);
  cbor_decref(&item);
}

static void test_serialize_single(void **state) {
  cbor_item_t *item = cbor_new_float4();
  cbor_set_float4(item, 100000.0f);

  assert_int_equal(5, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xFA, 0x47, 0xC3, 0x50, 0x00}),
                      5);
  cbor_decref(&item);
}

static void test_serialize_double(void **state) {
  cbor_item_t *item = cbor_new_float8();
  cbor_set_float8(item, -4.1);

  assert_int_equal(9, cbor_serialize(item, buffer, 512));
  assert_memory_equal(
      buffer,
      ((unsigned char[]){0xFB, 0xC0, 0x10, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66}),
      9);
  cbor_decref(&item);
}

static void test_serialize_ctrl(void **state) {
  cbor_item_t *item = cbor_new_undef();

  assert_int_equal(1, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xF7}), 1);
  cbor_decref(&item);
}

static void test_serialize_long_ctrl(void **state) {
  cbor_item_t *item = cbor_new_ctrl();
  cbor_set_ctrl(item, 254);

  assert_int_equal(2, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xF8, 0xFE}), 2);
  cbor_decref(&item);
}

static void test_auto_serialize(void **state) {
  cbor_item_t *item = cbor_new_definite_array(4);
  for (size_t i = 0; i < 4; i++)
    cbor_array_push(item, cbor_move(cbor_build_uint64(0)));

  unsigned char *buffer;
  size_t buffer_size;
  assert_int_equal(37, cbor_serialize_alloc(item, &buffer, &buffer_size));
  assert_int_equal(64, buffer_size);
  assert_memory_equal(buffer, ((unsigned char[]){0x84, 0x1B}), 2);
  cbor_decref(&item);
  _CBOR_FREE(buffer);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_serialize_uint8),
      cmocka_unit_test(test_serialize_uint16),
      cmocka_unit_test(test_serialize_uint32),
      cmocka_unit_test(test_serialize_uint64),
      cmocka_unit_test(test_serialize_negint8),
      cmocka_unit_test(test_serialize_negint16),
      cmocka_unit_test(test_serialize_negint32),
      cmocka_unit_test(test_serialize_negint64),
      cmocka_unit_test(test_serialize_definite_bytestring),
      cmocka_unit_test(test_serialize_indefinite_bytestring),
      cmocka_unit_test(test_serialize_definite_string),
      cmocka_unit_test(test_serialize_indefinite_string),
      cmocka_unit_test(test_serialize_definite_array),
      cmocka_unit_test(test_serialize_indefinite_array),
      cmocka_unit_test(test_serialize_definite_map),
      cmocka_unit_test(test_serialize_indefinite_map),
      cmocka_unit_test(test_serialize_tags),
      cmocka_unit_test(test_serialize_half),
      cmocka_unit_test(test_serialize_single),
      cmocka_unit_test(test_serialize_double),
      cmocka_unit_test(test_serialize_ctrl),
      cmocka_unit_test(test_serialize_long_ctrl),
      cmocka_unit_test(test_auto_serialize)};
  return cmocka_run_group_tests(tests, NULL, NULL);
}
