/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "builder_callbacks.h"
#include <string.h>
#include "../arrays.h"
#include "../bytestrings.h"
#include "../floats_ctrls.h"
#include "../ints.h"
#include "../maps.h"
#include "../strings.h"
#include "../tags.h"
#include "unicode.h"

void _cbor_builder_append(cbor_item_t *item,
                          struct _cbor_decoder_context *ctx) {
  if (ctx->stack->size == 0) {
    /* Top level item */
    ctx->root = item;
  } else {
    /* Part of a bigger structure */
    switch (ctx->stack->top->item->type) {
      case CBOR_TYPE_ARRAY: {
        if (cbor_array_is_definite(ctx->stack->top->item)) {
          /*
           * We don't need an explicit check for whether the item still belongs
           * into this array because if there are extra items, they will cause a
           * syntax error when decoded.
           */
          assert(ctx->stack->top->subitems > 0);
          cbor_array_push(ctx->stack->top->item, item);
          ctx->stack->top->subitems--;
          if (ctx->stack->top->subitems == 0) {
            cbor_item_t *item = ctx->stack->top->item;
            _cbor_stack_pop(ctx->stack);
            _cbor_builder_append(item, ctx);
          }
          cbor_decref(&item);
        } else {
          /* Indefinite array, don't bother with subitems */
          cbor_array_push(ctx->stack->top->item, item);
          cbor_decref(&item);
        }
        break;
      }
      case CBOR_TYPE_MAP: {
        /* We use 0 and 1 subitems to distinguish between keys and values in
         * indefinite items */
        if (ctx->stack->top->subitems % 2) {
          /* Odd record, this is a value */
          _cbor_map_add_value(ctx->stack->top->item, cbor_move(item));
        } else {
          /* Even record, this is a key */
          _cbor_map_add_key(ctx->stack->top->item, cbor_move(item));
        }
        if (cbor_map_is_definite(ctx->stack->top->item)) {
          ctx->stack->top->subitems--;
          if (ctx->stack->top->subitems == 0) {
            cbor_item_t *item = ctx->stack->top->item;
            _cbor_stack_pop(ctx->stack);
            _cbor_builder_append(item, ctx);
          }
        } else {
          ctx->stack->top->subitems ^=
              1; /* Flip the indicator for indefinite items */
        }
        break;
      }
      case CBOR_TYPE_TAG: {
        assert(ctx->stack->top->subitems == 1);
        cbor_tag_set_item(ctx->stack->top->item, item);
        cbor_decref(&item); /* Give up on our reference */
        cbor_item_t *item = ctx->stack->top->item;
        _cbor_stack_pop(ctx->stack);
        _cbor_builder_append(item, ctx);
        break;
      }
      default: {
        cbor_decref(&item);
        ctx->syntax_error = true;
      }
    }
  }
}

#define CHECK_RES(ctx, res)        \
  do {                             \
    if (res == NULL) {             \
      ctx->creation_failed = true; \
      return;                      \
    }                              \
  } while (0)

#define PUSH_CTX_STACK(ctx, res, subitems)                     \
  do {                                                         \
    if (_cbor_stack_push(ctx->stack, res, subitems) == NULL) { \
      cbor_decref(&res);                                       \
      ctx->creation_failed = true;                             \
    }                                                          \
  } while (0)

void cbor_builder_uint8_callback(void *context, uint8_t value) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_int8();
  CHECK_RES(ctx, res);
  cbor_mark_uint(res);
  cbor_set_uint8(res, value);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_uint16_callback(void *context, uint16_t value) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_int16();
  CHECK_RES(ctx, res);
  cbor_mark_uint(res);
  cbor_set_uint16(res, value);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_uint32_callback(void *context, uint32_t value) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_int32();
  CHECK_RES(ctx, res);
  cbor_mark_uint(res);
  cbor_set_uint32(res, value);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_uint64_callback(void *context, uint64_t value) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_int64();
  CHECK_RES(ctx, res);
  cbor_mark_uint(res);
  cbor_set_uint64(res, value);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_negint8_callback(void *context, uint8_t value) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_int8();
  CHECK_RES(ctx, res);
  cbor_mark_negint(res);
  cbor_set_uint8(res, value);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_negint16_callback(void *context, uint16_t value) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_int16();
  cbor_mark_negint(res);
  cbor_set_uint16(res, value);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_negint32_callback(void *context, uint32_t value) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_int32();
  CHECK_RES(ctx, res);
  cbor_mark_negint(res);
  cbor_set_uint32(res, value);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_negint64_callback(void *context, uint64_t value) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_int64();
  CHECK_RES(ctx, res);
  cbor_mark_negint(res);
  cbor_set_uint64(res, value);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_byte_string_callback(void *context, cbor_data data,
                                       size_t length) {
  struct _cbor_decoder_context *ctx = context;
  unsigned char *new_handle = _CBOR_MALLOC(length);
  if (new_handle == NULL) {
    ctx->creation_failed = true;
    return;
  }

  memcpy(new_handle, data, length);
  cbor_item_t *res = cbor_new_definite_bytestring();

  if (res == NULL) {
    _CBOR_FREE(new_handle);
    ctx->creation_failed = true;
    return;
  }

  cbor_bytestring_set_handle(res, new_handle, length);

  if (ctx->stack->size > 0 && cbor_isa_bytestring(ctx->stack->top->item)) {
    if (cbor_bytestring_is_indefinite(ctx->stack->top->item)) {
      cbor_bytestring_add_chunk(ctx->stack->top->item, cbor_move(res));
    } else {
      cbor_decref(&res);
      ctx->syntax_error = true;
    }
  } else {
    _cbor_builder_append(res, ctx);
  }
}

void cbor_builder_byte_string_start_callback(void *context) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_indefinite_bytestring();
  CHECK_RES(ctx, res);
  PUSH_CTX_STACK(ctx, res, 0);
}

void cbor_builder_string_callback(void *context, cbor_data data,
                                  size_t length) {
  struct _cbor_decoder_context *ctx = context;
  struct _cbor_unicode_status unicode_status;

  size_t codepoint_count =
      _cbor_unicode_codepoint_count(data, length, &unicode_status);

  if (unicode_status.status == _CBOR_UNICODE_BADCP) {
    ctx->syntax_error = true;
    return;
  }

  unsigned char *new_handle = _CBOR_MALLOC(length);

  if (new_handle == NULL) {
    ctx->creation_failed = true;
    return;
  }

  memcpy(new_handle, data, length);
  cbor_item_t *res = cbor_new_definite_string();
  if (res == NULL) {
    _CBOR_FREE(new_handle);
    ctx->creation_failed = true;
    return;
  }
  cbor_string_set_handle(res, new_handle, length);
  res->metadata.string_metadata.codepoint_count = codepoint_count;

  /* Careful here: order matters */
  if (ctx->stack->size > 0 && cbor_isa_string(ctx->stack->top->item)) {
    if (cbor_string_is_indefinite(ctx->stack->top->item)) {
      cbor_string_add_chunk(ctx->stack->top->item, cbor_move(res));
    } else {
      cbor_decref(&res);
      ctx->syntax_error = true;
    }
  } else {
    _cbor_builder_append(res, ctx);
  }
}

void cbor_builder_string_start_callback(void *context) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_indefinite_string();
  CHECK_RES(ctx, res);
  PUSH_CTX_STACK(ctx, res, 0);
}

void cbor_builder_array_start_callback(void *context, size_t size) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_definite_array(size);
  CHECK_RES(ctx, res);
  if (size > 0) {
    PUSH_CTX_STACK(ctx, res, size);
  } else {
    _cbor_builder_append(res, ctx);
  }
}

void cbor_builder_indef_array_start_callback(void *context) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_indefinite_array();
  CHECK_RES(ctx, res);
  PUSH_CTX_STACK(ctx, res, 0);
}

void cbor_builder_indef_map_start_callback(void *context) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_indefinite_map();
  CHECK_RES(ctx, res);
  PUSH_CTX_STACK(ctx, res, 0);
}

void cbor_builder_map_start_callback(void *context, size_t size) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_definite_map(size);
  CHECK_RES(ctx, res);
  if (size > 0) {
    PUSH_CTX_STACK(ctx, res, size * 2);
  } else {
    _cbor_builder_append(res, ctx);
  }
}

/**
 * Is the (partially constructed) item indefinite?
 */
bool _cbor_is_indefinite(cbor_item_t *item) {
  switch (item->type) {
    case CBOR_TYPE_BYTESTRING:
      return item->metadata.bytestring_metadata.type ==
             _CBOR_METADATA_INDEFINITE;
    case CBOR_TYPE_STRING:
      return item->metadata.string_metadata.type == _CBOR_METADATA_INDEFINITE;
    case CBOR_TYPE_ARRAY:
      return item->metadata.array_metadata.type == _CBOR_METADATA_INDEFINITE;
    case CBOR_TYPE_MAP:
      return item->metadata.map_metadata.type == _CBOR_METADATA_INDEFINITE;
    default:
      return false;
  }
}

void cbor_builder_indef_break_callback(void *context) {
  struct _cbor_decoder_context *ctx = context;
  /* There must be an item to break out of*/
  if (ctx->stack->size > 0) {
    cbor_item_t *item = ctx->stack->top->item;
    if (_cbor_is_indefinite(
            item) && /* Only indefinite items can be terminated by 0xFF */
        /* Special case: we cannot append up if an indefinite map is incomplete
           (we are expecting a value). */
        (item->type != CBOR_TYPE_MAP || ctx->stack->top->subitems % 2 == 0)) {
      _cbor_stack_pop(ctx->stack);
      _cbor_builder_append(item, ctx);
      return;
    }
  }

  ctx->syntax_error = true;
}

void cbor_builder_float2_callback(void *context, float value) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_float2();
  cbor_set_float2(res, value);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_float4_callback(void *context, float value) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_float4();
  CHECK_RES(ctx, res);
  cbor_set_float4(res, value);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_float8_callback(void *context, double value) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_float8();
  CHECK_RES(ctx, res);
  cbor_set_float8(res, value);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_null_callback(void *context) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_null();
  CHECK_RES(ctx, res);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_undefined_callback(void *context) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_undef();
  CHECK_RES(ctx, res);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_boolean_callback(void *context, bool value) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_build_bool(value);
  CHECK_RES(ctx, res);
  _cbor_builder_append(res, ctx);
}

void cbor_builder_tag_callback(void *context, uint64_t value) {
  struct _cbor_decoder_context *ctx = context;
  cbor_item_t *res = cbor_new_tag(value);
  CHECK_RES(ctx, res);
  PUSH_CTX_STACK(ctx, res, 1);
}
