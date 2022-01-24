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

cbor_item_t *arr;
struct cbor_load_result res;

unsigned char data1[] = {0x80, 0xFF};

static void test_empty_array(void **state) {
  arr = cbor_load(data1, 2, &res);
  assert_non_null(arr);
  assert_true(cbor_typeof(arr) == CBOR_TYPE_ARRAY);
  assert_true(cbor_isa_array(arr));
  assert_true(cbor_array_size(arr) == 0);
  assert_true(res.read == 1);
  cbor_decref(&arr);
  assert_null(arr);
}

unsigned char data2[] = {0x81, 0x01, 0xFF};

static void test_simple_array(void **state) {
  arr = cbor_load(data2, 3, &res);
  assert_non_null(arr);
  assert_true(cbor_typeof(arr) == CBOR_TYPE_ARRAY);
  assert_true(cbor_isa_array(arr));
  assert_int_equal(cbor_array_size(arr), 1);
  assert_true(res.read == 2);
  assert_int_equal(cbor_array_allocated(arr), 1);
  /* Check the values */
  assert_uint8(cbor_array_handle(arr)[0], 1);
  cbor_item_t *intermediate = cbor_array_get(arr, 0);
  assert_uint8(intermediate, 1);

  cbor_item_t *new_val = cbor_build_uint8(10);
  assert_false(cbor_array_set(arr, 1, new_val));
  assert_false(cbor_array_set(arr, 3, new_val));
  cbor_decref(&new_val);

  cbor_decref(&arr);
  cbor_decref(&intermediate);
  assert_null(arr);
  assert_null(intermediate);
}

unsigned char data3[] = {0x82, 0x01, 0x81, 0x01, 0xFF};

static void test_nested_arrays(void **state) {
  arr = cbor_load(data3, 5, &res);
  assert_non_null(arr);
  assert_true(cbor_typeof(arr) == CBOR_TYPE_ARRAY);
  assert_true(cbor_isa_array(arr));
  assert_true(cbor_array_size(arr) == 2);
  assert_true(res.read == 4);
  /* Check the values */
  assert_uint8(cbor_array_handle(arr)[0], 1);

  cbor_item_t *nested = cbor_array_handle(arr)[1];
  assert_true(cbor_isa_array(nested));
  assert_true(cbor_array_size(nested) == 1);
  assert_uint8(cbor_array_handle(nested)[0], 1);

  cbor_decref(&arr);
  assert_null(arr);
}

unsigned char test_indef_arrays_data[] = {0x9f, 0x01, 0x02, 0xFF};

static void test_indef_arrays(void **state) {
  arr = cbor_load(test_indef_arrays_data, 4, &res);
  assert_non_null(arr);
  assert_true(cbor_typeof(arr) == CBOR_TYPE_ARRAY);
  assert_true(cbor_isa_array(arr));
  assert_true(cbor_array_size(arr) == 2);
  assert_true(res.read == 4);
  /* Check the values */
  assert_uint8(cbor_array_handle(arr)[0], 1);
  assert_uint8(cbor_array_handle(arr)[1], 2);

  assert_true(cbor_array_set(arr, 1, cbor_move(cbor_build_uint8(10))));

  cbor_decref(&arr);
  assert_null(arr);
}

unsigned char test_nested_indef_arrays_data[] = {0x9f, 0x01, 0x9f, 0x02,
                                                 0xFF, 0x03, 0xFF};

static void test_nested_indef_arrays(void **state) {
  arr = cbor_load(test_nested_indef_arrays_data, 7, &res);
  assert_non_null(arr);
  assert_true(cbor_typeof(arr) == CBOR_TYPE_ARRAY);
  assert_true(cbor_isa_array(arr));
  assert_int_equal(cbor_array_size(arr), 3);
  assert_true(res.read == 7);
  /* Check the values */
  assert_uint8(cbor_array_handle(arr)[0], 1);

  cbor_item_t *nested = cbor_array_handle(arr)[1];
  assert_true(cbor_isa_array(nested));
  assert_true(cbor_array_size(nested) == 1);
  assert_uint8(cbor_array_handle(nested)[0], 2);

  cbor_decref(&arr);
  assert_null(arr);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_empty_array), cmocka_unit_test(test_simple_array),
      cmocka_unit_test(test_nested_arrays), cmocka_unit_test(test_indef_arrays),
      cmocka_unit_test(test_nested_indef_arrays)};
  return cmocka_run_group_tests(tests, NULL, NULL);
}
