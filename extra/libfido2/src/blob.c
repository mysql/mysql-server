/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <string.h>
#include "fido.h"

fido_blob_t *
fido_blob_new(void)
{
	return (calloc(1, sizeof(fido_blob_t)));
}

int
fido_blob_set(fido_blob_t *b, const unsigned char *ptr, size_t len)
{
	if (b->ptr != NULL) {
		explicit_bzero(b->ptr, b->len);
		free(b->ptr);
		b->ptr = NULL;
	}

	b->len = 0;

	if (ptr == NULL || len == 0) {
		fido_log_debug("%s: ptr=%p, len=%zu", __func__,
		    (const void *)ptr, len);
		return (-1);
	}

	if ((b->ptr = malloc(len)) == NULL) {
		fido_log_debug("%s: malloc", __func__);
		return (-1);
	}

	memcpy(b->ptr, ptr, len);
	b->len = len;

	return (0);
}

void
fido_blob_free(fido_blob_t **bp)
{
	fido_blob_t *b;

	if (bp == NULL || (b = *bp) == NULL)
		return;

	if (b->ptr) {
		explicit_bzero(b->ptr, b->len);
		free(b->ptr);
	}

	explicit_bzero(b, sizeof(*b));
	free(b);

	*bp = NULL;
}

void
fido_free_blob_array(fido_blob_array_t *array)
{
	if (array->ptr == NULL)
		return;

	for (size_t i = 0; i < array->len; i++) {
		fido_blob_t *b = &array->ptr[i];
		if (b->ptr != NULL) {
			explicit_bzero(b->ptr, b->len);
			free(b->ptr);
			b->ptr = NULL;
		}
	}

	free(array->ptr);
	array->ptr = NULL;
	array->len = 0;
}

cbor_item_t *
fido_blob_encode(const fido_blob_t *b)
{
	if (b == NULL || b->ptr == NULL)
		return (NULL);

	return (cbor_build_bytestring(b->ptr, b->len));
}

int
fido_blob_decode(const cbor_item_t *item, fido_blob_t *b)
{
	return (cbor_bytestring_copy(item, &b->ptr, &b->len));
}

int
fido_blob_is_empty(const fido_blob_t *b)
{
	return (b->ptr == NULL || b->len == 0);
}
