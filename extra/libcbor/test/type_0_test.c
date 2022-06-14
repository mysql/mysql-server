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

#include "cbor.h"

cbor_item_t *number;
struct cbor_load_result res;

unsigned char data1[] = {0x02, 0xFF};
unsigned char data2[] = {0x18, 0xFF, 0xFF};
unsigned char data3[] = {0x19, 0x01, 0xf4, 0xFF};
unsigned char data4[] = {0x1a, 0xa5, 0xf7, 0x02, 0xb3, 0xFF};
unsigned char data5[] = {0x1b, 0xa5, 0xf7, 0x02, 0xb3,
                         0xa5, 0xf7, 0x02, 0xb3, 0xFF};

static void test_very_short_int(void **state) {
  number = cbor_load(data1, 2, &res);
  assert_true(cbor_typeof(number) == CBOR_TYPE_UINT);
  assert_true(cbor_int_get_width(number) == CBOR_INT_8);
  assert_true(cbor_isa_uint(number));
  assert_false(cbor_isa_negint(number));
  assert_true(cbor_get_uint8(number) == 2);
  assert_true(res.error.code == 0);
  assert_true(res.read == 1);
  assert_true(cbor_is_int(number));
  cbor_decref(&number);
  assert_null(number);
}

static void test_incomplete_data(void **state) {
  number = cbor_load(data2, 1, &res);
  assert_null(number);
  assert_true(res.error.code == CBOR_ERR_NOTENOUGHDATA);
}

static void test_short_int(void **state) {
  number = cbor_load(data2, 3, &res);
  assert_true(cbor_typeof(number) == CBOR_TYPE_UINT);
  assert_true(cbor_int_get_width(number) == CBOR_INT_8);
  assert_true(cbor_isa_uint(number));
  assert_false(cbor_isa_negint(number));
  assert_true(cbor_get_uint8(number) == 255);
  assert_true(res.error.code == 0);
  assert_true(res.read == 2);
  assert_true(cbor_is_int(number));
  cbor_decref(&number);
  assert_null(number);
}

static void test_half_int(void **state) {
  number = cbor_load(data3, 5, &res);
  assert_true(cbor_typeof(number) == CBOR_TYPE_UINT);
  assert_true(cbor_int_get_width(number) == CBOR_INT_16);
  assert_true(cbor_isa_uint(number));
  assert_false(cbor_isa_negint(number));
  assert_true(cbor_get_uint16(number) == 500);
  assert_true(res.error.code == 0);
  assert_true(res.read == 3);
  assert_true(cbor_is_int(number));
  cbor_decref(&number);
  assert_null(number);
}

static void test_int(void **state) {
  number = cbor_load(data4, 6, &res);
  assert_true(cbor_typeof(number) == CBOR_TYPE_UINT);
  assert_true(cbor_int_get_width(number) == CBOR_INT_32);
  assert_true(cbor_isa_uint(number));
  assert_false(cbor_isa_negint(number));
  assert_true(cbor_get_uint32(number) == 2784428723);
  assert_true(res.error.code == 0);
  assert_true(res.read == 5);
  assert_true(cbor_is_int(number));
  cbor_decref(&number);
  assert_null(number);
}

static void test_long_int(void **state) {
  number = cbor_load(data5, 10, &res);
  assert_true(cbor_typeof(number) == CBOR_TYPE_UINT);
  assert_true(cbor_int_get_width(number) == CBOR_INT_64);
  assert_true(cbor_isa_uint(number));
  assert_false(cbor_isa_negint(number));
  assert_true(cbor_get_uint64(number) == 11959030306112471731ULL);
  assert_true(res.error.code == 0);
  assert_true(res.read == 9);
  assert_true(cbor_is_int(number));
  cbor_decref(&number);
  assert_null(number);
}

static void test_refcounting(void **state) {
  number = cbor_load(data5, 10, &res);
  cbor_incref(number);
  assert_true(number->refcount == 2);
  cbor_decref(&number);
  assert_non_null(number);
  cbor_decref(&number);
  assert_null(number);
}

static void test_empty_input(void **state) {
  number = cbor_load(data5, 0, &res);
  assert_null(number);
  assert_true(res.error.code == CBOR_ERR_NODATA);
}

static void test_inline_creation(void **state) {
  number = cbor_build_uint8(10);
  assert_true(cbor_get_int(number) == 10);
  cbor_decref(&number);

  number = cbor_build_uint16(10);
  assert_true(cbor_get_int(number) == 10);
  cbor_decref(&number);

  number = cbor_build_uint32(10);
  assert_true(cbor_get_int(number) == 10);
  cbor_decref(&number);

  number = cbor_build_uint64(10);
  assert_true(cbor_get_int(number) == 10);
  cbor_decref(&number);
}

int main(void) {
  const struct CMUnitTest tests[] = {cmocka_unit_test(test_very_short_int),
                                     cmocka_unit_test(test_short_int),
                                     cmocka_unit_test(test_half_int),
                                     cmocka_unit_test(test_int),
                                     cmocka_unit_test(test_long_int),
                                     cmocka_unit_test(test_incomplete_data),
                                     cmocka_unit_test(test_refcounting),
                                     cmocka_unit_test(test_empty_input),
                                     cmocka_unit_test(test_inline_creation)};
  return cmocka_run_group_tests(tests, NULL, NULL);
}
