/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "assertions.h"

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

void assert_decoder_result(size_t read, enum cbor_decoder_status status,
                           struct cbor_decoder_result result) {
  assert_true(read == result.read);
  assert_true(status == result.status);
  assert_true(0 == result.required);
}

void assert_decoder_result_nedata(size_t required,
                                  struct cbor_decoder_result result) {
  assert_true(0 == result.read);
  assert_true(CBOR_DECODER_NEDATA == result.status);
  assert_int_equal((int)required, (int)result.required);
}
