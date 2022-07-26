/*
 * Copyright (c) 2020 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <zlib.h>
#include "fido.h"

#define BOUND (1024UL * 1024UL)

static int
do_compress(fido_blob_t *out, const fido_blob_t *in, size_t origsiz, int decomp)
{
	u_long ilen, olen;
	int r;

	memset(out, 0, sizeof(*out));
	if (in->len > ULONG_MAX || (ilen = (u_long)in->len) > BOUND ||
	    origsiz > ULONG_MAX || (olen = decomp ? (u_long)origsiz :
	    compressBound(ilen)) > BOUND)
		return FIDO_ERR_INVALID_ARGUMENT;
	if ((out->ptr = calloc(1, olen)) == NULL)
		return FIDO_ERR_INTERNAL;
	out->len = olen;
	if (decomp)
		r = uncompress(out->ptr, &olen, in->ptr, ilen);
	else
		r = compress(out->ptr, &olen, in->ptr, ilen);
	if (r != Z_OK || olen > SIZE_MAX || olen > out->len) {
		fido_blob_reset(out);
		return FIDO_ERR_COMPRESS;
	}
	out->len = olen;

	return FIDO_OK;
}

int
fido_compress(fido_blob_t *out, const fido_blob_t *in)
{
	return do_compress(out, in, 0, 0);
}

int
fido_uncompress(fido_blob_t *out, const fido_blob_t *in, size_t origsiz)
{
	return do_compress(out, in, origsiz, 1);
}
