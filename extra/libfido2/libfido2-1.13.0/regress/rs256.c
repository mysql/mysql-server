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
#include <fido/rs256.h>

#include <openssl/bio.h>
#include <openssl/pem.h>

#define ASSERT_NOT_NULL(e)	assert((e) != NULL)
#define ASSERT_NULL(e)		assert((e) == NULL)
#define ASSERT_INVAL(e)		assert((e) == FIDO_ERR_INVALID_ARGUMENT)
#define ASSERT_OK(e)		assert((e) == FIDO_OK)

static char rsa1024[] = \
"-----BEGIN PUBLIC KEY-----\n"
"MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCw92gn9Ku/bEfFj1AutaZyltpf\n"
"zzXrg70kQFymNq+spMt/HlxKiImw8TZU08zWW4ZLE/Ch4JYjMW6ETAdQFhSC63Ih\n"
"Wecui0JJ1f+2CsUVg+h7lO1877LZYUpdNiJrbqMb5Yc4N3FPtvdl3NoLIIQsF76H\n"
"VRvpjQgkWipRfZ97JQIDAQAB\n"
"-----END PUBLIC KEY-----";

static char rsa2048[] = \
"-----BEGIN PUBLIC KEY-----\n"
"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEApvIq/55ZodBIxzo/8BnE\n"
"UQN1fo1hmJ6V20hQHSzJq5tHyxRCcvKikuJ1ZvR4RdZlEzdTdbEfMBdZ8sxve0/U\n"
"yYEjH92CG0vgTCYuUaFLJTaWZSvWa96G8Lw+V4VyNFDRCM7sflOaSVH5pAsz8OEc\n"
"TLZfM4NhnDsJAM+mQ6X7Tza0sczPchgDA+9KByXo/VIqyuBQs17rlKC2reMa8NkY\n"
"rBRQZJLNzi68d5/BHH1flGWE1l8wJ9dr1Ex93H/KdzX+7/28TWUC98nneUo8RfRx\n"
"FwUt/EInDMHOORCaCHSs28U/9IUyMjqLB1rxKhIp09yGXMiTrrT+p+Pcn8dO01HT\n"
"vQIDAQAB\n"
"-----END PUBLIC KEY-----";

static char rsa3072[] = \
"-----BEGIN PUBLIC KEY-----\n"
"MIIBojANBgkqhkiG9w0BAQEFAAOCAY8AMIIBigKCAYEAwZunKrMs/o92AniLPNTF\n"
"Ta4EYfhy5NDmMvQvRFT/eTYItLrOTPmYMap68KLyZYmgz/AdaxAL/992QWre7XTY\n"
"gqLwtZT+WsSu7xPHWKTTXrlVohKBeLHQ0I7Zy0NSMUxhlJEMrBAjSyFAS86zWm5w\n"
"ctC3pNCqfUKugA07BVj+d5Mv5fziwgMR86kuhkVuMYfsR4IYwX4+va0pyLzxx624\n"
"s9nJ107g+A+3MUk4bAto3lruFeeZPUI2AFzFQbGg5By6VtvVi3gKQ7lUNtAr0Onu\n"
"I6Fb+yz8sbFcvDpJcu5CXW20GrKMVP4KY5pn2LCajWuZjBl/dXWayPfm4UX5Y2O4\n"
"73tzPpUBNwnEdz79His0v80Vmvjwn5IuF2jAoimrBNPJFFwCCuVNy8kgj2vllk1l\n"
"RvLOG6hf8VnlDb40QZS3QAQ09xFfF+xlVLb8cHH6wllaAGEM230TrmawpC7xpz4Z\n"
"sTuwJwI0AWEi//noMsRz2BuF2fCp//aORYJQU2S8kYk3AgMBAAE=\n"
"-----END PUBLIC KEY-----";

static const unsigned char rsa2048_raw[] = {
	0xa6, 0xf2, 0x2a, 0xff, 0x9e, 0x59, 0xa1, 0xd0,
	0x48, 0xc7, 0x3a, 0x3f, 0xf0, 0x19, 0xc4, 0x51,
	0x03, 0x75, 0x7e, 0x8d, 0x61, 0x98, 0x9e, 0x95,
	0xdb, 0x48, 0x50, 0x1d, 0x2c, 0xc9, 0xab, 0x9b,
	0x47, 0xcb, 0x14, 0x42, 0x72, 0xf2, 0xa2, 0x92,
	0xe2, 0x75, 0x66, 0xf4, 0x78, 0x45, 0xd6, 0x65,
	0x13, 0x37, 0x53, 0x75, 0xb1, 0x1f, 0x30, 0x17,
	0x59, 0xf2, 0xcc, 0x6f, 0x7b, 0x4f, 0xd4, 0xc9,
	0x81, 0x23, 0x1f, 0xdd, 0x82, 0x1b, 0x4b, 0xe0,
	0x4c, 0x26, 0x2e, 0x51, 0xa1, 0x4b, 0x25, 0x36,
	0x96, 0x65, 0x2b, 0xd6, 0x6b, 0xde, 0x86, 0xf0,
	0xbc, 0x3e, 0x57, 0x85, 0x72, 0x34, 0x50, 0xd1,
	0x08, 0xce, 0xec, 0x7e, 0x53, 0x9a, 0x49, 0x51,
	0xf9, 0xa4, 0x0b, 0x33, 0xf0, 0xe1, 0x1c, 0x4c,
	0xb6, 0x5f, 0x33, 0x83, 0x61, 0x9c, 0x3b, 0x09,
	0x00, 0xcf, 0xa6, 0x43, 0xa5, 0xfb, 0x4f, 0x36,
	0xb4, 0xb1, 0xcc, 0xcf, 0x72, 0x18, 0x03, 0x03,
	0xef, 0x4a, 0x07, 0x25, 0xe8, 0xfd, 0x52, 0x2a,
	0xca, 0xe0, 0x50, 0xb3, 0x5e, 0xeb, 0x94, 0xa0,
	0xb6, 0xad, 0xe3, 0x1a, 0xf0, 0xd9, 0x18, 0xac,
	0x14, 0x50, 0x64, 0x92, 0xcd, 0xce, 0x2e, 0xbc,
	0x77, 0x9f, 0xc1, 0x1c, 0x7d, 0x5f, 0x94, 0x65,
	0x84, 0xd6, 0x5f, 0x30, 0x27, 0xd7, 0x6b, 0xd4,
	0x4c, 0x7d, 0xdc, 0x7f, 0xca, 0x77, 0x35, 0xfe,
	0xef, 0xfd, 0xbc, 0x4d, 0x65, 0x02, 0xf7, 0xc9,
	0xe7, 0x79, 0x4a, 0x3c, 0x45, 0xf4, 0x71, 0x17,
	0x05, 0x2d, 0xfc, 0x42, 0x27, 0x0c, 0xc1, 0xce,
	0x39, 0x10, 0x9a, 0x08, 0x74, 0xac, 0xdb, 0xc5,
	0x3f, 0xf4, 0x85, 0x32, 0x32, 0x3a, 0x8b, 0x07,
	0x5a, 0xf1, 0x2a, 0x12, 0x29, 0xd3, 0xdc, 0x86,
	0x5c, 0xc8, 0x93, 0xae, 0xb4, 0xfe, 0xa7, 0xe3,
	0xdc, 0x9f, 0xc7, 0x4e, 0xd3, 0x51, 0xd3, 0xbd,
	0x01, 0x00, 0x01,
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
rs256_pk_cmp(const char *ptr, size_t len)
{
	EVP_PKEY *pkA = NULL;
	EVP_PKEY *pkB = NULL;
	rs256_pk_t *k = NULL;
	int r, ok = -1;

	if ((pkA = EVP_PKEY_from_PEM(ptr, len)) == NULL) {
		warnx("EVP_PKEY_from_PEM");
		goto out;
	}
	if ((k = rs256_pk_new()) == NULL) {
		warnx("rs256_pk_new");
		goto out;
	}
	if ((r = rs256_pk_from_EVP_PKEY(k, pkA)) != FIDO_OK) {
		warnx("rs256_pk_from_EVP_PKEY: 0x%x", r);
		goto out;
	}
	if ((pkB = rs256_pk_to_EVP_PKEY(k)) == NULL) {
		warnx("rs256_pk_to_EVP_PKEY");
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
	rs256_pk_free(&k);

	return ok;
}

static void
invalid_size(const char *pem)
{
	EVP_PKEY *pkey;
	rs256_pk_t *pk;

	ASSERT_NOT_NULL((pkey = EVP_PKEY_from_PEM(pem, strlen(pem))));
	ASSERT_NOT_NULL((pk = rs256_pk_new()));
	ASSERT_INVAL(rs256_pk_from_EVP_PKEY(pk, pkey));

	EVP_PKEY_free(pkey);
	rs256_pk_free(&pk);
}

static void
valid_size(const char *pem, const unsigned char *raw, size_t raw_len)
{
	EVP_PKEY *pkeyA;
	EVP_PKEY *pkeyB;
	rs256_pk_t *pkA;
	rs256_pk_t *pkB;

	ASSERT_NOT_NULL((pkeyA = EVP_PKEY_from_PEM(pem, strlen(pem))));
	ASSERT_NOT_NULL((pkA = rs256_pk_new()));
	ASSERT_NOT_NULL((pkB = rs256_pk_new()));
	ASSERT_OK(rs256_pk_from_EVP_PKEY(pkA, pkeyA));
	ASSERT_OK(rs256_pk_from_ptr(pkB, raw, raw_len));
	ASSERT_NOT_NULL((pkeyB = rs256_pk_to_EVP_PKEY(pkB)));
	assert(EVP_PKEY_cmp(pkeyA, pkeyB) == 1);
	assert(rs256_pk_cmp(pem, strlen(pem)) == 0);

	EVP_PKEY_free(pkeyA);
	EVP_PKEY_free(pkeyB);
	rs256_pk_free(&pkA);
	rs256_pk_free(&pkB);
}

int
main(void)
{
	fido_init(0);

	invalid_size(rsa1024); 
	invalid_size(rsa3072); 
	valid_size(rsa2048, rsa2048_raw, sizeof(rsa2048_raw));

	exit(0);
}
