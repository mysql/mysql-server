/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "assertions.h"

#include "../src/cbor/internal/unicode.h"
struct _cbor_unicode_status status;

unsigned char missing_bytes_data[] = {0xC4, 0x8C};

/* Capital accented C */
static void test_missing_bytes(void **_CBOR_UNUSED(_state)) {
  assert_true(_cbor_unicode_codepoint_count(missing_bytes_data, 1, &status) ==
              0);
  assert_true(status.status == _CBOR_UNICODE_BADCP);
  assert_true(status.location == 1);

  assert_true(_cbor_unicode_codepoint_count(missing_bytes_data, 2, &status) ==
              1);
  assert_true(status.status == _CBOR_UNICODE_OK);
  assert_true(status.location == 0);
}

unsigned char invalid_sequence_data[] = {0x65, 0xC4, 0x00};

/* e, invalid seq */
static void test_invalid_sequence(void **_CBOR_UNUSED(_state)) {
  assert_true(
      _cbor_unicode_codepoint_count(invalid_sequence_data, 3, &status) == 0);
  assert_true(status.status == _CBOR_UNICODE_BADCP);
  assert_true(status.location == 2);
}

int main(void) {
  const struct CMUnitTest tests[] = {cmocka_unit_test(test_missing_bytes),
                                     cmocka_unit_test(test_invalid_sequence)};
  return cmocka_run_group_tests(tests, NULL, NULL);
}
