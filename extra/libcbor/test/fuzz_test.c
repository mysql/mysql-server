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

#include <time.h>
#include "cbor.h"

#ifdef HUGE_FUZZ
#define ROUNDS 65536ULL
#define MAXLEN 131072ULL
#else
#define ROUNDS 256ULL
#define MAXLEN 2048ULL
#endif

#ifdef PRINT_FUZZ
static void printmem(const unsigned char *ptr, size_t length) {
  for (size_t i = 0; i < length; i++) printf("%02X", ptr[i]);
  printf("\n");
}
#endif

unsigned seed;

#if CBOR_CUSTOM_ALLOC
void *mock_malloc(size_t size) {
  if (size > (1 << 19))
    return NULL;
  else
    return malloc(size);
}
#endif

static void run_round() {
  cbor_item_t *item;
  struct cbor_load_result res;

  size_t length = rand() % MAXLEN + 1;
  unsigned char *data = malloc(length);
  for (size_t i = 0; i < length; i++) {
    data[i] = rand() % 0xFF;
  }

#ifdef PRINT_FUZZ
  printmem(data, length);
#endif

  item = cbor_load(data, length, &res);

  if (res.error.code == CBOR_ERR_NONE) cbor_decref(&item);
  /* Otherwise there should be nothing left behind by the decoder */

  free(data);
}

static void fuzz(void **state) {
#if CBOR_CUSTOM_ALLOC
  cbor_set_allocs(mock_malloc, realloc, free);
#endif
  printf("Fuzzing %llu rounds of up to %llu bytes with seed %u\n", ROUNDS,
         MAXLEN, seed);
  srand(seed);

  for (size_t i = 0; i < ROUNDS; i++) run_round();

  printf("Successfully fuzzed through %llu kB of data\n",
         (ROUNDS * MAXLEN) / 1024);
}

int main(int argc, char *argv[]) {
  if (argc > 1)
    seed = (unsigned)strtoul(argv[1], NULL, 10);
  else
    seed = (unsigned)time(NULL);

  const struct CMUnitTest tests[] = {cmocka_unit_test(fuzz)};
  return cmocka_run_group_tests(tests, NULL, NULL);
}
