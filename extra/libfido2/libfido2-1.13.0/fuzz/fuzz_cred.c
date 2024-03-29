/*
 * Copyright (c) 2019-2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
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

/* Parameter set defining a FIDO2 make credential operation. */
struct param {
	char pin[MAXSTR];
	char rp_id[MAXSTR];
	char rp_name[MAXSTR];
	char user_icon[MAXSTR];
	char user_name[MAXSTR];
	char user_nick[MAXSTR];
	int ext;
	int seed;
	struct blob cdh;
	struct blob excl_cred;
	struct blob user_id;
	struct blob wire_data;
	uint8_t excl_count;
	uint8_t rk;
	uint8_t type;
	uint8_t opt;
	uint8_t uv;
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * make credential using the example parameters above.
 */
static const uint8_t dummy_wire_data_fido[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_AUTHKEY,
	WIREDATA_CTAP_CBOR_PINTOKEN,
	WIREDATA_CTAP_KEEPALIVE,
	WIREDATA_CTAP_KEEPALIVE,
	WIREDATA_CTAP_KEEPALIVE,
	WIREDATA_CTAP_CBOR_CRED,
};

/*
 * Collection of HID reports from an authenticator issued with a U2F
 * registration using the example parameters above.
 */
static const uint8_t dummy_wire_data_u2f[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_U2F_6985,
	WIREDATA_CTAP_U2F_6985,
	WIREDATA_CTAP_U2F_6985,
	WIREDATA_CTAP_U2F_6985,
	WIREDATA_CTAP_U2F_6985,
	WIREDATA_CTAP_U2F_REGISTER,
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
	    cbor_array_size(item) != 17 ||
	    (v = cbor_array_handle(item)) == NULL)
		goto fail;

	if (unpack_byte(v[0], &p->rk) < 0 ||
	    unpack_byte(v[1], &p->type) < 0 ||
	    unpack_byte(v[2], &p->opt) < 0 ||
	    unpack_byte(v[3], &p->uv) < 0 ||
	    unpack_byte(v[4], &p->excl_count) < 0 ||
	    unpack_int(v[5], &p->ext) < 0 ||
	    unpack_int(v[6], &p->seed) < 0 ||
	    unpack_string(v[7], p->pin) < 0 ||
	    unpack_string(v[8], p->rp_id) < 0 ||
	    unpack_string(v[9], p->rp_name) < 0 ||
	    unpack_string(v[10], p->user_icon) < 0 ||
	    unpack_string(v[11], p->user_name) < 0 ||
	    unpack_string(v[12], p->user_nick) < 0 ||
	    unpack_blob(v[13], &p->cdh) < 0 ||
	    unpack_blob(v[14], &p->user_id) < 0 ||
	    unpack_blob(v[15], &p->wire_data) < 0 ||
	    unpack_blob(v[16], &p->excl_cred) < 0)
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
	cbor_item_t *argv[17], *array = NULL;
	size_t cbor_alloc_len, cbor_len = 0;
	unsigned char *cbor = NULL;

	memset(argv, 0, sizeof(argv));

	if ((array = cbor_new_definite_array(17)) == NULL ||
	    (argv[0] = pack_byte(p->rk)) == NULL ||
	    (argv[1] = pack_byte(p->type)) == NULL ||
	    (argv[2] = pack_byte(p->opt)) == NULL ||
	    (argv[3] = pack_byte(p->uv)) == NULL ||
	    (argv[4] = pack_byte(p->excl_count)) == NULL ||
	    (argv[5] = pack_int(p->ext)) == NULL ||
	    (argv[6] = pack_int(p->seed)) == NULL ||
	    (argv[7] = pack_string(p->pin)) == NULL ||
	    (argv[8] = pack_string(p->rp_id)) == NULL ||
	    (argv[9] = pack_string(p->rp_name)) == NULL ||
	    (argv[10] = pack_string(p->user_icon)) == NULL ||
	    (argv[11] = pack_string(p->user_name)) == NULL ||
	    (argv[12] = pack_string(p->user_nick)) == NULL ||
	    (argv[13] = pack_blob(&p->cdh)) == NULL ||
	    (argv[14] = pack_blob(&p->user_id)) == NULL ||
	    (argv[15] = pack_blob(&p->wire_data)) == NULL ||
	    (argv[16] = pack_blob(&p->excl_cred)) == NULL)
		goto fail;

	for (size_t i = 0; i < 17; i++)
		if (cbor_array_push(array, argv[i]) == false)
			goto fail;

	if ((cbor_len = cbor_serialize_alloc(array, &cbor,
	    &cbor_alloc_len)) == 0 || cbor_len > len) {
		cbor_len = 0;
		goto fail;
	}

	memcpy(ptr, cbor, cbor_len);
fail:
	for (size_t i = 0; i < 17; i++)
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
	uint8_t blob[MAXCORPUS];
	size_t blob_len;

	memset(&dummy, 0, sizeof(dummy));

	dummy.type = 1;
	dummy.ext = FIDO_EXT_HMAC_SECRET;

	strlcpy(dummy.pin, dummy_pin, sizeof(dummy.pin));
	strlcpy(dummy.rp_id, dummy_rp_id, sizeof(dummy.rp_id));
	strlcpy(dummy.rp_name, dummy_rp_name, sizeof(dummy.rp_name));
	strlcpy(dummy.user_icon, dummy_user_icon, sizeof(dummy.user_icon));
	strlcpy(dummy.user_name, dummy_user_name, sizeof(dummy.user_name));
	strlcpy(dummy.user_nick, dummy_user_nick, sizeof(dummy.user_nick));

	dummy.cdh.len = sizeof(dummy_cdh);
	dummy.user_id.len = sizeof(dummy_user_id);
	dummy.wire_data.len = sizeof(dummy_wire_data_fido);

	memcpy(&dummy.cdh.body, &dummy_cdh, dummy.cdh.len);
	memcpy(&dummy.user_id.body, &dummy_user_id, dummy.user_id.len);
	memcpy(&dummy.wire_data.body, &dummy_wire_data_fido,
	    dummy.wire_data.len);

	assert((blob_len = pack(blob, sizeof(blob), &dummy)) != 0);

	if (blob_len > len) {
		memcpy(ptr, blob, len);
		return len;
	}

	memcpy(ptr, blob, blob_len);

	return blob_len;
}

static void
make_cred(fido_cred_t *cred, uint8_t opt, int type, const struct blob *cdh,
    const char *rp_id, const char *rp_name, const struct blob *user_id,
    const char *user_name, const char *user_nick, const char *user_icon,
    int ext, uint8_t rk, uint8_t uv, const char *pin, uint8_t excl_count,
    const struct blob *excl_cred)
{
	fido_dev_t *dev;

	if ((dev = open_dev(opt & 2)) == NULL)
		return;
	if (opt & 1)
		fido_dev_force_u2f(dev);

	for (uint8_t i = 0; i < excl_count; i++)
		fido_cred_exclude(cred, excl_cred->body, excl_cred->len);

	fido_cred_set_type(cred, type);
	fido_cred_set_clientdata_hash(cred, cdh->body, cdh->len);
	fido_cred_set_rp(cred, rp_id, rp_name);
	fido_cred_set_user(cred, user_id->body, user_id->len, user_name,
	    user_nick, user_icon);

	if (ext & FIDO_EXT_HMAC_SECRET)
		fido_cred_set_extensions(cred, FIDO_EXT_HMAC_SECRET);
	if (ext & FIDO_EXT_CRED_BLOB)
		fido_cred_set_blob(cred, user_id->body, user_id->len);
	if (ext & FIDO_EXT_LARGEBLOB_KEY)
		fido_cred_set_extensions(cred, FIDO_EXT_LARGEBLOB_KEY);
	if (ext & FIDO_EXT_MINPINLEN)
		fido_cred_set_pin_minlen(cred, strlen(pin));

	if (rk & 1)
		fido_cred_set_rk(cred, FIDO_OPT_TRUE);
	if (uv & 1)
		fido_cred_set_uv(cred, FIDO_OPT_TRUE);
	if (user_id->len)
		fido_cred_set_prot(cred, user_id->body[0] & 0x03);

	/* repeat memory operations to trigger reallocation paths */
	fido_cred_set_type(cred, type);
	fido_cred_set_clientdata_hash(cred, cdh->body, cdh->len);
	fido_cred_set_rp(cred, rp_id, rp_name);
	fido_cred_set_user(cred, user_id->body, user_id->len, user_name,
	    user_nick, user_icon);

	if (strlen(pin) == 0)
		pin = NULL;

	fido_dev_make_cred(dev, cred, (opt & 1) ? NULL : pin);

	fido_dev_cancel(dev);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
verify_cred(int type, const unsigned char *cdh_ptr, size_t cdh_len,
    const char *rp_id, const char *rp_name, const unsigned char *authdata_ptr,
    size_t authdata_len, const unsigned char *authdata_raw_ptr,
    size_t authdata_raw_len, int ext, uint8_t rk, uint8_t uv,
    const unsigned char *x5c_ptr, size_t x5c_len, const unsigned char *sig_ptr,
    size_t sig_len, const unsigned char *attstmt_ptr, size_t attstmt_len,
    const char *fmt, int prot, size_t minpinlen)
{
	fido_cred_t *cred;
	uint8_t flags;
	uint32_t sigcount;
	int r;

	if ((cred = fido_cred_new()) == NULL)
		return;

	fido_cred_set_type(cred, type);
	fido_cred_set_clientdata_hash(cred, cdh_ptr, cdh_len);
	fido_cred_set_rp(cred, rp_id, rp_name);
	consume(authdata_ptr, authdata_len);
	consume(authdata_raw_ptr, authdata_raw_len);
	consume(x5c_ptr, x5c_len);
	consume(sig_ptr, sig_len);
	consume(attstmt_ptr, attstmt_len);
	if (fido_cred_set_authdata(cred, authdata_ptr, authdata_len) != FIDO_OK)
		fido_cred_set_authdata_raw(cred, authdata_raw_ptr,
		    authdata_raw_len);
	fido_cred_set_extensions(cred, ext);
	if (fido_cred_set_attstmt(cred, attstmt_ptr, attstmt_len) != FIDO_OK) {
		fido_cred_set_x509(cred, x5c_ptr, x5c_len);
		fido_cred_set_sig(cred, sig_ptr, sig_len);
	}
	fido_cred_set_prot(cred, prot);
	fido_cred_set_pin_minlen(cred, minpinlen);

	if (rk & 1)
		fido_cred_set_rk(cred, FIDO_OPT_TRUE);
	if (uv & 1)
		fido_cred_set_uv(cred, FIDO_OPT_TRUE);
	if (fmt)
		fido_cred_set_fmt(cred, fmt);

	/* repeat memory operations to trigger reallocation paths */
	if (fido_cred_set_authdata(cred, authdata_ptr, authdata_len) != FIDO_OK)
		fido_cred_set_authdata_raw(cred, authdata_raw_ptr,
		    authdata_raw_len);
	if (fido_cred_set_attstmt(cred, attstmt_ptr, attstmt_len) != FIDO_OK) {
		fido_cred_set_x509(cred, x5c_ptr, x5c_len);
		fido_cred_set_sig(cred, sig_ptr, sig_len);
	}
	fido_cred_set_x509(cred, x5c_ptr, x5c_len);
	fido_cred_set_sig(cred, sig_ptr, sig_len);

	r = fido_cred_verify(cred);
	consume(&r, sizeof(r));
	r = fido_cred_verify_self(cred);
	consume(&r, sizeof(r));

	consume(fido_cred_pubkey_ptr(cred), fido_cred_pubkey_len(cred));
	consume(fido_cred_id_ptr(cred), fido_cred_id_len(cred));
	consume(fido_cred_aaguid_ptr(cred), fido_cred_aaguid_len(cred));
	consume(fido_cred_user_id_ptr(cred), fido_cred_user_id_len(cred));
	consume_str(fido_cred_user_name(cred));
	consume_str(fido_cred_display_name(cred));
	consume(fido_cred_largeblob_key_ptr(cred),
	    fido_cred_largeblob_key_len(cred));

	flags = fido_cred_flags(cred);
	consume(&flags, sizeof(flags));
	sigcount = fido_cred_sigcount(cred);
	consume(&sigcount, sizeof(sigcount));
	type = fido_cred_type(cred);
	consume(&type, sizeof(type));
	minpinlen = fido_cred_pin_minlen(cred);
	consume(&minpinlen, sizeof(minpinlen));

	fido_cred_free(&cred);
}

static void
test_cred(const struct param *p)
{
	fido_cred_t *cred = NULL;
	int cose_alg = 0;

	if ((cred = fido_cred_new()) == NULL)
		return;

	switch (p->type & 3) {
	case 0:
		cose_alg = COSE_ES256;
		break;
	case 1:
		cose_alg = COSE_RS256;
		break;
	case 2:
		cose_alg = COSE_ES384;
		break;
	default:
		cose_alg = COSE_EDDSA;
		break;
	}

	set_wire_data(p->wire_data.body, p->wire_data.len);

	make_cred(cred, p->opt, cose_alg, &p->cdh, p->rp_id, p->rp_name,
	    &p->user_id, p->user_name, p->user_nick, p->user_icon, p->ext,
	    p->rk, p->uv, p->pin, p->excl_count, &p->excl_cred);

	verify_cred(cose_alg,
	    fido_cred_clientdata_hash_ptr(cred),
	    fido_cred_clientdata_hash_len(cred), fido_cred_rp_id(cred),
	    fido_cred_rp_name(cred), fido_cred_authdata_ptr(cred),
	    fido_cred_authdata_len(cred), fido_cred_authdata_raw_ptr(cred),
	    fido_cred_authdata_raw_len(cred), p->ext, p->rk, p->uv,
	    fido_cred_x5c_ptr(cred), fido_cred_x5c_len(cred),
	    fido_cred_sig_ptr(cred), fido_cred_sig_len(cred),
	    fido_cred_attstmt_ptr(cred), fido_cred_attstmt_len(cred),
	    fido_cred_fmt(cred), fido_cred_prot(cred),
	    fido_cred_pin_minlen(cred));

	fido_cred_free(&cred);
}

static void
test_touch(const struct param *p)
{
	fido_dev_t *dev;
	int r;
	int touched;

	set_wire_data(p->wire_data.body, p->wire_data.len);

	if ((dev = open_dev(p->opt & 2)) == NULL)
		return;
	if (p->opt & 1)
		fido_dev_force_u2f(dev);

	r = fido_dev_get_touch_begin(dev);
	consume_str(fido_strerr(r));
	r = fido_dev_get_touch_status(dev, &touched, -1);
	consume_str(fido_strerr(r));
	consume(&touched, sizeof(touched));

	fido_dev_cancel(dev);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
test_misc(const struct param *p)
{
	fido_cred_t *cred = NULL;

	if ((cred = fido_cred_new()) == NULL)
		return;

	/* reuse user id as credential id */
	fido_cred_set_id(cred, p->user_id.body, p->user_id.len);
	consume(fido_cred_id_ptr(cred), fido_cred_id_len(cred));
	fido_cred_free(&cred);
}

void
test(const struct param *p)
{
	prng_init((unsigned int)p->seed);
	fuzz_clock_reset();
	fido_init(FIDO_DEBUG);
	fido_set_log_handler(consume_str);

	test_cred(p);
	test_touch(p);
	test_misc(p);
}

void
mutate(struct param *p, unsigned int seed, unsigned int flags) NO_MSAN
{
	if (flags & MUTATE_SEED)
		p->seed = (int)seed;

	if (flags & MUTATE_PARAM) {
		mutate_byte(&p->rk);
		mutate_byte(&p->type);
		mutate_byte(&p->opt);
		mutate_byte(&p->uv);
		mutate_byte(&p->excl_count);
		mutate_int(&p->ext);
		mutate_blob(&p->cdh);
		mutate_blob(&p->user_id);
		mutate_blob(&p->excl_cred);
		mutate_string(p->pin);
		mutate_string(p->user_icon);
		mutate_string(p->user_name);
		mutate_string(p->user_nick);
		mutate_string(p->rp_id);
		mutate_string(p->rp_name);
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
