/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <openssl/evp.h>
#include <openssl/sha.h>

#include "fido.h"
#include "fido/es256.h"

static int
do_ecdh(const es256_sk_t *sk, const es256_pk_t *pk, fido_blob_t **ecdh)
{
	EVP_PKEY	*pk_evp = NULL;
	EVP_PKEY	*sk_evp = NULL;
	EVP_PKEY_CTX	*ctx = NULL;
	fido_blob_t	*secret = NULL;
	int		 ok = -1;

	*ecdh = NULL;

	/* allocate blobs for secret & ecdh */
	if ((secret = fido_blob_new()) == NULL ||
	    (*ecdh = fido_blob_new()) == NULL)
		goto fail;

	/* wrap the keys as openssl objects */
	if ((pk_evp = es256_pk_to_EVP_PKEY(pk)) == NULL ||
	    (sk_evp = es256_sk_to_EVP_PKEY(sk)) == NULL) {
		fido_log_debug("%s: es256_to_EVP_PKEY", __func__);
		goto fail;
	}

	/* set ecdh parameters */
	if ((ctx = EVP_PKEY_CTX_new(sk_evp, NULL)) == NULL ||
	    EVP_PKEY_derive_init(ctx) <= 0 ||
	    EVP_PKEY_derive_set_peer(ctx, pk_evp) <= 0) {
		fido_log_debug("%s: EVP_PKEY_derive_init", __func__);
		goto fail;
	}

	/* perform ecdh */
	if (EVP_PKEY_derive(ctx, NULL, &secret->len) <= 0 ||
	    (secret->ptr = calloc(1, secret->len)) == NULL ||
	    EVP_PKEY_derive(ctx, secret->ptr, &secret->len) <= 0) {
		fido_log_debug("%s: EVP_PKEY_derive", __func__);
		goto fail;
	}

	/* use sha256 as a kdf on the resulting secret */
	(*ecdh)->len = SHA256_DIGEST_LENGTH;
	if (((*ecdh)->ptr = calloc(1, (*ecdh)->len)) == NULL ||
	    SHA256(secret->ptr, secret->len, (*ecdh)->ptr) != (*ecdh)->ptr) {
		fido_log_debug("%s: sha256", __func__);
		goto fail;
	}

	ok = 0;
fail:
	if (pk_evp != NULL)
		EVP_PKEY_free(pk_evp);
	if (sk_evp != NULL)
		EVP_PKEY_free(sk_evp);
	if (ctx != NULL)
		EVP_PKEY_CTX_free(ctx);
	if (ok < 0)
		fido_blob_free(ecdh);

	fido_blob_free(&secret);

	return (ok);
}

int
fido_do_ecdh(fido_dev_t *dev, es256_pk_t **pk, fido_blob_t **ecdh)
{
	es256_sk_t	*sk = NULL; /* our private key */
	es256_pk_t	*ak = NULL; /* authenticator's public key */
	int		 r;

	*pk = NULL; /* our public key; returned */
	*ecdh = NULL; /* shared ecdh secret; returned */

	if ((sk = es256_sk_new()) == NULL || (*pk = es256_pk_new()) == NULL) {
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	if (es256_sk_create(sk) < 0 || es256_derive_pk(sk, *pk) < 0) {
		fido_log_debug("%s: es256_derive_pk", __func__);
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	if ((ak = es256_pk_new()) == NULL ||
	    fido_dev_authkey(dev, ak) != FIDO_OK) {
		fido_log_debug("%s: fido_dev_authkey", __func__);
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	if (do_ecdh(sk, ak, ecdh) < 0) {
		fido_log_debug("%s: do_ecdh", __func__);
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	r = FIDO_OK;
fail:
	es256_sk_free(&sk);
	es256_pk_free(&ak);

	if (r != FIDO_OK) {
		es256_pk_free(pk);
		fido_blob_free(ecdh);
	}

	return (r);
}
