/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <openssl/bio.h>
#include <openssl/evp.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "../openbsd-compat/openbsd-compat.h"
#include "extern.h"

int
base64_encode(const void *ptr, size_t len, char **out)
{
	BIO  *bio_b64 = NULL;
	BIO  *bio_mem = NULL;
	char *b64_ptr = NULL;
	long  b64_len;
	int   n;
	int   ok = -1;

	if (ptr == NULL || out == NULL || len > INT_MAX)
		return (-1);

	*out = NULL;

	if ((bio_b64 = BIO_new(BIO_f_base64())) == NULL)
		goto fail;
	if ((bio_mem = BIO_new(BIO_s_mem())) == NULL)
		goto fail;

	BIO_set_flags(bio_b64, BIO_FLAGS_BASE64_NO_NL);
	BIO_push(bio_b64, bio_mem);

	n = BIO_write(bio_b64, ptr, (int)len);
	if (n < 0 || (size_t)n != len)
		goto fail;

	if (BIO_flush(bio_b64) < 0)
		goto fail;

	b64_len = BIO_get_mem_data(bio_b64, &b64_ptr);
	if (b64_len < 0 || (size_t)b64_len == SIZE_MAX || b64_ptr == NULL)
		goto fail;
	if ((*out = calloc(1, (size_t)b64_len + 1)) == NULL)
		goto fail;

	memcpy(*out, b64_ptr, (size_t)b64_len);
	ok = 0;

fail:
	BIO_free(bio_b64);
	BIO_free(bio_mem);

	return (ok);
}

int
base64_decode(const char *in, void **ptr, size_t *len)
{
	BIO    *bio_mem = NULL;
	BIO    *bio_b64 = NULL;
	size_t  alloc_len;
	int     n;
	int     ok = -1;

	if (in == NULL || ptr == NULL || len == NULL || strlen(in) > INT_MAX)
		return (-1);

	*ptr = NULL;
	*len = 0;

	if ((bio_b64 = BIO_new(BIO_f_base64())) == NULL)
		goto fail;
	if ((bio_mem = BIO_new_mem_buf((const void *)in, -1)) == NULL)
		goto fail;

	BIO_set_flags(bio_b64, BIO_FLAGS_BASE64_NO_NL);
	BIO_push(bio_b64, bio_mem);

	alloc_len = strlen(in);
	if ((*ptr = calloc(1, alloc_len)) == NULL)
		goto fail;

	n = BIO_read(bio_b64, *ptr, (int)alloc_len);
	if (n <= 0 || BIO_eof(bio_b64) == 0)
		goto fail;

	*len = (size_t)n;
	ok = 0;

fail:
	BIO_free(bio_b64);
	BIO_free(bio_mem);

	if (ok < 0) {
		free(*ptr);
		*ptr = NULL;
		*len = 0;
	}

	return (ok);
}

int
base64_read(FILE *f, struct blob *out)
{
	char *line = NULL;
	size_t linesize = 0;
	ssize_t n;

	out->ptr = NULL;
	out->len = 0;

	if ((n = getline(&line, &linesize, f)) <= 0 ||
	    (size_t)n != strlen(line)) {
		free(line); /* XXX should be free'd _even_ if getline() fails */
		return (-1);
	}

	if (base64_decode(line, (void **)&out->ptr, &out->len) < 0) {
		free(line);
		return (-1);
	}

	free(line);

	return (0);
}
