/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <stdlib.h>
#include "cbor.h"

void usage(void) {
  printf("Usage: streaming_array <N>\n");
  printf("Prints out serialized array [0, ..., N-1]\n");
  exit(1);
}

#define BUFFER_SIZE 8
unsigned char buffer[BUFFER_SIZE];
FILE* out;

void flush(size_t bytes) {
  if (bytes == 0) exit(1);  // All items should be successfully encoded
  if (fwrite(buffer, sizeof(unsigned char), bytes, out) != bytes) exit(1);
  if (fflush(out)) exit(1);
}

/*
 * Example of using the streaming encoding API to create an array of integers
 * on the fly. Notice that a partial output is produced with every element.
 */
int main(int argc, char* argv[]) {
  if (argc != 2) usage();
  long n = strtol(argv[1], NULL, 10);
  out = freopen(NULL, "wb", stdout);
  if (!out) exit(1);

  // Start an indefinite-length array
  flush(cbor_encode_indef_array_start(buffer, BUFFER_SIZE));
  // Write the array items one by one
  for (size_t i = 0; i < n; i++) {
    flush(cbor_encode_uint32(i, buffer, BUFFER_SIZE));
  }
  // Close the array
  flush(cbor_encode_break(buffer, BUFFER_SIZE));

  if (fclose(out)) exit(1);
}
