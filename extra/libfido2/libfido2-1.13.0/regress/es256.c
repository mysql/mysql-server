/*
 * Copyright (c) 2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#undef NDEBUG

#include <assert.h>
#include <string.h>

#define _FIDO_INTERNAL

#include <fido.h>
#include <fido/es256.h>

#include <openssl/bio.h>
#include <openssl/pem.h>

#define ASSERT_NOT_NULL(e)	assert((e) != NULL)
#define ASSERT_NULL(e)		assert((e) == NULL)
#define ASSERT_INVAL(e)		assert((e) == FIDO_ERR_INVALID_ARGUMENT)
#define ASSERT_OK(e)		assert((e) == FIDO_OK)

static const char short_x[] = \
"-----BEGIN PUBLIC KEY-----\n"
"MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEAAeeHTZj4LEbt7Czs+u5gEZJfnGE\n"
"6Z+YLe4AYu7SoGY7IH/2jKifsA7w+lkURL4DL63oEjd3f8foH9bX4eaVug==\n"
"-----END PUBLIC KEY-----";

static const char short_y[] = \
"-----BEGIN PUBLIC KEY-----\n"
"MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEL8CWUP1r0tpJ5QmkzLc69O74C/Ti\n"
"83hTiys/JFNVkp0ArW3pKt5jNRrgWSZYE4S/D3AMtpqifFXz/FLCzJqojQ==\n"
"-----END PUBLIC KEY-----\n";

static const char p256k1[] = \
"-----BEGIN PUBLIC KEY-----\n"
"MFYwEAYHKoZIzj0CAQYFK4EEAAoDQgAEU1y8c0Jg9FGr3vYChpEo9c4dpkijriYM\n"
"QzU/DeskC89hZjLNH1Sj8ra2MsBlVGGJTNPCZSyx8Jo7ERapxdN7UQ==\n"
"-----END PUBLIC KEY-----\n";

static const char p256v1[] = \
"-----BEGIN PUBLIC KEY-----\n"
"MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEOwiq14c80b7C1Jzsx5w1zMvk2GgW\n"
"5kfGMOKXjwF/U+51ZfBDKehs3ivdeXAJBkxIh7E3iA32s+HyNqk+ntl9fg==\n"
"-----END PUBLIC KEY-----\n";

static const unsigned char p256k1_raw[] = {
	0x04, 0x53, 0x5c, 0xbc, 0x73, 0x42, 0x60, 0xf4,
	0x51, 0xab, 0xde, 0xf6, 0x02, 0x86, 0x91, 0x28,
	0xf5, 0xce, 0x1d, 0xa6, 0x48, 0xa3, 0xae, 0x26,
	0x0c, 0x43, 0x35, 0x3f, 0x0d, 0xeb, 0x24, 0x0b,
	0xcf, 0x61, 0x66, 0x32, 0xcd, 0x1f, 0x54, 0xa3,
	0xf2, 0xb6, 0xb6, 0x32, 0xc0, 0x65, 0x54, 0x61,
	0x89, 0x4c, 0xd3, 0xc2, 0x65, 0x2c, 0xb1, 0xf0,
	0x9a, 0x3b, 0x11, 0x16, 0xa9, 0xc5, 0xd3, 0x7b,
	0x51,
};

static const unsigned char p256v1_raw[] = {
	0x04, 0x3b, 0x08, 0xaa, 0xd7, 0x87, 0x3c, 0xd1,
	0xbe, 0xc2, 0xd4, 0x9c, 0xec, 0xc7, 0x9c, 0x35,
	0xcc, 0xcb, 0xe4, 0xd8, 0x68, 0x16, 0xe6, 0x47,
	0xc6, 0x30, 0xe2, 0x97, 0x8f, 0x01, 0x7f, 0x53,
	0xee, 0x75, 0x65, 0xf0, 0x43, 0x29, 0xe8, 0x6c,
	0xde, 0x2b, 0xdd, 0x79, 0x70, 0x09, 0x06, 0x4c,
	0x48, 0x87, 0xb1, 0x37, 0x88, 0x0d, 0xf6, 0xb3,
	0xe1, 0xf2, 0x36, 0xa9, 0x3e, 0x9e, 0xd9, 0x7d,
	0x7e,
};

static EVP_PKEY *
EVP_PKEY_from_PEM(const char *ptr, size_t len)
{
	BIO *bio = NULL;
	EVP_PKEY *pkey = NULL;

	if ((bio = BIO_new(BIO_s_mem())) == NULL) {
		warnx("BIO_new");
		goto out;
	}
	if (len > INT_MAX || BIO_write(bio, ptr, (int)len) != (int)len) {
		warnx("BIO_write");
		goto out;
	}
	if ((pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL)) == NULL)
		warnx("PEM_read_bio_PUBKEY");
out:
	BIO_free(bio);

	return pkey;
}

static int
es256_pk_cmp(const char *ptr, size_t len)
{
	EVP_PKEY *pkA = NULL;
	EVP_PKEY *pkB = NULL;
	es256_pk_t *k = NULL;
	int r, ok = -1;

	if ((pkA = EVP_PKEY_from_PEM(ptr, len)) == NULL) {
		warnx("EVP_PKEY_from_PEM");
		goto out;
	}
	if ((k = es256_pk_new()) == NULL) {
		warnx("es256_pk_new");
		goto out;
	}
	if ((r = es256_pk_from_EVP_PKEY(k, pkA)) != FIDO_OK) {
		warnx("es256_pk_from_EVP_PKEY: 0x%x", r);
		goto out;
	}
	if ((pkB = es256_pk_to_EVP_PKEY(k)) == NULL) {
		warnx("es256_pk_to_EVP_PKEY");
		goto out;
	}
	if ((r = EVP_PKEY_cmp(pkA, pkB)) != 1) {
		warnx("EVP_PKEY_cmp: %d", r);
		goto out;
	}

	ok = 0;
out:
	EVP_PKEY_free(pkA);
	EVP_PKEY_free(pkB);
	es256_pk_free(&k);

	return ok;
}

static void
short_coord(void)
{
	assert(es256_pk_cmp(short_x, sizeof(short_x)) == 0);
	assert(es256_pk_cmp(short_y, sizeof(short_y)) == 0);
}

static void
invalid_curve(const unsigned char *raw, size_t raw_len)
{
	EVP_PKEY *pkey;
	es256_pk_t *pk;

	ASSERT_NOT_NULL((pkey = EVP_PKEY_from_PEM(p256k1, sizeof(p256k1))));
	ASSERT_NOT_NULL((pk = es256_pk_new()));
	ASSERT_INVAL(es256_pk_from_EVP_PKEY(pk, pkey));
	ASSERT_INVAL(es256_pk_from_ptr(pk, raw, raw_len));
	ASSERT_NULL(es256_pk_to_EVP_PKEY((const es256_pk_t *)raw));

	EVP_PKEY_free(pkey);
	es256_pk_free(&pk);
}

static void
full_coord(void)
{
	assert(es256_pk_cmp(p256v1, sizeof(p256v1)) == 0);
}

static void
valid_curve(const unsigned char *raw, size_t raw_len)
{
	EVP_PKEY *pkeyA;
	EVP_PKEY *pkeyB;
	es256_pk_t *pkA;
	es256_pk_t *pkB;

	ASSERT_NOT_NULL((pkeyA = EVP_PKEY_from_PEM(p256v1, sizeof(p256v1))));
	ASSERT_NOT_NULL((pkA = es256_pk_new()));
	ASSERT_NOT_NULL((pkB = es256_pk_new()));
	ASSERT_OK(es256_pk_from_EVP_PKEY(pkA, pkeyA));
	ASSERT_OK(es256_pk_from_ptr(pkB, raw, raw_len));
	ASSERT_NOT_NULL((pkeyB = es256_pk_to_EVP_PKEY(pkB)));
	assert(EVP_PKEY_cmp(pkeyA, pkeyB) == 1);

	EVP_PKEY_free(pkeyA);
	EVP_PKEY_free(pkeyB);
	es256_pk_free(&pkA);
	es256_pk_free(&pkB);
}

int
main(void)
{
	fido_init(0);

	short_coord();
	full_coord();

	invalid_curve(p256k1_raw, sizeof(p256k1_raw)); /* uncompressed */
	invalid_curve(p256k1_raw + 1, sizeof(p256k1_raw) - 1); /* libfido2 */
	valid_curve(p256v1_raw, sizeof(p256v1_raw)); /* uncompressed */
	valid_curve(p256v1_raw + 1, sizeof(p256v1_raw) - 1); /* libfido2 */

	exit(0);
}
