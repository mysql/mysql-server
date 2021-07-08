/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef LIBCBOR_STACK_H
#define LIBCBOR_STACK_H

#include "cbor/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Simple stack record for the parser */
struct _cbor_stack_record {
  struct _cbor_stack_record *lower;
  cbor_item_t *item;
  size_t subitems;
};

/** Stack handle - contents and size */
struct _cbor_stack {
  struct _cbor_stack_record *top;
  size_t size;
};

struct _cbor_stack _cbor_stack_init();

void _cbor_stack_pop(struct _cbor_stack *);

struct _cbor_stack_record *_cbor_stack_push(struct _cbor_stack *, cbor_item_t *,
                                            size_t);

#ifdef __cplusplus
}
#endif

#endif  // LIBCBOR_STACK_H
