/*
 * Copyright (c) 2020 Yubico AB. All rights reserved.
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

/* Parameter set defining a FIDO2 "large blob" operation. */
struct param {
	char pin[MAXSTR];
	int seed;
	struct blob key;
	struct blob get_wiredata;
	struct blob set_wiredata;
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'authenticatorLargeBlobs' 'get' command.
 */
static const uint8_t dummy_get_wiredata[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_LARGEBLOB_GET_ARRAY
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'authenticatorLargeBlobs' 'set' command.
 */
static const uint8_t dummy_set_wiredata[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_LARGEBLOB_GET_ARRAY,
	WIREDATA_CTAP_CBOR_AUTHKEY,
	WIREDATA_CTAP_CBOR_PINTOKEN,
	WIREDATA_CTAP_CBOR_STATUS
};

/*
 * XXX this needs to match the encrypted blob embedded in
 * WIREDATA_CTAP_CBOR_LARGEBLOB_GET_ARRAY.
 */
static const uint8_t dummy_key[] = {
	0xa9, 0x1b, 0xc4, 0xdd, 0xfc, 0x9a, 0x93, 0x79,
	0x75, 0xba, 0xf7, 0x7f, 0x4d, 0x57, 0xfc, 0xa6,
	0xe1, 0xf8, 0x06, 0x43, 0x23, 0x99, 0x51, 0x32,
	0xce, 0x6e, 0x19, 0x84, 0x50, 0x13, 0x2d, 0x7b
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
	    cbor_array_size(item) != 5 ||
	    (v = cbor_array_handle(item)) == NULL)
		goto fail;

	if (unpack_int(v[0], &p->seed) < 0 ||
	    unpack_string(v[1], p->pin) < 0 ||
	    unpack_blob(v[2], &p->key) < 0 ||
	    unpack_blob(v[3], &p->get_wiredata) < 0 ||
	    unpack_blob(v[4], &p->set_wiredata) < 0)
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
	cbor_item_t *argv[5], *array = NULL;
	size_t cbor_alloc_len, cbor_len = 0;
	unsigned char *cbor = NULL;

	memset(argv, 0, sizeof(argv));

	if ((array = cbor_new_definite_array(5)) == NULL ||
	    (argv[0] = pack_int(p->seed)) == NULL ||
	    (argv[1] = pack_string(p->pin)) == NULL ||
	    (argv[2] = pack_blob(&p->key)) == NULL ||
	    (argv[3] = pack_blob(&p->get_wiredata)) == NULL ||
	    (argv[4] = pack_blob(&p->set_wiredata)) == NULL)
		goto fail;

	for (size_t i = 0; i < 5; i++)
		if (cbor_array_push(array, argv[i]) == false)
			goto fail;

	if ((cbor_len = cbor_serialize_alloc(array, &cbor,
	    &cbor_alloc_len)) == 0 || cbor_len > len) {
		cbor_len = 0;
		goto fail;
	}

	memcpy(ptr, cbor, cbor_len);
fail:
	for (size_t i = 0; i < 5; i++)
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

	dummy.get_wiredata.len = sizeof(dummy_get_wiredata);
	dummy.set_wiredata.len = sizeof(dummy_set_wiredata);
	dummy.key.len = sizeof(dummy_key);

	memcpy(&dummy.get_wiredata.body, &dummy_get_wiredata,
	    dummy.get_wiredata.len);
	memcpy(&dummy.set_wiredata.body, &dummy_set_wiredata,
	    dummy.set_wiredata.len);
	memcpy(&dummy.key.body, &dummy_key, dummy.key.len);

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

	if ((dev = open_dev(0)) == NULL)
		return NULL;

	return dev;
}

static void
get_blob(const struct param *p, int array)
{
	fido_dev_t *dev;
	u_char *ptr = NULL;
	size_t len = 0;

	set_wire_data(p->get_wiredata.body, p->get_wiredata.len);

	if ((dev = prepare_dev()) == NULL)
		return;

	if (array)
		fido_dev_largeblob_get_array(dev, &ptr, &len);
	else
		fido_dev_largeblob_get(dev, p->key.body, p->key.len, &ptr, &len);
	consume(ptr, len);
	free(ptr);

	fido_dev_close(dev);
	fido_dev_free(&dev);
}


static void
set_blob(const struct param *p, int op)
{
	fido_dev_t *dev;
	const char *pin;

	set_wire_data(p->set_wiredata.body, p->set_wiredata.len);

	if ((dev = prepare_dev()) == NULL)
		return;
	pin = p->pin;
	if (strlen(pin) == 0)
		pin = NULL;

	switch (op) {
	case 0:
		fido_dev_largeblob_remove(dev, p->key.body, p->key.len, pin);
		break;
	case 1:
		/* XXX reuse p->get_wiredata as the blob to be set */
		fido_dev_largeblob_set(dev, p->key.body, p->key.len,
		    p->get_wiredata.body, p->get_wiredata.len, pin);
		break;
	case 2:
		/* XXX reuse p->get_wiredata as the body of the cbor array */
		fido_dev_largeblob_set_array(dev, p->get_wiredata.body,
		    p->get_wiredata.len, pin);
	}

	fido_dev_close(dev);
	fido_dev_free(&dev);
}

void
test(const struct param *p)
{
	prng_init((unsigned int)p->seed);
	fuzz_clock_reset();
	fido_init(FIDO_DEBUG);
	fido_set_log_handler(consume_str);

	get_blob(p, 0);
	get_blob(p, 1);
	set_blob(p, 0);
	set_blob(p, 1);
	set_blob(p, 2);
}

void
mutate(struct param *p, unsigned int seed, unsigned int flags) NO_MSAN
{
	if (flags & MUTATE_SEED)
		p->seed = (int)seed;

	if (flags & MUTATE_PARAM) {
		mutate_blob(&p->key);
		mutate_string(p->pin);
	}

	if (flags & MUTATE_WIREDATA) {
		mutate_blob(&p->get_wiredata);
		mutate_blob(&p->set_wiredata);
	}
}
