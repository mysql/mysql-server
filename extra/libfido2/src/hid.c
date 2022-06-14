/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <string.h>
#include "fido.h"

fido_dev_info_t *
fido_dev_info_new(size_t n)
{
	return (calloc(n, sizeof(fido_dev_info_t)));
}

void
fido_dev_info_free(fido_dev_info_t **devlist_p, size_t n)
{
	fido_dev_info_t *devlist;

	if (devlist_p == NULL || (devlist = *devlist_p) == NULL)
		return;

	for (size_t i = 0; i < n; i++) {
		const fido_dev_info_t *di = &devlist[i];
		free(di->path);
		free(di->manufacturer);
		free(di->product);
	}

	free(devlist);

	*devlist_p = NULL;
}

const fido_dev_info_t *
fido_dev_info_ptr(const fido_dev_info_t *devlist, size_t i)
{
	return (&devlist[i]);
}

const char *
fido_dev_info_path(const fido_dev_info_t *di)
{
	return (di->path);
}

int16_t
fido_dev_info_vendor(const fido_dev_info_t *di)
{
	return (di->vendor_id);
}

int16_t
fido_dev_info_product(const fido_dev_info_t *di)
{
	return (di->product_id);
}

const char *
fido_dev_info_manufacturer_string(const fido_dev_info_t *di)
{
	return (di->manufacturer);
}

const char *
fido_dev_info_product_string(const fido_dev_info_t *di)
{
	return (di->product);
}
