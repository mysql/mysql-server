/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "assertions.h"
#include "stream_expectations.h"

void assert_uint8(cbor_item_t* item, uint8_t num) {
  assert_true(cbor_isa_uint(item));
  assert_true(cbor_int_get_width(item) == CBOR_INT_8);
  assert_true(cbor_get_uint8(item) == num);
}

void assert_uint16(cbor_item_t* item, uint16_t num) {
  assert_true(cbor_isa_uint(item));
  assert_true(cbor_int_get_width(item) == CBOR_INT_16);
  assert_true(cbor_get_uint16(item) == num);
}

void assert_uint32(cbor_item_t* item, uint32_t num) {
  assert_true(cbor_isa_uint(item));
  assert_true(cbor_int_get_width(item) == CBOR_INT_32);
  assert_true(cbor_get_uint32(item) == num);
}

void assert_uint64(cbor_item_t* item, uint64_t num) {
  assert_true(cbor_isa_uint(item));
  assert_true(cbor_int_get_width(item) == CBOR_INT_64);
  assert_true(cbor_get_uint64(item) == num);
}

void assert_decoder_result(size_t expected_bytes_read,
                           enum cbor_decoder_status expected_status,
                           struct cbor_decoder_result actual_result) {
  assert_true(actual_result.read == expected_bytes_read);
  assert_true(actual_result.status == expected_status);
  assert_true(actual_result.required == 0);
}

void assert_decoder_result_nedata(size_t expected_bytes_required,
                                  struct cbor_decoder_result actual_result) {
  assert_true(actual_result.read == 0);
  assert_true(actual_result.status == CBOR_DECODER_NEDATA);
  assert_true(actual_result.required == expected_bytes_required);
}

void assert_minimum_input_size(size_t expected, cbor_data data) {
  for (size_t available = 1; available < expected; available++) {
    assert_decoder_result_nedata(expected, decode(data, 1));
  }
}

void _assert_size_equal(size_t actual, size_t expected, const char* src_file,
                        int src_line) {
  if (actual == expected) return;
  // Not using `fail_msg` since it mishandles variadic macro arguments, which
  // causes compiler warnings/
  // TODO file bug
  printf("(%s:%d) assert_size_equal: Expected %zu to equal %zu\n", src_file,
         src_line, actual, expected);
  fail();
}
