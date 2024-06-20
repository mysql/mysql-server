/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Configure a PIN on a given authenticator.
 */

#include <fido.h>
#include <stdio.h>
#include <stdlib.h>

#include "../openbsd-compat/openbsd-compat.h"

static void
setpin(const char *path, const char *pin, const char *oldpin)
{
	fido_dev_t *dev;
	int r;

	fido_init(0);

	if ((dev = fido_dev_new()) == NULL)
		errx(1, "fido_dev_new");

	if ((r = fido_dev_open(dev, path)) != FIDO_OK)
		errx(1, "fido_dev_open: %s (0x%x)", fido_strerr(r), r);

	if ((r = fido_dev_set_pin(dev, pin, oldpin)) != FIDO_OK)
		errx(1, "fido_dev_set_pin: %s (0x%x)", fido_strerr(r), r);

	if ((r = fido_dev_close(dev)) != FIDO_OK)
		errx(1, "fido_dev_close: %s (0x%x)", fido_strerr(r), r);

	fido_dev_free(&dev);
}

int
main(int argc, char **argv)
{
	if (argc < 3 || argc > 4) {
		fprintf(stderr, "usage: setpin <pin> [oldpin] <device>\n");
		exit(EXIT_FAILURE);
	}

	if (argc == 3)
		setpin(argv[2], argv[1], NULL);
	else
		setpin(argv[3], argv[1], argv[2]);

	exit(0);
}
