/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <assert.h>
#include <cbor.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fido.h"
#include "mutator_aux.h"

size_t LLVMFuzzerMutate(uint8_t *, size_t, size_t);

static const uint8_t *wire_data_ptr = NULL;
static size_t wire_data_len = 0;

size_t
xstrlen(const char *s)
{
	if (s == NULL)
		return 0;

	return strlen(s);
}

void
consume(const void *body, size_t len)
{
	const volatile uint8_t *ptr = body;
	volatile uint8_t x = 0;

#ifdef WITH_MSAN
	__msan_check_mem_is_initialized(body, len);
#endif

	while (len--)
		x ^= *ptr++;
}

void
consume_str(const char *str)
{
	consume(str, strlen(str));
}

int
unpack_int(cbor_item_t *item, int *v)
{
	if (cbor_is_int(item) == false ||
	    cbor_int_get_width(item) != CBOR_INT_64)
		return -1;

	if (cbor_isa_uint(item))
		*v = (int)cbor_get_uint64(item);
	else
		*v = (int)(-cbor_get_uint64(item) - 1);

	return 0;
}

int
unpack_string(cbor_item_t *item, char *v)
{
	size_t len;

	if (cbor_isa_bytestring(item) == false ||
	    (len = cbor_bytestring_length(item)) >= MAXSTR)
		return -1;

	memcpy(v, cbor_bytestring_handle(item), len);
	v[len] = '\0';

	return 0;
}

int
unpack_byte(cbor_item_t *item, uint8_t *v)
{
	if (cbor_isa_uint(item) == false ||
	    cbor_int_get_width(item) != CBOR_INT_8)
		return -1;

	*v = cbor_get_uint8(item);

	return 0;
}

int
unpack_blob(cbor_item_t *item, struct blob *v)
{
	if (cbor_isa_bytestring(item) == false ||
	    (v->len = cbor_bytestring_length(item)) > sizeof(v->body))
		return -1;

	memcpy(v->body, cbor_bytestring_handle(item), v->len);

	return 0;
}

cbor_item_t *
pack_int(int v) NO_MSAN
{
	if (v < 0)
		return cbor_build_negint64((uint64_t)(-(int64_t)v - 1));
	else
		return cbor_build_uint64((uint64_t)v);
}

cbor_item_t *
pack_string(const char *v) NO_MSAN
{
	if (strlen(v) >= MAXSTR)
		return NULL;

	return cbor_build_bytestring((const unsigned char *)v, strlen(v));
}

cbor_item_t *
pack_byte(uint8_t v) NO_MSAN
{
	return cbor_build_uint8(v);
}

cbor_item_t *
pack_blob(const struct blob *v) NO_MSAN
{
	return cbor_build_bytestring(v->body, v->len);
}

void
mutate_byte(uint8_t *b)
{
	LLVMFuzzerMutate(b, sizeof(*b), sizeof(*b));
}

void
mutate_int(int *i)
{
	LLVMFuzzerMutate((uint8_t *)i, sizeof(*i), sizeof(*i));
}

void
mutate_blob(struct blob *blob)
{
	blob->len = LLVMFuzzerMutate((uint8_t *)blob->body, blob->len,
	    sizeof(blob->body));
}

void
mutate_string(char *s)
{
	size_t n;

	n = LLVMFuzzerMutate((uint8_t *)s, strlen(s), MAXSTR - 1);
	s[n] = '\0';
}

void *
dev_open(const char *path)
{
	(void)path;

	return (void *)0xdeadbeef;
}

void
dev_close(void *handle)
{
	assert(handle == (void *)0xdeadbeef);
}

int
dev_read(void *handle, unsigned char *ptr, size_t len, int ms)
{
	size_t n;

	(void)ms;

	assert(handle == (void *)0xdeadbeef);
	assert(len >= CTAP_MIN_REPORT_LEN && len <= CTAP_MAX_REPORT_LEN);

	if (wire_data_len < len)
		n = wire_data_len;
	else
		n = len;

	memcpy(ptr, wire_data_ptr, n);

	wire_data_ptr += n;
	wire_data_len -= n;

	return (int)n;
}

int
dev_write(void *handle, const unsigned char *ptr, size_t len)
{
	assert(handle == (void *)0xdeadbeef);
	assert(len >= CTAP_MIN_REPORT_LEN + 1 &&
	    len <= CTAP_MAX_REPORT_LEN + 1);

	consume(ptr, len);

	if (uniform_random(400) < 1)
		return -1;

	return (int)len;
}

void
set_wire_data(const uint8_t *ptr, size_t len)
{
	wire_data_ptr = ptr;
	wire_data_len = len;
}
