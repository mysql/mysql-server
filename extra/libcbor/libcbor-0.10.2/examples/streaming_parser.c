/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <string.h>
#include "cbor.h"

#ifdef __GNUC__
#define UNUSED(x) __attribute__((__unused__)) x
#else
#define UNUSED(x) x
#endif

void usage(void) {
  printf("Usage: streaming_parser [input file]\n");
  exit(1);
}

/*
 * Illustrates how one might skim through a map (which is assumed to have
 * string keys and values only), looking for the value of a specific key
 *
 * Use the examples/data/map.cbor input to test this.
 */

const char* key = "a secret key";
bool key_found = false;

void find_string(void* UNUSED(_ctx), cbor_data buffer, uint64_t len) {
  if (key_found) {
    printf("Found the value: %.*s\n", (int)len, buffer);
    key_found = false;
  } else if (len == strlen(key)) {
    key_found = (memcmp(key, buffer, len) == 0);
  }
}

int main(int argc, char* argv[]) {
  if (argc != 2) usage();
  FILE* f = fopen(argv[1], "rb");
  if (f == NULL) usage();
  fseek(f, 0, SEEK_END);
  size_t length = (size_t)ftell(f);
  fseek(f, 0, SEEK_SET);
  unsigned char* buffer = malloc(length);
  fread(buffer, length, 1, f);

  struct cbor_callbacks callbacks = cbor_empty_callbacks;
  struct cbor_decoder_result decode_result;
  size_t bytes_read = 0;
  callbacks.string = find_string;
  while (bytes_read < length) {
    decode_result = cbor_stream_decode(buffer + bytes_read, length - bytes_read,
                                       &callbacks, NULL);
    bytes_read += decode_result.read;
  }

  free(buffer);
  fclose(f);
}
