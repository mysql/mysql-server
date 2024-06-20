/*
 * Copyright (c) 2019-2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#undef NDEBUG

#include <assert.h>
#include <string.h>
#include <time.h>

#define _FIDO_INTERNAL

#include <fido.h>

#include "../fuzz/wiredata_fido2.h"

#define REPORT_LEN	(64 + 1)

static uint8_t	 ctap_nonce[8];
static uint8_t	*wiredata_ptr;
static size_t	 wiredata_len;
static int	 fake_dev_handle;
static int	 initialised;
static long	 interval_ms;

#if defined(_MSC_VER)
static int
nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	if (rmtp != NULL) {
		errno = EINVAL;
		return (-1);
	}

	Sleep((DWORD)(rqtp->tv_sec * 1000) + (DWORD)(rqtp->tv_nsec / 1000000));

	return (0);
}
#endif

static void *
dummy_open(const char *path)
{
	(void)path;

	return (&fake_dev_handle);
}

static void
dummy_close(void *handle)
{
	assert(handle == &fake_dev_handle);
}

static int
dummy_read(void *handle, unsigned char *ptr, size_t len, int ms)
{
	struct timespec tv;
	size_t		n;
	long		d;

	assert(handle == &fake_dev_handle);
	assert(ptr != NULL);
	assert(len == REPORT_LEN - 1);

	if (wiredata_ptr == NULL)
		return (-1);

	if (!initialised) {
		assert(wiredata_len >= REPORT_LEN - 1);
		memcpy(&wiredata_ptr[7], &ctap_nonce, sizeof(ctap_nonce));
		initialised = 1;
	}

	if (ms >= 0 && ms < interval_ms)
		d = ms;
	else
		d = interval_ms;

	if (d) {
		tv.tv_sec = d / 1000;
		tv.tv_nsec = (d % 1000) * 1000000;
		if (nanosleep(&tv, NULL) == -1)
			err(1, "nanosleep");
	}

	if (d != interval_ms)
		return (-1); /* timeout */

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
	struct timespec tv;

	assert(handle == &fake_dev_handle);
	assert(ptr != NULL);
	assert(len == REPORT_LEN);

	if (!initialised)
		memcpy(&ctap_nonce, &ptr[8], sizeof(ctap_nonce));

	if (interval_ms) {
		tv.tv_sec = interval_ms / 1000;
		tv.tv_nsec = (interval_ms % 1000) * 1000000;
		if (nanosleep(&tv, NULL) == -1)
			err(1, "nanosleep");
	}

	return ((int)len);
}

static uint8_t *
wiredata_setup(const uint8_t *data, size_t len)
{
	const uint8_t ctap_init_data[] = { WIREDATA_CTAP_INIT };

	assert(wiredata_ptr == NULL);
	assert(SIZE_MAX - len > sizeof(ctap_init_data));
	assert((wiredata_ptr = malloc(sizeof(ctap_init_data) + len)) != NULL);

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:6386)
#endif
	memcpy(wiredata_ptr, ctap_init_data, sizeof(ctap_init_data));
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

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
	fido_dev_free(&dev);
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
	fido_dev_free(&dev);
	wiredata_clear(&wiredata);
}

static void
double_close(void)
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
	assert(fido_dev_close(dev) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_dev_set_io_functions(dev, &io) == FIDO_OK);
	assert(fido_dev_close(dev) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_close(dev) == FIDO_OK);
	assert(fido_dev_close(dev) == FIDO_ERR_INVALID_ARGUMENT);
	fido_dev_free(&dev);
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
	fido_dev_free(&dev);
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
	fido_dev_free(&dev);
	wiredata_clear(&wiredata);
}

static void
timeout_rx(void)
{
	const uint8_t	 timeout_rx_data[] = {
			    WIREDATA_CTAP_CBOR_INFO,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
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

	wiredata = wiredata_setup(timeout_rx_data, sizeof(timeout_rx_data));
	assert((dev = fido_dev_new()) != NULL);
	assert(fido_dev_set_io_functions(dev, &io) == FIDO_OK);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_set_timeout(dev, 3 * 1000) == FIDO_OK);
	interval_ms = 1000;
	assert(fido_dev_reset(dev) == FIDO_ERR_RX);
	assert(fido_dev_close(dev) == FIDO_OK);
	fido_dev_free(&dev);
	wiredata_clear(&wiredata);
	interval_ms = 0;
}

static void
timeout_ok(void)
{
	const uint8_t	 timeout_ok_data[] = {
			    WIREDATA_CTAP_CBOR_INFO,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
			    WIREDATA_CTAP_KEEPALIVE,
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

	wiredata = wiredata_setup(timeout_ok_data, sizeof(timeout_ok_data));
	assert((dev = fido_dev_new()) != NULL);
	assert(fido_dev_set_io_functions(dev, &io) == FIDO_OK);
	assert(fido_dev_open(dev, "dummy") == FIDO_OK);
	assert(fido_dev_set_timeout(dev, 30 * 1000) == FIDO_OK);
	interval_ms = 1000;
	assert(fido_dev_reset(dev) == FIDO_OK);
	assert(fido_dev_close(dev) == FIDO_OK);
	fido_dev_free(&dev);
	wiredata_clear(&wiredata);
	interval_ms = 0;
}

static void
timeout_misc(void)
{
	fido_dev_t *dev;

	assert((dev = fido_dev_new()) != NULL);
	assert(fido_dev_set_timeout(dev, -2) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_dev_set_timeout(dev, 3 * 1000) == FIDO_OK);
	assert(fido_dev_set_timeout(dev, -1) == FIDO_OK);
	fido_dev_free(&dev);
}

int
main(void)
{
	fido_init(0);

	open_iff_ok();
	reopen();
	double_open();
	double_close();
	is_fido2();
	has_pin();
	timeout_rx();
	timeout_ok();
	timeout_misc();

	exit(0);
}
