/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <string.h>
#include "assertions.h"
#include "cbor.h"
#include "test_allocator.h"

cbor_item_t *string;
struct cbor_load_result res;

unsigned char empty_string_data[] = {0x60};

static void test_empty_string(void **_CBOR_UNUSED(_state)) {
  string = cbor_load(empty_string_data, 1, &res);
  assert_non_null(string);
  assert_true(cbor_typeof(string) == CBOR_TYPE_STRING);
  assert_true(cbor_isa_string(string));
  assert_size_equal(cbor_string_length(string), 0);
  assert_size_equal(cbor_string_codepoint_count(string), 0);
  assert_true(res.read == 1);
  cbor_decref(&string);
  assert_null(string);
}

unsigned char short_string_data[] = {0x6C, 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20,
                                     0x77, 0x6F, 0x72, 0x6C, 0x64, 0x21};

/*                              0x60 + 12 | Hello world! */
static void test_short_string(void **_CBOR_UNUSED(_state)) {
  string = cbor_load(short_string_data, 13, &res);
  assert_non_null(string);
  assert_true(cbor_typeof(string) == CBOR_TYPE_STRING);
  assert_true(cbor_isa_string(string));
  assert_size_equal(cbor_string_length(string), 12);
  assert_size_equal(cbor_string_codepoint_count(string), 12);
  assert_memory_equal(&"Hello world!", cbor_string_handle(string), 12);
  assert_true(res.read == 13);
  cbor_decref(&string);
  assert_null(string);
}

unsigned char short_multibyte_string_data[] = {
    0x6F, 0xC4, 0x8C, 0x61, 0x75, 0x65, 0x73, 0x20,
    0xC3, 0x9F, 0x76, 0xC4, 0x9B, 0x74, 0x65, 0x21};

/*                              0x60 + 15 | Čaues ßvěte! */
static void test_short_multibyte_string(void **_CBOR_UNUSED(_state)) {
  string = cbor_load(short_multibyte_string_data, 16, &res);
  assert_non_null(string);
  assert_true(cbor_typeof(string) == CBOR_TYPE_STRING);
  assert_true(cbor_isa_string(string));
  assert_size_equal(cbor_string_length(string), 15);
  assert_size_equal(cbor_string_codepoint_count(string), 12);
  assert_memory_equal(&"Čaues ßvěte!", cbor_string_handle(string), 15);
  assert_true(res.read == 16);
  cbor_decref(&string);
  assert_null(string);
}

unsigned char int8_string_data[] = {
    0x78, 0x96, 0x4C, 0x6F, 0x72, 0x65, 0x6D, 0x20, 0x69, 0x70, 0x73, 0x75,
    0x6D, 0x20, 0x64, 0x6F, 0x6C, 0x6F, 0x72, 0x20, 0x73, 0x69, 0x74, 0x20,
    0x61, 0x6D, 0x65, 0x74, 0x2C, 0x20, 0x63, 0x6F, 0x6E, 0x73, 0x65, 0x63,
    0x74, 0x65, 0x74, 0x75, 0x72, 0x20, 0x61, 0x64, 0x69, 0x70, 0x69, 0x73,
    0x63, 0x69, 0x6E, 0x67, 0x20, 0x65, 0x6C, 0x69, 0x74, 0x2E, 0x20, 0x44,
    0x6F, 0x6E, 0x65, 0x63, 0x20, 0x6D, 0x69, 0x20, 0x74, 0x65, 0x6C, 0x6C,
    0x75, 0x73, 0x2C, 0x20, 0x69, 0x61, 0x63, 0x75, 0x6C, 0x69, 0x73, 0x20,
    0x6E, 0x65, 0x63, 0x20, 0x76, 0x65, 0x73, 0x74, 0x69, 0x62, 0x75, 0x6C,
    0x75, 0x6D, 0x20, 0x71, 0x75, 0x69, 0x73, 0x2C, 0x20, 0x66, 0x65, 0x72,
    0x6D, 0x65, 0x6E, 0x74, 0x75, 0x6D, 0x20, 0x6E, 0x6F, 0x6E, 0x20, 0x66,
    0x65, 0x6C, 0x69, 0x73, 0x2E, 0x20, 0x4D, 0x61, 0x65, 0x63, 0x65, 0x6E,
    0x61, 0x73, 0x20, 0x75, 0x74, 0x20, 0x6A, 0x75, 0x73, 0x74, 0x6F, 0x20,
    0x70, 0x6F, 0x73, 0x75, 0x65, 0x72, 0x65, 0x2E};

/*                                          150 | Lorem ....*/
static void test_int8_string(void **_CBOR_UNUSED(_state)) {
  string = cbor_load(int8_string_data, 152, &res);
  assert_non_null(string);
  assert_true(cbor_typeof(string) == CBOR_TYPE_STRING);
  assert_true(cbor_isa_string(string));
  assert_size_equal(cbor_string_length(string), 150);
  assert_size_equal(cbor_string_codepoint_count(string), 150);
  assert_memory_equal(
		&"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec mi tellus, iaculis nec vestibulum quis, fermentum non felis. Maecenas ut justo posuere.",
		cbor_string_handle(string),
		150
	);
  assert_true(res.read == 152);
  cbor_decref(&string);
  assert_null(string);
}

unsigned char int16_string_data[] = {
    0x79, 0x00, 0x96, 0x4C, 0x6F, 0x72, 0x65, 0x6D, 0x20, 0x69, 0x70, 0x73,
    0x75, 0x6D, 0x20, 0x64, 0x6F, 0x6C, 0x6F, 0x72, 0x20, 0x73, 0x69, 0x74,
    0x20, 0x61, 0x6D, 0x65, 0x74, 0x2C, 0x20, 0x63, 0x6F, 0x6E, 0x73, 0x65,
    0x63, 0x74, 0x65, 0x74, 0x75, 0x72, 0x20, 0x61, 0x64, 0x69, 0x70, 0x69,
    0x73, 0x63, 0x69, 0x6E, 0x67, 0x20, 0x65, 0x6C, 0x69, 0x74, 0x2E, 0x20,
    0x44, 0x6F, 0x6E, 0x65, 0x63, 0x20, 0x6D, 0x69, 0x20, 0x74, 0x65, 0x6C,
    0x6C, 0x75, 0x73, 0x2C, 0x20, 0x69, 0x61, 0x63, 0x75, 0x6C, 0x69, 0x73,
    0x20, 0x6E, 0x65, 0x63, 0x20, 0x76, 0x65, 0x73, 0x74, 0x69, 0x62, 0x75,
    0x6C, 0x75, 0x6D, 0x20, 0x71, 0x75, 0x69, 0x73, 0x2C, 0x20, 0x66, 0x65,
    0x72, 0x6D, 0x65, 0x6E, 0x74, 0x75, 0x6D, 0x20, 0x6E, 0x6F, 0x6E, 0x20,
    0x66, 0x65, 0x6C, 0x69, 0x73, 0x2E, 0x20, 0x4D, 0x61, 0x65, 0x63, 0x65,
    0x6E, 0x61, 0x73, 0x20, 0x75, 0x74, 0x20, 0x6A, 0x75, 0x73, 0x74, 0x6F,
    0x20, 0x70, 0x6F, 0x73, 0x75, 0x65, 0x72, 0x65, 0x2E};
/*                                          150 | Lorem ....*/
/* This valid but not realistic - length 150 could be encoded in a single
 * uint8_t (but we need to keep the test files reasonably compact) */
static void test_int16_string(void **_CBOR_UNUSED(_state)) {
  string = cbor_load(int16_string_data, 153, &res);
  assert_non_null(string);
  assert_true(cbor_typeof(string) == CBOR_TYPE_STRING);
  assert_true(cbor_isa_string(string));
  assert_size_equal(cbor_string_length(string), 150);
  assert_size_equal(cbor_string_codepoint_count(string), 150);
  assert_memory_equal(
		&"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec mi tellus, iaculis nec vestibulum quis, fermentum non felis. Maecenas ut justo posuere.",
		cbor_string_handle(string),
		150
	);
  assert_true(res.read == 153);
  cbor_decref(&string);
  assert_null(string);
}

unsigned char int32_string_data[] = {
    0x7A, 0x00, 0x00, 0x00, 0x96, 0x4C, 0x6F, 0x72, 0x65, 0x6D, 0x20, 0x69,
    0x70, 0x73, 0x75, 0x6D, 0x20, 0x64, 0x6F, 0x6C, 0x6F, 0x72, 0x20, 0x73,
    0x69, 0x74, 0x20, 0x61, 0x6D, 0x65, 0x74, 0x2C, 0x20, 0x63, 0x6F, 0x6E,
    0x73, 0x65, 0x63, 0x74, 0x65, 0x74, 0x75, 0x72, 0x20, 0x61, 0x64, 0x69,
    0x70, 0x69, 0x73, 0x63, 0x69, 0x6E, 0x67, 0x20, 0x65, 0x6C, 0x69, 0x74,
    0x2E, 0x20, 0x44, 0x6F, 0x6E, 0x65, 0x63, 0x20, 0x6D, 0x69, 0x20, 0x74,
    0x65, 0x6C, 0x6C, 0x75, 0x73, 0x2C, 0x20, 0x69, 0x61, 0x63, 0x75, 0x6C,
    0x69, 0x73, 0x20, 0x6E, 0x65, 0x63, 0x20, 0x76, 0x65, 0x73, 0x74, 0x69,
    0x62, 0x75, 0x6C, 0x75, 0x6D, 0x20, 0x71, 0x75, 0x69, 0x73, 0x2C, 0x20,
    0x66, 0x65, 0x72, 0x6D, 0x65, 0x6E, 0x74, 0x75, 0x6D, 0x20, 0x6E, 0x6F,
    0x6E, 0x20, 0x66, 0x65, 0x6C, 0x69, 0x73, 0x2E, 0x20, 0x4D, 0x61, 0x65,
    0x63, 0x65, 0x6E, 0x61, 0x73, 0x20, 0x75, 0x74, 0x20, 0x6A, 0x75, 0x73,
    0x74, 0x6F, 0x20, 0x70, 0x6F, 0x73, 0x75, 0x65, 0x72, 0x65, 0x2E};

/*                                          150 | Lorem ....*/
static void test_int32_string(void **_CBOR_UNUSED(_state)) {
  string = cbor_load(int32_string_data, 155, &res);
  assert_non_null(string);
  assert_true(cbor_typeof(string) == CBOR_TYPE_STRING);
  assert_true(cbor_isa_string(string));
  assert_size_equal(cbor_string_length(string), 150);
  assert_size_equal(cbor_string_codepoint_count(string), 150);
  assert_memory_equal(
		&"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec mi tellus, iaculis nec vestibulum quis, fermentum non felis. Maecenas ut justo posuere.",
		cbor_string_handle(string),
		150
	);
  assert_true(res.read == 155);
  cbor_decref(&string);
  assert_null(string);
}

unsigned char int64_string_data[] = {
    0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x96, 0x4C, 0x6F, 0x72,
    0x65, 0x6D, 0x20, 0x69, 0x70, 0x73, 0x75, 0x6D, 0x20, 0x64, 0x6F, 0x6C,
    0x6F, 0x72, 0x20, 0x73, 0x69, 0x74, 0x20, 0x61, 0x6D, 0x65, 0x74, 0x2C,
    0x20, 0x63, 0x6F, 0x6E, 0x73, 0x65, 0x63, 0x74, 0x65, 0x74, 0x75, 0x72,
    0x20, 0x61, 0x64, 0x69, 0x70, 0x69, 0x73, 0x63, 0x69, 0x6E, 0x67, 0x20,
    0x65, 0x6C, 0x69, 0x74, 0x2E, 0x20, 0x44, 0x6F, 0x6E, 0x65, 0x63, 0x20,
    0x6D, 0x69, 0x20, 0x74, 0x65, 0x6C, 0x6C, 0x75, 0x73, 0x2C, 0x20, 0x69,
    0x61, 0x63, 0x75, 0x6C, 0x69, 0x73, 0x20, 0x6E, 0x65, 0x63, 0x20, 0x76,
    0x65, 0x73, 0x74, 0x69, 0x62, 0x75, 0x6C, 0x75, 0x6D, 0x20, 0x71, 0x75,
    0x69, 0x73, 0x2C, 0x20, 0x66, 0x65, 0x72, 0x6D, 0x65, 0x6E, 0x74, 0x75,
    0x6D, 0x20, 0x6E, 0x6F, 0x6E, 0x20, 0x66, 0x65, 0x6C, 0x69, 0x73, 0x2E,
    0x20, 0x4D, 0x61, 0x65, 0x63, 0x65, 0x6E, 0x61, 0x73, 0x20, 0x75, 0x74,
    0x20, 0x6A, 0x75, 0x73, 0x74, 0x6F, 0x20, 0x70, 0x6F, 0x73, 0x75, 0x65,
    0x72, 0x65, 0x2E};

/*                                          150 | Lorem ....*/
static void test_int64_string(void **_CBOR_UNUSED(_state)) {
  string = cbor_load(int64_string_data, 159, &res);
  assert_non_null(string);
  assert_true(cbor_typeof(string) == CBOR_TYPE_STRING);
  assert_true(cbor_isa_string(string));
  assert_size_equal(cbor_string_length(string), 150);
  assert_size_equal(cbor_string_codepoint_count(string), 150);
  assert_memory_equal(
		&"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec mi tellus, iaculis nec vestibulum quis, fermentum non felis. Maecenas ut justo posuere.",
		cbor_string_handle(string),
		150
	);
  assert_true(res.read == 159);
  cbor_decref(&string);
  assert_null(string);
}

unsigned char short_indef_string_data[] = {0x7F, 0x78, 0x01, 0x65, 0xFF, 0xFF};

/*                                         start |   string      | break| extra
 */

static void test_short_indef_string(void **_CBOR_UNUSED(_state)) {
  string = cbor_load(short_indef_string_data, 6, &res);
  assert_non_null(string);
  assert_true(cbor_typeof(string) == CBOR_TYPE_STRING);
  assert_true(cbor_isa_string(string));
  assert_true(cbor_string_length(string) == 0);
  assert_true(cbor_string_is_indefinite(string));
  assert_true(cbor_string_chunk_count(string) == 1);
  assert_true(res.read == 5);
  assert_true(cbor_isa_string(cbor_string_chunks_handle(string)[0]));
  assert_true(cbor_string_length(cbor_string_chunks_handle(string)[0]) == 1);
  assert_true(*cbor_string_handle(cbor_string_chunks_handle(string)[0]) == 'e');
  cbor_decref(&string);
  assert_null(string);
}

static void test_invalid_utf(void **_CBOR_UNUSED(_state)) {
  /* 0x60 + 1 | 0xC5 (invalid unfinished 2B codepoint) */
  unsigned char string_data[] = {0x61, 0xC5};
  string = cbor_load(string_data, 2, &res);

  assert_non_null(string);
  assert_true(cbor_typeof(string) == CBOR_TYPE_STRING);
  assert_true(cbor_isa_string(string));
  assert_size_equal(cbor_string_length(string), 1);
  assert_size_equal(cbor_string_codepoint_count(string), 0);
  assert_true(cbor_string_is_definite(string));
  assert_true(res.read == 2);

  cbor_decref(&string);
}

static void test_inline_creation(void **_CBOR_UNUSED(_state)) {
  string = cbor_build_string("Hello!");
  assert_memory_equal(cbor_string_handle(string), "Hello!", strlen("Hello!"));
  cbor_decref(&string);
}

static void test_string_creation(void **_CBOR_UNUSED(_state)) {
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

static void test_string_add_chunk(void **_CBOR_UNUSED(_state)) {
  WITH_MOCK_MALLOC(
      {
        cbor_item_t *string = cbor_new_indefinite_string();
        cbor_item_t *chunk = cbor_build_string("Hello!");

        assert_false(cbor_string_add_chunk(string, chunk));
        assert_size_equal(cbor_string_chunk_count(string), 0);
        assert_size_equal(((struct cbor_indefinite_string_data *)string->data)
                              ->chunk_capacity,
                          0);

        cbor_decref(&chunk);
        cbor_decref(&string);
      },
      5, MALLOC, MALLOC, MALLOC, MALLOC, REALLOC_FAIL);
}

static void test_add_chunk_reallocation_overflow(void **_CBOR_UNUSED(_state)) {
  string = cbor_new_indefinite_string();
  cbor_item_t *chunk = cbor_build_string("Hello!");
  struct cbor_indefinite_string_data *metadata =
      (struct cbor_indefinite_string_data *)string->data;
  // Pretend we already have many chunks allocated
  metadata->chunk_count = SIZE_MAX;
  metadata->chunk_capacity = SIZE_MAX;

  assert_false(cbor_string_add_chunk(string, chunk));
  assert_size_equal(cbor_refcount(chunk), 1);

  metadata->chunk_count = 0;
  metadata->chunk_capacity = 0;
  cbor_decref(&chunk);
  cbor_decref(&string);
}

static void test_set_handle(void **_CBOR_UNUSED(_state)) {
  string = cbor_new_definite_string();
  char *test_string = "Hello";
  unsigned char *string_data = malloc(strlen(test_string));
  memcpy(string_data, test_string, strlen(test_string));
  assert_ptr_not_equal(string_data, NULL);
  cbor_string_set_handle(string, string_data, strlen(test_string));

  assert_ptr_equal(cbor_string_handle(string), string_data);
  assert_size_equal(cbor_string_length(string), 5);
  assert_size_equal(cbor_string_codepoint_count(string), 5);

  cbor_decref(&string);
}

static void test_set_handle_multibyte_codepoint(void **_CBOR_UNUSED(_state)) {
  string = cbor_new_definite_string();
  // "Štěstíčko" in UTF-8
  char *test_string = "\xc5\xa0t\xc4\x9bst\xc3\xad\xc4\x8dko";
  unsigned char *string_data = malloc(strlen(test_string));
  memcpy(string_data, test_string, strlen(test_string));
  assert_ptr_not_equal(string_data, NULL);
  cbor_string_set_handle(string, string_data, strlen(test_string));

  assert_ptr_equal(cbor_string_handle(string), string_data);
  assert_size_equal(cbor_string_length(string), 13);
  assert_size_equal(cbor_string_codepoint_count(string), 9);

  cbor_decref(&string);
}

static void test_set_handle_invalid_utf(void **_CBOR_UNUSED(_state)) {
  string = cbor_new_definite_string();
  // Invalid multi-byte character (missing the second byte).
  char *test_string = "Test: \xc5";
  unsigned char *string_data = malloc(strlen(test_string));
  memcpy(string_data, test_string, strlen(test_string));
  assert_ptr_not_equal(string_data, NULL);
  cbor_string_set_handle(string, string_data, strlen(test_string));

  assert_ptr_equal(cbor_string_handle(string), string_data);
  assert_size_equal(cbor_string_length(string), 7);
  assert_size_equal(cbor_string_codepoint_count(string), 0);

  cbor_decref(&string);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_empty_string),
      cmocka_unit_test(test_short_string),
      cmocka_unit_test(test_short_multibyte_string),
      cmocka_unit_test(test_int8_string),
      cmocka_unit_test(test_int16_string),
      cmocka_unit_test(test_int32_string),
      cmocka_unit_test(test_int64_string),
      cmocka_unit_test(test_short_indef_string),
      cmocka_unit_test(test_invalid_utf),
      cmocka_unit_test(test_inline_creation),
      cmocka_unit_test(test_string_creation),
      cmocka_unit_test(test_string_add_chunk),
      cmocka_unit_test(test_add_chunk_reallocation_overflow),
      cmocka_unit_test(test_set_handle),
      cmocka_unit_test(test_set_handle_multibyte_codepoint),
      cmocka_unit_test(test_set_handle_invalid_utf),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
