/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <openssl/evp.h>
#include <string.h>

#include "fido.h"

int
aes256_cbc_enc(const fido_blob_t *key, const fido_blob_t *in, fido_blob_t *out)
{
	EVP_CIPHER_CTX	*ctx = NULL;
	unsigned char	 iv[32];
	int		 len;
	int		 ok = -1;

	memset(iv, 0, sizeof(iv));
	out->ptr = NULL;
	out->len = 0;

	/* sanity check */
	if (in->len > INT_MAX || (in->len % 16) != 0 ||
	    (out->ptr = calloc(1, in->len)) == NULL) {
		fido_log_debug("%s: in->len=%zu", __func__, in->len);
		goto fail;
	}

	if ((ctx = EVP_CIPHER_CTX_new()) == NULL || key->len != 32 ||
	    !EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key->ptr, iv) ||
	    !EVP_CIPHER_CTX_set_padding(ctx, 0) ||
	    !EVP_EncryptUpdate(ctx, out->ptr, &len, in->ptr, (int)in->len) ||
	    len < 0 || (size_t)len != in->len) {
		fido_log_debug("%s: EVP_Encrypt", __func__);
		goto fail;
	}

	out->len = (size_t)len;

	ok = 0;
fail:
	if (ctx != NULL)
		EVP_CIPHER_CTX_free(ctx);

	if (ok < 0) {
		free(out->ptr);
		out->ptr = NULL;
		out->len = 0;
	}

	return (ok);
}

int
aes256_cbc_dec(const fido_blob_t *key, const fido_blob_t *in, fido_blob_t *out)
{
	EVP_CIPHER_CTX	*ctx = NULL;
	unsigned char	 iv[32];
	int		 len;
	int		 ok = -1;

	memset(iv, 0, sizeof(iv));
	out->ptr = NULL;
	out->len = 0;

	/* sanity check */
	if (in->len > INT_MAX || (in->len % 16) != 0 ||
	    (out->ptr = calloc(1, in->len)) == NULL) {
		fido_log_debug("%s: in->len=%zu", __func__, in->len);
		goto fail;
	}

	if ((ctx = EVP_CIPHER_CTX_new()) == NULL || key->len != 32 ||
	    !EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key->ptr, iv) ||
	    !EVP_CIPHER_CTX_set_padding(ctx, 0) ||
	    !EVP_DecryptUpdate(ctx, out->ptr, &len, in->ptr, (int)in->len) ||
	    len < 0 || (size_t)len > in->len + 32) {
		fido_log_debug("%s: EVP_Decrypt", __func__);
		goto fail;
	}

	out->len = (size_t)len;

	ok = 0;
fail:
	if (ctx != NULL)
		EVP_CIPHER_CTX_free(ctx);

	if (ok < 0) {
		free(out->ptr);
		out->ptr = NULL;
		out->len = 0;
	}

	return (ok);
}
