/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include "cbor.h"

/*
 * Illustrates how to use the contiguous storage of nested items with
 * standard library functions.
 */

int compareUint(const void *a, const void *b) {
  uint8_t av = cbor_get_uint8(*(cbor_item_t **)a),
          bv = cbor_get_uint8(*(cbor_item_t **)b);

  if (av < bv)
    return -1;
  else if (av == bv)
    return 0;
  else
    return 1;
}

int main(void) {
  cbor_item_t *array = cbor_new_definite_array(4);
  bool success = cbor_array_push(array, cbor_move(cbor_build_uint8(4)));
  success &= cbor_array_push(array, cbor_move(cbor_build_uint8(3)));
  success &= cbor_array_push(array, cbor_move(cbor_build_uint8(1)));
  success &= cbor_array_push(array, cbor_move(cbor_build_uint8(2)));
  if (!success) return 1;

  qsort(cbor_array_handle(array), cbor_array_size(array), sizeof(cbor_item_t *),
        compareUint);

  cbor_describe(array, stdout);
  fflush(stdout);
}
