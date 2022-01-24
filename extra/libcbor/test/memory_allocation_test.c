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

// This test simulates cases when malloc unexpectedly fails and leaves a
// possibly partially constructed object behind. It this is especially useful
// in conjunction with the memory correctness check.
//
// WARNING: The test only works with CBOR_CUSTOM_ALLOC

typedef enum call_expectation {
  MALLOC,
  MALLOC_FAIL,
  REALLOC,
  REALLOC_FAIL
} call_expectation;

// How many alloc calls we expect
int alloc_calls_expected;
// How many alloc calls we got
int alloc_calls;
// Array of booleans indicating whether to return a block or fail with NULL
call_expectation *expectations;

void set_mock_malloc(int calls, ...) {
  va_list args;
  va_start(args, calls);
  alloc_calls_expected = calls;
  alloc_calls = 0;
  expectations = calloc(calls, sizeof(expectations));
  for (int i = 0; i < calls; i++) {
    // Promotable types, baby
    expectations[i] = va_arg(args, call_expectation);
  }
  va_end(args);
}

void finalize_mock_malloc() {
  assert_int_equal(alloc_calls, alloc_calls_expected);
  free(expectations);
}

void *instrumented_malloc(size_t size) {
  if (alloc_calls >= alloc_calls_expected) {
    goto error;
  }

  if (expectations[alloc_calls] == MALLOC) {
    alloc_calls++;
    return malloc(size);
  } else if (expectations[alloc_calls] == MALLOC_FAIL) {
    alloc_calls++;
    return NULL;
  }

error:
  print_error("Unexpected call to malloc");
  fail();
  return NULL;
}

void *instrumented_realloc(void *ptr, size_t size) {
  if (alloc_calls >= alloc_calls_expected) {
    goto error;
  }

  if (expectations[alloc_calls] == REALLOC) {
    alloc_calls++;
    return realloc(ptr, size);
  } else if (expectations[alloc_calls] == REALLOC_FAIL) {
    alloc_calls++;
    return NULL;
  }

error:
  print_error("Unexpected call to realloc");
  fail();
  return NULL;
}

#define WITH_MOCK_MALLOC(block, malloc_calls, ...) \
  do {                                             \
    set_mock_malloc(malloc_calls, __VA_ARGS__);    \
    block;                                         \
    finalize_mock_malloc();                        \
  } while (0)

#define WITH_FAILING_MALLOC(block) WITH_MOCK_MALLOC(block, 1, MALLOC_FAIL)

static void test_int_creation(void **state) {
  WITH_FAILING_MALLOC({ assert_null(cbor_new_int8()); });
  WITH_FAILING_MALLOC({ assert_null(cbor_new_int16()); });
  WITH_FAILING_MALLOC({ assert_null(cbor_new_int32()); });
  WITH_FAILING_MALLOC({ assert_null(cbor_new_int64()); });

  WITH_FAILING_MALLOC({ assert_null(cbor_build_uint8(0xFF)); });
  WITH_FAILING_MALLOC({ assert_null(cbor_build_uint16(0xFF)); });
  WITH_FAILING_MALLOC({ assert_null(cbor_build_uint32(0xFF)); });
  WITH_FAILING_MALLOC({ assert_null(cbor_build_uint64(0xFF)); });

  WITH_FAILING_MALLOC({ assert_null(cbor_build_negint8(0xFF)); });
  WITH_FAILING_MALLOC({ assert_null(cbor_build_negint16(0xFF)); });
  WITH_FAILING_MALLOC({ assert_null(cbor_build_negint32(0xFF)); });
  WITH_FAILING_MALLOC({ assert_null(cbor_build_negint64(0xFF)); });
}

static void test_bytestring_creation(void **state) {
  WITH_FAILING_MALLOC({ assert_null(cbor_new_definite_bytestring()); });

  WITH_FAILING_MALLOC({ assert_null(cbor_new_indefinite_bytestring()); });
  WITH_MOCK_MALLOC({ assert_null(cbor_new_indefinite_bytestring()); }, 2,
                   MALLOC, MALLOC_FAIL);

  unsigned char bytes[] = {0, 0, 0xFF, 0xAB};

  WITH_FAILING_MALLOC({ assert_null(cbor_build_bytestring(bytes, 4)); });
  WITH_MOCK_MALLOC({ assert_null(cbor_build_bytestring(bytes, 4)); }, 2, MALLOC,
                   MALLOC_FAIL);
}

static void test_string_creation(void **state) {
  WITH_FAILING_MALLOC({ assert_null(cbor_new_definite_string()); });

  WITH_FAILING_MALLOC({ assert_null(cbor_new_indefinite_string()); });
  WITH_MOCK_MALLOC({ assert_null(cbor_new_indefinite_string()); }, 2, MALLOC,
                   MALLOC_FAIL);

  WITH_FAILING_MALLOC({ assert_null(cbor_build_string("Test")); });
  WITH_MOCK_MALLOC({ assert_null(cbor_build_string("Test")); }, 2, MALLOC,
                   MALLOC_FAIL);

  WITH_FAILING_MALLOC({ assert_null(cbor_build_stringn("Test", 4)); });
  WITH_MOCK_MALLOC({ assert_null(cbor_build_stringn("Test", 4)); }, 2, MALLOC,
                   MALLOC_FAIL);
}

static void test_array_creation(void **state) {
  WITH_FAILING_MALLOC({ assert_null(cbor_new_definite_array(42)); });
  WITH_MOCK_MALLOC({ assert_null(cbor_new_definite_array(42)); }, 2, MALLOC,
                   MALLOC_FAIL);

  WITH_FAILING_MALLOC({ assert_null(cbor_new_indefinite_array()); });
}

static void test_map_creation(void **state) {
  WITH_FAILING_MALLOC({ assert_null(cbor_new_definite_map(42)); });
  WITH_MOCK_MALLOC({ assert_null(cbor_new_definite_map(42)); }, 2, MALLOC,
                   MALLOC_FAIL);

  WITH_FAILING_MALLOC({ assert_null(cbor_new_indefinite_map()); });
}

static void test_tag_creation(void **state) {
  WITH_FAILING_MALLOC({ assert_null(cbor_new_tag(42)); });
}

static void test_float_ctrl_creation(void **state) {
  WITH_FAILING_MALLOC({ assert_null(cbor_new_ctrl()); });
  WITH_FAILING_MALLOC({ assert_null(cbor_new_float2()); });
  WITH_FAILING_MALLOC({ assert_null(cbor_new_float4()); });
  WITH_FAILING_MALLOC({ assert_null(cbor_new_float8()); });
  WITH_FAILING_MALLOC({ assert_null(cbor_new_null()); });
  WITH_FAILING_MALLOC({ assert_null(cbor_new_undef()); });

  WITH_FAILING_MALLOC({ assert_null(cbor_build_bool(false)); });
  WITH_FAILING_MALLOC({ assert_null(cbor_build_float2(3.14)); });
  WITH_FAILING_MALLOC({ assert_null(cbor_build_float4(3.14)); });
  WITH_FAILING_MALLOC({ assert_null(cbor_build_float8(3.14)); });
  WITH_FAILING_MALLOC({ assert_null(cbor_build_ctrl(0xAF)); });
}

static void test_bytestring_add_chunk(void **state) {
  unsigned char bytes[] = {0, 0, 0xFF, 0xAB};
  WITH_MOCK_MALLOC(
      {
        cbor_item_t *bytestring = cbor_new_indefinite_bytestring();
        cbor_item_t *chunk = cbor_build_bytestring(bytes, 4);

        assert_false(cbor_bytestring_add_chunk(bytestring, chunk));
        assert_int_equal(cbor_bytestring_chunk_count(bytestring), 0);
        assert_int_equal(
            ((struct cbor_indefinite_string_data *)bytestring->data)
                ->chunk_capacity,
            0);

        cbor_decref(&chunk);
        cbor_decref(&bytestring);
      },
      5, MALLOC, MALLOC, MALLOC, MALLOC, REALLOC_FAIL);
}

static void test_string_add_chunk(void **state) {
  WITH_MOCK_MALLOC(
      {
        cbor_item_t *string = cbor_new_indefinite_string();
        cbor_item_t *chunk = cbor_build_string("Hello!");

        assert_false(cbor_string_add_chunk(string, chunk));
        assert_int_equal(cbor_string_chunk_count(string), 0);
        assert_int_equal(((struct cbor_indefinite_string_data *)string->data)
                             ->chunk_capacity,
                         0);

        cbor_decref(&chunk);
        cbor_decref(&string);
      },
      5, MALLOC, MALLOC, MALLOC, MALLOC, REALLOC_FAIL);
}

static void test_array_push(void **state) {
  WITH_MOCK_MALLOC(
      {
        cbor_item_t *array = cbor_new_indefinite_array();
        cbor_item_t *string = cbor_build_string("Hello!");

        assert_false(cbor_array_push(array, string));
        assert_int_equal(cbor_array_allocated(array), 0);
        assert_null(array->data);
        assert_int_equal(array->metadata.array_metadata.end_ptr, 0);

        cbor_decref(&string);
        cbor_decref(&array);
      },
      4, MALLOC, MALLOC, MALLOC, REALLOC_FAIL);
}

static void test_map_add(void **state) {
  WITH_MOCK_MALLOC(
      {
        cbor_item_t *map = cbor_new_indefinite_map();
        cbor_item_t *key = cbor_build_uint8(0);
        cbor_item_t *value = cbor_build_bool(true);

        assert_false(
            cbor_map_add(map, (struct cbor_pair){.key = key, .value = value}));
        assert_int_equal(cbor_map_allocated(map), 0);
        assert_null(map->data);

        cbor_decref(&map);
        cbor_decref(&key);
        cbor_decref(&value);
      },
      4, MALLOC, MALLOC, MALLOC, REALLOC_FAIL);
}

int main(void) {
  cbor_set_allocs(instrumented_malloc, instrumented_realloc, free);

  // TODO: string chunks realloc test
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_int_creation),
      cmocka_unit_test(test_bytestring_creation),
      cmocka_unit_test(test_string_creation),
      cmocka_unit_test(test_array_creation),
      cmocka_unit_test(test_map_creation),
      cmocka_unit_test(test_tag_creation),
      cmocka_unit_test(test_float_ctrl_creation),

      cmocka_unit_test(test_bytestring_add_chunk),
      cmocka_unit_test(test_string_add_chunk),
      cmocka_unit_test(test_array_push),
      cmocka_unit_test(test_map_add),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
