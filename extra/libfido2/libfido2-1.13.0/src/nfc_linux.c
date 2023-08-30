/*
 * Copyright (c) 2020-2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include <linux/nfc.h>

#include <errno.h>
#include <libudev.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "fido.h"
#include "fido/param.h"
#include "netlink.h"
#include "iso7816.h"

struct nfc_linux {
	int             fd;
	uint32_t        dev;
	uint32_t        target;
	sigset_t	sigmask;
	const sigset_t *sigmaskp;
	struct fido_nl *nl;
};

static char *
get_parent_attr(struct udev_device *dev, const char *subsystem,
    const char *devtype, const char *attr)
{
	struct udev_device *parent;
	const char *value;

	if ((parent = udev_device_get_parent_with_subsystem_devtype(dev,
	    subsystem, devtype)) == NULL || (value =
	    udev_device_get_sysattr_value(parent, attr)) == NULL)
		return NULL;

	return strdup(value);
}

static char *
get_usb_attr(struct udev_device *dev, const char *attr)
{
	return get_parent_attr(dev, "usb", "usb_device", attr);
}

static int
copy_info(fido_dev_info_t *di, struct udev *udev,
    struct udev_list_entry *udev_entry)
{
	const char *name;
	char *str;
	struct udev_device *dev = NULL;
	uint64_t id;
	int ok = -1;

	memset(di, 0, sizeof(*di));

	if ((name = udev_list_entry_get_name(udev_entry)) == NULL ||
	    (dev = udev_device_new_from_syspath(udev, name)) == NULL)
		goto fail;
	if (asprintf(&di->path, "%s/%s", FIDO_NFC_PREFIX, name) == -1) {
		di->path = NULL;
		goto fail;
	}
	if (nfc_is_fido(di->path) == false) {
		fido_log_debug("%s: nfc_is_fido: %s", __func__, di->path);
		goto fail;
	}
	if ((di->manufacturer = get_usb_attr(dev, "manufacturer")) == NULL)
		di->manufacturer = strdup("");
	if ((di->product = get_usb_attr(dev, "product")) == NULL)
		di->product = strdup("");
	if (di->manufacturer == NULL || di->product == NULL)
		goto fail;
	/* XXX assumes USB for vendor/product info */
	if ((str = get_usb_attr(dev, "idVendor")) != NULL &&
	    fido_to_uint64(str, 16, &id) == 0 && id <= UINT16_MAX)
		di->vendor_id = (int16_t)id;
	free(str);
	if ((str = get_usb_attr(dev, "idProduct")) != NULL &&
	    fido_to_uint64(str, 16, &id) == 0 && id <= UINT16_MAX)
		di->product_id = (int16_t)id;
	free(str);

	ok = 0;
fail:
	if (dev != NULL)
		udev_device_unref(dev);

	if (ok < 0) {
		free(di->path);
		free(di->manufacturer);
		free(di->product);
		explicit_bzero(di, sizeof(*di));
	}

	return ok;
}

static int
sysnum_from_syspath(const char *path)
{
	struct udev *udev = NULL;
	struct udev_device *dev = NULL;
	const char *str;
	uint64_t idx64;
	int idx = -1;

	if ((udev = udev_new()) != NULL &&
	    (dev = udev_device_new_from_syspath(udev, path)) != NULL &&
	    (str = udev_device_get_sysnum(dev)) != NULL &&
	    fido_to_uint64(str, 10, &idx64) == 0 && idx64 < INT_MAX)
		idx = (int)idx64;

	if (dev != NULL)
		udev_device_unref(dev);
	if (udev != NULL)
		udev_unref(udev);

	return idx;
}

int
fido_nfc_manifest(fido_dev_info_t *devlist, size_t ilen, size_t *olen)
{
	struct udev *udev = NULL;
	struct udev_enumerate *udev_enum = NULL;
	struct udev_list_entry *udev_list;
	struct udev_list_entry *udev_entry;
	int r = FIDO_ERR_INTERNAL;

	*olen = 0;

	if (ilen == 0)
		return FIDO_OK;

	if (devlist == NULL)
		return FIDO_ERR_INVALID_ARGUMENT;

	if ((udev = udev_new()) == NULL ||
	    (udev_enum = udev_enumerate_new(udev)) == NULL)
		goto fail;

	if (udev_enumerate_add_match_subsystem(udev_enum, "nfc") < 0 ||
	    udev_enumerate_scan_devices(udev_enum) < 0)
		goto fail;

	if ((udev_list = udev_enumerate_get_list_entry(udev_enum)) == NULL) {
		r = FIDO_OK; /* zero nfc devices */
		goto fail;
	}

	udev_list_entry_foreach(udev_entry, udev_list) {
		if (copy_info(&devlist[*olen], udev, udev_entry) == 0) {
			devlist[*olen].io = (fido_dev_io_t) {
				fido_nfc_open,
				fido_nfc_close,
				fido_nfc_read,
				fido_nfc_write,
			};
			devlist[*olen].transport = (fido_dev_transport_t) {
				fido_nfc_rx,
				fido_nfc_tx,
			};
			if (++(*olen) == ilen)
				break;
		}
	}

	r = FIDO_OK;
fail:
	if (udev_enum != NULL)
		udev_enumerate_unref(udev_enum);
	if (udev != NULL)
		udev_unref(udev);

	return r;
}

static int
nfc_target_connect(struct nfc_linux *ctx)
{
	struct sockaddr_nfc sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_family = AF_NFC;
	sa.dev_idx = ctx->dev;
	sa.target_idx = ctx->target;
	sa.nfc_protocol = NFC_PROTO_ISO14443;

	if ((ctx->fd = socket(AF_NFC, SOCK_SEQPACKET | SOCK_CLOEXEC,
	    NFC_SOCKPROTO_RAW)) == -1) {
		fido_log_error(errno, "%s: socket", __func__);
		return -1;
	}
	if (connect(ctx->fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
		fido_log_error(errno, "%s: connect", __func__);
		if (close(ctx->fd) == -1)
			fido_log_error(errno, "%s: close", __func__);
		ctx->fd = -1;
		return -1;
	}

	return 0;
}

static void
nfc_free(struct nfc_linux **ctx_p)
{
	struct nfc_linux *ctx;

	if (ctx_p == NULL || (ctx = *ctx_p) == NULL)
		return;
	if (ctx->fd != -1 && close(ctx->fd) == -1)
		fido_log_error(errno, "%s: close", __func__);
	if (ctx->nl != NULL)
		fido_nl_free(&ctx->nl);

	free(ctx);
	*ctx_p = NULL;
}

static struct nfc_linux *
nfc_new(uint32_t dev)
{
	struct nfc_linux *ctx;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL ||
	    (ctx->nl = fido_nl_new()) == NULL) {
		nfc_free(&ctx);
		return NULL;
	}

	ctx->fd = -1;
	ctx->dev = dev;

	return ctx;
}

void *
fido_nfc_open(const char *path)
{
	struct nfc_linux *ctx = NULL;
	int idx;

	if (strncmp(path, FIDO_NFC_PREFIX, strlen(FIDO_NFC_PREFIX)) != 0) {
		fido_log_debug("%s: bad prefix", __func__);
		goto fail;
	}
	if ((idx = sysnum_from_syspath(path + strlen(FIDO_NFC_PREFIX))) < 0 ||
	    (ctx = nfc_new((uint32_t)idx)) == NULL) {
		fido_log_debug("%s: nfc_new", __func__);
		goto fail;
	}
	if (fido_nl_power_nfc(ctx->nl, ctx->dev) < 0 ||
	    fido_nl_get_nfc_target(ctx->nl, ctx->dev, &ctx->target) < 0 ||
	    nfc_target_connect(ctx) < 0) {
		fido_log_debug("%s: netlink", __func__);
		goto fail;
	}

	return ctx;
fail:
	nfc_free(&ctx);
	return NULL;
}

void
fido_nfc_close(void *handle)
{
	struct nfc_linux *ctx = handle;

	nfc_free(&ctx);
}

int
fido_nfc_set_sigmask(void *handle, const fido_sigset_t *sigmask)
{
	struct nfc_linux *ctx = handle;

	ctx->sigmask = *sigmask;
	ctx->sigmaskp = &ctx->sigmask;

	return FIDO_OK;
}

int
fido_nfc_read(void *handle, unsigned char *buf, size_t len, int ms)
{
	struct nfc_linux *ctx = handle;
	struct iovec iov[2];
	uint8_t preamble;
	ssize_t	r;

	memset(&iov, 0, sizeof(iov));
	iov[0].iov_base = &preamble;
	iov[0].iov_len = sizeof(preamble);
	iov[1].iov_base = buf;
	iov[1].iov_len = len;

	if (fido_hid_unix_wait(ctx->fd, ms, ctx->sigmaskp) < 0) {
		fido_log_debug("%s: fido_hid_unix_wait", __func__);
		return -1;
	}
	if ((r = readv(ctx->fd, iov, nitems(iov))) == -1) {
		fido_log_error(errno, "%s: read", __func__);
		return -1;
	}
	if (r < 1) {
		fido_log_debug("%s: %zd < 1", __func__, r);
		return -1;
	}
	if (preamble != 0x00) {
		fido_log_debug("%s: preamble", __func__);
		return -1;
	}

	r--;
	fido_log_xxd(buf, (size_t)r, "%s", __func__);

	return (int)r;
}

int
fido_nfc_write(void *handle, const unsigned char *buf, size_t len)
{
	struct nfc_linux *ctx = handle;
	ssize_t	r;

	fido_log_xxd(buf, len, "%s", __func__);

	if (len > INT_MAX) {
		fido_log_debug("%s: len", __func__);
		return -1;
	}
	if ((r = write(ctx->fd, buf, len)) == -1) {
		fido_log_error(errno, "%s: write", __func__);
		return -1;
	}
	if (r < 0 || (size_t)r != len) {
		fido_log_debug("%s: %zd != %zu", __func__, r, len);
		return -1;
	}

	return (int)r;
}
