/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef LIBCBOR_SERIALIZATION_H
#define LIBCBOR_SERIALIZATION_H

#include "cbor/cbor_export.h"
#include "cbor/common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ============================================================================
 * High level encoding
 * ============================================================================
 */

/** Serialize the given item
 *
 * @param item[borrow] A data item
 * @param buffer Buffer to serialize to
 * @param buffer_size Size of the \p buffer
 * @return Length of the result. 0 on failure.
 */
CBOR_EXPORT size_t cbor_serialize(const cbor_item_t *item,
                                  cbor_mutable_data buffer, size_t buffer_size);

/** Serialize the given item, allocating buffers as needed
 *
 * \rst
 * .. warning:: It is your responsibility to free the buffer using an
 * appropriate ``free`` implementation. \endrst
 *
 * @param item[borrow] A data item
 * @param buffer[out] Buffer containing the result
 * @param buffer_size[out] Size of the \p buffer
 * @return Length of the result. 0 on failure, in which case \p buffer is
 * ``NULL``.
 */
CBOR_EXPORT size_t cbor_serialize_alloc(const cbor_item_t *item,
                                        cbor_mutable_data *buffer,
                                        size_t *buffer_size);

/** Serialize an uint
 *
 * @param item[borrow] A uint
 * @param buffer Buffer to serialize to
 * @param buffer_size Size of the \p buffer
 * @return Length of the result. 0 on failure.
 */
CBOR_EXPORT size_t cbor_serialize_uint(const cbor_item_t *, cbor_mutable_data,
                                       size_t);

/** Serialize a negint
 *
 * @param item[borrow] A neging
 * @param buffer Buffer to serialize to
 * @param buffer_size Size of the \p buffer
 * @return Length of the result. 0 on failure.
 */
CBOR_EXPORT size_t cbor_serialize_negint(const cbor_item_t *, cbor_mutable_data,
                                         size_t);

/** Serialize a bytestring
 *
 * @param item[borrow] A bytestring
 * @param buffer Buffer to serialize to
 * @param buffer_size Size of the \p buffer
 * @return Length of the result. 0 on failure.
 */
CBOR_EXPORT size_t cbor_serialize_bytestring(const cbor_item_t *,
                                             cbor_mutable_data, size_t);

/** Serialize a string
 *
 * @param item[borrow] A string
 * @param buffer Buffer to serialize to
 * @param buffer_size Size of the \p buffer
 * @return Length of the result. 0 on failure.
 */
CBOR_EXPORT size_t cbor_serialize_string(const cbor_item_t *, cbor_mutable_data,
                                         size_t);

/** Serialize an array
 *
 * @param item[borrow] An array
 * @param buffer Buffer to serialize to
 * @param buffer_size Size of the \p buffer
 * @return Length of the result. 0 on failure.
 */
CBOR_EXPORT size_t cbor_serialize_array(const cbor_item_t *, cbor_mutable_data,
                                        size_t);

/** Serialize a map
 *
 * @param item[borrow] A map
 * @param buffer Buffer to serialize to
 * @param buffer_size Size of the \p buffer
 * @return Length of the result. 0 on failure.
 */
CBOR_EXPORT size_t cbor_serialize_map(const cbor_item_t *, cbor_mutable_data,
                                      size_t);

/** Serialize a tag
 *
 * @param item[borrow] A tag
 * @param buffer Buffer to serialize to
 * @param buffer_size Size of the \p buffer
 * @return Length of the result. 0 on failure.
 */
CBOR_EXPORT size_t cbor_serialize_tag(const cbor_item_t *, cbor_mutable_data,
                                      size_t);

/** Serialize a
 *
 * @param item[borrow] A float or ctrl
 * @param buffer Buffer to serialize to
 * @param buffer_size Size of the \p buffer
 * @return Length of the result. 0 on failure.
 */
CBOR_EXPORT size_t cbor_serialize_float_ctrl(const cbor_item_t *,
                                             cbor_mutable_data, size_t);

#ifdef __cplusplus
}
#endif

#endif  // LIBCBOR_SERIALIZATION_H
