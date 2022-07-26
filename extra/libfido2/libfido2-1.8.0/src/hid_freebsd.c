/*
 * Copyright (c) 2020 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <sys/types.h>

#include <dev/usb/usb_ioctl.h>
#include <dev/usb/usbhid.h>

#include <errno.h>
#include <unistd.h>

#include "fido.h"

#define MAX_UHID	64

struct hid_freebsd {
	int             fd;
	size_t          report_in_len;
	size_t          report_out_len;
	sigset_t        sigmask;
	const sigset_t *sigmaskp;
};

static bool
is_fido(int fd)
{
	char				buf[64];
	struct usb_gen_descriptor	ugd;
	uint32_t			usage_page = 0;

	memset(&buf, 0, sizeof(buf));
	memset(&ugd, 0, sizeof(ugd));

	ugd.ugd_report_type = UHID_FEATURE_REPORT;
	ugd.ugd_data = buf;
	ugd.ugd_maxlen = sizeof(buf);

	if (ioctl(fd, IOCTL_REQ(USB_GET_REPORT_DESC), &ugd) == -1) {
		fido_log_error(errno, "%s: ioctl", __func__);
		return (false);
	}
	if (ugd.ugd_actlen > sizeof(buf) || fido_hid_get_usage(ugd.ugd_data,
	    ugd.ugd_actlen, &usage_page) < 0) {
		fido_log_debug("%s: fido_hid_get_usage", __func__);
		return (false);
	}

	return (usage_page == 0xf1d0);
}

static int
copy_info(fido_dev_info_t *di, const char *path)
{
	int			fd = -1;
	int			ok = -1;
	struct usb_device_info	udi;

	memset(di, 0, sizeof(*di));
	memset(&udi, 0, sizeof(udi));

	if ((fd = fido_hid_unix_open(path)) == -1 || is_fido(fd) == 0)
		goto fail;

	if (ioctl(fd, IOCTL_REQ(USB_GET_DEVICEINFO), &udi) == -1) {
		fido_log_error(errno, "%s: ioctl", __func__);
		strlcpy(udi.udi_vendor, "FreeBSD", sizeof(udi.udi_vendor));
		strlcpy(udi.udi_product, "uhid(4)", sizeof(udi.udi_product));
		udi.udi_vendorNo = 0x0b5d; /* stolen from PCI_VENDOR_OPENBSD */
	}

	if ((di->path = strdup(path)) == NULL ||
	    (di->manufacturer = strdup(udi.udi_vendor)) == NULL ||
	    (di->product = strdup(udi.udi_product)) == NULL)
		goto fail;

	di->vendor_id = (int16_t)udi.udi_vendorNo;
	di->product_id = (int16_t)udi.udi_productNo;

	ok = 0;
fail:
	if (fd != -1)
		close(fd);

	if (ok < 0) {
		free(di->path);
		free(di->manufacturer);
		free(di->product);
		explicit_bzero(di, sizeof(*di));
	}

	return (ok);
}

int
fido_hid_manifest(fido_dev_info_t *devlist, size_t ilen, size_t *olen)
{
	char	path[64];
	size_t	i;

	*olen = 0;

	if (ilen == 0)
		return (FIDO_OK); /* nothing to do */

	if (devlist == NULL || olen == NULL)
		return (FIDO_ERR_INVALID_ARGUMENT);

	for (i = *olen = 0; i < MAX_UHID && *olen < ilen; i++) {
		snprintf(path, sizeof(path), "/dev/uhid%zu", i);
		if (copy_info(&devlist[*olen], path) == 0) {
			devlist[*olen].io = (fido_dev_io_t) {
				fido_hid_open,
				fido_hid_close,
				fido_hid_read,
				fido_hid_write,
			};
			++(*olen);
		}
	}

	return (FIDO_OK);
}

void *
fido_hid_open(const char *path)
{
	char				 buf[64];
	struct hid_freebsd		*ctx;
	struct usb_gen_descriptor	 ugd;
	int				 r;

	memset(&buf, 0, sizeof(buf));
	memset(&ugd, 0, sizeof(ugd));

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL)
		return (NULL);

	if ((ctx->fd = fido_hid_unix_open(path)) == -1) {
		free(ctx);
		return (NULL);
	}

	ugd.ugd_report_type = UHID_FEATURE_REPORT;
	ugd.ugd_data = buf;
	ugd.ugd_maxlen = sizeof(buf);

	if ((r = ioctl(ctx->fd, IOCTL_REQ(USB_GET_REPORT_DESC), &ugd) == -1) ||
	    ugd.ugd_actlen > sizeof(buf) ||
	    fido_hid_get_report_len(ugd.ugd_data, ugd.ugd_actlen,
	    &ctx->report_in_len, &ctx->report_out_len) < 0) {
		if (r == -1)
			fido_log_error(errno, "%s: ioctl", __func__);
		fido_log_debug("%s: using default report sizes", __func__);
		ctx->report_in_len = CTAP_MAX_REPORT_LEN;
		ctx->report_out_len = CTAP_MAX_REPORT_LEN;
	}

	return (ctx);
}

void
fido_hid_close(void *handle)
{
	struct hid_freebsd *ctx = handle;

	if (close(ctx->fd) == -1)
		fido_log_error(errno, "%s: close", __func__);

	free(ctx);
}

int
fido_hid_set_sigmask(void *handle, const fido_sigset_t *sigmask)
{
	struct hid_freebsd *ctx = handle;

	ctx->sigmask = *sigmask;
	ctx->sigmaskp = &ctx->sigmask;

	return (FIDO_OK);
}

int
fido_hid_read(void *handle, unsigned char *buf, size_t len, int ms)
{
	struct hid_freebsd	*ctx = handle;
	ssize_t			 r;

	if (len != ctx->report_in_len) {
		fido_log_debug("%s: len %zu", __func__, len);
		return (-1);
	}

	if (fido_hid_unix_wait(ctx->fd, ms, ctx->sigmaskp) < 0) {
		fido_log_debug("%s: fd not ready", __func__);
		return (-1);
	}

	if ((r = read(ctx->fd, buf, len)) == -1) {
		fido_log_error(errno, "%s: read", __func__);
		return (-1);
	}

	if (r < 0 || (size_t)r != len) {
		fido_log_debug("%s: %zd != %zu", __func__, r, len);
		return (-1);
	}

	return ((int)r);
}

int
fido_hid_write(void *handle, const unsigned char *buf, size_t len)
{
	struct hid_freebsd	*ctx = handle;
	ssize_t			 r;

	if (len != ctx->report_out_len + 1) {
		fido_log_debug("%s: len %zu", __func__, len);
		return (-1);
	}

	if ((r = write(ctx->fd, buf + 1, len - 1)) == -1) {
		fido_log_error(errno, "%s: write", __func__);
		return (-1);
	}

	if (r < 0 || (size_t)r != len - 1) {
		fido_log_debug("%s: %zd != %zu", __func__, r, len - 1);
		return (-1);
	}

	return ((int)len);
}

size_t
fido_hid_report_in_len(void *handle)
{
	struct hid_freebsd *ctx = handle;

	return (ctx->report_in_len);
}

size_t
fido_hid_report_out_len(void *handle)
{
	struct hid_freebsd *ctx = handle;

	return (ctx->report_out_len);
}
