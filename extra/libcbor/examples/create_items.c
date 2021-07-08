/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include "cbor.h"

int main(int argc, char* argv[]) {
  /* Preallocate the map structure */
  cbor_item_t* root = cbor_new_definite_map(2);
  /* Add the content */
  cbor_map_add(root,
               (struct cbor_pair){
                   .key = cbor_move(cbor_build_string("Is CBOR awesome?")),
                   .value = cbor_move(cbor_build_bool(true))});
  cbor_map_add(root,
               (struct cbor_pair){
                   .key = cbor_move(cbor_build_uint8(42)),
                   .value = cbor_move(cbor_build_string("Is the answer"))});
  /* Output: `length` bytes of data in the `buffer` */
  unsigned char* buffer;
  size_t buffer_size,
      length = cbor_serialize_alloc(root, &buffer, &buffer_size);

  fwrite(buffer, 1, length, stdout);
  free(buffer);

  fflush(stdout);
  cbor_decref(&root);
}
