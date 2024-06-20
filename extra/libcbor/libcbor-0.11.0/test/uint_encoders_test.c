/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "assertions.h"
#include "cbor.h"

unsigned char buffer[512];

static void test_embedded_uint8(void **_CBOR_UNUSED(_state)) {
  assert_size_equal(1, cbor_encode_uint8(14, buffer, 512));
  assert_memory_equal(buffer, (unsigned char[]){0x0E}, 1);
}

static void test_uint8(void **_CBOR_UNUSED(_state)) {
  assert_size_equal(0, cbor_encode_uint8(180, buffer, 1));
  assert_size_equal(2, cbor_encode_uint8(255, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x18, 0xFF}), 2);
}

static void test_uint16(void **_CBOR_UNUSED(_state)) {
  assert_size_equal(0, cbor_encode_uint16(1000, buffer, 2));
  assert_size_equal(3, cbor_encode_uint16(1000, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x19, 0x03, 0xE8}), 3);
}

static void test_uint32(void **_CBOR_UNUSED(_state)) {
  assert_size_equal(0, cbor_encode_uint32(1000000, buffer, 4));
  assert_size_equal(5, cbor_encode_uint32(1000000, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x1A, 0x00, 0x0F, 0x42, 0x40}),
                      5);
}

static void test_uint64(void **_CBOR_UNUSED(_state)) {
  assert_size_equal(0, cbor_encode_uint64(18446744073709551615ULL, buffer, 8));
  assert_size_equal(9,
                    cbor_encode_uint64(18446744073709551615ULL, buffer, 512));
  assert_memory_equal(
      buffer,
      ((unsigned char[]){0x1B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}),
      9);
}

static void test_unspecified(void **_CBOR_UNUSED(_state)) {
  assert_size_equal(9, cbor_encode_uint(18446744073709551615ULL, buffer, 512));
  assert_memory_equal(
      buffer,
      ((unsigned char[]){0x1B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}),
      9);
  assert_size_equal(5, cbor_encode_uint(1000000, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x1A, 0x00, 0x0F, 0x42, 0x40}),
                      5);
  assert_size_equal(3, cbor_encode_uint(1000, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x19, 0x03, 0xE8}), 3);
  assert_size_equal(2, cbor_encode_uint(255, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0x18, 0xFF}), 2);
}

int main(void) {
  const struct CMUnitTest tests[] = {cmocka_unit_test(test_embedded_uint8),
                                     cmocka_unit_test(test_uint8),
                                     cmocka_unit_test(test_uint16),
                                     cmocka_unit_test(test_uint32),
                                     cmocka_unit_test(test_uint64),
                                     cmocka_unit_test(test_unspecified)};
  return cmocka_run_group_tests(tests, NULL, NULL);
}
