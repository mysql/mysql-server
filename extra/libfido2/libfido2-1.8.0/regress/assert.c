/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <assert.h>
#include <fido.h>
#include <fido/es256.h>
#include <fido/rs256.h>
#include <fido/eddsa.h>
#include <string.h>

#define FAKE_DEV_HANDLE	((void *)0xdeadbeef)

static const unsigned char es256_pk[64] = {
	0x34, 0xeb, 0x99, 0x77, 0x02, 0x9c, 0x36, 0x38,
	0xbb, 0xc2, 0xae, 0xa0, 0xa0, 0x18, 0xc6, 0x64,
	0xfc, 0xe8, 0x49, 0x92, 0xd7, 0x74, 0x9e, 0x0c,
	0x46, 0x8c, 0x9d, 0xa6, 0xdf, 0x46, 0xf7, 0x84,
	0x60, 0x1e, 0x0f, 0x8b, 0x23, 0x85, 0x4a, 0x9a,
	0xec, 0xc1, 0x08, 0x9f, 0x30, 0xd0, 0x0d, 0xd7,
	0x76, 0x7b, 0x55, 0x48, 0x91, 0x7c, 0x4f, 0x0f,
	0x64, 0x1a, 0x1d, 0xf8, 0xbe, 0x14, 0x90, 0x8a,
};

static const unsigned char cdh[32] = {
	0xec, 0x8d, 0x8f, 0x78, 0x42, 0x4a, 0x2b, 0xb7,
	0x82, 0x34, 0xaa, 0xca, 0x07, 0xa1, 0xf6, 0x56,
	0x42, 0x1c, 0xb6, 0xf6, 0xb3, 0x00, 0x86, 0x52,
	0x35, 0x2d, 0xa2, 0x62, 0x4a, 0xbe, 0x89, 0x76,
};

static const unsigned char authdata[39] = {
	0x58, 0x25, 0x49, 0x96, 0x0d, 0xe5, 0x88, 0x0e,
	0x8c, 0x68, 0x74, 0x34, 0x17, 0x0f, 0x64, 0x76,
	0x60, 0x5b, 0x8f, 0xe4, 0xae, 0xb9, 0xa2, 0x86,
	0x32, 0xc7, 0x99, 0x5c, 0xf3, 0xba, 0x83, 0x1d,
	0x97, 0x63, 0x00, 0x00, 0x00, 0x00, 0x03,
};

static const unsigned char sig[72] = {
	0x30, 0x46, 0x02, 0x21, 0x00, 0xf6, 0xd1, 0xa3,
	0xd5, 0x24, 0x2b, 0xde, 0xee, 0xa0, 0x90, 0x89,
	0xcd, 0xf8, 0x9e, 0xbd, 0x6b, 0x4d, 0x55, 0x79,
	0xe4, 0xc1, 0x42, 0x27, 0xb7, 0x9b, 0x9b, 0xa4,
	0x0a, 0xe2, 0x47, 0x64, 0x0e, 0x02, 0x21, 0x00,
	0xe5, 0xc9, 0xc2, 0x83, 0x47, 0x31, 0xc7, 0x26,
	0xe5, 0x25, 0xb2, 0xb4, 0x39, 0xa7, 0xfc, 0x3d,
	0x70, 0xbe, 0xe9, 0x81, 0x0d, 0x4a, 0x62, 0xa9,
	0xab, 0x4a, 0x91, 0xc0, 0x7d, 0x2d, 0x23, 0x1e,
};

static void *
dummy_open(const char *path)
{
	(void)path;

	return (FAKE_DEV_HANDLE);
}

static void
dummy_close(void *handle)
{
	assert(handle == FAKE_DEV_HANDLE);
}

static int
dummy_read(void *handle, unsigned char *buf, size_t len, int ms)
{
	(void)handle;
	(void)buf;
	(void)len;
	(void)ms;

	abort();
	/* NOTREACHED */
}

static int
dummy_write(void *handle, const unsigned char *buf, size_t len)
{
	(void)handle;
	(void)buf;
	(void)len;

	abort();
	/* NOTREACHED */
}

static fido_assert_t *
alloc_assert(void)
{
	fido_assert_t *a;

	a = fido_assert_new();
	assert(a != NULL);

	return (a);
}

static void
free_assert(fido_assert_t *a)
{
	fido_assert_free(&a);
	assert(a == NULL);
}

static fido_dev_t *
alloc_dev(void)
{
	fido_dev_t *d;

	d = fido_dev_new();
	assert(d != NULL);

	return (d);
}

static void
free_dev(fido_dev_t *d)
{
	fido_dev_free(&d);
	assert(d == NULL);
}

static es256_pk_t *
alloc_es256_pk(void)
{
	es256_pk_t *pk;

	pk = es256_pk_new();
	assert(pk != NULL);

	return (pk);
}

static void
free_es256_pk(es256_pk_t *pk)
{
	es256_pk_free(&pk);
	assert(pk == NULL);
}

static rs256_pk_t *
alloc_rs256_pk(void)
{
	rs256_pk_t *pk;

	pk = rs256_pk_new();
	assert(pk != NULL);

	return (pk);
}

static void
free_rs256_pk(rs256_pk_t *pk)
{
	rs256_pk_free(&pk);
	assert(pk == NULL);
}

static eddsa_pk_t *
alloc_eddsa_pk(void)
{
	eddsa_pk_t *pk;

	pk = eddsa_pk_new();
	assert(pk != NULL);

	return (pk);
}

static void
free_eddsa_pk(eddsa_pk_t *pk)
{
	eddsa_pk_free(&pk);
	assert(pk == NULL);
}

static void
empty_assert(fido_dev_t *d, fido_assert_t *a, size_t idx)
{
	es256_pk_t *es256;
	rs256_pk_t *rs256;
	eddsa_pk_t *eddsa;

	assert(fido_assert_flags(a, idx) == 0);
	assert(fido_assert_authdata_len(a, idx) == 0);
	assert(fido_assert_authdata_ptr(a, idx) == NULL);
	assert(fido_assert_clientdata_hash_len(a) == 0);
	assert(fido_assert_clientdata_hash_ptr(a) == NULL);
	assert(fido_assert_id_len(a, idx) == 0);
	assert(fido_assert_id_ptr(a, idx) == NULL);
	assert(fido_assert_rp_id(a) == NULL);
	assert(fido_assert_sig_len(a, idx) == 0);
	assert(fido_assert_sig_ptr(a, idx) == NULL);
	assert(fido_assert_user_display_name(a, idx) == NULL);
	assert(fido_assert_user_icon(a, idx) == NULL);
	assert(fido_assert_user_id_len(a, idx) == 0);
	assert(fido_assert_user_id_ptr(a, idx) == NULL);
	assert(fido_assert_user_name(a, idx) == NULL);

	es256 = alloc_es256_pk();
	rs256 = alloc_rs256_pk();
	eddsa = alloc_eddsa_pk();

	fido_dev_force_u2f(d);
	assert(fido_dev_get_assert(d, a, NULL) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_dev_get_assert(d, a, "") == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_assert_verify(a, idx, COSE_ES256,
	    NULL) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_assert_verify(a, idx, COSE_ES256,
	    es256) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_assert_verify(a, idx, -1,
	    es256) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_assert_verify(a, idx, COSE_RS256,
	    rs256) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_assert_verify(a, idx, COSE_EDDSA,
	    eddsa) == FIDO_ERR_INVALID_ARGUMENT);

	fido_dev_force_fido2(d);
	assert(fido_dev_get_assert(d, a, NULL) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_dev_get_assert(d, a, "") == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_assert_verify(a, idx, COSE_ES256,
	    NULL) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_assert_verify(a, idx, COSE_ES256,
	    es256) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_assert_verify(a, idx, -1,
	    es256) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_assert_verify(a, idx, COSE_RS256,
	    rs256) == FIDO_ERR_INVALID_ARGUMENT);
	assert(fido_assert_verify(a, idx, COSE_EDDSA,
	    eddsa) == FIDO_ERR_INVALID_ARGUMENT);

	free_es256_pk(es256);
	free_rs256_pk(rs256);
	free_eddsa_pk(eddsa);
}

static void
empty_assert_tests(void)
{
	fido_assert_t *a;
	fido_dev_t *d;
	fido_dev_io_t io_f;
	size_t i;

	memset(&io_f, 0, sizeof(io_f));

	a = alloc_assert();
	d = alloc_dev();

	io_f.open = dummy_open;
	io_f.close = dummy_close;
	io_f.read = dummy_read;
	io_f.write = dummy_write;

	assert(fido_dev_set_io_functions(d, &io_f) == FIDO_OK);

	empty_assert(d, a, 0);
	assert(fido_assert_count(a) == 0);
	assert(fido_assert_set_count(a, 4) == FIDO_OK);
	assert(fido_assert_count(a) == 4);
	for (i = 0; i < 4; i++) {
		empty_assert(d, a, i);
	}
	empty_assert(d, a, 10);
	free_assert(a);
	free_dev(d);
}

static void
valid_assert(void)
{
	fido_assert_t *a;
	es256_pk_t *es256;
	rs256_pk_t *rs256;
	eddsa_pk_t *eddsa;

	a = alloc_assert();
	es256 = alloc_es256_pk();
	rs256 = alloc_rs256_pk();
	eddsa = alloc_eddsa_pk();
	assert(es256_pk_from_ptr(es256, es256_pk, sizeof(es256_pk)) == FIDO_OK);
	assert(fido_assert_set_clientdata_hash(a, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_assert_set_rp(a, "localhost") == FIDO_OK);
	assert(fido_assert_set_count(a, 1) == FIDO_OK);
	assert(fido_assert_set_authdata(a, 0, authdata,
	    sizeof(authdata)) == FIDO_OK);
	assert(fido_assert_set_up(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_uv(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_sig(a, 0, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_assert_verify(a, 0, COSE_ES256, es256) == FIDO_OK);
	assert(fido_assert_verify(a, 0, COSE_RS256, rs256) == FIDO_ERR_INVALID_SIG);
	assert(fido_assert_verify(a, 0, COSE_EDDSA, eddsa) == FIDO_ERR_INVALID_SIG);
	free_assert(a);
	free_es256_pk(es256);
	free_rs256_pk(rs256);
	free_eddsa_pk(eddsa);
}

static void
no_cdh(void)
{
	fido_assert_t *a;
	es256_pk_t *pk;

	a = alloc_assert();
	pk = alloc_es256_pk();
	assert(es256_pk_from_ptr(pk, es256_pk, sizeof(es256_pk)) == FIDO_OK);
	assert(fido_assert_set_rp(a, "localhost") == FIDO_OK);
	assert(fido_assert_set_count(a, 1) == FIDO_OK);
	assert(fido_assert_set_authdata(a, 0, authdata,
	    sizeof(authdata)) == FIDO_OK);
	assert(fido_assert_set_up(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_uv(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_sig(a, 0, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_assert_verify(a, 0, COSE_ES256,
	    pk) == FIDO_ERR_INVALID_ARGUMENT);
	free_assert(a);
	free_es256_pk(pk);
}

static void
no_rp(void)
{
	fido_assert_t *a;
	es256_pk_t *pk;

	a = alloc_assert();
	pk = alloc_es256_pk();
	assert(es256_pk_from_ptr(pk, es256_pk, sizeof(es256_pk)) == FIDO_OK);
	assert(fido_assert_set_clientdata_hash(a, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_assert_set_count(a, 1) == FIDO_OK);
	assert(fido_assert_set_authdata(a, 0, authdata,
	    sizeof(authdata)) == FIDO_OK);
	assert(fido_assert_set_up(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_uv(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_sig(a, 0, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_assert_verify(a, 0, COSE_ES256,
	    pk) == FIDO_ERR_INVALID_ARGUMENT);
	free_assert(a);
	free_es256_pk(pk);
}

static void
no_authdata(void)
{
	fido_assert_t *a;
	es256_pk_t *pk;

	a = alloc_assert();
	pk = alloc_es256_pk();
	assert(es256_pk_from_ptr(pk, es256_pk, sizeof(es256_pk)) == FIDO_OK);
	assert(fido_assert_set_clientdata_hash(a, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_assert_set_rp(a, "localhost") == FIDO_OK);
	assert(fido_assert_set_count(a, 1) == FIDO_OK);
	assert(fido_assert_set_up(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_uv(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_sig(a, 0, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_assert_verify(a, 0, COSE_ES256,
	    pk) == FIDO_ERR_INVALID_ARGUMENT);
	free_assert(a);
	free_es256_pk(pk);
}

static void
no_sig(void)
{
	fido_assert_t *a;
	es256_pk_t *pk;

	a = alloc_assert();
	pk = alloc_es256_pk();
	assert(es256_pk_from_ptr(pk, es256_pk, sizeof(es256_pk)) == FIDO_OK);
	assert(fido_assert_set_clientdata_hash(a, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_assert_set_rp(a, "localhost") == FIDO_OK);
	assert(fido_assert_set_count(a, 1) == FIDO_OK);
	assert(fido_assert_set_authdata(a, 0, authdata,
	    sizeof(authdata)) == FIDO_OK);
	assert(fido_assert_set_up(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_uv(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_verify(a, 0, COSE_ES256,
	    pk) == FIDO_ERR_INVALID_ARGUMENT);
	free_assert(a);
	free_es256_pk(pk);
}

static void
junk_cdh(void)
{
	fido_assert_t *a;
	es256_pk_t *pk;
	unsigned char *junk;

	junk = malloc(sizeof(cdh));
	assert(junk != NULL);
	memcpy(junk, cdh, sizeof(cdh));
	junk[0] = ~junk[0];

	a = alloc_assert();
	pk = alloc_es256_pk();
	assert(es256_pk_from_ptr(pk, es256_pk, sizeof(es256_pk)) == FIDO_OK);
	assert(fido_assert_set_clientdata_hash(a, junk, sizeof(cdh)) == FIDO_OK);
	assert(fido_assert_set_rp(a, "localhost") == FIDO_OK);
	assert(fido_assert_set_count(a, 1) == FIDO_OK);
	assert(fido_assert_set_authdata(a, 0, authdata,
	    sizeof(authdata)) == FIDO_OK);
	assert(fido_assert_set_up(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_uv(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_sig(a, 0, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_assert_verify(a, 0, COSE_ES256, pk) == FIDO_ERR_INVALID_SIG);
	free_assert(a);
	free_es256_pk(pk);
	free(junk);
}

static void
junk_rp(void)
{
	fido_assert_t *a;
	es256_pk_t *pk;

	a = alloc_assert();
	pk = alloc_es256_pk();
	assert(es256_pk_from_ptr(pk, es256_pk, sizeof(es256_pk)) == FIDO_OK);
	assert(fido_assert_set_clientdata_hash(a, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_assert_set_rp(a, "potato") == FIDO_OK);
	assert(fido_assert_set_count(a, 1) == FIDO_OK);
	assert(fido_assert_set_authdata(a, 0, authdata,
	    sizeof(authdata)) == FIDO_OK);
	assert(fido_assert_set_up(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_uv(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_sig(a, 0, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_assert_verify(a, 0, COSE_ES256,
	    pk) == FIDO_ERR_INVALID_PARAM);
	free_assert(a);
	free_es256_pk(pk);
}

static void
junk_authdata(void)
{
	fido_assert_t *a;
	unsigned char *junk;

	junk = malloc(sizeof(authdata));
	assert(junk != NULL);
	memcpy(junk, authdata, sizeof(authdata));
	junk[0] = ~junk[0];

	a = alloc_assert();
	assert(fido_assert_set_count(a, 1) == FIDO_OK);
	assert(fido_assert_set_authdata(a, 0, junk,
	    sizeof(authdata)) == FIDO_ERR_INVALID_ARGUMENT);
	free_assert(a);
	free(junk);
}

static void
junk_sig(void)
{
	fido_assert_t *a;
	es256_pk_t *pk;
	unsigned char *junk;

	junk = malloc(sizeof(sig));
	assert(junk != NULL);
	memcpy(junk, sig, sizeof(sig));
	junk[0] = ~junk[0];

	a = alloc_assert();
	pk = alloc_es256_pk();
	assert(es256_pk_from_ptr(pk, es256_pk, sizeof(es256_pk)) == FIDO_OK);
	assert(fido_assert_set_clientdata_hash(a, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_assert_set_rp(a, "localhost") == FIDO_OK);
	assert(fido_assert_set_count(a, 1) == FIDO_OK);
	assert(fido_assert_set_authdata(a, 0, authdata,
	    sizeof(authdata)) == FIDO_OK);
	assert(fido_assert_set_up(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_uv(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_sig(a, 0, junk, sizeof(sig)) == FIDO_OK);
	assert(fido_assert_verify(a, 0, COSE_ES256, pk) == FIDO_ERR_INVALID_SIG);
	free_assert(a);
	free_es256_pk(pk);
	free(junk);
}

static void
wrong_options(void)
{
	fido_assert_t *a;
	es256_pk_t *pk;

	a = alloc_assert();
	pk = alloc_es256_pk();
	assert(es256_pk_from_ptr(pk, es256_pk, sizeof(es256_pk)) == FIDO_OK);
	assert(fido_assert_set_clientdata_hash(a, cdh, sizeof(cdh)) == FIDO_OK);
	assert(fido_assert_set_rp(a, "localhost") == FIDO_OK);
	assert(fido_assert_set_count(a, 1) == FIDO_OK);
	assert(fido_assert_set_authdata(a, 0, authdata,
	    sizeof(authdata)) == FIDO_OK);
	assert(fido_assert_set_up(a, FIDO_OPT_TRUE) == FIDO_OK);
	assert(fido_assert_set_uv(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_sig(a, 0, sig, sizeof(sig)) == FIDO_OK);
	assert(fido_assert_verify(a, 0, COSE_ES256,
	    pk) == FIDO_ERR_INVALID_PARAM);
	assert(fido_assert_set_up(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_uv(a, FIDO_OPT_TRUE) == FIDO_OK);
	assert(fido_assert_verify(a, 0, COSE_ES256,
	    pk) == FIDO_ERR_INVALID_PARAM);
	assert(fido_assert_set_up(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_set_uv(a, FIDO_OPT_FALSE) == FIDO_OK);
	assert(fido_assert_verify(a, 0, COSE_ES256, pk) == FIDO_OK);
	free_assert(a);
	free_es256_pk(pk);
}

/* cbor_serialize_alloc misuse */
static void
bad_cbor_serialize(void)
{
	fido_assert_t *a;

	a = alloc_assert();
	assert(fido_assert_set_count(a, 1) == FIDO_OK);
	assert(fido_assert_set_authdata(a, 0, authdata,
	    sizeof(authdata)) == FIDO_OK);
	assert(fido_assert_authdata_len(a, 0) == sizeof(authdata));
	free_assert(a);
}

int
main(void)
{
	fido_init(0);

	empty_assert_tests();
	valid_assert();
	no_cdh();
	no_rp();
	no_authdata();
	no_sig();
	junk_cdh();
	junk_rp();
	junk_authdata();
	junk_sig();
	wrong_options();
	bad_cbor_serialize();

	exit(0);
}
