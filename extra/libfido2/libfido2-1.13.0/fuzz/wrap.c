/*
 * Copyright (c) 2019-2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/random.h>
#include <sys/socket.h>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <cbor.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#include "mutator_aux.h"

extern int prng_up;

int fuzz_save_corpus;

/*
 * Build wrappers around functions of interest, and have them fail
 * in a pseudo-random manner. A uniform probability of 0.25% (1/400)
 * allows for a depth of log(0.5)/log(399/400) > 276 operations
 * before simulated errors become statistically more likely. 
 */

#define WRAP(type, name, args, retval, param, prob)	\
extern type __wrap_##name args;				\
extern type __real_##name args;				\
type __wrap_##name args {				\
	if (prng_up && uniform_random(400) < (prob)) {	\
		return (retval);			\
	}						\
							\
	return (__real_##name param);			\
}

WRAP(void *,
	malloc,
	(size_t size),
	NULL,
	(size),
	1
)

WRAP(void *,
	calloc,
	(size_t nmemb, size_t size),
	NULL,
	(nmemb, size),
	1
)

WRAP(void *,
	realloc,
	(void *ptr, size_t size),
	NULL,
	(ptr, size),
	1
)

WRAP(char *,
	strdup,
	(const char *s),
	NULL,
	(s),
	1
)

WRAP(ssize_t,
	getrandom,
	(void *buf, size_t buflen, unsigned int flags),
	-1,
	(buf, buflen, flags),
	1
)

WRAP(int,
	EVP_Cipher,
	(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in,
	    unsigned int inl),
	-1,
	(ctx, out, in, inl),
	1
)

WRAP(int,
	EVP_CIPHER_CTX_ctrl,
	(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr),
	0,
	(ctx, type, arg, ptr),
	1
)

WRAP(EVP_CIPHER_CTX *,
	EVP_CIPHER_CTX_new,
	(void),
	NULL,
	(),
	1
)

WRAP(int,
	EVP_CipherInit,
	(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
	    const unsigned char *key, const unsigned char *iv, int enc),
	0,
	(ctx, cipher, key, iv, enc),
	1
)

WRAP(RSA *,
	EVP_PKEY_get0_RSA,
	(EVP_PKEY *pkey),
	NULL,
	(pkey),
	1
)

WRAP(EC_KEY *,
	EVP_PKEY_get0_EC_KEY,
	(EVP_PKEY *pkey),
	NULL,
	(pkey),
	1
)

WRAP(int,
	EVP_PKEY_get_raw_public_key,
	(const EVP_PKEY *pkey, unsigned char *pub, size_t *len),
	0,
	(pkey, pub, len),
	1
)

WRAP(EVP_MD_CTX *,
	EVP_MD_CTX_new,
	(void),
	NULL,
	(),
	1
)

WRAP(int,
	EVP_DigestVerifyInit,
	(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx, const EVP_MD *type, ENGINE *e,
	    EVP_PKEY *pkey),
	0,
	(ctx, pctx, type, e, pkey),
	1
)

WRAP(int,
	EVP_DigestInit_ex,
	(EVP_MD_CTX *ctx, const EVP_MD *type, ENGINE *impl),
	0,
	(ctx, type, impl),
	1
)

WRAP(int,
	EVP_DigestUpdate,
	(EVP_MD_CTX *ctx, const void *data, size_t count),
	0,
	(ctx, data, count),
	1
)

WRAP(int,
	EVP_DigestFinal_ex,
	(EVP_MD_CTX *ctx, unsigned char *md, unsigned int *isize),
	0,
	(ctx, md, isize),
	1
)

WRAP(BIGNUM *,
	BN_bin2bn,
	(const unsigned char *s, int len, BIGNUM *ret),
	NULL,
	(s, len, ret),
	1
)

WRAP(int,
	BN_bn2bin,
	(const BIGNUM *a, unsigned char *to),
	-1,
	(a, to),
	1
)

WRAP(BIGNUM *,
	BN_CTX_get,
	(BN_CTX *ctx),
	NULL,
	(ctx),
	1
)

WRAP(BN_CTX *,
	BN_CTX_new,
	(void),
	NULL,
	(),
	1
)

WRAP(BIGNUM *,
	BN_new,
	(void),
	NULL,
	(),
	1
)

WRAP(RSA *,
	RSA_new,
	(void),
	NULL,
	(),
	1
)

WRAP(int,
	RSA_set0_key,
	(RSA *r, BIGNUM *n, BIGNUM *e, BIGNUM *d),
	0,
	(r, n, e, d),
	1
)

WRAP(int,
	RSA_pkey_ctx_ctrl,
	(EVP_PKEY_CTX *ctx, int optype, int cmd, int p1, void *p2),
	-1,
	(ctx, optype, cmd, p1, p2),
	1
)

WRAP(EC_KEY *,
	EC_KEY_new_by_curve_name,
	(int nid),
	NULL,
	(nid),
	1
)

WRAP(const EC_GROUP *,
	EC_KEY_get0_group,
	(const EC_KEY *key),
	NULL,
	(key),
	1
)

WRAP(const BIGNUM *,
	EC_KEY_get0_private_key,
	(const EC_KEY *key),
	NULL,
	(key),
	1
)

WRAP(EC_POINT *,
	EC_POINT_new,
	(const EC_GROUP *group),
	NULL,
	(group),
	1
)

WRAP(int,
	EC_POINT_get_affine_coordinates_GFp,
	(const EC_GROUP *group, const EC_POINT *p, BIGNUM *x, BIGNUM *y, BN_CTX *ctx),
	0,
	(group, p, x, y, ctx),
	1
)

WRAP(EVP_PKEY *,
	EVP_PKEY_new,
	(void),
	NULL,
	(),
	1
)

WRAP(int,
	EVP_PKEY_assign,
	(EVP_PKEY *pkey, int type, void *key),
	0,
	(pkey, type, key),
	1
)

WRAP(int,
	EVP_PKEY_keygen_init,
	(EVP_PKEY_CTX *ctx),
	0,
	(ctx),
	1
)

WRAP(int,
	EVP_PKEY_keygen,
	(EVP_PKEY_CTX *ctx, EVP_PKEY **ppkey),
	0,
	(ctx, ppkey),
	1
)

WRAP(int,
	EVP_PKEY_paramgen_init,
	(EVP_PKEY_CTX *ctx),
	0,
	(ctx),
	1
)

WRAP(int,
	EVP_PKEY_paramgen,
	(EVP_PKEY_CTX *ctx, EVP_PKEY **ppkey),
	0,
	(ctx, ppkey),
	1
)

WRAP(EVP_PKEY *,
	EVP_PKEY_new_raw_public_key,
	(int type, ENGINE *e, const unsigned char *key, size_t keylen),
	NULL,
	(type, e, key, keylen),
	1
)

WRAP(EVP_PKEY_CTX *,
	EVP_PKEY_CTX_new,
	(EVP_PKEY *pkey, ENGINE *e),
	NULL,
	(pkey, e),
	1
)

WRAP(EVP_PKEY_CTX *,
	EVP_PKEY_CTX_new_id,
	(int id, ENGINE *e),
	NULL,
	(id, e),
	1
)

WRAP(int,
	EVP_PKEY_derive,
	(EVP_PKEY_CTX *ctx, unsigned char *key, size_t *pkeylen),
	0,
	(ctx, key, pkeylen),
	1
)

WRAP(int,
	EVP_PKEY_derive_init,
	(EVP_PKEY_CTX *ctx),
	0,
	(ctx),
	1
)

WRAP(int,
	EVP_PKEY_derive_set_peer,
	(EVP_PKEY_CTX *ctx, EVP_PKEY *peer),
	0,
	(ctx, peer),
	1
)

WRAP(int,
	EVP_PKEY_verify_init,
	(EVP_PKEY_CTX *ctx),
	0,
	(ctx),
	1
)

WRAP(int,
	EVP_PKEY_CTX_ctrl,
	(EVP_PKEY_CTX *ctx, int keytype, int optype, int cmd, int p1, void *p2),
	-1,
	(ctx, keytype, optype, cmd, p1, p2),
	1
)

WRAP(const EVP_MD *,
	EVP_sha1,
	(void),
	NULL,
	(),
	1
)

WRAP(const EVP_MD *,
	EVP_sha256,
	(void),
	NULL,
	(),
	1
)

WRAP(const EVP_CIPHER *,
	EVP_aes_256_cbc,
	(void),
	NULL,
	(),
	1
)

WRAP(const EVP_CIPHER *,
	EVP_aes_256_gcm,
	(void),
	NULL,
	(),
	1
)

WRAP(unsigned char *,
	HMAC,
	(const EVP_MD *evp_md, const void *key, int key_len,
	    const unsigned char *d, int n, unsigned char *md,
	    unsigned int *md_len),
	NULL,
	(evp_md, key, key_len, d, n, md, md_len),
	1
)

WRAP(HMAC_CTX *,
	HMAC_CTX_new,
	(void),
	NULL,
	(),
	1
)

WRAP(int,
	HMAC_Init_ex,
	(HMAC_CTX *ctx, const void *key, int key_len, const EVP_MD *md,
	    ENGINE *impl),
	0,
	(ctx, key, key_len, md, impl),
	1
)

WRAP(int,
	HMAC_Update,
	(HMAC_CTX *ctx, const unsigned char *data, int len),
	0,
	(ctx, data, len),
	1
)

WRAP(int,
	HMAC_Final,
	(HMAC_CTX *ctx, unsigned char *md, unsigned int *len),
	0,
	(ctx, md, len),
	1
)

WRAP(unsigned char *,
	SHA1,
	(const unsigned char *d, size_t n, unsigned char *md),
	NULL,
	(d, n, md),
	1
)

WRAP(unsigned char *,
	SHA256,
	(const unsigned char *d, size_t n, unsigned char *md),
	NULL,
	(d, n, md),
	1
)

WRAP(cbor_item_t *,
	cbor_build_string,
	(const char *val),
	NULL,
	(val),
	1
)

WRAP(cbor_item_t *,
	cbor_build_bytestring,
	(cbor_data handle, size_t length),
	NULL,
	(handle, length),
	1
)

WRAP(cbor_item_t *,
	cbor_build_bool,
	(bool value),
	NULL,
	(value),
	1
)

WRAP(cbor_item_t *,
	cbor_build_negint8,
	(uint8_t value),
	NULL,
	(value),
	1
)

WRAP(cbor_item_t *,
	cbor_build_negint16,
	(uint16_t value),
	NULL,
	(value),
	1
)

WRAP(cbor_item_t *,
	cbor_load,
	(cbor_data source, size_t source_size, struct cbor_load_result *result),
	NULL,
	(source, source_size, result),
	1
)

WRAP(cbor_item_t *,
	cbor_build_uint8,
	(uint8_t value),
	NULL,
	(value),
	1
)

WRAP(cbor_item_t *,
	cbor_build_uint16,
	(uint16_t value),
	NULL,
	(value),
	1
)

WRAP(cbor_item_t *,
	cbor_build_uint32,
	(uint32_t value),
	NULL,
	(value),
	1
)

WRAP(cbor_item_t *,
	cbor_build_uint64,
	(uint64_t value),
	NULL,
	(value),
	1
)

WRAP(struct cbor_pair *,
	cbor_map_handle,
	(const cbor_item_t *item),
	NULL,
	(item),
	1
)

WRAP(cbor_item_t **,
	cbor_array_handle,
	(const cbor_item_t *item),
	NULL,
	(item),
	1
)

WRAP(bool,
	cbor_array_push,
	(cbor_item_t *array, cbor_item_t *pushee),
	false,
	(array, pushee),
	1
)

WRAP(bool,
	cbor_map_add,
	(cbor_item_t *item, struct cbor_pair pair),
	false,
	(item, pair),
	1
)

WRAP(cbor_item_t *,
	cbor_new_definite_map,
	(size_t size),
	NULL,
	(size),
	1
)

WRAP(cbor_item_t *,
	cbor_new_definite_array,
	(size_t size),
	NULL,
	(size),
	1
)

WRAP(cbor_item_t *,
	cbor_new_definite_bytestring,
	(void),
	NULL,
	(),
	1
)

WRAP(size_t,
	cbor_serialize_alloc,
	(const cbor_item_t *item, cbor_mutable_data *buffer,
	    size_t *buffer_size),
	0,
	(item, buffer, buffer_size),
	1
)

WRAP(int,
	fido_tx,
	(fido_dev_t *d, uint8_t cmd, const void *buf, size_t count, int *ms),
	-1,
	(d, cmd, buf, count, ms),
	1
)

WRAP(int,
	bind,
	(int sockfd, const struct sockaddr *addr, socklen_t addrlen),
	-1,
	(sockfd, addr, addrlen),
	1
)

WRAP(int,
	deflateInit2_,
	(z_streamp strm, int level, int method, int windowBits, int memLevel,
	    int strategy, const char *version, int stream_size),
	Z_STREAM_ERROR,
	(strm, level, method, windowBits, memLevel, strategy, version,
	    stream_size),
	1
)

int __wrap_deflate(z_streamp, int);
int __real_deflate(z_streamp, int);

int
__wrap_deflate(z_streamp strm, int flush)
{
	if (prng_up && uniform_random(400) < 1) {
		return Z_BUF_ERROR;
	}
	/* should never happen, but we check for it */
	if (prng_up && uniform_random(400) < 1) {
		strm->avail_out = UINT_MAX;
		return Z_STREAM_END;
	}

	return __real_deflate(strm, flush);
}

int __wrap_asprintf(char **, const char *, ...);

int
__wrap_asprintf(char **strp, const char *fmt, ...)
{
	va_list ap;
	int r;

	if (prng_up && uniform_random(400) < 1) {
		*strp = (void *)0xdeadbeef;
		return -1;
	}

	va_start(ap, fmt);
	r = vasprintf(strp, fmt, ap);
	va_end(ap);

	return r;
}
