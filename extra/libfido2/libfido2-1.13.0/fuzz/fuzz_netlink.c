/*
 * Copyright (c) 2020 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../openbsd-compat/openbsd-compat.h"
#include "mutator_aux.h"
#include "dummy.h"

struct param {
	int seed;
	int dev;
	struct blob wiredata;
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
	    cbor_array_size(item) != 3 ||
	    (v = cbor_array_handle(item)) == NULL)
		goto fail;

	if (unpack_int(v[0], &p->seed) < 0 ||
	    unpack_int(v[1], &p->dev) < 0 ||
	    unpack_blob(v[2], &p->wiredata) < 0)
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
	cbor_item_t *argv[3], *array = NULL;
	size_t cbor_alloc_len, cbor_len = 0;
	unsigned char *cbor = NULL;

	memset(argv, 0, sizeof(argv));

	if ((array = cbor_new_definite_array(3)) == NULL ||
	    (argv[0] = pack_int(p->seed)) == NULL ||
	    (argv[1] = pack_int(p->dev)) == NULL ||
	    (argv[2] = pack_blob(&p->wiredata)) == NULL)
		goto fail;

	for (size_t i = 0; i < 3; i++)
		if (cbor_array_push(array, argv[i]) == false)
			goto fail;

	if ((cbor_len = cbor_serialize_alloc(array, &cbor,
	    &cbor_alloc_len)) == 0 || cbor_len > len) {
		cbor_len = 0;
		goto fail;
	}

	memcpy(ptr, cbor, cbor_len);
fail:
	for (size_t i = 0; i < 3; i++)
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

	dummy.wiredata.len = sizeof(dummy_netlink_wiredata);
	memcpy(&dummy.wiredata.body, &dummy_netlink_wiredata,
	    dummy.wiredata.len);

	assert((blob_len = pack(blob, sizeof(blob), &dummy)) != 0);

	if (blob_len > len) {
		memcpy(ptr, blob, len);
		return len;
	}

	memcpy(ptr, blob, blob_len);

	return blob_len;
}

void
test(const struct param *p)
{
	fido_nl_t *nl;
	uint32_t target;

	prng_init((unsigned int)p->seed);
	fuzz_clock_reset();
	fido_init(FIDO_DEBUG);
	fido_set_log_handler(consume_str);

	set_netlink_io_functions(fd_read, fd_write);
	set_wire_data(p->wiredata.body, p->wiredata.len);

	if ((nl = fido_nl_new()) == NULL)
		return;

	consume(&nl->fd, sizeof(nl->fd));
	consume(&nl->nfc_type, sizeof(nl->nfc_type));
	consume(&nl->nfc_mcastgrp, sizeof(nl->nfc_mcastgrp));
	consume(&nl->saddr, sizeof(nl->saddr));

	fido_nl_power_nfc(nl, (uint32_t)p->dev);

	if (fido_nl_get_nfc_target(nl, (uint32_t)p->dev, &target) == 0)
		consume(&target, sizeof(target));

	fido_nl_free(&nl);
}

void
mutate(struct param *p, unsigned int seed, unsigned int flags) NO_MSAN
{
	if (flags & MUTATE_SEED)
		p->seed = (int)seed;

	if (flags & MUTATE_PARAM)
		mutate_int(&p->dev);

	if (flags & MUTATE_WIREDATA)
		mutate_blob(&p->wiredata);
}
