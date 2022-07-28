/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <openssl/bn.h>
#include <openssl/obj_mac.h>

#include "fido.h"
#include "fido/eddsa.h"

#if defined(LIBRESSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER < 0x10101000L
EVP_PKEY *
EVP_PKEY_new_raw_public_key(int type, ENGINE *e, const unsigned char *key,
    size_t keylen)
{
	(void)type;
	(void)e;
	(void)key;
	(void)keylen;

	fido_log_debug("%s: unimplemented", __func__);

	return (NULL);
}

int
EVP_PKEY_get_raw_public_key(const EVP_PKEY *pkey, unsigned char *pub,
    size_t *len)
{
	(void)pkey;
	(void)pub;
	(void)len;

	fido_log_debug("%s: unimplemented", __func__);

	return (0);
}

int
EVP_DigestVerify(EVP_MD_CTX *ctx, const unsigned char *sigret, size_t siglen,
    const unsigned char *tbs, size_t tbslen)
{
	(void)ctx;
	(void)sigret;
	(void)siglen;
	(void)tbs;
	(void)tbslen;

	fido_log_debug("%s: unimplemented", __func__);

	return (0);
}
#endif /* LIBRESSL_VERSION_NUMBER || OPENSSL_VERSION_NUMBER < 0x10101000L */

#if OPENSSL_VERSION_NUMBER < 0x10100000L
EVP_MD_CTX *
EVP_MD_CTX_new(void)
{
	fido_log_debug("%s: unimplemented", __func__);

	return (NULL);
}

void
EVP_MD_CTX_free(EVP_MD_CTX *ctx)
{
	(void)ctx;
}
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */

static int
decode_coord(const cbor_item_t *item, void *xy, size_t xy_len)
{
	if (cbor_isa_bytestring(item) == false ||
	    cbor_bytestring_is_definite(item) == false ||
	    cbor_bytestring_length(item) != xy_len) {
		fido_log_debug("%s: cbor type", __func__);
		return (-1);
	}

	memcpy(xy, cbor_bytestring_handle(item), xy_len);

	return (0);
}

static int
decode_pubkey_point(const cbor_item_t *key, const cbor_item_t *val, void *arg)
{
	eddsa_pk_t *k = arg;

	if (cbor_isa_negint(key) == false ||
	    cbor_int_get_width(key) != CBOR_INT_8)
		return (0); /* ignore */

	switch (cbor_get_uint8(key)) {
	case 1: /* x coordinate */
		return (decode_coord(val, &k->x, sizeof(k->x)));
	}

	return (0); /* ignore */
}

int
eddsa_pk_decode(const cbor_item_t *item, eddsa_pk_t *k)
{
	if (cbor_isa_map(item) == false ||
	    cbor_map_is_definite(item) == false ||
	    cbor_map_iter(item, k, decode_pubkey_point) < 0) {
		fido_log_debug("%s: cbor type", __func__);
		return (-1);
	}

	return (0);
}

eddsa_pk_t *
eddsa_pk_new(void)
{
	return (calloc(1, sizeof(eddsa_pk_t)));
}

void
eddsa_pk_free(eddsa_pk_t **pkp)
{
	eddsa_pk_t *pk;

	if (pkp == NULL || (pk = *pkp) == NULL)
		return;

	freezero(pk, sizeof(*pk));
	*pkp = NULL;
}

int
eddsa_pk_from_ptr(eddsa_pk_t *pk, const void *ptr, size_t len)
{
	if (len < sizeof(*pk))
		return (FIDO_ERR_INVALID_ARGUMENT);

	memcpy(pk, ptr, sizeof(*pk));

	return (FIDO_OK);
}

EVP_PKEY *
eddsa_pk_to_EVP_PKEY(const eddsa_pk_t *k)
{
	EVP_PKEY *pkey = NULL;

	if ((pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, k->x,
	    sizeof(k->x))) == NULL)
		fido_log_debug("%s: EVP_PKEY_new_raw_public_key", __func__);

	return (pkey);
}

int
eddsa_pk_from_EVP_PKEY(eddsa_pk_t *pk, const EVP_PKEY *pkey)
{
	size_t len = 0;

	if (EVP_PKEY_get_raw_public_key(pkey, NULL, &len) != 1 ||
	    len != sizeof(pk->x))
		return (FIDO_ERR_INTERNAL);
	if (EVP_PKEY_get_raw_public_key(pkey, pk->x, &len) != 1 ||
	    len != sizeof(pk->x))
		return (FIDO_ERR_INTERNAL);

	return (FIDO_OK);
}
