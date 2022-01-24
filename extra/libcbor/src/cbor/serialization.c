/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "serialization.h"
#include <string.h>
#include "cbor/arrays.h"
#include "cbor/bytestrings.h"
#include "cbor/floats_ctrls.h"
#include "cbor/ints.h"
#include "cbor/maps.h"
#include "cbor/strings.h"
#include "cbor/tags.h"
#include "encoding.h"
#include "internal/memory_utils.h"

size_t cbor_serialize(const cbor_item_t *item, unsigned char *buffer,
                      size_t buffer_size) {
  switch (cbor_typeof(item)) {
    case CBOR_TYPE_UINT:
      return cbor_serialize_uint(item, buffer, buffer_size);
    case CBOR_TYPE_NEGINT:
      return cbor_serialize_negint(item, buffer, buffer_size);
    case CBOR_TYPE_BYTESTRING:
      return cbor_serialize_bytestring(item, buffer, buffer_size);
    case CBOR_TYPE_STRING:
      return cbor_serialize_string(item, buffer, buffer_size);
    case CBOR_TYPE_ARRAY:
      return cbor_serialize_array(item, buffer, buffer_size);
    case CBOR_TYPE_MAP:
      return cbor_serialize_map(item, buffer, buffer_size);
    case CBOR_TYPE_TAG:
      return cbor_serialize_tag(item, buffer, buffer_size);
    case CBOR_TYPE_FLOAT_CTRL:
      return cbor_serialize_float_ctrl(item, buffer, buffer_size);
    default:
      return 0;
  }
}

size_t cbor_serialize_alloc(const cbor_item_t *item, unsigned char **buffer,
                            size_t *buffer_size) {
  size_t bfr_size = 32;
  unsigned char *bfr = _CBOR_MALLOC(bfr_size), *tmp_bfr;
  if (bfr == NULL) {
    return 0;
  }

  size_t written;

  /* This is waaay too optimistic - figure out something smarter (eventually) */
  while ((written = cbor_serialize(item, bfr, bfr_size)) == 0) {
    if (!_cbor_safe_to_multiply(CBOR_BUFFER_GROWTH, bfr_size)) {
      _CBOR_FREE(bfr);
      return 0;
    }

    tmp_bfr = _CBOR_REALLOC(bfr, bfr_size *= 2);

    if (tmp_bfr == NULL) {
      _CBOR_FREE(bfr);
      return 0;
    }
    bfr = tmp_bfr;
  }
  *buffer = bfr;
  *buffer_size = bfr_size;
  return written;
}

size_t cbor_serialize_uint(const cbor_item_t *item, unsigned char *buffer,
                           size_t buffer_size) {
  assert(cbor_isa_uint(item));
  switch (cbor_int_get_width(item)) {
    case CBOR_INT_8:
      return cbor_encode_uint8(cbor_get_uint8(item), buffer, buffer_size);
    case CBOR_INT_16:
      return cbor_encode_uint16(cbor_get_uint16(item), buffer, buffer_size);
    case CBOR_INT_32:
      return cbor_encode_uint32(cbor_get_uint32(item), buffer, buffer_size);
    case CBOR_INT_64:
      return cbor_encode_uint64(cbor_get_uint64(item), buffer, buffer_size);
    default:
      return 0;
  }
}

size_t cbor_serialize_negint(const cbor_item_t *item, unsigned char *buffer,
                             size_t buffer_size) {
  assert(cbor_isa_negint(item));
  switch (cbor_int_get_width(item)) {
    case CBOR_INT_8:
      return cbor_encode_negint8(cbor_get_uint8(item), buffer, buffer_size);
    case CBOR_INT_16:
      return cbor_encode_negint16(cbor_get_uint16(item), buffer, buffer_size);
    case CBOR_INT_32:
      return cbor_encode_negint32(cbor_get_uint32(item), buffer, buffer_size);
    case CBOR_INT_64:
      return cbor_encode_negint64(cbor_get_uint64(item), buffer, buffer_size);
    default:
      return 0;
  }
}

size_t cbor_serialize_bytestring(const cbor_item_t *item, unsigned char *buffer,
                                 size_t buffer_size) {
  assert(cbor_isa_bytestring(item));
  if (cbor_bytestring_is_definite(item)) {
    size_t length = cbor_bytestring_length(item);
    size_t written = cbor_encode_bytestring_start(length, buffer, buffer_size);
    if (written && (buffer_size - written >= length)) {
      memcpy(buffer + written, cbor_bytestring_handle(item), length);
      return written + length;
    } else
      return 0;
  } else {
    assert(cbor_bytestring_is_indefinite(item));
    size_t chunk_count = cbor_bytestring_chunk_count(item);
    size_t written = cbor_encode_indef_bytestring_start(buffer, buffer_size);

    if (written == 0) return 0;

    cbor_item_t **chunks = cbor_bytestring_chunks_handle(item);
    for (size_t i = 0; i < chunk_count; i++) {
      size_t chunk_written = cbor_serialize_bytestring(
          chunks[i], buffer + written, buffer_size - written);
      if (chunk_written == 0)
        return 0;
      else
        written += chunk_written;
    }
    if (cbor_encode_break(buffer + written, buffer_size - written) > 0)
      return written + 1;
    else
      return 0;
  }
}

size_t cbor_serialize_string(const cbor_item_t *item, unsigned char *buffer,
                             size_t buffer_size) {
  assert(cbor_isa_string(item));
  if (cbor_string_is_definite(item)) {
    size_t length = cbor_string_length(item);
    size_t written = cbor_encode_string_start(length, buffer, buffer_size);
    if (written && (buffer_size - written >= length)) {
      memcpy(buffer + written, cbor_string_handle(item), length);
      return written + length;
    } else
      return 0;
  } else {
    assert(cbor_string_is_indefinite(item));
    size_t chunk_count = cbor_string_chunk_count(item);
    size_t written = cbor_encode_indef_string_start(buffer, buffer_size);

    if (written == 0) return 0;

    cbor_item_t **chunks = cbor_string_chunks_handle(item);
    for (size_t i = 0; i < chunk_count; i++) {
      size_t chunk_written = cbor_serialize_string(chunks[i], buffer + written,
                                                   buffer_size - written);
      if (chunk_written == 0)
        return 0;
      else
        written += chunk_written;
    }
    if (cbor_encode_break(buffer + written, buffer_size - written) > 0)
      return written + 1;
    else
      return 0;
  }
}

size_t cbor_serialize_array(const cbor_item_t *item, unsigned char *buffer,
                            size_t buffer_size) {
  assert(cbor_isa_array(item));
  size_t size = cbor_array_size(item), written = 0;
  cbor_item_t **handle = cbor_array_handle(item);
  if (cbor_array_is_definite(item)) {
    written = cbor_encode_array_start(size, buffer, buffer_size);
  } else {
    assert(cbor_array_is_indefinite(item));
    written = cbor_encode_indef_array_start(buffer, buffer_size);
  }
  if (written == 0) return 0;

  size_t item_written;
  for (size_t i = 0; i < size; i++) {
    item_written =
        cbor_serialize(*(handle++), buffer + written, buffer_size - written);
    if (item_written == 0)
      return 0;
    else
      written += item_written;
  }

  if (cbor_array_is_definite(item)) {
    return written;
  } else {
    assert(cbor_array_is_indefinite(item));
    item_written = cbor_encode_break(buffer + written, buffer_size - written);
    if (item_written == 0)
      return 0;
    else
      return written + 1;
  }
}

size_t cbor_serialize_map(const cbor_item_t *item, unsigned char *buffer,
                          size_t buffer_size) {
  assert(cbor_isa_map(item));
  size_t size = cbor_map_size(item), written = 0;
  struct cbor_pair *handle = cbor_map_handle(item);

  if (cbor_map_is_definite(item)) {
    written = cbor_encode_map_start(size, buffer, buffer_size);
  } else {
    assert(cbor_map_is_indefinite(item));
    written = cbor_encode_indef_map_start(buffer, buffer_size);
  }
  if (written == 0) return 0;

  size_t item_written;
  for (size_t i = 0; i < size; i++) {
    item_written =
        cbor_serialize(handle->key, buffer + written, buffer_size - written);
    if (item_written == 0)
      return 0;
    else
      written += item_written;
    item_written = cbor_serialize((handle++)->value, buffer + written,
                                  buffer_size - written);
    if (item_written == 0)
      return 0;
    else
      written += item_written;
  }

  if (cbor_map_is_definite(item)) {
    return written;
  } else {
    assert(cbor_map_is_indefinite(item));
    item_written = cbor_encode_break(buffer + written, buffer_size - written);
    if (item_written == 0)
      return 0;
    else
      return written + 1;
  }
}

size_t cbor_serialize_tag(const cbor_item_t *item, unsigned char *buffer,
                          size_t buffer_size) {
  assert(cbor_isa_tag(item));
  size_t written = cbor_encode_tag(cbor_tag_value(item), buffer, buffer_size);
  if (written == 0) return 0;

  size_t item_written = cbor_serialize(cbor_move(cbor_tag_item(item)),
                                       buffer + written, buffer_size - written);
  if (item_written == 0)
    return 0;
  else
    return written + item_written;
}

size_t cbor_serialize_float_ctrl(const cbor_item_t *item, unsigned char *buffer,
                                 size_t buffer_size) {
  assert(cbor_isa_float_ctrl(item));
  switch (cbor_float_get_width(item)) {
    case CBOR_FLOAT_0:
      /* CTRL - special treatment */
      return cbor_encode_ctrl(cbor_ctrl_value(item), buffer, buffer_size);
    case CBOR_FLOAT_16:
      return cbor_encode_half(cbor_float_get_float2(item), buffer, buffer_size);
    case CBOR_FLOAT_32:
      return cbor_encode_single(cbor_float_get_float4(item), buffer,
                                buffer_size);
    case CBOR_FLOAT_64:
      return cbor_encode_double(cbor_float_get_float8(item), buffer,
                                buffer_size);
  }

  /* Should never happen - make the compiler happy */
  return 0;
}
