/*
 * Copyright (c) 2020 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/ioctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fido.h"

#define MAX_UHID	64

struct hid_netbsd {
	int             fd;
	size_t          report_in_len;
	size_t          report_out_len;
	sigset_t        sigmask;
	const sigset_t *sigmaskp;
};

/* Hack to make this work with newer kernels even if /usr/include is old.  */
#if __NetBSD_Version__ < 901000000	/* 9.1 */
#define	USB_HID_GET_RAW	_IOR('h', 1, int)
#define	USB_HID_SET_RAW	_IOW('h', 2, int)
#endif

static bool
is_fido(int fd)
{
	struct usb_ctl_report_desc	ucrd;
	uint32_t			usage_page = 0;
	int				raw = 1;

	memset(&ucrd, 0, sizeof(ucrd));

	if (ioctl(fd, IOCTL_REQ(USB_GET_REPORT_DESC), &ucrd) == -1) {
		fido_log_error(errno, "%s: ioctl", __func__);
		return (false);
	}

	if (ucrd.ucrd_size < 0 ||
	    (size_t)ucrd.ucrd_size > sizeof(ucrd.ucrd_data) ||
	    fido_hid_get_usage(ucrd.ucrd_data, (size_t)ucrd.ucrd_size,
		&usage_page) < 0) {
		fido_log_debug("%s: fido_hid_get_usage", __func__);
		return (false);
	}

	if (usage_page != 0xf1d0)
		return (false);

	/*
	 * This step is not strictly necessary -- NetBSD puts fido
	 * devices into raw mode automatically by default, but in
	 * principle that might change, and this serves as a test to
	 * verify that we're running on a kernel with support for raw
	 * mode at all so we don't get confused issuing writes that try
	 * to set the report descriptor rather than transfer data on
	 * the output interrupt pipe as we need.
	 */
	if (ioctl(fd, IOCTL_REQ(USB_HID_SET_RAW), &raw) == -1) {
		fido_log_error(errno, "%s: unable to set raw", __func__);
		return (false);
	}

	return (true);
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
		goto fail;
	}

	if ((di->path = strdup(path)) == NULL ||
	    (di->manufacturer = strdup(udi.udi_vendor)) == NULL ||
	    (di->product = strdup(udi.udi_product)) == NULL)
		goto fail;

	di->vendor_id = (int16_t)udi.udi_vendorNo;
	di->product_id = (int16_t)udi.udi_productNo;

	ok = 0;
fail:
	if (fd != -1 && close(fd) == -1)
		fido_log_error(errno, "%s: close", __func__);

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

	if (devlist == NULL || olen == NULL)
		return (FIDO_ERR_INVALID_ARGUMENT);

	*olen = 0;

	if (ilen == 0)
		return (FIDO_OK); /* nothing to do */

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

/*
 * Workaround for NetBSD (as of 201910) bug that loses
 * sync of DATA0/DATA1 sequence bit across uhid open/close.
 * Send pings until we get a response - early pings with incorrect
 * sequence bits will be ignored as duplicate packets by the device.
 */
static int
terrible_ping_kludge(struct hid_netbsd *ctx)
{
	u_char data[256];
	int i, n;
	struct pollfd pfd;

	if (sizeof(data) < ctx->report_out_len + 1)
		return -1;
	for (i = 0; i < 4; i++) {
		memset(data, 0, sizeof(data));
		/* broadcast channel ID */
		data[1] = 0xff;
		data[2] = 0xff;
		data[3] = 0xff;
		data[4] = 0xff;
		/* Ping command */
		data[5] = 0x81;
		/* One byte ping only, Vasili */
		data[6] = 0;
		data[7] = 1;
		fido_log_debug("%s: send ping %d", __func__, i);
		if (fido_hid_write(ctx, data, ctx->report_out_len + 1) == -1)
			return -1;
		fido_log_debug("%s: wait reply", __func__);
		memset(&pfd, 0, sizeof(pfd));
		pfd.fd = ctx->fd;
		pfd.events = POLLIN;
		if ((n = poll(&pfd, 1, 100)) == -1) {
			fido_log_error(errno, "%s: poll", __func__);
			return -1;
		} else if (n == 0) {
			fido_log_debug("%s: timed out", __func__);
			continue;
		}
		if (fido_hid_read(ctx, data, ctx->report_out_len, 250) == -1)
			return -1;
		/*
		 * Ping isn't always supported on the broadcast channel,
		 * so we might get an error, but we don't care - we're
		 * synched now.
		 */
		fido_log_xxd(data, ctx->report_out_len, "%s: got reply",
		    __func__);
		return 0;
	}
	fido_log_debug("%s: no response", __func__);
	return -1;
}

void *
fido_hid_open(const char *path)
{
	struct hid_netbsd		*ctx;
	struct usb_ctl_report_desc	 ucrd;
	int				 r;

	memset(&ucrd, 0, sizeof(ucrd));

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL ||
	    (ctx->fd = fido_hid_unix_open(path)) == -1) {
		free(ctx);
		return (NULL);
	}

	if ((r = ioctl(ctx->fd, IOCTL_REQ(USB_GET_REPORT_DESC), &ucrd)) == -1 ||
	    ucrd.ucrd_size < 0 ||
	    (size_t)ucrd.ucrd_size > sizeof(ucrd.ucrd_data) ||
	    fido_hid_get_report_len(ucrd.ucrd_data, (size_t)ucrd.ucrd_size,
		&ctx->report_in_len, &ctx->report_out_len) < 0) {
		if (r == -1)
			fido_log_error(errno, "%s: ioctl", __func__);
		fido_log_debug("%s: using default report sizes", __func__);
		ctx->report_in_len = CTAP_MAX_REPORT_LEN;
		ctx->report_out_len = CTAP_MAX_REPORT_LEN;
	}

	/*
	 * NetBSD has a bug that causes it to lose
	 * track of the DATA0/DATA1 sequence toggle across uhid device
	 * open and close. This is a terrible hack to work around it.
	 */
	if (!is_fido(ctx->fd) || terrible_ping_kludge(ctx) != 0) {
		fido_hid_close(ctx);
		return NULL;
	}

	return (ctx);
}

void
fido_hid_close(void *handle)
{
	struct hid_netbsd *ctx = handle;

	if (close(ctx->fd) == -1)
		fido_log_error(errno, "%s: close", __func__);

	free(ctx);
}

int
fido_hid_set_sigmask(void *handle, const fido_sigset_t *sigmask)
{
	struct hid_netbsd *ctx = handle;

	ctx->sigmask = *sigmask;
	ctx->sigmaskp = &ctx->sigmask;

	return (FIDO_OK);
}

int
fido_hid_read(void *handle, unsigned char *buf, size_t len, int ms)
{
	struct hid_netbsd	*ctx = handle;
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
		fido_log_error(errno, "%s: %zd != %zu", __func__, r, len);
		return (-1);
	}

	return ((int)r);
}

int
fido_hid_write(void *handle, const unsigned char *buf, size_t len)
{
	struct hid_netbsd	*ctx = handle;
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
		fido_log_error(errno, "%s: %zd != %zu", __func__, r, len - 1);
		return (-1);
	}

	return ((int)len);
}

size_t
fido_hid_report_in_len(void *handle)
{
	struct hid_netbsd *ctx = handle;

	return (ctx->report_in_len);
}

size_t
fido_hid_report_out_len(void *handle)
{
	struct hid_netbsd *ctx = handle;

	return (ctx->report_out_len);
}
