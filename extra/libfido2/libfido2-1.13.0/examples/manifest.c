/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <fido.h>
#include <stdio.h>
#include <stdlib.h>

#include "../openbsd-compat/openbsd-compat.h"

int
main(void)
{
	fido_dev_info_t	*devlist;
	size_t		 ndevs;
	int		 r;

	fido_init(0);

	if ((devlist = fido_dev_info_new(64)) == NULL)
		errx(1, "fido_dev_info_new");

	if ((r = fido_dev_info_manifest(devlist, 64, &ndevs)) != FIDO_OK)
		errx(1, "fido_dev_info_manifest: %s (0x%x)", fido_strerr(r), r);

	for (size_t i = 0; i < ndevs; i++) {
		const fido_dev_info_t *di = fido_dev_info_ptr(devlist, i);
		printf("%s: vendor=0x%04x, product=0x%04x (%s %s)\n",
		    fido_dev_info_path(di),
		    (uint16_t)fido_dev_info_vendor(di),
		    (uint16_t)fido_dev_info_product(di),
		    fido_dev_info_manufacturer_string(di),
		    fido_dev_info_product_string(di));
	}

	fido_dev_info_free(&devlist, ndevs);

	exit(0);
}
