/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <string.h>

#include "assertions.h"
#include "cbor.h"

void assert_describe_result(cbor_item_t *item, char *expected_result) {
#if CBOR_PRETTY_PRINTER
  // We know the expected size based on `expected_result`, but read everything
  // in order to get the full actual output in a useful error message.
  const size_t buffer_size = 512;
  FILE *outfile = tmpfile();
  cbor_describe(item, outfile);
  rewind(outfile);
  // Treat string as null-terminated since cmocka doesn't have asserts
  // for explicit length strings.
  char *output = malloc(buffer_size);
  assert_non_null(output);
  size_t output_size = fread(output, sizeof(char), buffer_size, outfile);
  output[output_size] = '\0';
  assert_string_equal(output, expected_result);
  assert_true(feof(outfile));
  free(output);
  fclose(outfile);
#endif
}

static void test_uint(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_build_uint8(42);
  assert_describe_result(item, "[CBOR_TYPE_UINT] Width: 1B, Value: 42\n");
  cbor_decref(&item);
}

static void test_negint(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_build_negint16(40);
  assert_describe_result(item,
                         "[CBOR_TYPE_NEGINT] Width: 2B, Value: -40 - 1\n");
  cbor_decref(&item);
}

static void test_definite_bytestring(void **_CBOR_UNUSED(_state)) {
  unsigned char data[] = {0x01, 0x02, 0x03};
  cbor_item_t *item = cbor_build_bytestring(data, 3);
  assert_describe_result(item,
                         "[CBOR_TYPE_BYTESTRING] Definite, Length: 3B, Data:\n"
                         "    010203\n");
  cbor_decref(&item);
}

static void test_indefinite_bytestring(void **_CBOR_UNUSED(_state)) {
  unsigned char data[] = {0x01, 0x02, 0x03};
  cbor_item_t *item = cbor_new_indefinite_bytestring();
  assert_true(cbor_bytestring_add_chunk(
      item, cbor_move(cbor_build_bytestring(data, 3))));
  assert_true(cbor_bytestring_add_chunk(
      item, cbor_move(cbor_build_bytestring(data, 2))));
  assert_describe_result(
      item,
      "[CBOR_TYPE_BYTESTRING] Indefinite, Chunks: 2, Chunk data:\n"
      "    [CBOR_TYPE_BYTESTRING] Definite, Length: 3B, Data:\n"
      "        010203\n"
      "    [CBOR_TYPE_BYTESTRING] Definite, Length: 2B, Data:\n"
      "        0102\n");
  cbor_decref(&item);
}

static void test_definite_string(void **_CBOR_UNUSED(_state)) {
  char *string = "Hello!";
  cbor_item_t *item = cbor_build_string(string);
  assert_describe_result(
      item,
      "[CBOR_TYPE_STRING] Definite, Length: 6B, Codepoints: 6, Data:\n"
      "    Hello!\n");
  cbor_decref(&item);
}

static void test_indefinite_string(void **_CBOR_UNUSED(_state)) {
  char *string = "Hello!";
  cbor_item_t *item = cbor_new_indefinite_string();
  assert_true(
      cbor_string_add_chunk(item, cbor_move(cbor_build_string(string))));
  assert_true(
      cbor_string_add_chunk(item, cbor_move(cbor_build_string(string))));
  assert_describe_result(
      item,
      "[CBOR_TYPE_STRING] Indefinite, Chunks: 2, Chunk data:\n"
      "    [CBOR_TYPE_STRING] Definite, Length: 6B, Codepoints: 6, Data:\n"
      "        Hello!\n"
      "    [CBOR_TYPE_STRING] Definite, Length: 6B, Codepoints: 6, Data:\n"
      "        Hello!\n");
  cbor_decref(&item);
}

static void test_multibyte_string(void **_CBOR_UNUSED(_state)) {
  // "Štěstíčko" in UTF-8
  char *string = "\xc5\xa0t\xc4\x9bst\xc3\xad\xc4\x8dko";
  cbor_item_t *item = cbor_build_string(string);
  assert_describe_result(
      item,
      "[CBOR_TYPE_STRING] Definite, Length: 13B, Codepoints: 9, Data:\n"
      "    \xc5\xa0t\xc4\x9bst\xc3\xad\xc4\x8dko\n");
  cbor_decref(&item);
}

static void test_definite_array(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_definite_array(2);
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(1))));
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(2))));
  assert_describe_result(item,
                         "[CBOR_TYPE_ARRAY] Definite, Size: 2, Contents:\n"
                         "    [CBOR_TYPE_UINT] Width: 1B, Value: 1\n"
                         "    [CBOR_TYPE_UINT] Width: 1B, Value: 2\n");
  cbor_decref(&item);
}

static void test_indefinite_array(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_array();
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(1))));
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(2))));
  assert_describe_result(item,
                         "[CBOR_TYPE_ARRAY] Indefinite, Size: 2, Contents:\n"
                         "    [CBOR_TYPE_UINT] Width: 1B, Value: 1\n"
                         "    [CBOR_TYPE_UINT] Width: 1B, Value: 2\n");
  cbor_decref(&item);
}

static void test_definite_map(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_definite_map(1);
  assert_true(cbor_map_add(
      item, (struct cbor_pair){.key = cbor_move(cbor_build_uint8(1)),
                               .value = cbor_move(cbor_build_uint8(2))}));
  assert_describe_result(item,
                         "[CBOR_TYPE_MAP] Definite, Size: 1, Contents:\n"
                         "    Map entry 0\n"
                         "        [CBOR_TYPE_UINT] Width: 1B, Value: 1\n"
                         "        [CBOR_TYPE_UINT] Width: 1B, Value: 2\n");
  cbor_decref(&item);
}

static void test_indefinite_map(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_map();
  assert_true(cbor_map_add(
      item, (struct cbor_pair){.key = cbor_move(cbor_build_uint8(1)),
                               .value = cbor_move(cbor_build_uint8(2))}));
  assert_describe_result(item,
                         "[CBOR_TYPE_MAP] Indefinite, Size: 1, Contents:\n"
                         "    Map entry 0\n"
                         "        [CBOR_TYPE_UINT] Width: 1B, Value: 1\n"
                         "        [CBOR_TYPE_UINT] Width: 1B, Value: 2\n");
  cbor_decref(&item);
}

static void test_tag(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_build_tag(42, cbor_move(cbor_build_uint8(1)));
  assert_describe_result(item,
                         "[CBOR_TYPE_TAG] Value: 42\n"
                         "    [CBOR_TYPE_UINT] Width: 1B, Value: 1\n");
  cbor_decref(&item);
}

static void test_floats(void **_CBOR_UNUSED(_state)) {
  cbor_item_t *item = cbor_new_indefinite_array();
  assert_true(cbor_array_push(item, cbor_move(cbor_build_bool(true))));
  assert_true(
      cbor_array_push(item, cbor_move(cbor_build_ctrl(CBOR_CTRL_UNDEF))));
  assert_true(
      cbor_array_push(item, cbor_move(cbor_build_ctrl(CBOR_CTRL_NULL))));
  assert_true(cbor_array_push(item, cbor_move(cbor_build_ctrl(24))));
  assert_true(cbor_array_push(item, cbor_move(cbor_build_float4(3.14f))));
  assert_describe_result(
      item,
      "[CBOR_TYPE_ARRAY] Indefinite, Size: 5, Contents:\n"
      "    [CBOR_TYPE_FLOAT_CTRL] Bool: true\n"
      "    [CBOR_TYPE_FLOAT_CTRL] Undefined\n"
      "    [CBOR_TYPE_FLOAT_CTRL] Null\n"
      "    [CBOR_TYPE_FLOAT_CTRL] Simple value: 24\n"
      "    [CBOR_TYPE_FLOAT_CTRL] Width: 4B, Value: 3.140000\n");
  cbor_decref(&item);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_uint),
      cmocka_unit_test(test_negint),
      cmocka_unit_test(test_definite_bytestring),
      cmocka_unit_test(test_indefinite_bytestring),
      cmocka_unit_test(test_definite_string),
      cmocka_unit_test(test_indefinite_string),
      cmocka_unit_test(test_multibyte_string),
      cmocka_unit_test(test_definite_array),
      cmocka_unit_test(test_indefinite_array),
      cmocka_unit_test(test_definite_map),
      cmocka_unit_test(test_indefinite_map),
      cmocka_unit_test(test_tag),
      cmocka_unit_test(test_floats),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
