/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

// cbor_serialize_alloc
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include "assertions.h"
#include "cbor.h"
#include "test_allocator.h"

unsigned char buffer[512];

static void test_serialize_uint8_embed(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_int8();
  cbor_set_uint8(item, 0);
  assert_size_equal(1, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, (unsigned char[]){0x00}, 1);
  assert_size_equal(cbor_serialized_size(item), 1);
  cbor_decref(&item);
}

static void test_serialize_uint8(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_int8();
  cbor_set_uint8(item, 42);
  assert_size_equal(2, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x18, 0x2a}), 2);
  assert_size_equal(cbor_serialized_size(item), 2);
  cbor_decref(&item);
}

static void test_serialize_uint16(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_int16();
  cbor_set_uint16(item, 1000);
  assert_size_equal(3, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x19, 0x03, 0xE8}), 3);
  assert_size_equal(cbor_serialized_size(item), 3);
  cbor_decref(&item);
}

static void test_serialize_uint32(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_int32();
  cbor_set_uint32(item, 1000000);
  assert_size_equal(5, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x1A, 0x00, 0x0F, 0x42, 0x40}),
                      5);
  assert_size_equal(cbor_serialized_size(item), 5);
  cbor_decref(&item);
}

static void test_serialize_uint64(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_int64();
  cbor_set_uint64(item, 1000000000000);
  assert_size_equal(9, cbor_serialize(item, buffer, 512));
  assert_memory_equal(
      buffer,
      ((unsigned char[]){0x1B, 0x00, 0x00, 0x00, 0xE8, 0xD4, 0xA5, 0x10, 0x00}),
      9);
  assert_size_equal(cbor_serialized_size(item), 9);
  cbor_decref(&item);
}

static void test_serialize_negint8_embed(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_int8();
  cbor_set_uint8(item, 0);
  cbor_mark_negint(item);
  assert_size_equal(1, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, (unsigned char[]){0x20}, 1);
  assert_size_equal(cbor_serialized_size(item), 1);
  cbor_decref(&item);
}

static void test_serialize_negint8(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_int8();
  cbor_set_uint8(item, 42);
  cbor_mark_negint(item);
  assert_size_equal(2, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x38, 0x2a}), 2);
  assert_size_equal(cbor_serialized_size(item), 2);
  cbor_decref(&item);
}

static void test_serialize_negint16(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_int16();
  cbor_set_uint16(item, 1000);
  cbor_mark_negint(item);
  assert_size_equal(3, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x39, 0x03, 0xE8}), 3);
  assert_size_equal(cbor_serialized_size(item), 3);
  cbor_decref(&item);
}

static void test_serialize_negint32(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_int32();
  cbor_set_uint32(item, 1000000);
  cbor_mark_negint(item);
  assert_size_equal(5, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x3A, 0x00, 0x0F, 0x42, 0x40}),
                      5);
  assert_size_equal(cbor_serialized_size(item), 5);
  cbor_decref(&item);
}

static void test_serialize_negint64(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_int64();
  cbor_set_uint64(item, 1000000000000);
  cbor_mark_negint(item);
  assert_size_equal(9, cbor_serialize(item, buffer, 512));
  assert_memory_equal(
      buffer,
      ((unsigned char[]){0x3B, 0x00, 0x00, 0x00, 0xE8, 0xD4, 0xA5, 0x10, 0x00}),
      9);
  assert_size_equal(cbor_serialized_size(item), 9);
  cbor_decref(&item);
}

static void test_serialize_definite_bytestring(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_definite_bytestring();
  unsigned char *data = malloc(256);
  cbor_bytestring_set_handle(item, data, 256);
  memset(data, 0, 256); /* Prevent undefined behavior in comparison */
  assert_size_equal(256 + 3, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x59, 0x01, 0x00}), 3);
  assert_memory_equal(buffer + 3, data, 256);
  assert_size_equal(cbor_serialized_size(item), 259);
  cbor_decref(&item);
}

static void test_serialize_indefinite_bytestring(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_bytestring();

  cbor_item_t *chunk = cbor_new_definite_bytestring();
  unsigned char *data = malloc(256);
  memset(data, 0, 256); /* Prevent undefined behavior in comparison */
  cbor_bytestring_set_handle(chunk, data, 256);

  assert_true(cbor_bytestring_add_chunk(item, cbor_move(chunk)));
  assert_size_equal(cbor_bytestring_chunk_count(item), 1);

  assert_size_equal(1 + 3 + 256 + 1, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x5F, 0x59, 0x01, 0x00}), 4);
  assert_memory_equal(buffer + 4, data, 256);
  assert_memory_equal(buffer + 4 + 256, ((unsigned char[]){0xFF}), 1);
  assert_size_equal(cbor_serialized_size(item), 261);
  cbor_decref(&item);
}

static void test_serialize_bytestring_size_overflow(
    void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_definite_bytestring();

  // Fake having a huge chunk of data
  unsigned char *data = malloc(1);
  cbor_bytestring_set_handle(item, data, SIZE_MAX);

  // Would require 1 + 8 + SIZE_MAX bytes, which overflows size_t
  assert_size_equal(cbor_serialize(item, buffer, 512), 0);
  assert_size_equal(cbor_serialized_size(item), 0);
  cbor_decref(&item);
}

static void test_serialize_bytestring_no_space(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_definite_bytestring();
  unsigned char *data = malloc(12);
  cbor_bytestring_set_handle(item, data, 12);

  assert_size_equal(cbor_serialize(item, buffer, 1), 0);

  cbor_decref(&item);
}

static void test_serialize_indefinite_bytestring_no_space(
    void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_bytestring();
  cbor_item_t *chunk = cbor_new_definite_bytestring();
  unsigned char *data = malloc(256);
  cbor_bytestring_set_handle(chunk, data, 256);
  assert_true(cbor_bytestring_add_chunk(item, cbor_move(chunk)));

  // Not enough space for the leading byte
  assert_size_equal(cbor_serialize(item, buffer, 0), 0);

  // Not enough space for the chunk
  assert_size_equal(cbor_serialize(item, buffer, 30), 0);

  // Not enough space for the indef break
  assert_size_equal(
      cbor_serialize(item, buffer, 1 + cbor_serialized_size(chunk)), 0);

  cbor_decref(&item);
}

static void test_serialize_definite_string(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_definite_string();
  unsigned char *data = malloc(12);
  strncpy((char *)data, "Hello world!", 12);
  cbor_string_set_handle(item, data, 12);
  assert_size_equal(1 + 12, cbor_serialize(item, buffer, 512));
  assert_memory_equal(
      buffer,
      ((unsigned char[]){0x6C, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77, 0x6F,
                         0x72, 0x6C, 0x64, 0x21}),
      13);
  assert_size_equal(cbor_serialized_size(item), 13);
  cbor_decref(&item);
}

static void test_serialize_definite_string_4b_header(
    void **_CBOR_UNUSED(_state)) {
#if SIZE_MAX > UINT16_MAX
  cbor_item_t *item = cbor_new_definite_string();
  const size_t size = (size_t)UINT16_MAX + 1;
  unsigned char *data = malloc(size);
  memset(data, 0, size);
  cbor_string_set_handle(item, data, size);
  assert_size_equal(cbor_serialized_size(item), 1 + 4 + size);
  cbor_decref(&item);
#endif
}

static void test_serialize_definite_string_8b_header(
    void **_CBOR_UNUSED(_state)) {
#if SIZE_MAX > UINT32_MAX
  cbor_item_t *item = cbor_new_definite_string();
  const size_t size = (size_t)UINT32_MAX + 1;
  unsigned char *data = malloc(1);
  data[0] = '\0';
  cbor_string_set_handle(item, data, 1);
  // Pretend that we have a big item to avoid the huge malloc
  item->metadata.string_metadata.length = size;
  assert_size_equal(cbor_serialized_size(item), 1 + 8 + size);
  cbor_decref(&item);
#endif
}

static void test_serialize_indefinite_string(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_string();
  cbor_item_t *chunk = cbor_new_definite_string();

  unsigned char *data = malloc(12);
  strncpy((char *)data, "Hello world!", 12);
  cbor_string_set_handle(chunk, data, 12);

  assert_true(cbor_string_add_chunk(item, cbor_move(chunk)));
  assert_size_equal(cbor_string_chunk_count(item), 1);

  assert_size_equal(15, cbor_serialize(item, buffer, 512));
  assert_memory_equal(
      buffer,
      ((unsigned char[]){0x7F, 0x6C, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77,
                         0x6F, 0x72, 0x6C, 0x64, 0x21, 0xFF}),
      15);
  assert_size_equal(cbor_serialized_size(item), 15);
  cbor_decref(&item);
}

static void test_serialize_string_no_space(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_definite_string();
  unsigned char *data = malloc(12);
  memset(data, 0, 12);
  cbor_string_set_handle(item, data, 12);

  assert_size_equal(cbor_serialize(item, buffer, 1), 0);

  cbor_decref(&item);
}

static void test_serialize_indefinite_string_no_space(
    void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_string();
  cbor_item_t *chunk = cbor_new_definite_string();
  unsigned char *data = malloc(256);
  memset(data, 0, 256);
  cbor_string_set_handle(chunk, data, 256);
  assert_true(cbor_string_add_chunk(item, cbor_move(chunk)));

  // Not enough space for the leading byte
  assert_size_equal(cbor_serialize(item, buffer, 0), 0);

  // Not enough space for the chunk
  assert_size_equal(cbor_serialize(item, buffer, 30), 0);

  // Not enough space for the indef break
  assert_size_equal(
      cbor_serialize(item, buffer, 1 + cbor_serialized_size(chunk)), 0);

  cbor_decref(&item);
}

static void test_serialize_definite_array(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_definite_array(2);
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_item_t *two = cbor_build_uint8(2);

  assert_true(cbor_array_push(item, one));
  assert_true(cbor_array_set(item, 1, two));
  assert_true(cbor_array_replace(item, 0, one));

  assert_size_equal(3, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x82, 0x01, 0x02}), 3);
  assert_size_equal(cbor_serialized_size(item), 3);
  cbor_decref(&item);
  cbor_decref(&one);
  cbor_decref(&two);
}

static void test_serialize_array_no_space(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_array();
  cbor_item_t *one = cbor_build_uint8(1);
  assert_true(cbor_array_push(item, one));
  assert_size_equal(cbor_serialized_size(item), 3);

  // Not enough space for the leading byte
  assert_size_equal(0, cbor_serialize(item, buffer, 0));

  // Not enough space for the item
  assert_size_equal(0, cbor_serialize(item, buffer, 1));

  // Not enough space for the indef break
  assert_size_equal(0, cbor_serialize(item, buffer, 2));

  cbor_decref(&item);
  cbor_decref(&one);
}

static void test_serialize_indefinite_array(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_array();
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_item_t *two = cbor_build_uint8(2);

  assert_true(cbor_array_push(item, one));
  assert_true(cbor_array_push(item, two));

  assert_size_equal(4, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x9F, 0x01, 0x02, 0xFF}), 4);
  assert_size_equal(cbor_serialized_size(item), 4);
  cbor_decref(&item);
  cbor_decref(&one);
  cbor_decref(&two);
}

static void test_serialize_definite_map(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_definite_map(2);
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_item_t *two = cbor_build_uint8(2);

  assert_true(cbor_map_add(item, (struct cbor_pair){.key = one, .value = two}));
  assert_true(cbor_map_add(item, (struct cbor_pair){.key = two, .value = one}));

  assert_size_equal(5, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xA2, 0x01, 0x02, 0x02, 0x01}),
                      5);
  assert_size_equal(cbor_serialized_size(item), 5);
  cbor_decref(&item);
  cbor_decref(&one);
  cbor_decref(&two);
}

static void test_serialize_indefinite_map(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_map();
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_item_t *two = cbor_build_uint8(2);

  assert_true(cbor_map_add(item, (struct cbor_pair){.key = one, .value = two}));
  assert_true(cbor_map_add(item, (struct cbor_pair){.key = two, .value = one}));

  assert_size_equal(6, cbor_serialize(item, buffer, 512));
  assert_memory_equal(
      buffer, ((unsigned char[]){0xBF, 0x01, 0x02, 0x02, 0x01, 0xFF}), 6);
  assert_size_equal(cbor_serialized_size(item), 6);
  cbor_decref(&item);
  cbor_decref(&one);
  cbor_decref(&two);
}

static void test_serialize_map_no_space(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_map();
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_item_t *two = cbor_build_uint8(2);
  assert_true(cbor_map_add(item, (struct cbor_pair){.key = one, .value = two}));
  assert_size_equal(cbor_serialized_size(item), 4);

  // Not enough space for the leading byte
  assert_size_equal(cbor_serialize(item, buffer, 0), 0);

  // Not enough space for the key
  assert_size_equal(cbor_serialize(item, buffer, 1), 0);

  // Not enough space for the value
  assert_size_equal(cbor_serialize(item, buffer, 2), 0);

  // Not enough space for the indef break
  assert_size_equal(cbor_serialize(item, buffer, 3), 0);

  cbor_decref(&item);
  cbor_decref(&one);
  cbor_decref(&two);
}

static void test_serialize_tags(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_tag(21);
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_tag_set_item(item, one);

  assert_size_equal(2, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xD5, 0x01}), 2);
  assert_size_equal(cbor_serialized_size(item), 2);
  cbor_decref(&item);
  cbor_decref(&one);
}

static void test_serialize_tags_no_space(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_tag(21);
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_tag_set_item(item, one);
  assert_size_equal(cbor_serialized_size(item), 2);

  // Not enough space for the leading byte
  assert_size_equal(cbor_serialize(item, buffer, 0), 0);

  // Not enough space for the item
  assert_size_equal(cbor_serialize(item, buffer, 1), 0);

  cbor_decref(&item);
  cbor_decref(&one);
}

static void test_serialize_half(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_float2();
  cbor_set_float2(item, NAN);

  assert_size_equal(3, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xF9, 0x7E, 0x00}), 3);
  assert_size_equal(cbor_serialized_size(item), 3);
  cbor_decref(&item);
}

static void test_serialize_single(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_float4();
  cbor_set_float4(item, 100000.0f);

  assert_size_equal(5, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xFA, 0x47, 0xC3, 0x50, 0x00}),
                      5);
  assert_size_equal(cbor_serialized_size(item), 5);
  cbor_decref(&item);
}

static void test_serialize_double(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_float8();
  cbor_set_float8(item, -4.1);

  assert_size_equal(9, cbor_serialize(item, buffer, 512));
  assert_memory_equal(
      buffer,
      ((unsigned char[]){0xFB, 0xC0, 0x10, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66}),
      9);
  assert_size_equal(cbor_serialized_size(item), 9);
  cbor_decref(&item);
}

static void test_serialize_ctrl(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_undef();

  assert_size_equal(1, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xF7}), 1);
  assert_size_equal(cbor_serialized_size(item), 1);
  cbor_decref(&item);
}

static void test_serialize_long_ctrl(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_ctrl();
  cbor_set_ctrl(item, 254);

  assert_size_equal(2, cbor_serialize(item, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xF8, 0xFE}), 2);
  assert_size_equal(cbor_serialized_size(item), 2);
  cbor_decref(&item);
}

static void test_auto_serialize(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_definite_array(4);
  for (size_t i = 0; i < 4; i++) {
    assert_true(cbor_array_push(item, cbor_move(cbor_build_uint64(0))));
  }

  unsigned char *output;
  size_t output_size;
  assert_size_equal(cbor_serialize_alloc(item, &output, &output_size), 37);
  assert_size_equal(output_size, 37);
  assert_size_equal(cbor_serialized_size(item), 37);
  assert_memory_equal(output, ((unsigned char[]){0x84, 0x1B}), 2);
  cbor_decref(&item);
  _cbor_free(output);
}

static void test_auto_serialize_no_size(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_build_uint8(1);

  unsigned char *output;
  assert_size_equal(cbor_serialize_alloc(item, &output, NULL), 1);
  assert_memory_equal(output, ((unsigned char[]){0x01}), 1);
  assert_size_equal(cbor_serialized_size(item), 1);
  cbor_decref(&item);
  _cbor_free(output);
}

static void test_auto_serialize_too_large(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_string();
  cbor_item_t *chunk = cbor_new_definite_string();
  assert_true(cbor_string_add_chunk(item, chunk));

  // Pretend the chunk is huge
  chunk->metadata.string_metadata.length = SIZE_MAX;
  assert_true(SIZE_MAX + 2 == 1);
  assert_size_equal(cbor_serialized_size(item), 0);
  unsigned char *output;
  size_t output_size;
  assert_size_equal(cbor_serialize_alloc(item, &output, &output_size), 0);
  assert_size_equal(output_size, 0);
  assert_null(output);

  chunk->metadata.string_metadata.length = 0;
  cbor_decref(&chunk);
  cbor_decref(&item);
}

static void test_auto_serialize_alloc_fail(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_build_uint8(42);

  WITH_FAILING_MALLOC({
    unsigned char *output;
    size_t output_size;
    assert_size_equal(cbor_serialize_alloc(item, &output, &output_size), 0);
    assert_size_equal(output_size, 0);
    assert_null(output);
  });

  cbor_decref(&item);
}

static void test_auto_serialize_zero_len_bytestring(
    void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_build_bytestring((cbor_data) "", 0);

  unsigned char *output;
  assert_size_equal(cbor_serialize_alloc(item, &output, NULL), 1);
  assert_memory_equal(output, ((unsigned char[]){0x40}), 1);
  assert_size_equal(cbor_serialized_size(item), 1);
  cbor_decref(&item);
  _cbor_free(output);
}

static void test_auto_serialize_zero_len_string(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_build_string("");

  unsigned char *output;
  assert_size_equal(cbor_serialize_alloc(item, &output, NULL), 1);
  assert_memory_equal(output, ((unsigned char[]){0x60}), 1);
  assert_size_equal(cbor_serialized_size(item), 1);
  cbor_decref(&item);
  _cbor_free(output);
}

static void test_auto_serialize_zero_len_bytestring_chunk(
    void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_bytestring();

  assert_true(cbor_bytestring_add_chunk(
      item, cbor_move(cbor_build_bytestring((cbor_data) "", 0))));

  unsigned char *output;
  assert_size_equal(cbor_serialize_alloc(item, &output, NULL), 3);
  assert_memory_equal(output, ((unsigned char[]){0x5f, 0x40, 0xff}), 3);
  assert_size_equal(cbor_serialized_size(item), 3);
  cbor_decref(&item);
  _cbor_free(output);
}

static void test_auto_serialize_zero_len_string_chunk(
    void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_string();

  assert_true(cbor_string_add_chunk(item, cbor_move(cbor_build_string(""))));

  unsigned char *output;
  assert_size_equal(cbor_serialize_alloc(item, &output, NULL), 3);
  assert_memory_equal(output, ((unsigned char[]){0x7f, 0x60, 0xff}), 3);
  assert_size_equal(cbor_serialized_size(item), 3);
  cbor_decref(&item);
  _cbor_free(output);
}

static void test_auto_serialize_zero_len_array(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_definite_array(0);

  unsigned char *output;
  assert_size_equal(cbor_serialize_alloc(item, &output, NULL), 1);
  assert_memory_equal(output, ((unsigned char[]){0x80}), 1);
  assert_size_equal(cbor_serialized_size(item), 1);
  cbor_decref(&item);
  _cbor_free(output);
}

static void test_auto_serialize_zero_len_indef_array(
    void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_array();

  unsigned char *output;
  assert_size_equal(cbor_serialize_alloc(item, &output, NULL), 2);
  assert_memory_equal(output, ((unsigned char[]){0x9f, 0xff}), 2);
  assert_size_equal(cbor_serialized_size(item), 2);
  cbor_decref(&item);
  _cbor_free(output);
}

static void test_auto_serialize_zero_len_map(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_definite_map(0);

  unsigned char *output;
  assert_size_equal(cbor_serialize_alloc(item, &output, NULL), 1);
  assert_memory_equal(output, ((unsigned char[]){0xa0}), 1);
  assert_size_equal(cbor_serialized_size(item), 1);
  cbor_decref(&item);
  _cbor_free(output);
}

static void test_auto_serialize_zero_len_indef_map(
    void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_map();

  unsigned char *output;
  assert_size_equal(cbor_serialize_alloc(item, &output, NULL), 2);
  assert_memory_equal(output, ((unsigned char[]){0xbf, 0xff}), 2);
  assert_size_equal(cbor_serialized_size(item), 2);
  cbor_decref(&item);
  _cbor_free(output);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_serialize_uint8_embed),
      cmocka_unit_test(test_serialize_uint8),
      cmocka_unit_test(test_serialize_uint16),
      cmocka_unit_test(test_serialize_uint32),
      cmocka_unit_test(test_serialize_uint64),
      cmocka_unit_test(test_serialize_negint8_embed),
      cmocka_unit_test(test_serialize_negint8),
      cmocka_unit_test(test_serialize_negint16),
      cmocka_unit_test(test_serialize_negint32),
      cmocka_unit_test(test_serialize_negint64),
      cmocka_unit_test(test_serialize_definite_bytestring),
      cmocka_unit_test(test_serialize_indefinite_bytestring),
      cmocka_unit_test(test_serialize_bytestring_size_overflow),
      cmocka_unit_test(test_serialize_bytestring_no_space),
      cmocka_unit_test(test_serialize_indefinite_bytestring_no_space),
      cmocka_unit_test(test_serialize_definite_string),
      cmocka_unit_test(test_serialize_definite_string_4b_header),
      cmocka_unit_test(test_serialize_definite_string_8b_header),
      cmocka_unit_test(test_serialize_indefinite_string),
      cmocka_unit_test(test_serialize_string_no_space),
      cmocka_unit_test(test_serialize_indefinite_string_no_space),
      cmocka_unit_test(test_serialize_definite_array),
      cmocka_unit_test(test_serialize_indefinite_array),
      cmocka_unit_test(test_serialize_array_no_space),
      cmocka_unit_test(test_serialize_definite_map),
      cmocka_unit_test(test_serialize_indefinite_map),
      cmocka_unit_test(test_serialize_map_no_space),
      cmocka_unit_test(test_serialize_tags),
      cmocka_unit_test(test_serialize_tags_no_space),
      cmocka_unit_test(test_serialize_half),
      cmocka_unit_test(test_serialize_single),
      cmocka_unit_test(test_serialize_double),
      cmocka_unit_test(test_serialize_ctrl),
      cmocka_unit_test(test_serialize_long_ctrl),
      cmocka_unit_test(test_auto_serialize),
      cmocka_unit_test(test_auto_serialize_no_size),
      cmocka_unit_test(test_auto_serialize_too_large),
      cmocka_unit_test(test_auto_serialize_alloc_fail),
      cmocka_unit_test(test_auto_serialize_zero_len_bytestring),
      cmocka_unit_test(test_auto_serialize_zero_len_string),
      cmocka_unit_test(test_auto_serialize_zero_len_bytestring_chunk),
      cmocka_unit_test(test_auto_serialize_zero_len_string_chunk),
      cmocka_unit_test(test_auto_serialize_zero_len_array),
      cmocka_unit_test(test_auto_serialize_zero_len_indef_array),
      cmocka_unit_test(test_auto_serialize_zero_len_map),
      cmocka_unit_test(test_auto_serialize_zero_len_indef_map),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
