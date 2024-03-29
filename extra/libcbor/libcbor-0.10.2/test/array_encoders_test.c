/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "assertions.h"
#include "cbor.h"

unsigned char buffer[512];

static void test_embedded_array_start(void **_CBOR_UNUSED(_state)) {
  assert_size_equal(1, cbor_encode_array_start(1, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x81}), 1);
}

static void test_array_start(void **_CBOR_UNUSED(_state)) {
  assert_size_equal(5, cbor_encode_array_start(1000000, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x9A, 0x00, 0x0F, 0x42, 0x40}),
                      5);
}

static void test_indef_array_start(void **_CBOR_UNUSED(_state)) {
  assert_size_equal(1, cbor_encode_indef_array_start(buffer, 512));
  assert_size_equal(0, cbor_encode_indef_array_start(buffer, 0));
  assert_memory_equal(buffer, ((unsigned char[]){0x9F}), 1);
}

static void test_indef_array_encoding(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *array = cbor_new_indefinite_array();
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_item_t *two = cbor_build_uint8(2);
  assert_true(cbor_array_push(array, one));
  assert_true(cbor_array_push(array, two));

  assert_size_equal(4, cbor_serialize_array(array, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x9F, 0x01, 0x02, 0xFF}), 4);

  cbor_decref(&array);
  cbor_decref(&one);
  cbor_decref(&two);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_embedded_array_start),
      cmocka_unit_test(test_array_start),
      cmocka_unit_test(test_indef_array_start),
      cmocka_unit_test(test_indef_array_encoding)};
  return cmocka_run_group_tests(tests, NULL, NULL);
}
