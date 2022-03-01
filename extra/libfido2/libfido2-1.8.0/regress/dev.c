/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <assert.h>
#include <fido.h>
#include <string.h>

#include "../fuzz/wiredata_fido2.h"

#define FAKE_DEV_HANDLE	((void *)0xdeadbeef)
#define REPORT_LEN	(64 + 1)

static uint8_t	 ctap_nonce[8];
static uint8_t	*wiredata_ptr;
static size_t	 wiredata_len;
static int	 initialised;

static void *
dummy_open(const char *path)
{
	(void)path;

	return (FAKE_DEV_HANDLE);
}

static void
dummy_close(void *handle)
{
	assert(handle == FAKE_DEV_HANDLE);
}

static int
dummy_read(void *handle, unsigned char *ptr, size_t len, int ms)
{
	size_t n;

	(void)ms;

	assert(handle == FAKE_DEV_HANDLE);
	assert(ptr != NULL);
	assert(len == REPORT_LEN - 1);

	if (wiredata_ptr == NULL)
		return (-1);

	if (!initialised) {
		assert(wiredata_len >= REPORT_LEN - 1);
		memcpy(&wiredata_ptr[7], &ctap_nonce, sizeof(ctap_nonce));
		initialised = 1;
	}

	if (wiredata_len < len)
		n = wiredata_len;
	else
		n = len;

	memcpy(ptr, wiredata_ptr, n);
	wiredata_ptr += n;
	wiredata_len -= n;

	return ((int)n);
}

static int
dummy_write(void *handle, const unsigned char *ptr, size_t len)
{
	assert(handle == FAKE_DEV_HANDLE);
	assert(ptr != NULL);
	assert(len == REPORT_LEN);

	if (!initialised)
		memcpy(&ctap_nonce, &ptr[8], sizeof(ctap_nonce));

	return ((int)len);
}

static uint8_t *
wiredata_setup(const uint8_t *data, size_t len)
{
	const uint8_t ctap_init_data[] = { WIREDATA_CTAP_INIT };

	assert(wiredata_ptr == NULL);
	assert(SIZE_MAX - len > sizeof(ctap_init_data));
	assert((wiredata_ptr = malloc(sizeof(ctap_init_data) + len)) != NULL);

	memcpy(wiredata_ptr, ctap_init_data, sizeof(ctap_init_data));

	if (len)
		memcpy(wiredata_ptr + sizeof(ctap_init_data), data, len);

	wiredata_len = sizeof(ctap_init_data) + len;

	return (wiredata_ptr);
}

static void
wiredata_clear(uint8_t **wiredata)
{
	free(*wiredata);
	*wiredata = NULL;
	wiredata_ptr = NULL;
	wiredata_len = 0;
	initialised = 0;
}

/* gh#56 */
static void
open_iff_ok(void)
{
	fido_dev_t	*dev = NULL;
	fido_dev_io_t	 io;

	memset(&io, 0, sizeof(io));

	io.open = dummy_open;
	io.close = dummy_close;
	io.read = dummy_read;
	io.write = dummy_write;

	assert((dev = fido_dev_new()) != NULL);
	assert(fido_dev_set_io_functions(dev, &io) == FIDO_OK);
	assert(fido_dev_open(dev, "dummy") == FIDO_ERR_RX);
	assert(fido_dev_close(dev) == FIDO_ERR_INVALID_ARGUMENT);

	fido_dev_free(&dev);
}

static void
reopen(void)
{
	const uint8_t	 cbor_info_data[] = { WIREDATA_CTAP_CBOR_INFO };
	uint8_t		*wiredata;
	fido_dev_t	*dev = NULL;
	fido_dev_io_t	 io;

	memset(&io, 0, sizeof(io));

	io.open = dummy_open;
	io.close = dummy_close;
	io.read = dummy_read;
	io.write = dummy_write;

	wiredata = wiredata_setup(cbor_info_data, sizeof(cbor_info_data));
	assert((dev = fido_dev_new()) != NULL);
	assert(fido_dev_set_io_functions(dev, &io) == FIDO_OK);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_close(dev) == FIDO_OK);
	wiredata_clear(&wiredata);

	wiredata = wiredata_setup(cbor_info_data, sizeof(cbor_info_data));
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_close(dev) == FIDO_OK);
	wiredata_clear(&wiredata);
}

static void
double_open(void)
{
	const uint8_t	 cbor_info_data[] = { WIREDATA_CTAP_CBOR_INFO };
	uint8_t		*wiredata;
	fido_dev_t	*dev = NULL;
	fido_dev_io_t	 io;

	memset(&io, 0, sizeof(io));

	io.open = dummy_open;
	io.close = dummy_close;
	io.read = dummy_read;
	io.write = dummy_write;

	wiredata = wiredata_setup(cbor_info_data, sizeof(cbor_info_data));
	assert((dev = fido_dev_new()) != NULL);
	assert(fido_dev_set_io_functions(dev, &io) == FIDO_OK);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_open(dev, "dummy") == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_dev_close(dev) == FIDO_OK);
	wiredata_clear(&wiredata);
}

static void
is_fido2(void)
{
	const uint8_t	 cbor_info_data[] = { WIREDATA_CTAP_CBOR_INFO };
	uint8_t		*wiredata;
	fido_dev_t	*dev = NULL;
	fido_dev_io_t	 io;

	memset(&io, 0, sizeof(io));

	io.open = dummy_open;
	io.close = dummy_close;
	io.read = dummy_read;
	io.write = dummy_write;

	wiredata = wiredata_setup(cbor_info_data, sizeof(cbor_info_data));
	assert((dev = fido_dev_new()) != NULL);
	assert(fido_dev_set_io_functions(dev, &io) == FIDO_OK);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_is_fido2(dev) == true);
	assert(fido_dev_supports_pin(dev) == true);
	fido_dev_force_u2f(dev);
	assert(fido_dev_is_fido2(dev) == false);
	assert(fido_dev_supports_pin(dev) == false);
	assert(fido_dev_close(dev) == FIDO_OK);
	wiredata_clear(&wiredata);

	wiredata = wiredata_setup(NULL, 0);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_is_fido2(dev) == false);
	assert(fido_dev_supports_pin(dev) == false);
	fido_dev_force_fido2(dev);
	assert(fido_dev_is_fido2(dev) == true);
	assert(fido_dev_supports_pin(dev) == false);
	assert(fido_dev_close(dev) == FIDO_OK);
	wiredata_clear(&wiredata);
}

static void
has_pin(void)
{
	const uint8_t	 set_pin_data[] = {
			    WIREDATA_CTAP_CBOR_INFO,
			    WIREDATA_CTAP_CBOR_AUTHKEY,
			    WIREDATA_CTAP_CBOR_STATUS,
			    WIREDATA_CTAP_CBOR_STATUS
			 };
	uint8_t		*wiredata;
	fido_dev_t	*dev = NULL;
	fido_dev_io_t	 io;

	memset(&io, 0, sizeof(io));

	io.open = dummy_open;
	io.close = dummy_close;
	io.read = dummy_read;
	io.write = dummy_write;

	wiredata = wiredata_setup(set_pin_data, sizeof(set_pin_data));
	assert((dev = fido_dev_new()) != NULL);
	assert(fido_dev_set_io_functions(dev, &io) == FIDO_OK);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_has_pin(dev) == false);
	assert(fido_dev_set_pin(dev, "top secret", NULL) == FIDO_OK);
	assert(fido_dev_has_pin(dev) == true);
	assert(fido_dev_reset(dev) == FIDO_OK);
	assert(fido_dev_has_pin(dev) == false);
	assert(fido_dev_close(dev) == FIDO_OK);
	wiredata_clear(&wiredata);
}

int
main(void)
{
	fido_init(0);

	open_iff_ok();
	reopen();
	double_open();
	is_fido2();
	has_pin();

	exit(0);
}
