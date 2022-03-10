/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <assert.h>
#include <fido.h>
#include <string.h>

#define FAKE_DEV_HANDLE	((void *)0xdeadbeef)
#define REPORT_LEN	(64 + 1)

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
	(void)ptr;
	(void)len;
	(void)ms;

	assert(handle == FAKE_DEV_HANDLE);

	return (-1);
}

static int
dummy_write(void *handle, const unsigned char *ptr, size_t len)
{
	assert(handle == FAKE_DEV_HANDLE);
	assert(ptr != NULL);
	assert(len == REPORT_LEN);

	return ((int)len);
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

int
main(void)
{
	fido_init(0);

	open_iff_ok();

	exit(0);
}
