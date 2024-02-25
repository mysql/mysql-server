/*
 * Copyright (c) 2019-2021 Yubico AB. All rights reserved.
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
#include "dummy.h"

#include "../openbsd-compat/openbsd-compat.h"

/* Parameter set defining a FIDO2 credential management operation. */
struct param {
	char pin[MAXSTR];
	char rp_id[MAXSTR];
	int seed;
	struct blob cred_id;
	struct blob del_wire_data;
	struct blob meta_wire_data;
	struct blob rk_wire_data;
	struct blob rp_wire_data;
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'getCredsMetadata' credential management command.
 */
static const uint8_t dummy_meta_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_AUTHKEY,
	WIREDATA_CTAP_CBOR_PINTOKEN,
	WIREDATA_CTAP_CBOR_CREDMAN_META,
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'enumerateRPsBegin' credential management command.
 */
static const uint8_t dummy_rp_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_AUTHKEY,
	WIREDATA_CTAP_CBOR_PINTOKEN,
	WIREDATA_CTAP_CBOR_CREDMAN_RPLIST,
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'enumerateCredentialsBegin' credential management command.
 */
static const uint8_t dummy_rk_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_AUTHKEY,
	WIREDATA_CTAP_CBOR_PINTOKEN,
	WIREDATA_CTAP_CBOR_CREDMAN_RKLIST,
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'deleteCredential' credential management command.
 */
static const uint8_t dummy_del_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_AUTHKEY,
	WIREDATA_CTAP_CBOR_PINTOKEN,
	WIREDATA_CTAP_CBOR_STATUS,
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
	    cbor_array_size(item) != 8 ||
	    (v = cbor_array_handle(item)) == NULL)
		goto fail;

	if (unpack_int(v[0], &p->seed) < 0 ||
	    unpack_string(v[1], p->pin) < 0 ||
	    unpack_string(v[2], p->rp_id) < 0 ||
	    unpack_blob(v[3], &p->cred_id) < 0 ||
	    unpack_blob(v[4], &p->meta_wire_data) < 0 ||
	    unpack_blob(v[5], &p->rp_wire_data) < 0 ||
	    unpack_blob(v[6], &p->rk_wire_data) < 0 ||
	    unpack_blob(v[7], &p->del_wire_data) < 0)
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
	cbor_item_t *argv[8], *array = NULL;
	size_t cbor_alloc_len, cbor_len = 0;
	unsigned char *cbor = NULL;

	memset(argv, 0, sizeof(argv));

	if ((array = cbor_new_definite_array(8)) == NULL ||
	    (argv[0] = pack_int(p->seed)) == NULL ||
	    (argv[1] = pack_string(p->pin)) == NULL ||
	    (argv[2] = pack_string(p->rp_id)) == NULL ||
	    (argv[3] = pack_blob(&p->cred_id)) == NULL ||
	    (argv[4] = pack_blob(&p->meta_wire_data)) == NULL ||
	    (argv[5] = pack_blob(&p->rp_wire_data)) == NULL ||
	    (argv[6] = pack_blob(&p->rk_wire_data)) == NULL ||
	    (argv[7] = pack_blob(&p->del_wire_data)) == NULL)
		goto fail;

	for (size_t i = 0; i < 8; i++)
		if (cbor_array_push(array, argv[i]) == false)
			goto fail;

	if ((cbor_len = cbor_serialize_alloc(array, &cbor,
	    &cbor_alloc_len)) == 0 || cbor_len > len) {
		cbor_len = 0;
		goto fail;
	}

	memcpy(ptr, cbor, cbor_len);
fail:
	for (size_t i = 0; i < 8; i++)
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

	strlcpy(dummy.pin, dummy_pin, sizeof(dummy.pin));
	strlcpy(dummy.rp_id, dummy_rp_id, sizeof(dummy.rp_id));

	dummy.meta_wire_data.len = sizeof(dummy_meta_wire_data);
	dummy.rp_wire_data.len = sizeof(dummy_rp_wire_data);
	dummy.rk_wire_data.len = sizeof(dummy_rk_wire_data);
	dummy.del_wire_data.len = sizeof(dummy_del_wire_data);
	dummy.cred_id.len = sizeof(dummy_cred_id);

	memcpy(&dummy.meta_wire_data.body, &dummy_meta_wire_data,
	    dummy.meta_wire_data.len);
	memcpy(&dummy.rp_wire_data.body, &dummy_rp_wire_data,
	    dummy.rp_wire_data.len);
	memcpy(&dummy.rk_wire_data.body, &dummy_rk_wire_data,
	    dummy.rk_wire_data.len);
	memcpy(&dummy.del_wire_data.body, &dummy_del_wire_data,
	    dummy.del_wire_data.len);
	memcpy(&dummy.cred_id.body, &dummy_cred_id, dummy.cred_id.len);

	assert((blob_len = pack(blob, sizeof(blob), &dummy)) != 0);

	if (blob_len > len) {
		memcpy(ptr, blob, len);
		return len;
	}

	memcpy(ptr, blob, blob_len);

	return blob_len;
}

static fido_dev_t *
prepare_dev(void)
{
	fido_dev_t *dev;
	bool x;

	if ((dev = open_dev(0)) == NULL)
		return NULL;

	x = fido_dev_is_fido2(dev);
	consume(&x, sizeof(x));
	x = fido_dev_supports_cred_prot(dev);
	consume(&x, sizeof(x));
	x = fido_dev_supports_credman(dev);
	consume(&x, sizeof(x));

	return dev;
}

static void
get_metadata(const struct param *p)
{
	fido_dev_t *dev;
	fido_credman_metadata_t *metadata;
	uint64_t existing;
	uint64_t remaining;

	set_wire_data(p->meta_wire_data.body, p->meta_wire_data.len);

	if ((dev = prepare_dev()) == NULL)
		return;

	if ((metadata = fido_credman_metadata_new()) == NULL) {
		fido_dev_close(dev);
		fido_dev_free(&dev);
		return;
	}

	fido_credman_get_dev_metadata(dev, metadata, p->pin);

	existing = fido_credman_rk_existing(metadata);
	remaining = fido_credman_rk_remaining(metadata);
	consume(&existing, sizeof(existing));
	consume(&remaining, sizeof(remaining));

	fido_credman_metadata_free(&metadata);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
get_rp_list(const struct param *p)
{
	fido_dev_t *dev;
	fido_credman_rp_t *rp;

	set_wire_data(p->rp_wire_data.body, p->rp_wire_data.len);

	if ((dev = prepare_dev()) == NULL)
		return;

	if ((rp = fido_credman_rp_new()) == NULL) {
		fido_dev_close(dev);
		fido_dev_free(&dev);
		return;
	}

	fido_credman_get_dev_rp(dev, rp, p->pin);

	/* +1 on purpose */
	for (size_t i = 0; i < fido_credman_rp_count(rp) + 1; i++) {
		consume(fido_credman_rp_id_hash_ptr(rp, i),
		    fido_credman_rp_id_hash_len(rp, i));
		consume_str(fido_credman_rp_id(rp, i));
		consume_str(fido_credman_rp_name(rp, i));
	}

	fido_credman_rp_free(&rp);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
get_rk_list(const struct param *p)
{
	fido_dev_t *dev;
	fido_credman_rk_t *rk;
	const fido_cred_t *cred;
	int val;

	set_wire_data(p->rk_wire_data.body, p->rk_wire_data.len);

	if ((dev = prepare_dev()) == NULL)
		return;

	if ((rk = fido_credman_rk_new()) == NULL) {
		fido_dev_close(dev);
		fido_dev_free(&dev);
		return;
	}

	fido_credman_get_dev_rk(dev, p->rp_id, rk, p->pin);

	/* +1 on purpose */
	for (size_t i = 0; i < fido_credman_rk_count(rk) + 1; i++) {
		if ((cred = fido_credman_rk(rk, i)) == NULL) {
			assert(i >= fido_credman_rk_count(rk));
			continue;
		}
		val = fido_cred_type(cred);
		consume(&val, sizeof(val));
		consume(fido_cred_id_ptr(cred), fido_cred_id_len(cred));
		consume(fido_cred_pubkey_ptr(cred), fido_cred_pubkey_len(cred));
		consume(fido_cred_user_id_ptr(cred),
		    fido_cred_user_id_len(cred));
		consume_str(fido_cred_user_name(cred));
		consume_str(fido_cred_display_name(cred));
		val = fido_cred_prot(cred);
		consume(&val, sizeof(val));
	}

	fido_credman_rk_free(&rk);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
del_rk(const struct param *p)
{
	fido_dev_t *dev;

	set_wire_data(p->del_wire_data.body, p->del_wire_data.len);

	if ((dev = prepare_dev()) == NULL)
		return;

	fido_credman_del_dev_rk(dev, p->cred_id.body, p->cred_id.len, p->pin);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
set_rk(const struct param *p)
{
	fido_dev_t *dev = NULL;
	fido_cred_t *cred = NULL;
	const char *pin = p->pin;
	int r0, r1, r2;

	set_wire_data(p->del_wire_data.body, p->del_wire_data.len);

	if ((dev = prepare_dev()) == NULL)
		return;
	if ((cred = fido_cred_new()) == NULL)
		goto out;
	r0 = fido_cred_set_id(cred, p->cred_id.body, p->cred_id.len);
	r1 = fido_cred_set_user(cred, p->cred_id.body, p->cred_id.len, p->rp_id,
	    NULL, NULL);
	if (strlen(pin) == 0)
		pin = NULL;
	r2 = fido_credman_set_dev_rk(dev, cred, pin);
	consume(&r0, sizeof(r0));
	consume(&r1, sizeof(r1));
	consume(&r2, sizeof(r2));
out:
	fido_dev_close(dev);
	fido_dev_free(&dev);
	fido_cred_free(&cred);
}

void
test(const struct param *p)
{
	prng_init((unsigned int)p->seed);
	fuzz_clock_reset();
	fido_init(FIDO_DEBUG);
	fido_set_log_handler(consume_str);

	get_metadata(p);
	get_rp_list(p);
	get_rk_list(p);
	del_rk(p);
	set_rk(p);
}

void
mutate(struct param *p, unsigned int seed, unsigned int flags) NO_MSAN
{
	if (flags & MUTATE_SEED)
		p->seed = (int)seed;

	if (flags & MUTATE_PARAM) {
		mutate_blob(&p->cred_id);
		mutate_string(p->pin);
		mutate_string(p->rp_id);
	}

	if (flags & MUTATE_WIREDATA) {
		mutate_blob(&p->meta_wire_data);
		mutate_blob(&p->rp_wire_data);
		mutate_blob(&p->rk_wire_data);
		mutate_blob(&p->del_wire_data);
	}
}
