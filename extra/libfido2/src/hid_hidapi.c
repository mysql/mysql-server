/*
 * Copyright (c) 2019 Google LLC. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <fcntl.h>
#endif

#include <hidapi.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "fido.h"

struct hid_hidapi {
	void *handle;
	size_t report_in_len;
	size_t report_out_len;
};

static size_t
fido_wcslen(const wchar_t *wcs)
{
	size_t l = 0;
	while (*wcs++ != L'\0')
		l++;
	return l;
}

static char *
wcs_to_cs(const wchar_t *wcs)
{
	char *cs;
	size_t i;

	if (wcs == NULL || (cs = calloc(fido_wcslen(wcs) + 1, 1)) == NULL)
		return NULL;

	for (i = 0; i < fido_wcslen(wcs); i++) {
		if (wcs[i] >= 128) {
			/* give up on parsing non-ASCII text */
			free(cs);
			return strdup("hidapi device");
		}
		cs[i] = (char)wcs[i];
	}

	return cs;
}

static int
copy_info(fido_dev_info_t *di, const struct hid_device_info *d)
{
	memset(di, 0, sizeof(*di));

	if (d->path != NULL)
		di->path = strdup(d->path);
	else
		di->path = strdup("");

	if (d->manufacturer_string != NULL)
		di->manufacturer = wcs_to_cs(d->manufacturer_string);
	else
		di->manufacturer = strdup("");

	if (d->product_string != NULL)
		di->product = wcs_to_cs(d->product_string);
	else
		di->product = strdup("");

	if (di->path == NULL ||
	    di->manufacturer == NULL ||
	    di->product == NULL) {
		free(di->path);
		free(di->manufacturer);
		free(di->product);
		explicit_bzero(di, sizeof(*di));
		return -1;
	}

	di->product_id = (int16_t)d->product_id;
	di->vendor_id = (int16_t)d->vendor_id;
	di->io = (fido_dev_io_t) {
		&fido_hid_open,
		&fido_hid_close,
		&fido_hid_read,
		&fido_hid_write,
	};

	return 0;
}

#ifdef __linux__
static int
get_key_len(uint8_t tag, uint8_t *key, size_t *key_len)
{
	*key = tag & 0xfc;
	if ((*key & 0xf0) == 0xf0) {
		fido_log_debug("%s: *key=0x%02x", __func__, *key);
		return -1;
	}

	*key_len = tag & 0x3;
	if (*key_len == 3) {
		*key_len = 4;
	}

	return 0;
}

static int
get_key_val(const void *body, size_t key_len, uint32_t *val)
{
	const uint8_t *ptr = body;

	switch (key_len) {
	case 0:
		*val = 0;
		break;
	case 1:
		*val = ptr[0];
		break;
	case 2:
		*val = (uint32_t)((ptr[1] << 8) | ptr[0]);
		break;
	default:
		fido_log_debug("%s: key_len=%zu", __func__, key_len);
		return -1;
	}

	return 0;
}

static int
get_usage_info(const struct hidraw_report_descriptor *hrd, uint32_t *usage_page,
    uint32_t *usage)
{
	const uint8_t *ptr = hrd->value;
	size_t len = hrd->size;

	while (len > 0) {
		const uint8_t tag = ptr[0];

		ptr++;
		len--;

		uint8_t  key;
		size_t   key_len;
		uint32_t key_val;

		if (get_key_len(tag, &key, &key_len) < 0 || key_len > len ||
		    get_key_val(ptr, key_len, &key_val) < 0) {
			return -1;
		}

		if (key == 0x4) {
			*usage_page = key_val;
		} else if (key == 0x8) {
			*usage = key_val;
		}

		ptr += key_len;
		len -= key_len;
	}

	return 0;
}

static int
get_report_descriptor(const char *path, struct hidraw_report_descriptor *hrd)
{
	int fd;
	int s = -1;
	int ok = -1;

	if ((fd = open(path, O_RDONLY)) < 0) {
		fido_log_debug("%s: open", __func__);
		return -1;
	}

	if (ioctl(fd, HIDIOCGRDESCSIZE, &s) < 0 || s < 0 ||
	    (unsigned)s > HID_MAX_DESCRIPTOR_SIZE) {
		fido_log_debug("%s: ioctl HIDIOCGRDESCSIZE", __func__);
		goto fail;
	}

	hrd->size = (unsigned)s;

	if (ioctl(fd, HIDIOCGRDESC, hrd) < 0) {
		fido_log_debug("%s: ioctl HIDIOCGRDESC", __func__);
		goto fail;
	}

	ok = 0;
fail:
	if (fd != -1)
		close(fd);

	return ok;
}

static bool
is_fido(const struct hid_device_info *hdi)
{
	uint32_t usage = 0;
	uint32_t usage_page = 0;
	struct hidraw_report_descriptor hrd;

	memset(&hrd, 0, sizeof(hrd));

	if (get_report_descriptor(hdi->path, &hrd) < 0 ||
	    get_usage_info(&hrd, &usage_page, &usage) < 0) {
		return false;
	}

	return usage_page == 0xf1d0;
}
#elif defined(_WIN32) || defined(__APPLE__)
static bool
is_fido(const struct hid_device_info *hdi)
{
	return hdi->usage_page == 0xf1d0;
}
#else
static bool
is_fido(const struct hid_device_info *hdi)
{
	(void)hdi;
	fido_log_debug("%s: assuming FIDO HID", __func__);
	return true;
}
#endif

void *
fido_hid_open(const char *path)
{
	struct hid_hidapi *ctx;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL) {
		return (NULL);
	}

	if ((ctx->handle = hid_open_path(path)) == NULL) {
		free(ctx);
		return (NULL);
	}

	ctx->report_in_len = ctx->report_out_len = CTAP_MAX_REPORT_LEN;

	return ctx;
}

void
fido_hid_close(void *handle)
{
	struct hid_hidapi *ctx = handle;

	hid_close(ctx->handle);
	free(ctx);
}

int
fido_hid_read(void *handle, unsigned char *buf, size_t len, int ms)
{
	struct hid_hidapi *ctx = handle;

	if (len != ctx->report_in_len) {
		fido_log_debug("%s: len %zu", __func__, len);
		return -1;
	}

	return hid_read_timeout(ctx->handle, buf, len, ms);
}

int
fido_hid_write(void *handle, const unsigned char *buf, size_t len)
{
	struct hid_hidapi *ctx = handle;

	if (len != ctx->report_out_len + 1) {
		fido_log_debug("%s: len %zu", __func__, len);
		return -1;
	}

	return hid_write(ctx->handle, buf, len);
}

int
fido_hid_manifest(fido_dev_info_t *devlist, size_t ilen, size_t *olen)
{
	struct hid_device_info *hdi;

	*olen = 0;

	if (ilen == 0)
		return FIDO_OK; /* nothing to do */
	if (devlist == NULL)
		return FIDO_ERR_INVALID_ARGUMENT;
	if ((hdi = hid_enumerate(0, 0)) == NULL)
		return FIDO_OK; /* nothing to do */

	for (struct hid_device_info *d = hdi; d != NULL; d = d->next) {
		if (is_fido(d) == false)
			continue;
		if (copy_info(&devlist[*olen], d) == 0) {
			if (++(*olen) == ilen)
				break;
		}
	}

	hid_free_enumeration(hdi);

	return FIDO_OK;
}

size_t
fido_hid_report_in_len(void *handle)
{
	struct hid_hidapi *ctx = handle;

	return (ctx->report_in_len);
}

size_t
fido_hid_report_out_len(void *handle)
{
	struct hid_hidapi *ctx = handle;

	return (ctx->report_out_len);
}
