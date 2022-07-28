/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <openssl/bn.h>
#include <openssl/obj_mac.h>

#include "fido.h"
#include "fido/es256.h"

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
	es256_pk_t *k = arg;

	if (cbor_isa_negint(key) == false ||
	    cbor_int_get_width(key) != CBOR_INT_8)
		return (0); /* ignore */

	switch (cbor_get_uint8(key)) {
	case 1: /* x coordinate */
		return (decode_coord(val, &k->x, sizeof(k->x)));
	case 2: /* y coordinate */
		return (decode_coord(val, &k->y, sizeof(k->y)));
	}

	return (0); /* ignore */
}

int
es256_pk_decode(const cbor_item_t *item, es256_pk_t *k)
{
	if (cbor_isa_map(item) == false ||
	    cbor_map_is_definite(item) == false ||
	    cbor_map_iter(item, k, decode_pubkey_point) < 0) {
		fido_log_debug("%s: cbor type", __func__);
		return (-1);
	}

	return (0);
}

cbor_item_t *
es256_pk_encode(const es256_pk_t *pk, int ecdh)
{
	cbor_item_t		*item = NULL;
	struct cbor_pair	 argv[5];
	int			 alg;
	int			 ok = -1;

	memset(argv, 0, sizeof(argv));

	if ((item = cbor_new_definite_map(5)) == NULL)
		goto fail;

	/* kty */
	if ((argv[0].key = cbor_build_uint8(1)) == NULL ||
	    (argv[0].value = cbor_build_uint8(2)) == NULL ||
	    !cbor_map_add(item, argv[0]))
		goto fail;

	/*
	 * "The COSEAlgorithmIdentifier used is -25 (ECDH-ES +
	 * HKDF-256) although this is NOT the algorithm actually
	 * used. Setting this to a different value may result in
	 * compatibility issues."
	 */
	if (ecdh)
		alg = COSE_ECDH_ES256;
	else
		alg = COSE_ES256;

	/* alg */
	if ((argv[1].key = cbor_build_uint8(3)) == NULL ||
	    (argv[1].value = cbor_build_negint8((uint8_t)(-alg - 1))) == NULL ||
	    !cbor_map_add(item, argv[1]))
		goto fail;

	/* crv */
	if ((argv[2].key = cbor_build_negint8(0)) == NULL ||
	    (argv[2].value = cbor_build_uint8(1)) == NULL ||
	    !cbor_map_add(item, argv[2]))
		goto fail;

	/* x */
	if ((argv[3].key = cbor_build_negint8(1)) == NULL ||
	    (argv[3].value = cbor_build_bytestring(pk->x,
	    sizeof(pk->x))) == NULL || !cbor_map_add(item, argv[3]))
		goto fail;

	/* y */
	if ((argv[4].key = cbor_build_negint8(2)) == NULL ||
	    (argv[4].value = cbor_build_bytestring(pk->y,
	    sizeof(pk->y))) == NULL || !cbor_map_add(item, argv[4]))
		goto fail;

	ok = 0;
fail:
	if (ok < 0) {
		if (item != NULL) {
			cbor_decref(&item);
			item = NULL;
		}
	}

	for (size_t i = 0; i < 5; i++) {
		if (argv[i].key)
			cbor_decref(&argv[i].key);
		if (argv[i].value)
			cbor_decref(&argv[i].value);
	}

	return (item);
}

es256_sk_t *
es256_sk_new(void)
{
	return (calloc(1, sizeof(es256_sk_t)));
}

void
es256_sk_free(es256_sk_t **skp)
{
	es256_sk_t *sk;

	if (skp == NULL || (sk = *skp) == NULL)
		return;

	freezero(sk, sizeof(*sk));
	*skp = NULL;
}

es256_pk_t *
es256_pk_new(void)
{
	return (calloc(1, sizeof(es256_pk_t)));
}

void
es256_pk_free(es256_pk_t **pkp)
{
	es256_pk_t *pk;

	if (pkp == NULL || (pk = *pkp) == NULL)
		return;

	freezero(pk, sizeof(*pk));
	*pkp = NULL;
}

int
es256_pk_from_ptr(es256_pk_t *pk, const void *ptr, size_t len)
{
	const uint8_t *p = ptr;

	if (len < sizeof(*pk))
		return (FIDO_ERR_INVALID_ARGUMENT);

	if (len == sizeof(*pk) + 1 && *p == 0x04)
		memcpy(pk, ++p, sizeof(*pk)); /* uncompressed format */
	else
		memcpy(pk, ptr, sizeof(*pk)); /* libfido2 x||y format */

	return (FIDO_OK);
}

int
es256_pk_set_x(es256_pk_t *pk, const unsigned char *x)
{
	memcpy(pk->x, x, sizeof(pk->x));

	return (0);
}

int
es256_pk_set_y(es256_pk_t *pk, const unsigned char *y)
{
	memcpy(pk->y, y, sizeof(pk->y));

	return (0);
}

int
es256_sk_create(es256_sk_t *key)
{
	EVP_PKEY_CTX	*pctx = NULL;
	EVP_PKEY_CTX	*kctx = NULL;
	EVP_PKEY	*p = NULL;
	EVP_PKEY	*k = NULL;
	const EC_KEY	*ec;
	const BIGNUM	*d;
	const int	 nid = NID_X9_62_prime256v1;
	int		 n;
	int		 ok = -1;

	if ((pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL)) == NULL ||
	    EVP_PKEY_paramgen_init(pctx) <= 0 ||
	    EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, nid) <= 0 ||
	    EVP_PKEY_paramgen(pctx, &p) <= 0) {
		fido_log_debug("%s: EVP_PKEY_paramgen", __func__);
		goto fail;
	}

	if ((kctx = EVP_PKEY_CTX_new(p, NULL)) == NULL ||
	    EVP_PKEY_keygen_init(kctx) <= 0 || EVP_PKEY_keygen(kctx, &k) <= 0) {
		fido_log_debug("%s: EVP_PKEY_keygen", __func__);
		goto fail;
	}

	if ((ec = EVP_PKEY_get0_EC_KEY(k)) == NULL ||
	    (d = EC_KEY_get0_private_key(ec)) == NULL ||
	    (n = BN_num_bytes(d)) < 0 || (size_t)n > sizeof(key->d) ||
	    (n = BN_bn2bin(d, key->d)) < 0 || (size_t)n > sizeof(key->d)) {
		fido_log_debug("%s: EC_KEY_get0_private_key", __func__);
		goto fail;
	}

	ok = 0;
fail:
	if (p != NULL)
		EVP_PKEY_free(p);
	if (k != NULL)
		EVP_PKEY_free(k);
	if (pctx != NULL)
		EVP_PKEY_CTX_free(pctx);
	if (kctx != NULL)
		EVP_PKEY_CTX_free(kctx);

	return (ok);
}

EVP_PKEY *
es256_pk_to_EVP_PKEY(const es256_pk_t *k)
{
	BN_CTX		*bnctx = NULL;
	EC_KEY		*ec = NULL;
	EC_POINT	*q = NULL;
	EVP_PKEY	*pkey = NULL;
	BIGNUM		*x = NULL;
	BIGNUM		*y = NULL;
	const EC_GROUP	*g = NULL;
	const int	 nid = NID_X9_62_prime256v1;
	int		 ok = -1;

	if ((bnctx = BN_CTX_new()) == NULL)
		goto fail;

	BN_CTX_start(bnctx);

	if ((x = BN_CTX_get(bnctx)) == NULL ||
	    (y = BN_CTX_get(bnctx)) == NULL)
		goto fail;

	if (BN_bin2bn(k->x, sizeof(k->x), x) == NULL ||
	    BN_bin2bn(k->y, sizeof(k->y), y) == NULL) {
		fido_log_debug("%s: BN_bin2bn", __func__);
		goto fail;
	}

	if ((ec = EC_KEY_new_by_curve_name(nid)) == NULL ||
	    (g = EC_KEY_get0_group(ec)) == NULL) {
		fido_log_debug("%s: EC_KEY init", __func__);
		goto fail;
	}

	if ((q = EC_POINT_new(g)) == NULL ||
	    EC_POINT_set_affine_coordinates_GFp(g, q, x, y, bnctx) == 0 ||
	    EC_KEY_set_public_key(ec, q) == 0) {
		fido_log_debug("%s: EC_KEY_set_public_key", __func__);
		goto fail;
	}

	if ((pkey = EVP_PKEY_new()) == NULL ||
	    EVP_PKEY_assign_EC_KEY(pkey, ec) == 0) {
		fido_log_debug("%s: EVP_PKEY_assign_EC_KEY", __func__);
		goto fail;
	}

	ec = NULL; /* at this point, ec belongs to evp */

	ok = 0;
fail:
	if (bnctx != NULL) {
		BN_CTX_end(bnctx);
		BN_CTX_free(bnctx);
	}

	if (ec != NULL)
		EC_KEY_free(ec);
	if (q != NULL)
		EC_POINT_free(q);

	if (ok < 0 && pkey != NULL) {
		EVP_PKEY_free(pkey);
		pkey = NULL;
	}

	return (pkey);
}

int
es256_pk_from_EC_KEY(es256_pk_t *pk, const EC_KEY *ec)
{
	BN_CTX		*bnctx = NULL;
	BIGNUM		*x = NULL;
	BIGNUM		*y = NULL;
	const EC_POINT	*q = NULL;
	const EC_GROUP	*g = NULL;
	int		 ok = FIDO_ERR_INTERNAL;
	int		 n;

	if ((q = EC_KEY_get0_public_key(ec)) == NULL ||
	    (g = EC_KEY_get0_group(ec)) == NULL ||
	    (bnctx = BN_CTX_new()) == NULL)
		goto fail;

	BN_CTX_start(bnctx);

	if ((x = BN_CTX_get(bnctx)) == NULL ||
	    (y = BN_CTX_get(bnctx)) == NULL)
		goto fail;

	if (EC_POINT_get_affine_coordinates_GFp(g, q, x, y, bnctx) == 0 ||
	    (n = BN_num_bytes(x)) < 0 || (size_t)n > sizeof(pk->x) ||
	    (n = BN_num_bytes(y)) < 0 || (size_t)n > sizeof(pk->y)) {
		fido_log_debug("%s: EC_POINT_get_affine_coordinates_GFp",
		    __func__);
		goto fail;
	}

	if ((n = BN_bn2bin(x, pk->x)) < 0 || (size_t)n > sizeof(pk->x) ||
	    (n = BN_bn2bin(y, pk->y)) < 0 || (size_t)n > sizeof(pk->y)) {
		fido_log_debug("%s: BN_bn2bin", __func__);
		goto fail;
	}

	ok = FIDO_OK;
fail:
	if (bnctx != NULL) {
		BN_CTX_end(bnctx);
		BN_CTX_free(bnctx);
	}

	return (ok);
}

EVP_PKEY *
es256_sk_to_EVP_PKEY(const es256_sk_t *k)
{
	BN_CTX		*bnctx = NULL;
	EC_KEY		*ec = NULL;
	EVP_PKEY	*pkey = NULL;
	BIGNUM		*d = NULL;
	const int	 nid = NID_X9_62_prime256v1;
	int		 ok = -1;

	if ((bnctx = BN_CTX_new()) == NULL)
		goto fail;

	BN_CTX_start(bnctx);

	if ((d = BN_CTX_get(bnctx)) == NULL ||
	    BN_bin2bn(k->d, sizeof(k->d), d) == NULL) {
		fido_log_debug("%s: BN_bin2bn", __func__);
		goto fail;
	}

	if ((ec = EC_KEY_new_by_curve_name(nid)) == NULL ||
	    EC_KEY_set_private_key(ec, d) == 0) {
		fido_log_debug("%s: EC_KEY_set_private_key", __func__);
		goto fail;
	}

	if ((pkey = EVP_PKEY_new()) == NULL ||
	    EVP_PKEY_assign_EC_KEY(pkey, ec) == 0) {
		fido_log_debug("%s: EVP_PKEY_assign_EC_KEY", __func__);
		goto fail;
	}

	ec = NULL; /* at this point, ec belongs to evp */

	ok = 0;
fail:
	if (bnctx != NULL) {
		BN_CTX_end(bnctx);
		BN_CTX_free(bnctx);
	}

	if (ec != NULL)
		EC_KEY_free(ec);

	if (ok < 0 && pkey != NULL) {
		EVP_PKEY_free(pkey);
		pkey = NULL;
	}

	return (pkey);
}

int
es256_derive_pk(const es256_sk_t *sk, es256_pk_t *pk)
{
	BIGNUM		*d = NULL;
	EC_KEY		*ec = NULL;
	EC_POINT	*q = NULL;
	const EC_GROUP	*g = NULL;
	const int	 nid = NID_X9_62_prime256v1;
	int		 ok = -1;

	if ((d = BN_bin2bn(sk->d, (int)sizeof(sk->d), NULL)) == NULL ||
	    (ec = EC_KEY_new_by_curve_name(nid)) == NULL ||
	    (g = EC_KEY_get0_group(ec)) == NULL ||
	    (q = EC_POINT_new(g)) == NULL) {
		fido_log_debug("%s: get", __func__);
		goto fail;
	}

	if (EC_POINT_mul(g, q, d, NULL, NULL, NULL) == 0 ||
	    EC_KEY_set_public_key(ec, q) == 0 ||
	    es256_pk_from_EC_KEY(pk, ec) != FIDO_OK) {
		fido_log_debug("%s: set", __func__);
		goto fail;
	}

	ok = 0;
fail:
	if (d != NULL)
		BN_clear_free(d);
	if (q != NULL)
		EC_POINT_free(q);
	if (ec != NULL)
		EC_KEY_free(ec);

	return (ok);
}
