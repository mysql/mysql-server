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
#include <fido/eddsa.h>

#include <openssl/bio.h>
#include <openssl/pem.h>

#define ASSERT_NOT_NULL(e)	assert((e) != NULL)
#define ASSERT_NULL(e)		assert((e) == NULL)
#define ASSERT_INVAL(e)		assert((e) == FIDO_ERR_INVALID_ARGUMENT)
#define ASSERT_OK(e)		assert((e) == FIDO_OK)

static const char ecdsa[] = \
"-----BEGIN PUBLIC KEY-----\n"
"MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEOwiq14c80b7C1Jzsx5w1zMvk2GgW\n"
"5kfGMOKXjwF/U+51ZfBDKehs3ivdeXAJBkxIh7E3iA32s+HyNqk+ntl9fg==\n"
"-----END PUBLIC KEY-----\n";

static const char eddsa[] = \
"-----BEGIN PUBLIC KEY-----\n"
"MCowBQYDK2VwAyEADt/RHErAxAHxH9FUmsjOhQ2ALl6Y8nE0m3zQxkEE2iM=\n"
"-----END PUBLIC KEY-----\n";

static const unsigned char eddsa_raw[] = {
	0x0e, 0xdf, 0xd1, 0x1c, 0x4a, 0xc0, 0xc4, 0x01,
	0xf1, 0x1f, 0xd1, 0x54, 0x9a, 0xc8, 0xce, 0x85,
	0x0d, 0x80, 0x2e, 0x5e, 0x98, 0xf2, 0x71, 0x34,
	0x9b, 0x7c, 0xd0, 0xc6, 0x41, 0x04, 0xda, 0x23,
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
eddsa_pk_cmp(const char *ptr, size_t len)
{
	EVP_PKEY *pkA = NULL;
	EVP_PKEY *pkB = NULL;
	eddsa_pk_t *k = NULL;
	int r, ok = -1;

	if ((pkA = EVP_PKEY_from_PEM(ptr, len)) == NULL) {
		warnx("EVP_PKEY_from_PEM");
		goto out;
	}
	if ((k = eddsa_pk_new()) == NULL) {
		warnx("eddsa_pk_new");
		goto out;
	}
	if ((r = eddsa_pk_from_EVP_PKEY(k, pkA)) != FIDO_OK) {
		warnx("eddsa_pk_from_EVP_PKEY: 0x%x", r);
		goto out;
	}
	if ((pkB = eddsa_pk_to_EVP_PKEY(k)) == NULL) {
		warnx("eddsa_pk_to_EVP_PKEY");
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
	eddsa_pk_free(&k);

	return ok;
}

static void
invalid_key(void)
{
	EVP_PKEY *pkey;
	eddsa_pk_t *pk;

	ASSERT_NOT_NULL((pkey = EVP_PKEY_from_PEM(ecdsa, sizeof(ecdsa))));
	ASSERT_NOT_NULL((pk = eddsa_pk_new()));
	ASSERT_INVAL(eddsa_pk_from_EVP_PKEY(pk, pkey));

	EVP_PKEY_free(pkey);
	eddsa_pk_free(&pk);
}

static void
valid_key(void)
{
	EVP_PKEY *pkeyA = NULL;
	EVP_PKEY *pkeyB = NULL;
	eddsa_pk_t *pkA = NULL;
	eddsa_pk_t *pkB = NULL;

#if defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x3070000f
	/* incomplete support; test what we can */
	ASSERT_NULL(EVP_PKEY_from_PEM(eddsa, sizeof(eddsa)));
	ASSERT_NOT_NULL((pkB = eddsa_pk_new()));
	ASSERT_INVAL(eddsa_pk_from_ptr(pkB, eddsa_raw, sizeof(eddsa_raw)));
	ASSERT_NULL(eddsa_pk_to_EVP_PKEY((const eddsa_pk_t *)eddsa_raw));
	assert(eddsa_pk_cmp(eddsa, sizeof(eddsa)) < 0);
#else
	ASSERT_NOT_NULL((pkeyA = EVP_PKEY_from_PEM(eddsa, sizeof(eddsa))));
	ASSERT_NOT_NULL((pkA = eddsa_pk_new()));
	ASSERT_NOT_NULL((pkB = eddsa_pk_new()));
	ASSERT_OK(eddsa_pk_from_EVP_PKEY(pkA, pkeyA));
	ASSERT_OK(eddsa_pk_from_ptr(pkB, eddsa_raw, sizeof(eddsa_raw)));
	ASSERT_NOT_NULL((pkeyB = eddsa_pk_to_EVP_PKEY((const eddsa_pk_t *)eddsa_raw)));
	assert(EVP_PKEY_cmp(pkeyA, pkeyB) == 1);
	assert(eddsa_pk_cmp(eddsa, sizeof(eddsa)) == 0);
#endif

	EVP_PKEY_free(pkeyA);
	EVP_PKEY_free(pkeyB);
	eddsa_pk_free(&pkA);
	eddsa_pk_free(&pkB);
}

int
main(void)
{
	fido_init(0);

	invalid_key();
	valid_key();

	exit(0);
}
