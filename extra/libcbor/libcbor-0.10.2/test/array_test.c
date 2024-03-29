/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "assertions.h"
#include "cbor.h"
#include "test_allocator.h"

cbor_item_t *arr;
struct cbor_load_result res;

unsigned char data1[] = {0x80, 0xFF};

static void test_empty_array(void **_CBOR_UNUSED(_state)) {
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

static void test_simple_array(void **_CBOR_UNUSED(_state)) {
  arr = cbor_load(data2, 3, &res);
  assert_non_null(arr);
  assert_true(cbor_typeof(arr) == CBOR_TYPE_ARRAY);
  assert_true(cbor_isa_array(arr));
  assert_size_equal(cbor_array_size(arr), 1);
  assert_true(res.read == 2);
  assert_size_equal(cbor_array_allocated(arr), 1);
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

static void test_nested_arrays(void **_CBOR_UNUSED(_state)) {
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

static void test_indef_arrays(void **_CBOR_UNUSED(_state)) {
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

static void test_nested_indef_arrays(void **_CBOR_UNUSED(_state)) {
  arr = cbor_load(test_nested_indef_arrays_data, 7, &res);
  assert_non_null(arr);
  assert_true(cbor_typeof(arr) == CBOR_TYPE_ARRAY);
  assert_true(cbor_isa_array(arr));
  assert_size_equal(cbor_array_size(arr), 3);
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

static void test_array_replace(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *array = cbor_new_definite_array(2);
  assert_size_equal(cbor_array_size(array), 0);
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_item_t *three = cbor_build_uint8(3);
  assert_size_equal(cbor_refcount(one), 1);
  assert_size_equal(cbor_refcount(three), 1);

  // No item to replace
  assert_false(cbor_array_replace(array, 0, three));
  assert_size_equal(cbor_refcount(three), 1);

  // Add items [1, 2]
  assert_true(cbor_array_push(array, one));
  assert_true(cbor_array_push(array, cbor_move(cbor_build_uint8(2))));
  assert_size_equal(cbor_refcount(one), 2);
  assert_size_equal(cbor_array_size(array), 2);

  // Array has only two items
  assert_false(cbor_array_replace(array, 2, three));
  assert_size_equal(cbor_refcount(three), 1);

  // Change [1, 2] to [3, 2]
  assert_true(cbor_array_replace(array, 0, three));
  assert_size_equal(cbor_refcount(one), 1);
  assert_size_equal(cbor_refcount(three), 2);
  assert_uint8(cbor_move(cbor_array_get(array, 0)), 3);
  assert_uint8(cbor_move(cbor_array_get(array, 1)), 2);

  cbor_decref(&one);
  cbor_decref(&three);
  cbor_decref(&array);
}

static void test_array_push_overflow(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *array = cbor_new_indefinite_array();
  cbor_item_t *one = cbor_build_uint8(1);
  struct _cbor_array_metadata *metadata =
      (struct _cbor_array_metadata *)&array->metadata;
  // Pretend we already have a huge block allocated
  metadata->allocated = SIZE_MAX;
  metadata->end_ptr = SIZE_MAX;

  assert_false(cbor_array_push(array, one));
  assert_size_equal(cbor_refcount(one), 1);

  cbor_decref(&one);
  metadata->allocated = 0;
  metadata->end_ptr = 0;
  cbor_decref(&array);
}

static void test_array_creation(void **_CBOR_UNUSED(_state)) {
  WITH_FAILING_MALLOC({ assert_null(cbor_new_definite_array(42)); });
  WITH_MOCK_MALLOC({ assert_null(cbor_new_definite_array(42)); }, 2, MALLOC,
                   MALLOC_FAIL);

  WITH_FAILING_MALLOC({ assert_null(cbor_new_indefinite_array()); });
}

static void test_array_push(void **_CBOR_UNUSED(_state)) {
  WITH_MOCK_MALLOC(
      {
        cbor_item_t *array = cbor_new_indefinite_array();
        cbor_item_t *string = cbor_build_string("Hello!");

        assert_false(cbor_array_push(array, string));
        assert_size_equal(cbor_array_allocated(array), 0);
        assert_null(array->data);
        assert_size_equal(array->metadata.array_metadata.end_ptr, 0);

        cbor_decref(&string);
        cbor_decref(&array);
      },
      4, MALLOC, MALLOC, MALLOC, REALLOC_FAIL);
}

static unsigned char simple_indef_array[] = {0x9F, 0x01, 0x02, 0xFF};
static void test_indef_array_decode(void **_CBOR_UNUSED(_state)) {
  WITH_MOCK_MALLOC(
      {
        cbor_item_t *array;
        struct cbor_load_result res;
        array = cbor_load(simple_indef_array, 4, &res);

        assert_null(array);
        assert_size_equal(res.error.code, CBOR_ERR_MEMERROR);
      },
      4, MALLOC, MALLOC, MALLOC, REALLOC_FAIL);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_empty_array),
      cmocka_unit_test(test_simple_array),
      cmocka_unit_test(test_nested_arrays),
      cmocka_unit_test(test_indef_arrays),
      cmocka_unit_test(test_nested_indef_arrays),
      cmocka_unit_test(test_array_replace),
      cmocka_unit_test(test_array_push_overflow),
      cmocka_unit_test(test_array_creation),
      cmocka_unit_test(test_array_push),
      cmocka_unit_test(test_indef_array_decode),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
