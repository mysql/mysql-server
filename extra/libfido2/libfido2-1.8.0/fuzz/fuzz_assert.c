/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mutator_aux.h"
#include "wiredata_fido2.h"
#include "wiredata_u2f.h"
#include "dummy.h"

#include "../openbsd-compat/openbsd-compat.h"

/* Parameter set defining a FIDO2 get assertion operation. */
struct param {
	char pin[MAXSTR];
	char rp_id[MAXSTR];
	int ext;
	int seed;
	struct blob cdh;
	struct blob cred;
	struct blob es256;
	struct blob rs256;
	struct blob eddsa;
	struct blob wire_data;
	uint8_t cred_count;
	uint8_t type;
	uint8_t opt;
	uint8_t up;
	uint8_t uv;
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * get assertion using the example parameters above.
 */
static const uint8_t dummy_wire_data_fido[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_AUTHKEY,
	WIREDATA_CTAP_CBOR_PINTOKEN,
	WIREDATA_CTAP_CBOR_ASSERT,
};

/*
 * Collection of HID reports from an authenticator issued with a U2F
 * authentication using the example parameters above.
 */
static const uint8_t dummy_wire_data_u2f[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_U2F_6985,
	WIREDATA_CTAP_U2F_6985,
	WIREDATA_CTAP_U2F_6985,
	WIREDATA_CTAP_U2F_6985,
	WIREDATA_CTAP_U2F_AUTH,
};

struct param *
unpack(const uint8_t *ptr, size_t len)
{
	cbor_item_t *item = NULL, **v;
	struct cbor_load_result cbor;
	struct param *p;
	int ok = -1;

	if ((p = calloc(1, sizeof(*p))) == NULL ||
	    (item = cbor_load(ptr, len, &cbor)) == NULL ||
	    cbor.read != len ||
	    cbor_isa_array(item) == false ||
	    cbor_array_is_definite(item) == false ||
	    cbor_array_size(item) != 15 ||
	    (v = cbor_array_handle(item)) == NULL)
		goto fail;

	if (unpack_byte(v[0], &p->uv) < 0 ||
	    unpack_byte(v[1], &p->up) < 0 ||
	    unpack_byte(v[2], &p->opt) < 0 ||
	    unpack_byte(v[3], &p->type) < 0 ||
	    unpack_byte(v[4], &p->cred_count) < 0 ||
	    unpack_int(v[5], &p->ext) < 0 ||
	    unpack_int(v[6], &p->seed) < 0 ||
	    unpack_string(v[7], p->rp_id) < 0 ||
	    unpack_string(v[8], p->pin) < 0 ||
	    unpack_blob(v[9], &p->wire_data) < 0 ||
	    unpack_blob(v[10], &p->rs256) < 0 ||
	    unpack_blob(v[11], &p->es256) < 0 ||
	    unpack_blob(v[12], &p->eddsa) < 0 ||
	    unpack_blob(v[13], &p->cred) < 0 ||
	    unpack_blob(v[14], &p->cdh) < 0)
		goto fail;

	ok = 0;
fail:
	if (ok < 0) {
		free(p);
		p = NULL;
	}

	if (item)
		cbor_decref(&item);

	return p;
}

size_t
pack(uint8_t *ptr, size_t len, const struct param *p)
{
	cbor_item_t *argv[15], *array = NULL;
	size_t cbor_alloc_len, cbor_len = 0;
	unsigned char *cbor = NULL;

	memset(argv, 0, sizeof(argv));

	if ((array = cbor_new_definite_array(15)) == NULL ||
	    (argv[0] = pack_byte(p->uv)) == NULL ||
	    (argv[1] = pack_byte(p->up)) == NULL ||
	    (argv[2] = pack_byte(p->opt)) == NULL ||
	    (argv[3] = pack_byte(p->type)) == NULL ||
	    (argv[4] = pack_byte(p->cred_count)) == NULL ||
	    (argv[5] = pack_int(p->ext)) == NULL ||
	    (argv[6] = pack_int(p->seed)) == NULL ||
	    (argv[7] = pack_string(p->rp_id)) == NULL ||
	    (argv[8] = pack_string(p->pin)) == NULL ||
	    (argv[9] = pack_blob(&p->wire_data)) == NULL ||
	    (argv[10] = pack_blob(&p->rs256)) == NULL ||
	    (argv[11] = pack_blob(&p->es256)) == NULL ||
	    (argv[12] = pack_blob(&p->eddsa)) == NULL ||
	    (argv[13] = pack_blob(&p->cred)) == NULL ||
	    (argv[14] = pack_blob(&p->cdh)) == NULL)
		goto fail;

	for (size_t i = 0; i < 15; i++)
		if (cbor_array_push(array, argv[i]) == false)
			goto fail;

	if ((cbor_len = cbor_serialize_alloc(array, &cbor,
	    &cbor_alloc_len)) > len) {
		cbor_len = 0;
		goto fail;
	}

	memcpy(ptr, cbor, cbor_len);
fail:
	for (size_t i = 0; i < 15; i++)
		if (argv[i])
			cbor_decref(&argv[i]);

	if (array)
		cbor_decref(&array);

	free(cbor);

	return cbor_len;
}

size_t
pack_dummy(uint8_t *ptr, size_t len)
{
	struct param dummy;
	uint8_t blob[4096];
	size_t blob_len;

	memset(&dummy, 0, sizeof(dummy));

	dummy.type = 1; /* rsa */
	dummy.ext = FIDO_EXT_HMAC_SECRET;

	strlcpy(dummy.pin, dummy_pin, sizeof(dummy.pin));
	strlcpy(dummy.rp_id, dummy_rp_id, sizeof(dummy.rp_id));

	dummy.cred.len = sizeof(dummy_cdh); /* XXX */
	dummy.cdh.len = sizeof(dummy_cdh);
	dummy.es256.len = sizeof(dummy_es256);
	dummy.rs256.len = sizeof(dummy_rs256);
	dummy.eddsa.len = sizeof(dummy_eddsa);
	dummy.wire_data.len = sizeof(dummy_wire_data_fido);

	memcpy(&dummy.cred.body, &dummy_cdh, dummy.cred.len); /* XXX */
	memcpy(&dummy.cdh.body, &dummy_cdh, dummy.cdh.len);
	memcpy(&dummy.wire_data.body, &dummy_wire_data_fido,
	    dummy.wire_data.len);
	memcpy(&dummy.es256.body, &dummy_es256, dummy.es256.len);
	memcpy(&dummy.rs256.body, &dummy_rs256, dummy.rs256.len);
	memcpy(&dummy.eddsa.body, &dummy_eddsa, dummy.eddsa.len);

	assert((blob_len = pack(blob, sizeof(blob), &dummy)) != 0);

	if (blob_len > len) {
		memcpy(ptr, blob, len);
		return len;
	}

	memcpy(ptr, blob, blob_len);

	return blob_len;
}

static void
get_assert(fido_assert_t *assert, uint8_t opt, const struct blob *cdh,
    const char *rp_id, int ext, uint8_t up, uint8_t uv, const char *pin,
    uint8_t cred_count, const struct blob *cred)
{
	fido_dev_t *dev;

	if ((dev = open_dev(opt & 2)) == NULL)
		return;
	if (opt & 1)
		fido_dev_force_u2f(dev);
	if (ext & FIDO_EXT_HMAC_SECRET)
		fido_assert_set_extensions(assert, FIDO_EXT_HMAC_SECRET);
	if (ext & FIDO_EXT_CRED_BLOB)
		fido_assert_set_extensions(assert, FIDO_EXT_CRED_BLOB);
	if (ext & FIDO_EXT_LARGEBLOB_KEY)
		fido_assert_set_extensions(assert, FIDO_EXT_LARGEBLOB_KEY);
	if (up & 1)
		fido_assert_set_up(assert, FIDO_OPT_TRUE);
	else if (opt & 1)
		fido_assert_set_up(assert, FIDO_OPT_FALSE);
	if (uv & 1)
		fido_assert_set_uv(assert, FIDO_OPT_TRUE);

	for (uint8_t i = 0; i < cred_count; i++)
		fido_assert_allow_cred(assert, cred->body, cred->len);

	fido_assert_set_clientdata_hash(assert, cdh->body, cdh->len);
	fido_assert_set_rp(assert, rp_id);
	/* XXX reuse cred as hmac salt */
	fido_assert_set_hmac_salt(assert, cred->body, cred->len);

	/* repeat memory operations to trigger reallocation paths */
	fido_assert_set_clientdata_hash(assert, cdh->body, cdh->len);
	fido_assert_set_rp(assert, rp_id);
	fido_assert_set_hmac_salt(assert, cred->body, cred->len);

	if (strlen(pin) == 0)
		pin = NULL;

	fido_dev_get_assert(dev, assert, (opt & 1) ? NULL : pin);

	fido_dev_cancel(dev);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
verify_assert(int type, const unsigned char *cdh_ptr, size_t cdh_len,
    const char *rp_id, const unsigned char *authdata_ptr, size_t authdata_len,
    const unsigned char *sig_ptr, size_t sig_len, uint8_t up, uint8_t uv,
    int ext, void *pk)
{
	fido_assert_t *assert = NULL;

	if ((assert = fido_assert_new()) == NULL)
		return;

	fido_assert_set_clientdata_hash(assert, cdh_ptr, cdh_len);
	fido_assert_set_rp(assert, rp_id);
	fido_assert_set_count(assert, 1);

	if (fido_assert_set_authdata(assert, 0, authdata_ptr,
	    authdata_len) != FIDO_OK) {
		fido_assert_set_authdata_raw(assert, 0, authdata_ptr,
		    authdata_len);
	}

	if (up & 1)
		fido_assert_set_up(assert, FIDO_OPT_TRUE);
	if (uv & 1)
		fido_assert_set_uv(assert, FIDO_OPT_TRUE);

	fido_assert_set_extensions(assert, ext);
	fido_assert_set_sig(assert, 0, sig_ptr, sig_len);

	/* repeat memory operations to trigger reallocation paths */
	if (fido_assert_set_authdata(assert, 0, authdata_ptr,
	    authdata_len) != FIDO_OK) {
		fido_assert_set_authdata_raw(assert, 0, authdata_ptr,
		    authdata_len);
	}
	fido_assert_set_sig(assert, 0, sig_ptr, sig_len);

	assert(fido_assert_verify(assert, 0, type, pk) != FIDO_OK);

	fido_assert_free(&assert);
}

/*
 * Do a dummy conversion to exercise rs256_pk_from_RSA().
 */
static void
rs256_convert(const rs256_pk_t *k)
{
	EVP_PKEY *pkey = NULL;
	rs256_pk_t *pk = NULL;
	RSA *rsa = NULL;
	volatile int r;

	if ((pkey = rs256_pk_to_EVP_PKEY(k)) == NULL ||
	    (pk = rs256_pk_new()) == NULL ||
	    (rsa = EVP_PKEY_get0_RSA(pkey)) == NULL)
		goto out;

	r = rs256_pk_from_RSA(pk, rsa);
out:
	if (pk)
		rs256_pk_free(&pk);
	if (pkey)
		EVP_PKEY_free(pkey);
}

/*
 * Do a dummy conversion to exercise eddsa_pk_from_EVP_PKEY().
 */
static void
eddsa_convert(const eddsa_pk_t *k)
{
	EVP_PKEY *pkey = NULL;
	eddsa_pk_t *pk = NULL;
	volatile int r;

	if ((pkey = eddsa_pk_to_EVP_PKEY(k)) == NULL ||
	    (pk = eddsa_pk_new()) == NULL)
		goto out;

	r = eddsa_pk_from_EVP_PKEY(pk, pkey);
out:
	if (pk)
		eddsa_pk_free(&pk);
	if (pkey)
		EVP_PKEY_free(pkey);
}

void
test(const struct param *p)
{
	fido_assert_t *assert = NULL;
	es256_pk_t *es256_pk = NULL;
	rs256_pk_t *rs256_pk = NULL;
	eddsa_pk_t *eddsa_pk = NULL;
	uint8_t flags;
	uint32_t sigcount;
	int cose_alg = 0;
	void *pk;

	prng_init((unsigned int)p->seed);
	fido_init(FIDO_DEBUG);
	fido_set_log_handler(consume_str);

	switch (p->type & 3) {
	case 0:
		cose_alg = COSE_ES256;

		if ((es256_pk = es256_pk_new()) == NULL)
			return;

		es256_pk_from_ptr(es256_pk, p->es256.body, p->es256.len);
		pk = es256_pk;

		break;
	case 1:
		cose_alg = COSE_RS256;

		if ((rs256_pk = rs256_pk_new()) == NULL)
			return;

		rs256_pk_from_ptr(rs256_pk, p->rs256.body, p->rs256.len);
		pk = rs256_pk;

		rs256_convert(pk);

		break;
	default:
		cose_alg = COSE_EDDSA;

		if ((eddsa_pk = eddsa_pk_new()) == NULL)
			return;

		eddsa_pk_from_ptr(eddsa_pk, p->eddsa.body, p->eddsa.len);
		pk = eddsa_pk;

		eddsa_convert(pk);

		break;
	}

	if ((assert = fido_assert_new()) == NULL)
		goto out;

	set_wire_data(p->wire_data.body, p->wire_data.len);

	get_assert(assert, p->opt, &p->cdh, p->rp_id, p->ext, p->up, p->uv,
	    p->pin, p->cred_count, &p->cred);

	/* XXX +1 on purpose */
	for (size_t i = 0; i <= fido_assert_count(assert); i++) {
		verify_assert(cose_alg,
		    fido_assert_clientdata_hash_ptr(assert),
		    fido_assert_clientdata_hash_len(assert),
		    fido_assert_rp_id(assert),
		    fido_assert_authdata_ptr(assert, i),
		    fido_assert_authdata_len(assert, i),
		    fido_assert_sig_ptr(assert, i),
		    fido_assert_sig_len(assert, i), p->up, p->uv, p->ext, pk);
		consume(fido_assert_id_ptr(assert, i),
		    fido_assert_id_len(assert, i));
		consume(fido_assert_user_id_ptr(assert, i),
		    fido_assert_user_id_len(assert, i));
		consume(fido_assert_hmac_secret_ptr(assert, i),
		    fido_assert_hmac_secret_len(assert, i));
		consume_str(fido_assert_user_icon(assert, i));
		consume_str(fido_assert_user_name(assert, i));
		consume_str(fido_assert_user_display_name(assert, i));
		consume(fido_assert_blob_ptr(assert, i),
		    fido_assert_blob_len(assert, i));
		consume(fido_assert_largeblob_key_ptr(assert, i),
		    fido_assert_largeblob_key_len(assert, i));
		flags = fido_assert_flags(assert, i);
		consume(&flags, sizeof(flags));
		sigcount = fido_assert_sigcount(assert, i);
		consume(&sigcount, sizeof(sigcount));
	}

out:
	es256_pk_free(&es256_pk);
	rs256_pk_free(&rs256_pk);
	eddsa_pk_free(&eddsa_pk);

	fido_assert_free(&assert);
}

void
mutate(struct param *p, unsigned int seed, unsigned int flags) NO_MSAN
{
	if (flags & MUTATE_SEED)
		p->seed = (int)seed;

	if (flags & MUTATE_PARAM) {
		mutate_byte(&p->uv);
		mutate_byte(&p->up);
		mutate_byte(&p->opt);
		mutate_byte(&p->type);
		mutate_byte(&p->cred_count);
		mutate_int(&p->ext);
		mutate_blob(&p->rs256);
		mutate_blob(&p->es256);
		mutate_blob(&p->eddsa);
		mutate_blob(&p->cred);
		mutate_blob(&p->cdh);
		mutate_string(p->rp_id);
		mutate_string(p->pin);
	}

	if (flags & MUTATE_WIREDATA) {
		if (p->opt & 1) {
			p->wire_data.len = sizeof(dummy_wire_data_u2f);
			memcpy(&p->wire_data.body, &dummy_wire_data_u2f,
			    p->wire_data.len);
		} else {
			p->wire_data.len = sizeof(dummy_wire_data_fido);
			memcpy(&p->wire_data.body, &dummy_wire_data_fido,
			    p->wire_data.len);
		}
		mutate_blob(&p->wire_data);
	}
}
