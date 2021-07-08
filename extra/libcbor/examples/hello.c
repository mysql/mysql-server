/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include "cbor.h"

int main(int argc, char* argv[]) {
  printf("Hello from libcbor %s\n", CBOR_VERSION);
  printf("Custom allocation support: %s\n", CBOR_CUSTOM_ALLOC ? "yes" : "no");
  printf("Pretty-printer support: %s\n", CBOR_PRETTY_PRINTER ? "yes" : "no");
  printf("Buffer growth factor: %f\n", (float)CBOR_BUFFER_GROWTH);
}
