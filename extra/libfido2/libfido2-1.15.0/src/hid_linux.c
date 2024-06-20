/*
 * Copyright (c) 2019-2024 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#include <linux/hidraw.h>
#include <linux/input.h>

#include <errno.h>
#include <libudev.h>
#include <time.h>
#include <unistd.h>

#include "fido.h"

struct hid_linux {
	int             fd;
	size_t          report_in_len;
	size_t          report_out_len;
	sigset_t        sigmask;
	const sigset_t *sigmaskp;
};

static int
get_report_descriptor(int fd, struct hidraw_report_descriptor *hrd)
{
	int s = -1;

	if (ioctl(fd, IOCTL_REQ(HIDIOCGRDESCSIZE), &s) == -1) {
		fido_log_error(errno, "%s: ioctl HIDIOCGRDESCSIZE", __func__);
		return (-1);
	}

	if (s < 0 || (unsigned)s > HID_MAX_DESCRIPTOR_SIZE) {
		fido_log_debug("%s: HIDIOCGRDESCSIZE %d", __func__, s);
		return (-1);
	}

	hrd->size = (unsigned)s;

	if (ioctl(fd, IOCTL_REQ(HIDIOCGRDESC), hrd) == -1) {
		fido_log_error(errno, "%s: ioctl HIDIOCGRDESC", __func__);
		return (-1);
	}

	return (0);
}

static bool
is_fido(const char *path)
{
	int				 fd = -1;
	uint32_t			 usage_page = 0;
	struct hidraw_report_descriptor	*hrd = NULL;

	if ((hrd = calloc(1, sizeof(*hrd))) == NULL ||
	    (fd = fido_hid_unix_open(path)) == -1)
		goto out;
	if (get_report_descriptor(fd, hrd) < 0 ||
	    fido_hid_get_usage(hrd->value, hrd->size, &usage_page) < 0)
		usage_page = 0;

out:
	free(hrd);

	if (fd != -1 && close(fd) == -1)
		fido_log_error(errno, "%s: close", __func__);

	return (usage_page == 0xf1d0);
}

static int
parse_uevent(const char *uevent, int *bus, int16_t *vendor_id,
    int16_t *product_id, char **hid_name)
{
	char			*cp;
	char			*p;
	char			*s;
	bool			 found_id = false;
	bool			 found_name = false;
	short unsigned int	 x;
	short unsigned int	 y;
	short unsigned int	 z;

	if ((s = cp = strdup(uevent)) == NULL)
		return (-1);

	while ((p = strsep(&cp, "\n")) != NULL && *p != '\0') {
		if (!found_id && strncmp(p, "HID_ID=", 7) == 0) {
			if (sscanf(p + 7, "%hx:%hx:%hx", &x, &y, &z) == 3) {
				*bus = (int)x;
				*vendor_id = (int16_t)y;
				*product_id = (int16_t)z;
				found_id = true;
			}
		} else if (!found_name && strncmp(p, "HID_NAME=", 9) == 0) {
			if ((*hid_name = strdup(p + 9)) != NULL)
				found_name = true;
		}
	}

	free(s);

	if (!found_name || !found_id)
		return (-1);

	return (0);
}

static char *
get_parent_attr(struct udev_device *dev, const char *subsystem,
    const char *devtype, const char *attr)
{
	struct udev_device	*parent;
	const char		*value;

	if ((parent = udev_device_get_parent_with_subsystem_devtype(dev,
	    subsystem, devtype)) == NULL || (value =
	    udev_device_get_sysattr_value(parent, attr)) == NULL)
		return (NULL);

	return (strdup(value));
}

static char *
get_usb_attr(struct udev_device *dev, const char *attr)
{
	return (get_parent_attr(dev, "usb", "usb_device", attr));
}

static int
copy_info(fido_dev_info_t *di, struct udev *udev,
    struct udev_list_entry *udev_entry)
{
	const char		*name;
	const char		*path;
	char			*uevent = NULL;
	struct udev_device	*dev = NULL;
	int			 bus = 0;
	char			*hid_name = NULL;
	int			 ok = -1;

	memset(di, 0, sizeof(*di));

	if ((name = udev_list_entry_get_name(udev_entry)) == NULL ||
	    (dev = udev_device_new_from_syspath(udev, name)) == NULL ||
	    (path = udev_device_get_devnode(dev)) == NULL ||
	    is_fido(path) == 0)
		goto fail;

	if ((uevent = get_parent_attr(dev, "hid", NULL, "uevent")) == NULL ||
	    parse_uevent(uevent, &bus, &di->vendor_id, &di->product_id,
	    &hid_name) < 0) {
		fido_log_debug("%s: uevent", __func__);
		goto fail;
	}

#ifndef FIDO_HID_ANY
	if (bus != BUS_USB) {
		fido_log_debug("%s: bus", __func__);
		goto fail;
	}
#endif

	di->path = strdup(path);
	di->manufacturer = get_usb_attr(dev, "manufacturer");
	di->product = get_usb_attr(dev, "product");

	if (di->manufacturer == NULL && di->product == NULL) {
		di->product = hid_name;  /* fallback */
		hid_name = NULL;
	}
	if (di->manufacturer == NULL)
		di->manufacturer = strdup("");
	if (di->product == NULL)
		di->product = strdup("");
	if (di->path == NULL || di->manufacturer == NULL || di->product == NULL)
		goto fail;

	ok = 0;
fail:
	if (dev != NULL)
		udev_device_unref(dev);

	free(uevent);
	free(hid_name);

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
	struct udev		*udev = NULL;
	struct udev_enumerate	*udev_enum = NULL;
	struct udev_list_entry	*udev_list;
	struct udev_list_entry	*udev_entry;
	int			 r = FIDO_ERR_INTERNAL;

	*olen = 0;

	if (ilen == 0)
		return (FIDO_OK); /* nothing to do */

	if (devlist == NULL)
		return (FIDO_ERR_INVALID_ARGUMENT);

	if ((udev = udev_new()) == NULL ||
	    (udev_enum = udev_enumerate_new(udev)) == NULL)
		goto fail;

	if (udev_enumerate_add_match_subsystem(udev_enum, "hidraw") < 0 ||
	    udev_enumerate_scan_devices(udev_enum) < 0)
		goto fail;

	if ((udev_list = udev_enumerate_get_list_entry(udev_enum)) == NULL) {
		r = FIDO_OK; /* zero hidraw devices */
		goto fail;
	}

	udev_list_entry_foreach(udev_entry, udev_list) {
		if (copy_info(&devlist[*olen], udev, udev_entry) == 0) {
			devlist[*olen].io = (fido_dev_io_t) {
				fido_hid_open,
				fido_hid_close,
				fido_hid_read,
				fido_hid_write,
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

	return (r);
}

void *
fido_hid_open(const char *path)
{
	struct hid_linux *ctx;
	struct hidraw_report_descriptor *hrd;
	struct timespec tv_pause;
	long interval_ms, retries = 0;
	bool looped;

retry:
	looped = false;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL ||
	    (ctx->fd = fido_hid_unix_open(path)) == -1) {
		free(ctx);
		return (NULL);
	}

	while (flock(ctx->fd, LOCK_EX|LOCK_NB) == -1) {
		if (errno != EWOULDBLOCK) {
			fido_log_error(errno, "%s: flock", __func__);
			fido_hid_close(ctx);
			return (NULL);
		}
		looped = true;
		if (retries++ >= 20) {
			fido_log_debug("%s: flock timeout", __func__);
			fido_hid_close(ctx);
			return (NULL);
		}
		interval_ms = retries * 100000000L;
		tv_pause.tv_sec = interval_ms / 1000000000L;
		tv_pause.tv_nsec = interval_ms % 1000000000L;
		if (nanosleep(&tv_pause, NULL) == -1) {
			fido_log_error(errno, "%s: nanosleep", __func__);
			fido_hid_close(ctx);
			return (NULL);
		}
	}

	if (looped) {
		fido_log_debug("%s: retrying", __func__);
		fido_hid_close(ctx);
		goto retry;
	}

	if ((hrd = calloc(1, sizeof(*hrd))) == NULL ||
	    get_report_descriptor(ctx->fd, hrd) < 0 ||
	    fido_hid_get_report_len(hrd->value, hrd->size, &ctx->report_in_len,
	    &ctx->report_out_len) < 0 || ctx->report_in_len == 0 ||
	    ctx->report_out_len == 0) {
		fido_log_debug("%s: using default report sizes", __func__);
		ctx->report_in_len = CTAP_MAX_REPORT_LEN;
		ctx->report_out_len = CTAP_MAX_REPORT_LEN;
	}

	free(hrd);

	return (ctx);
}

void
fido_hid_close(void *handle)
{
	struct hid_linux *ctx = handle;

	if (close(ctx->fd) == -1)
		fido_log_error(errno, "%s: close", __func__);

	free(ctx);
}

int
fido_hid_set_sigmask(void *handle, const fido_sigset_t *sigmask)
{
	struct hid_linux *ctx = handle;

	ctx->sigmask = *sigmask;
	ctx->sigmaskp = &ctx->sigmask;

	return (FIDO_OK);
}

int
fido_hid_read(void *handle, unsigned char *buf, size_t len, int ms)
{
	struct hid_linux	*ctx = handle;
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
	struct hid_linux	*ctx = handle;
	ssize_t			 r;

	if (len != ctx->report_out_len + 1) {
		fido_log_debug("%s: len %zu", __func__, len);
		return (-1);
	}

	if ((r = write(ctx->fd, buf, len)) == -1) {
		fido_log_error(errno, "%s: write", __func__);
		return (-1);
	}

	if (r < 0 || (size_t)r != len) {
		fido_log_debug("%s: %zd != %zu", __func__, r, len);
		return (-1);
	}

	return ((int)r);
}

size_t
fido_hid_report_in_len(void *handle)
{
	struct hid_linux *ctx = handle;

	return (ctx->report_in_len);
}

size_t
fido_hid_report_out_len(void *handle)
{
	struct hid_linux *ctx = handle;

	return (ctx->report_out_len);
}
