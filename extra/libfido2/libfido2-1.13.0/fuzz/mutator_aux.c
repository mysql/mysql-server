/*
 * Copyright (c) 2019-2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <cbor.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mutator_aux.h"

int fido_nfc_rx(fido_dev_t *, uint8_t, unsigned char *, size_t, int);
int fido_nfc_tx(fido_dev_t *, uint8_t, const unsigned char *, size_t);
size_t LLVMFuzzerMutate(uint8_t *, size_t, size_t);

extern int prng_up;
static const uint8_t *wire_data_ptr = NULL;
static size_t wire_data_len = 0;

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

	(void)x;
}

void
consume_str(const char *str)
{
	if (str != NULL)
		consume(str, strlen(str) + 1);
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

static int
buf_read(unsigned char *ptr, size_t len, int ms)
{
	size_t n;

	(void)ms;

	if (prng_up && uniform_random(400) < 1) {
		errno = EIO;
		return -1;
	}

	if (wire_data_len < len)
		n = wire_data_len;
	else
		n = len;

	memcpy(ptr, wire_data_ptr, n);

	wire_data_ptr += n;
	wire_data_len -= n;

	return (int)n;
}

static int
buf_write(const unsigned char *ptr, size_t len)
{
	consume(ptr, len);

	if (prng_up && uniform_random(400) < 1) {
		errno = EIO;
		return -1;
	}

	return (int)len;
}

static void *
hid_open(const char *path)
{
	(void)path;

	return (void *)HID_DEV_HANDLE;
}

static void
hid_close(void *handle)
{
	assert(handle == (void *)HID_DEV_HANDLE);
}

static int
hid_read(void *handle, unsigned char *ptr, size_t len, int ms)
{
	assert(handle == (void *)HID_DEV_HANDLE);
	assert(len >= CTAP_MIN_REPORT_LEN && len <= CTAP_MAX_REPORT_LEN);

	return buf_read(ptr, len, ms);
}

static int
hid_write(void *handle, const unsigned char *ptr, size_t len)
{
	assert(handle == (void *)HID_DEV_HANDLE);
	assert(len >= CTAP_MIN_REPORT_LEN + 1 &&
	    len <= CTAP_MAX_REPORT_LEN + 1);

	return buf_write(ptr, len);
}

static void *
nfc_open(const char *path)
{
	(void)path;

	return (void *)NFC_DEV_HANDLE;
}

static void
nfc_close(void *handle)
{
	assert(handle == (void *)NFC_DEV_HANDLE);
}

int
nfc_read(void *handle, unsigned char *ptr, size_t len, int ms)
{
	assert(handle == (void *)NFC_DEV_HANDLE);
	assert(len > 0 && len <= 264);

	return buf_read(ptr, len, ms);
}

int
nfc_write(void *handle, const unsigned char *ptr, size_t len)
{
	assert(handle == (void *)NFC_DEV_HANDLE);
	assert(len > 0 && len <= 256 + 2);

	return buf_write(ptr, len);
}

ssize_t
fd_read(int fd, void *ptr, size_t len)
{
	assert(fd != -1);

	return buf_read(ptr, len, -1);
}

ssize_t
fd_write(int fd, const void *ptr, size_t len)
{
	assert(fd != -1);

	return buf_write(ptr, len);
}

fido_dev_t *
open_dev(int nfc)
{
	fido_dev_t *dev;
	fido_dev_io_t io;
	fido_dev_transport_t t;

	memset(&io, 0, sizeof(io));
	memset(&t, 0, sizeof(t));

	if ((dev = fido_dev_new()) == NULL)
		return NULL;

	if (nfc) {
		io.open = nfc_open;
		io.close = nfc_close;
		io.read = nfc_read;
		io.write = nfc_write;
	} else {
		io.open = hid_open;
		io.close = hid_close;
		io.read = hid_read;
		io.write = hid_write;
	}

	if (fido_dev_set_io_functions(dev, &io) != FIDO_OK)
		goto fail;

	if (nfc) {
		t.rx = fido_nfc_rx;
		t.tx = fido_nfc_tx;
		if (fido_dev_set_transport_functions(dev, &t) != FIDO_OK)
			goto fail;
	}

	if (fido_dev_set_timeout(dev, 300) != FIDO_OK ||
	    fido_dev_open(dev, "nodev") != FIDO_OK)
		goto fail;

	return dev;
fail:
	fido_dev_free(&dev);

	return NULL;
}

void
set_wire_data(const uint8_t *ptr, size_t len)
{
	wire_data_ptr = ptr;
	wire_data_len = len;
}
