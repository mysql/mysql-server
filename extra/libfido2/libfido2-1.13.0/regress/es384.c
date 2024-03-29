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
#include <fido/es384.h>

#include <openssl/bio.h>
#include <openssl/pem.h>

#define ASSERT_NOT_NULL(e)	assert((e) != NULL)
#define ASSERT_NULL(e)		assert((e) == NULL)
#define ASSERT_INVAL(e)		assert((e) == FIDO_ERR_INVALID_ARGUMENT)
#define ASSERT_OK(e)		assert((e) == FIDO_OK)

static const char short_x[] = \
"-----BEGIN PUBLIC KEY-----\n"
"MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAEAAZ/VVCUmFU6aH9kJdDnUHCCglkatFTX\n"
"onMwIvNYyS8BW/HOoZiOQLs2Hg+qifwaP1pHKILzCVfFmWuZMhxhtmjNXFuOPDnS\n"
"Wa1PMdkCoWXA2BbXxnqL9v36gIOcFBil\n"
"-----END PUBLIC KEY-----";

static const char short_y[] = \
"-----BEGIN PUBLIC KEY-----\n"
"MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAEuDpRBAg87cnWVhxbWnaWlnj100w9pm5k\n"
"6T4eYToISaIhEK70TnGwULHX0+qHCYEGACOM7B/ZJbqjo6I7MIXaKZLemGi+tqvy\n"
"ajBAsTVSyrYBLQjTMMcaFmYmsxvFx7pK\n"
"-----END PUBLIC KEY-----\n";

static const char brainpoolP384r1[] = \
"-----BEGIN PUBLIC KEY-----\n"
"MHowFAYHKoZIzj0CAQYJKyQDAwIIAQELA2IABFKswbBzqqyZ4h1zz8rivqHzJxAO\n"
"XC2aLyC9x5gwBM7GVu8k6jkX7VypRpg3yyCneiIQ+vVCNXgbDchJ0cPVuhwm3Zru\n"
"AK49dezUPahWF0YiJRFVeV+KyB/MEaaZvinzqw==\n"
"-----END PUBLIC KEY-----\n";

static const char secp384r1[] = \
"-----BEGIN PUBLIC KEY-----\n"
"MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAEdJN9DoqPtTNAOmjnECHBIqnJgyBW0rct\n"
"tbUSqQjb6UG2lldmrQJbgCP/ywuXvkkJl4yfXxOr0UP3rgcnqTVA1/46s2TG+R5u\n"
"NSQbCM1JPQuvTyFlAn5mdR8ZJJ8yPBQm\n"
"-----END PUBLIC KEY-----\n";

static const unsigned char brainpoolP384r1_raw[] = {
	0x04, 0x52, 0xac, 0xc1, 0xb0, 0x73, 0xaa, 0xac,
	0x99, 0xe2, 0x1d, 0x73, 0xcf, 0xca, 0xe2, 0xbe,
	0xa1, 0xf3, 0x27, 0x10, 0x0e, 0x5c, 0x2d, 0x9a,
	0x2f, 0x20, 0xbd, 0xc7, 0x98, 0x30, 0x04, 0xce,
	0xc6, 0x56, 0xef, 0x24, 0xea, 0x39, 0x17, 0xed,
	0x5c, 0xa9, 0x46, 0x98, 0x37, 0xcb, 0x20, 0xa7,
	0x7a, 0x22, 0x10, 0xfa, 0xf5, 0x42, 0x35, 0x78,
	0x1b, 0x0d, 0xc8, 0x49, 0xd1, 0xc3, 0xd5, 0xba,
	0x1c, 0x26, 0xdd, 0x9a, 0xee, 0x00, 0xae, 0x3d,
	0x75, 0xec, 0xd4, 0x3d, 0xa8, 0x56, 0x17, 0x46,
	0x22, 0x25, 0x11, 0x55, 0x79, 0x5f, 0x8a, 0xc8,
	0x1f, 0xcc, 0x11, 0xa6, 0x99, 0xbe, 0x29, 0xf3,
	0xab,
};

static const unsigned char secp384r1_raw[] = {
	0x04, 0x74, 0x93, 0x7d, 0x0e, 0x8a, 0x8f, 0xb5,
	0x33, 0x40, 0x3a, 0x68, 0xe7, 0x10, 0x21, 0xc1,
	0x22, 0xa9, 0xc9, 0x83, 0x20, 0x56, 0xd2, 0xb7,
	0x2d, 0xb5, 0xb5, 0x12, 0xa9, 0x08, 0xdb, 0xe9,
	0x41, 0xb6, 0x96, 0x57, 0x66, 0xad, 0x02, 0x5b,
	0x80, 0x23, 0xff, 0xcb, 0x0b, 0x97, 0xbe, 0x49,
	0x09, 0x97, 0x8c, 0x9f, 0x5f, 0x13, 0xab, 0xd1,
	0x43, 0xf7, 0xae, 0x07, 0x27, 0xa9, 0x35, 0x40,
	0xd7, 0xfe, 0x3a, 0xb3, 0x64, 0xc6, 0xf9, 0x1e,
	0x6e, 0x35, 0x24, 0x1b, 0x08, 0xcd, 0x49, 0x3d,
	0x0b, 0xaf, 0x4f, 0x21, 0x65, 0x02, 0x7e, 0x66,
	0x75, 0x1f, 0x19, 0x24, 0x9f, 0x32, 0x3c, 0x14,
	0x26,
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
es384_pk_cmp(const char *ptr, size_t len)
{
	EVP_PKEY *pkA = NULL;
	EVP_PKEY *pkB = NULL;
	es384_pk_t *k = NULL;
	int r, ok = -1;

	if ((pkA = EVP_PKEY_from_PEM(ptr, len)) == NULL) {
		warnx("EVP_PKEY_from_PEM");
		goto out;
	}
	if ((k = es384_pk_new()) == NULL) {
		warnx("es384_pk_new");
		goto out;
	}
	if ((r = es384_pk_from_EVP_PKEY(k, pkA)) != FIDO_OK) {
		warnx("es384_pk_from_EVP_PKEY: 0x%x", r);
		goto out;
	}
	if ((pkB = es384_pk_to_EVP_PKEY(k)) == NULL) {
		warnx("es384_pk_to_EVP_PKEY");
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
	es384_pk_free(&k);

	return ok;
}

static void
short_coord(void)
{
	assert(es384_pk_cmp(short_x, sizeof(short_x)) == 0);
	assert(es384_pk_cmp(short_y, sizeof(short_y)) == 0);
}

static void
invalid_curve(const unsigned char *raw, size_t raw_len)
{
	EVP_PKEY *pkey;
	es384_pk_t *pk;

	pkey = EVP_PKEY_from_PEM(brainpoolP384r1, sizeof(brainpoolP384r1));
	if (pkey == NULL)
		return; /* assume no brainpool support in libcrypto */
	ASSERT_NOT_NULL((pk = es384_pk_new()));
	ASSERT_INVAL(es384_pk_from_EVP_PKEY(pk, pkey));
	ASSERT_INVAL(es384_pk_from_ptr(pk, raw, raw_len));
	ASSERT_NULL(es384_pk_to_EVP_PKEY((const es384_pk_t *)raw));

	EVP_PKEY_free(pkey);
	es384_pk_free(&pk);
}

static void
full_coord(void)
{
	assert(es384_pk_cmp(secp384r1, sizeof(secp384r1)) == 0);
}

static void
valid_curve(const unsigned char *raw, size_t raw_len)
{
	EVP_PKEY *pkeyA;
	EVP_PKEY *pkeyB;
	es384_pk_t *pkA;
	es384_pk_t *pkB;

	ASSERT_NOT_NULL((pkeyA = EVP_PKEY_from_PEM(secp384r1, sizeof(secp384r1))));
	ASSERT_NOT_NULL((pkA = es384_pk_new()));
	ASSERT_NOT_NULL((pkB = es384_pk_new()));
	ASSERT_OK(es384_pk_from_EVP_PKEY(pkA, pkeyA));
	ASSERT_OK(es384_pk_from_ptr(pkB, raw, raw_len));
	ASSERT_NOT_NULL((pkeyB = es384_pk_to_EVP_PKEY(pkB)));
	assert(EVP_PKEY_cmp(pkeyA, pkeyB) == 1);

	EVP_PKEY_free(pkeyA);
	EVP_PKEY_free(pkeyB);
	es384_pk_free(&pkA);
	es384_pk_free(&pkB);
}

int
main(void)
{
	fido_init(0);

	short_coord();
	full_coord();

	invalid_curve(brainpoolP384r1_raw, sizeof(brainpoolP384r1_raw)); /* uncompressed */
	invalid_curve(brainpoolP384r1_raw + 1, sizeof(brainpoolP384r1_raw) - 1); /* libfido2 */
	valid_curve(secp384r1_raw, sizeof(secp384r1_raw)); /* uncompressed */
	valid_curve(secp384r1_raw + 1, sizeof(secp384r1_raw) - 1); /* libfido2 */

	exit(0);
}
