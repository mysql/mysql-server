/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */
#include "assertions.h"
#include "cbor.h"
#include "cbor/internal/builder_callbacks.h"
#include "cbor/internal/stack.h"
#include "test_allocator.h"

unsigned char data[] = {
    0x93, 0x01, 0x19, 0x01, 0x01, 0x1A, 0x00, 0x01, 0x05, 0xB8, 0x1B, 0x00,
    0x00, 0x00, 0x01, 0x8F, 0x5A, 0xE8, 0xB8, 0x20, 0x39, 0x01, 0x00, 0x3A,
    0x00, 0x01, 0x05, 0xB7, 0x3B, 0x00, 0x00, 0x00, 0x01, 0x8F, 0x5A, 0xE8,
    0xB7, 0x5F, 0x41, 0x01, 0x41, 0x02, 0xFF, 0x7F, 0x61, 0x61, 0x61, 0x62,
    0xFF, 0x9F, 0xFF, 0xA1, 0x61, 0x61, 0x61, 0x62, 0xC0, 0xBF, 0xFF, 0xF9,
    0x3C, 0x00, 0xFA, 0x47, 0xC3, 0x50, 0x00, 0xFB, 0x7E, 0x37, 0xE4, 0x3C,
    0x88, 0x00, 0x75, 0x9C, 0xF6, 0xF7, 0xF5};

/* Exercise the default callbacks */
static void test_default_callbacks(void** _CBOR_UNUSED(_state)) {
  size_t read = 0;
  while (read < 79) {
    struct cbor_decoder_result result =
        cbor_stream_decode(data + read, 79 - read, &cbor_empty_callbacks, NULL);
    read += result.read;
  }
}

unsigned char bytestring_data[] = {0x01, 0x02, 0x03};
static void test_builder_byte_string_callback_append(
    void** _CBOR_UNUSED(_state)) {
  struct _cbor_stack stack = _cbor_stack_init();
  assert_non_null(
      _cbor_stack_push(&stack, cbor_new_indefinite_bytestring(), 0));
  struct _cbor_decoder_context context = {
      .creation_failed = false,
      .syntax_error = false,
      .root = NULL,
      .stack = &stack,
  };

  cbor_builder_byte_string_callback(&context, bytestring_data, 3);

  assert_false(context.creation_failed);
  assert_false(context.syntax_error);
  assert_size_equal(context.stack->size, 1);

  cbor_item_t* bytestring = stack.top->item;
  assert_size_equal(cbor_refcount(bytestring), 1);
  assert_true(cbor_typeof(bytestring) == CBOR_TYPE_BYTESTRING);
  assert_true(cbor_isa_bytestring(bytestring));
  assert_size_equal(cbor_bytestring_length(bytestring), 0);
  assert_true(cbor_bytestring_is_indefinite(bytestring));
  assert_size_equal(cbor_bytestring_chunk_count(bytestring), 1);

  cbor_item_t* chunk = cbor_bytestring_chunks_handle(bytestring)[0];
  assert_size_equal(cbor_refcount(chunk), 1);
  assert_true(cbor_typeof(bytestring) == CBOR_TYPE_BYTESTRING);
  assert_true(cbor_isa_bytestring(chunk));
  assert_true(cbor_bytestring_is_definite(chunk));
  assert_size_equal(cbor_bytestring_length(chunk), 3);
  assert_memory_equal(cbor_bytestring_handle(chunk), bytestring_data, 3);
  // Data is copied
  assert_ptr_not_equal(cbor_bytestring_handle(chunk), bytestring_data);

  cbor_decref(&bytestring);
  _cbor_stack_pop(&stack);
}

static void test_builder_byte_string_callback_append_alloc_failure(
    void** _CBOR_UNUSED(_state)) {
  struct _cbor_stack stack = _cbor_stack_init();
  assert_non_null(
      _cbor_stack_push(&stack, cbor_new_indefinite_bytestring(), 0));
  struct _cbor_decoder_context context = {
      .creation_failed = false,
      .syntax_error = false,
      .root = NULL,
      .stack = &stack,
  };

  WITH_FAILING_MALLOC(
      { cbor_builder_byte_string_callback(&context, bytestring_data, 3); });

  assert_true(context.creation_failed);
  assert_false(context.syntax_error);
  assert_size_equal(context.stack->size, 1);

  // The stack remains unchanged
  cbor_item_t* bytestring = stack.top->item;
  assert_size_equal(cbor_refcount(bytestring), 1);
  assert_true(cbor_typeof(bytestring) == CBOR_TYPE_BYTESTRING);
  assert_true(cbor_isa_bytestring(bytestring));
  assert_size_equal(cbor_bytestring_length(bytestring), 0);
  assert_true(cbor_bytestring_is_indefinite(bytestring));
  assert_size_equal(cbor_bytestring_chunk_count(bytestring), 0);

  cbor_decref(&bytestring);
  _cbor_stack_pop(&stack);
}

static void test_builder_byte_string_callback_append_item_alloc_failure(
    void** _CBOR_UNUSED(_state)) {
  struct _cbor_stack stack = _cbor_stack_init();
  assert_non_null(
      _cbor_stack_push(&stack, cbor_new_indefinite_bytestring(), 0));
  struct _cbor_decoder_context context = {
      .creation_failed = false,
      .syntax_error = false,
      .root = NULL,
      .stack = &stack,
  };

  // Allocate new data block, but fail to allocate a new item with it
  WITH_MOCK_MALLOC(
      { cbor_builder_byte_string_callback(&context, bytestring_data, 3); }, 2,
      MALLOC, MALLOC_FAIL);

  assert_true(context.creation_failed);
  assert_false(context.syntax_error);
  assert_size_equal(context.stack->size, 1);

  // The stack remains unchanged
  cbor_item_t* bytestring = stack.top->item;
  assert_size_equal(cbor_refcount(bytestring), 1);
  assert_true(cbor_typeof(bytestring) == CBOR_TYPE_BYTESTRING);
  assert_true(cbor_isa_bytestring(bytestring));
  assert_size_equal(cbor_bytestring_length(bytestring), 0);
  assert_true(cbor_bytestring_is_indefinite(bytestring));
  assert_size_equal(cbor_bytestring_chunk_count(bytestring), 0);

  cbor_decref(&bytestring);
  _cbor_stack_pop(&stack);
}

static void test_builder_byte_string_callback_append_parent_alloc_failure(
    void** _CBOR_UNUSED(_state)) {
  struct _cbor_stack stack = _cbor_stack_init();
  assert_non_null(
      _cbor_stack_push(&stack, cbor_new_indefinite_bytestring(), 0));
  struct _cbor_decoder_context context = {
      .creation_failed = false,
      .syntax_error = false,
      .root = NULL,
      .stack = &stack,
  };

  // Allocate new item, but fail to push it into the parent on the stack
  WITH_MOCK_MALLOC(
      { cbor_builder_byte_string_callback(&context, bytestring_data, 3); }, 3,
      MALLOC, MALLOC, REALLOC_FAIL);

  assert_true(context.creation_failed);
  assert_false(context.syntax_error);
  assert_size_equal(context.stack->size, 1);

  // The stack remains unchanged
  cbor_item_t* bytestring = stack.top->item;
  assert_size_equal(cbor_refcount(bytestring), 1);
  assert_true(cbor_typeof(bytestring) == CBOR_TYPE_BYTESTRING);
  assert_true(cbor_isa_bytestring(bytestring));
  assert_size_equal(cbor_bytestring_length(bytestring), 0);
  assert_true(cbor_bytestring_is_indefinite(bytestring));
  assert_size_equal(cbor_bytestring_chunk_count(bytestring), 0);

  cbor_decref(&bytestring);
  _cbor_stack_pop(&stack);
}

unsigned char string_data[] = {0x61, 0x62, 0x63};
static void test_builder_string_callback_append(void** _CBOR_UNUSED(_state)) {
  struct _cbor_stack stack = _cbor_stack_init();
  assert_non_null(_cbor_stack_push(&stack, cbor_new_indefinite_string(), 0));
  struct _cbor_decoder_context context = {
      .creation_failed = false,
      .syntax_error = false,
      .root = NULL,
      .stack = &stack,
  };

  cbor_builder_string_callback(&context, string_data, 3);

  assert_false(context.creation_failed);
  assert_false(context.syntax_error);
  assert_size_equal(context.stack->size, 1);

  cbor_item_t* string = stack.top->item;
  assert_size_equal(cbor_refcount(string), 1);
  assert_true(cbor_isa_string(string));
  assert_size_equal(cbor_string_length(string), 0);
  assert_true(cbor_string_is_indefinite(string));
  assert_size_equal(cbor_string_chunk_count(string), 1);

  cbor_item_t* chunk = cbor_string_chunks_handle(string)[0];
  assert_size_equal(cbor_refcount(chunk), 1);
  assert_true(cbor_isa_string(chunk));
  assert_true(cbor_string_is_definite(chunk));
  assert_size_equal(cbor_string_length(chunk), 3);
  assert_memory_equal(cbor_string_handle(chunk), "abc", 3);
  // Data is copied
  assert_ptr_not_equal(cbor_string_handle(chunk), string_data);

  cbor_decref(&string);
  _cbor_stack_pop(&stack);
}

static void test_builder_string_callback_append_alloc_failure(
    void** _CBOR_UNUSED(_state)) {
  struct _cbor_stack stack = _cbor_stack_init();
  assert_non_null(_cbor_stack_push(&stack, cbor_new_indefinite_string(), 0));
  struct _cbor_decoder_context context = {
      .creation_failed = false,
      .syntax_error = false,
      .root = NULL,
      .stack = &stack,
  };

  WITH_FAILING_MALLOC(
      { cbor_builder_string_callback(&context, string_data, 3); });

  assert_true(context.creation_failed);
  assert_false(context.syntax_error);
  assert_size_equal(context.stack->size, 1);

  // The stack remains unchanged
  cbor_item_t* string = stack.top->item;
  assert_size_equal(cbor_refcount(string), 1);
  assert_true(cbor_typeof(string) == CBOR_TYPE_STRING);
  assert_true(cbor_isa_string(string));
  assert_size_equal(cbor_string_length(string), 0);
  assert_true(cbor_string_is_indefinite(string));
  assert_size_equal(cbor_string_chunk_count(string), 0);

  cbor_decref(&string);
  _cbor_stack_pop(&stack);
}

static void test_builder_string_callback_append_item_alloc_failure(
    void** _CBOR_UNUSED(_state)) {
  struct _cbor_stack stack = _cbor_stack_init();
  assert_non_null(_cbor_stack_push(&stack, cbor_new_indefinite_string(), 0));
  struct _cbor_decoder_context context = {
      .creation_failed = false,
      .syntax_error = false,
      .root = NULL,
      .stack = &stack,
  };

  // Allocate new data block, but fail to allocate a new item with it
  WITH_MOCK_MALLOC({ cbor_builder_string_callback(&context, string_data, 3); },
                   2, MALLOC, MALLOC_FAIL);

  assert_true(context.creation_failed);
  assert_false(context.syntax_error);
  assert_size_equal(context.stack->size, 1);

  // The stack remains unchanged
  cbor_item_t* string = stack.top->item;
  assert_size_equal(cbor_refcount(string), 1);
  assert_true(cbor_typeof(string) == CBOR_TYPE_STRING);
  assert_true(cbor_isa_string(string));
  assert_size_equal(cbor_string_length(string), 0);
  assert_true(cbor_string_is_indefinite(string));
  assert_size_equal(cbor_string_chunk_count(string), 0);

  cbor_decref(&string);
  _cbor_stack_pop(&stack);
}

static void test_builder_string_callback_append_parent_alloc_failure(
    void** _CBOR_UNUSED(_state)) {
  struct _cbor_stack stack = _cbor_stack_init();
  assert_non_null(_cbor_stack_push(&stack, cbor_new_indefinite_string(), 0));
  struct _cbor_decoder_context context = {
      .creation_failed = false,
      .syntax_error = false,
      .root = NULL,
      .stack = &stack,
  };

  // Allocate new item, but fail to push it into the parent on the stack
  WITH_MOCK_MALLOC({ cbor_builder_string_callback(&context, string_data, 3); },
                   3, MALLOC, MALLOC, REALLOC_FAIL);

  assert_true(context.creation_failed);
  assert_false(context.syntax_error);
  assert_size_equal(context.stack->size, 1);

  // The stack remains unchanged
  cbor_item_t* string = stack.top->item;
  assert_size_equal(cbor_refcount(string), 1);
  assert_true(cbor_typeof(string) == CBOR_TYPE_STRING);
  assert_true(cbor_isa_string(string));
  assert_size_equal(cbor_string_length(string), 0);
  assert_true(cbor_string_is_indefinite(string));
  assert_size_equal(cbor_string_chunk_count(string), 0);

  cbor_decref(&string);
  _cbor_stack_pop(&stack);
}

static void test_append_array_failure(void** _CBOR_UNUSED(_state)) {
  struct _cbor_stack stack = _cbor_stack_init();
  assert_non_null(_cbor_stack_push(&stack, cbor_new_definite_array(0), 0));
  stack.top->subitems = 1;
  struct _cbor_decoder_context context = {
      .creation_failed = false,
      .syntax_error = false,
      .root = NULL,
      .stack = &stack,
  };
  cbor_item_t* item = cbor_build_uint8(42);

  _cbor_builder_append(item, &context);

  assert_true(context.creation_failed);
  assert_false(context.syntax_error);
  assert_size_equal(context.stack->size, 1);

  // The stack remains unchanged
  cbor_item_t* array = stack.top->item;
  assert_size_equal(cbor_refcount(array), 1);
  assert_true(cbor_isa_array(array));
  assert_size_equal(cbor_array_size(array), 0);

  // item free'd by _cbor_builder_append
  cbor_decref(&array);
  _cbor_stack_pop(&stack);
}

static void test_append_map_failure(void** _CBOR_UNUSED(_state)) {
  struct _cbor_stack stack = _cbor_stack_init();
  assert_non_null(
      _cbor_stack_push(&stack, cbor_new_indefinite_map(), /*subitems=*/0));
  struct _cbor_decoder_context context = {
      .creation_failed = false,
      .syntax_error = false,
      .root = NULL,
      .stack = &stack,
  };
  cbor_item_t* item = cbor_build_uint8(42);

  WITH_MOCK_MALLOC({ _cbor_builder_append(item, &context); }, 1, REALLOC_FAIL);

  assert_true(context.creation_failed);
  assert_false(context.syntax_error);
  assert_size_equal(context.stack->size, 1);

  // The stack remains unchanged
  cbor_item_t* map = stack.top->item;
  assert_size_equal(cbor_refcount(map), 1);
  assert_true(cbor_isa_map(map));
  assert_size_equal(cbor_map_size(map), 0);

  // item free'd by _cbor_builder_append
  cbor_decref(&map);
  _cbor_stack_pop(&stack);
}

// Size 1 array start, but we get an indef break
unsigned char invalid_indef_break_data[] = {0x81, 0xFF};
static void test_invalid_indef_break(void** _CBOR_UNUSED(_state)) {
  struct cbor_load_result res;
  cbor_item_t* item = cbor_load(invalid_indef_break_data, 2, &res);

  assert_null(item);
  assert_size_equal(res.read, 2);
  assert_true(res.error.code == CBOR_ERR_SYNTAXERROR);
}

static void test_invalid_state_indef_break(void** _CBOR_UNUSED(_state)) {
  struct _cbor_stack stack = _cbor_stack_init();
  assert_non_null(_cbor_stack_push(&stack, cbor_new_int8(), /*subitems=*/0));
  struct _cbor_decoder_context context = {
      .creation_failed = false,
      .syntax_error = false,
      .root = NULL,
      .stack = &stack,
  };

  cbor_builder_indef_break_callback(&context);

  assert_false(context.creation_failed);
  assert_true(context.syntax_error);
  assert_size_equal(context.stack->size, 1);
  // The stack remains unchanged
  cbor_item_t* small_int = stack.top->item;
  assert_size_equal(cbor_refcount(small_int), 1);
  assert_true(cbor_isa_uint(small_int));

  cbor_decref(&small_int);
  _cbor_stack_pop(&stack);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_default_callbacks),
      cmocka_unit_test(test_builder_byte_string_callback_append),
      cmocka_unit_test(test_builder_byte_string_callback_append_alloc_failure),
      cmocka_unit_test(
          test_builder_byte_string_callback_append_item_alloc_failure),
      cmocka_unit_test(
          test_builder_byte_string_callback_append_parent_alloc_failure),
      cmocka_unit_test(test_builder_string_callback_append),
      cmocka_unit_test(test_builder_string_callback_append_alloc_failure),
      cmocka_unit_test(test_builder_string_callback_append_item_alloc_failure),
      cmocka_unit_test(
          test_builder_string_callback_append_parent_alloc_failure),
      cmocka_unit_test(test_append_array_failure),
      cmocka_unit_test(test_append_map_failure),
      cmocka_unit_test(test_invalid_indef_break),
      cmocka_unit_test(test_invalid_state_indef_break),
  };

  cmocka_run_group_tests(tests, NULL, NULL);
}
