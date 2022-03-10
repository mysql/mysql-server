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

#include <stdio.h>
#include "cbor.h"

unsigned char data[] = {0x8B, 0x01, 0x20, 0x5F, 0x41, 0x01, 0x41, 0x02,
                        0xFF, 0x7F, 0x61, 0x61, 0x61, 0x62, 0xFF, 0x9F,
                        0xFF, 0xA1, 0x61, 0x61, 0x61, 0x62, 0xC0, 0xBF,
                        0xFF, 0xFB, 0x40, 0x09, 0x1E, 0xB8, 0x51, 0xEB,
                        0x85, 0x1F, 0xF6, 0xF7, 0xF5};

static void test_pretty_printer(void **state) {
#if CBOR_PRETTY_PRINTER
  FILE *outfile = tmpfile();
  struct cbor_load_result res;
  cbor_item_t *item = cbor_load(data, 37, &res);
  cbor_describe(item, outfile);
  cbor_decref(&item);

  item = cbor_new_ctrl();
  cbor_set_ctrl(item, 1);
  cbor_describe(item, outfile);
  cbor_decref(&item);

  fclose(outfile);
#endif
}

int main(void) {
  const struct CMUnitTest tests[] = {cmocka_unit_test(test_pretty_printer)};
  return cmocka_run_group_tests(tests, NULL, NULL);
}
