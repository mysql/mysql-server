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

unsigned char buffer[512];

static void test_embedded_tag(void **state) {
  assert_int_equal(1, cbor_encode_tag(1, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xC1}), 1);
}

static void test_tag(void **state) {
  assert_int_equal(5, cbor_encode_tag(1000000, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xDA, 0x00, 0x0F, 0x42, 0x40}),
                      5);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_embedded_tag),
      cmocka_unit_test(test_tag),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
