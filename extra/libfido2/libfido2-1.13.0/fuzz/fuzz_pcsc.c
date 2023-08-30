/*
 * Copyright (c) 2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define _FIDO_INTERNAL

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <winscard.h>

#include "mutator_aux.h"
#include "wiredata_fido2.h"
#include "dummy.h"

#include "../src/extern.h"

struct param {
	int seed;
	char path[MAXSTR];
	struct blob pcsc_list;
	struct blob tx_apdu;
	struct blob wiredata_init;
	struct blob wiredata_msg;
};

static const uint8_t dummy_tx_apdu[] = { WIREDATA_CTAP_EXTENDED_APDU };
static const uint8_t dummy_wiredata_init[] = { WIREDATA_CTAP_NFC_INIT };
static const uint8_t dummy_wiredata_msg[] = { WIREDATA_CTAP_NFC_MSG };

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
	    cbor_array_size(item) != 6 ||
	    (v = cbor_array_handle(item)) == NULL)
		goto fail;

	if (unpack_int(v[0], &p->seed) < 0 ||
	    unpack_string(v[1], p->path) < 0 ||
	    unpack_blob(v[2], &p->pcsc_list) < 0 ||
	    unpack_blob(v[3], &p->tx_apdu) < 0 ||
	    unpack_blob(v[4], &p->wiredata_init) < 0 ||
	    unpack_blob(v[5], &p->wiredata_msg) < 0)
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
	cbor_item_t *argv[6], *array = NULL;
	size_t cbor_alloc_len, cbor_len = 0;
	unsigned char *cbor = NULL;

	memset(argv, 0, sizeof(argv));

	if ((array = cbor_new_definite_array(6)) == NULL ||
	    (argv[0] = pack_int(p->seed)) == NULL ||
	    (argv[1] = pack_string(p->path)) == NULL ||
	    (argv[2] = pack_blob(&p->pcsc_list)) == NULL ||
	    (argv[3] = pack_blob(&p->tx_apdu)) == NULL ||
	    (argv[4] = pack_blob(&p->wiredata_init)) == NULL ||
	    (argv[5] = pack_blob(&p->wiredata_msg)) == NULL)
		goto fail;

	for (size_t i = 0; i < 6; i++)
		if (cbor_array_push(array, argv[i]) == false)
			goto fail;

	if ((cbor_len = cbor_serialize_alloc(array, &cbor,
	    &cbor_alloc_len)) == 0 || cbor_len > len) {
		cbor_len = 0;
		goto fail;
	}

	memcpy(ptr, cbor, cbor_len);
fail:
	for (size_t i = 0; i < 6; i++)
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
	uint8_t	blob[MAXCORPUS];
	size_t blob_len;

	memset(&dummy, 0, sizeof(dummy));

	strlcpy(dummy.path, dummy_pcsc_path, sizeof(dummy.path));

	dummy.pcsc_list.len = sizeof(dummy_pcsc_list);
	memcpy(&dummy.pcsc_list.body, &dummy_pcsc_list, dummy.pcsc_list.len);

	dummy.tx_apdu.len = sizeof(dummy_tx_apdu);
	memcpy(&dummy.tx_apdu.body, &dummy_tx_apdu, dummy.tx_apdu.len);

	dummy.wiredata_init.len = sizeof(dummy_wiredata_init);
	memcpy(&dummy.wiredata_init.body, &dummy_wiredata_init,
	    dummy.wiredata_init.len);

	dummy.wiredata_msg.len = sizeof(dummy_wiredata_msg);
	memcpy(&dummy.wiredata_msg.body, &dummy_wiredata_msg,
	    dummy.wiredata_msg.len);

	assert((blob_len = pack(blob, sizeof(blob), &dummy)) != 0);

	if (blob_len > len) {
		memcpy(ptr, blob, len);
		return len;
	}

	memcpy(ptr, blob, blob_len);

	return blob_len;
}

static void
test_manifest(void)
{
	size_t ndevs, nfound;
	fido_dev_info_t *devlist = NULL;
	int16_t vendor_id, product_id;
	int r;

	r = fido_pcsc_manifest(NULL, 0, &nfound);
	assert(r == FIDO_OK && nfound == 0);
	r = fido_pcsc_manifest(NULL, 1, &nfound);
	assert(r == FIDO_ERR_INVALID_ARGUMENT);

	ndevs = uniform_random(64);
	if ((devlist = fido_dev_info_new(ndevs)) == NULL ||
	    fido_pcsc_manifest(devlist, ndevs, &nfound) != FIDO_OK)
		goto out;

	for (size_t i = 0; i < nfound; i++) {
		const fido_dev_info_t *di = fido_dev_info_ptr(devlist, i);
		consume_str(fido_dev_info_path(di));
		consume_str(fido_dev_info_manufacturer_string(di));
		consume_str(fido_dev_info_product_string(di));
		vendor_id = fido_dev_info_vendor(di);
		product_id = fido_dev_info_product(di);
		consume(&vendor_id, sizeof(vendor_id));
		consume(&product_id, sizeof(product_id));
	}

out:
	fido_dev_info_free(&devlist, ndevs);
}

static void
test_tx(const char *path, const struct blob *apdu, uint8_t cmd, u_char *rx_buf,
    size_t rx_len)
{
	fido_dev_t dev;
	const u_char *tx_ptr = NULL;
	size_t tx_len = 0;
	int n;

	memset(&dev, 0, sizeof(dev));

	if (fido_dev_set_pcsc(&dev) < 0)
		return;
	if ((dev.io_handle = fido_pcsc_open(path)) == NULL)
		return;

	if (apdu) {
		tx_ptr = apdu->body;
		tx_len = apdu->len;
	}

	fido_pcsc_tx(&dev, cmd, tx_ptr, tx_len);

	if ((n = fido_pcsc_rx(&dev, cmd, rx_buf, rx_len, -1)) >= 0)
		consume(rx_buf, n);

	fido_pcsc_close(dev.io_handle);
}

static void
test_misc(void)
{
	assert(fido_pcsc_open(NULL) == NULL);
	assert(fido_pcsc_write(NULL, NULL, INT_MAX + 1LL) == -1);
}

void
test(const struct param *p)
{
	u_char buf[512];

	prng_init((unsigned int)p->seed);
	fuzz_clock_reset();
	fido_init(FIDO_DEBUG);
	fido_set_log_handler(consume_str);

	set_pcsc_parameters(&p->pcsc_list);
	set_pcsc_io_functions(nfc_read, nfc_write, consume);

	set_wire_data(p->wiredata_init.body, p->wiredata_init.len);
	test_manifest();

	test_misc();

	set_wire_data(p->wiredata_init.body, p->wiredata_init.len);
	test_tx(p->path, NULL, CTAP_CMD_INIT, buf, uniform_random(20));

	set_wire_data(p->wiredata_msg.body, p->wiredata_msg.len);
	test_tx(p->path, &p->tx_apdu, CTAP_CMD_MSG, buf, sizeof(buf));

	set_wire_data(p->wiredata_msg.body, p->wiredata_msg.len);
	test_tx(p->path, &p->tx_apdu, CTAP_CMD_CBOR, buf, sizeof(buf));

	set_wire_data(p->wiredata_msg.body, p->wiredata_msg.len);
	test_tx(p->path, &p->tx_apdu, CTAP_CMD_LOCK, buf, sizeof(buf));
}

void
mutate(struct param *p, unsigned int seed, unsigned int flags) NO_MSAN
{
	if (flags & MUTATE_SEED)
		p->seed = (int)seed;

	if (flags & MUTATE_PARAM) {
		mutate_string(p->path);
		mutate_blob(&p->pcsc_list);
		mutate_blob(&p->tx_apdu);
	}
 
	if (flags & MUTATE_WIREDATA) {
		mutate_blob(&p->wiredata_init);
		mutate_blob(&p->wiredata_msg);
	}
}
