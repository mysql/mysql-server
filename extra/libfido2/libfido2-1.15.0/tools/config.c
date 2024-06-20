/*
 * Copyright (c) 2020 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fido.h>
#include <fido/config.h>

#include "../openbsd-compat/openbsd-compat.h"
#include "extern.h"

int
config_entattest(char *path)
{
	fido_dev_t *dev;
	char *pin = NULL;
	int r, ok = 1;

	dev = open_dev(path);
	if ((r = fido_dev_enable_entattest(dev, NULL)) != FIDO_OK &&
	    should_retry_with_pin(dev, r)) {
		if ((pin = get_pin(path)) == NULL)
			goto out;
		r = fido_dev_enable_entattest(dev, pin);
		freezero(pin, PINBUF_LEN);
		pin = NULL;
	}
	if (r != FIDO_OK) {
		warnx("fido_dev_enable_entattest: %s (0x%x)",
		    fido_strerr(r), r);
		goto out;
	}

	ok = 0;
out:
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

int
config_always_uv(char *path, int toggle)
{
	fido_dev_t *dev;
	char *pin = NULL;
	int v, r, ok = 1;

	dev = open_dev(path);
	if (get_devopt(dev, "alwaysUv", &v) < 0) {
		warnx("%s: getdevopt", __func__);
		goto out;
	}
	if (v == -1) {
		warnx("%s: option not found", __func__);
		goto out;
	}
	if (v == toggle) {
		ok = 0;
		goto out;
	}
	if ((r = fido_dev_toggle_always_uv(dev, NULL)) != FIDO_OK &&
	    should_retry_with_pin(dev, r)) {
		if ((pin = get_pin(path)) == NULL)
			goto out;
		r = fido_dev_toggle_always_uv(dev, pin);
		freezero(pin, PINBUF_LEN);
		pin = NULL;
	}
	if (r != FIDO_OK) {
		warnx("fido_dev_toggle_always_uv: %s (0x%x)",
		    fido_strerr(r), r);
		goto out;
	}

	ok = 0;
out:
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

int
config_pin_minlen(char *path, const char *pinlen)
{
	fido_dev_t *dev;
	char *pin = NULL;
	int len, r, ok = 1;

	dev = open_dev(path);
	if ((len = base10(pinlen)) < 0 || len > 63) {
		warnx("%s: len > 63", __func__);
		goto out;
	}
	if ((r = fido_dev_set_pin_minlen(dev, (size_t)len, NULL)) != FIDO_OK &&
	    should_retry_with_pin(dev, r)) {
		if ((pin = get_pin(path)) == NULL)
			goto out;
		r = fido_dev_set_pin_minlen(dev, (size_t)len, pin);
		freezero(pin, PINBUF_LEN);
		pin = NULL;
	}
	if (r != FIDO_OK) {
		warnx("fido_dev_set_pin_minlen: %s (0x%x)", fido_strerr(r), r);
		goto out;
	}

	ok = 0;
out:
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

int
config_force_pin_change(char *path)
{
	fido_dev_t *dev;
	char *pin = NULL;
	int r, ok = 1;

	dev = open_dev(path);
	if ((r = fido_dev_force_pin_change(dev, NULL)) != FIDO_OK &&
	    should_retry_with_pin(dev, r)) {
		if ((pin = get_pin(path)) == NULL)
			goto out;
		r = fido_dev_force_pin_change(dev, pin);
		freezero(pin, PINBUF_LEN);
		pin = NULL;
	}
	if (r != FIDO_OK) {
		warnx("fido_dev_force_pin_change: %s (0x%x)", fido_strerr(r), r);
		goto out;
	}

	ok = 0;
out:
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

int
config_pin_minlen_rpid(char *path, const char *rpids)
{
	fido_dev_t *dev;
	char *otmp, *tmp, *cp;
	char *pin = NULL, **rpid = NULL;
	int r, ok = 1;
	size_t n;

	if ((tmp = strdup(rpids)) == NULL)
		err(1, "strdup");
	otmp = tmp;
	for (n = 0; (cp = strsep(&tmp, ",")) != NULL; n++) {
		if (n == SIZE_MAX || (rpid = recallocarray(rpid, n, n + 1,
		    sizeof(*rpid))) == NULL)
			err(1, "recallocarray");
		if ((rpid[n] = strdup(cp)) == NULL)
			err(1, "strdup");
		if (*rpid[n] == '\0')
			errx(1, "empty rpid");
	}
	free(otmp);
	if (rpid == NULL || n == 0)
		errx(1, "could not parse rp_id");
	dev = open_dev(path);
	if ((r = fido_dev_set_pin_minlen_rpid(dev, (const char * const *)rpid,
	    n, NULL)) != FIDO_OK && should_retry_with_pin(dev, r)) {
		if ((pin = get_pin(path)) == NULL)
			goto out;
		r = fido_dev_set_pin_minlen_rpid(dev, (const char * const *)rpid,
		    n, pin);
		freezero(pin, PINBUF_LEN);
		pin = NULL;
	}
	if (r != FIDO_OK) {
		warnx("fido_dev_set_pin_minlen_rpid: %s (0x%x)",
		    fido_strerr(r), r);
		goto out;
	}

	ok = 0;
out:
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}
