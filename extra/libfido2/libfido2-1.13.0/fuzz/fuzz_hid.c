/*
 * Copyright (c) 2020-2021 Yubico AB. All rights reserved.
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

extern int fido_hid_get_usage(const uint8_t *, size_t, uint32_t *);
extern int fido_hid_get_report_len(const uint8_t *, size_t, size_t *, size_t *);
extern void set_udev_parameters(const char *, const struct blob *);

struct param {
	int seed;
	char uevent[MAXSTR];
	struct blob report_descriptor;
	struct blob netlink_wiredata;
};

/*
 * Sample HID report descriptor from the FIDO HID interface of a YubiKey 5.
 */
static const uint8_t dummy_report_descriptor[] = {
	0x06, 0xd0, 0xf1, 0x09, 0x01, 0xa1, 0x01, 0x09,
	0x20, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08,
	0x95, 0x40, 0x81, 0x02, 0x09, 0x21, 0x15, 0x00,
	0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x40, 0x91,
	0x02, 0xc0
};

/*
 * Sample uevent file from a Yubico Security Key.
 */
static const char dummy_uevent[] =
	"DRIVER=hid-generic\n"
	"HID_ID=0003:00001050:00000120\n"
	"HID_NAME=Yubico Security Key by Yubico\n"
	"HID_PHYS=usb-0000:00:14.0-3/input0\n"
	"HID_UNIQ=\n"
	"MODALIAS=hid:b0003g0001v00001050p00000120\n";

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
	    cbor_array_size(item) != 4 ||
	    (v = cbor_array_handle(item)) == NULL)
		goto fail;

	if (unpack_int(v[0], &p->seed) < 0 ||
	    unpack_string(v[1], p->uevent) < 0 ||
	    unpack_blob(v[2], &p->report_descriptor) < 0 ||
	    unpack_blob(v[3], &p->netlink_wiredata) < 0)
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
	cbor_item_t *argv[4], *array = NULL;
	size_t cbor_alloc_len, cbor_len = 0;
	unsigned char *cbor = NULL;

	memset(argv, 0, sizeof(argv));

	if ((array = cbor_new_definite_array(4)) == NULL ||
	    (argv[0] = pack_int(p->seed)) == NULL ||
	    (argv[1] = pack_string(p->uevent)) == NULL ||
	    (argv[2] = pack_blob(&p->report_descriptor)) == NULL ||
	    (argv[3] = pack_blob(&p->netlink_wiredata)) == NULL)
		goto fail;

	for (size_t i = 0; i < 4; i++)
		if (cbor_array_push(array, argv[i]) == false)
			goto fail;

	if ((cbor_len = cbor_serialize_alloc(array, &cbor,
	    &cbor_alloc_len)) == 0 || cbor_len > len) {
		cbor_len = 0;
		goto fail;
	}

	memcpy(ptr, cbor, cbor_len);
fail:
	for (size_t i = 0; i < 4; i++)
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

	dummy.report_descriptor.len = sizeof(dummy_report_descriptor);
	strlcpy(dummy.uevent, dummy_uevent, sizeof(dummy.uevent));
	memcpy(&dummy.report_descriptor.body, &dummy_report_descriptor,
	    dummy.report_descriptor.len);
	dummy.netlink_wiredata.len = sizeof(dummy_netlink_wiredata);
	memcpy(&dummy.netlink_wiredata.body, &dummy_netlink_wiredata,
	    dummy.netlink_wiredata.len);

	assert((blob_len = pack(blob, sizeof(blob), &dummy)) != 0);
	if (blob_len > len)
		blob_len = len;

	memcpy(ptr, blob, blob_len);

	return blob_len;
}

static void
get_usage(const struct param *p)
{
	uint32_t usage_page = 0;

	fido_hid_get_usage(p->report_descriptor.body, p->report_descriptor.len,
	    &usage_page);
	consume(&usage_page, sizeof(usage_page));
}

static void
get_report_len(const struct param *p)
{
	size_t report_in_len = 0;
	size_t report_out_len = 0;

	fido_hid_get_report_len(p->report_descriptor.body,
	    p->report_descriptor.len, &report_in_len, &report_out_len);
	consume(&report_in_len, sizeof(report_in_len));
	consume(&report_out_len, sizeof(report_out_len));
}

static void
manifest(const struct param *p)
{
	size_t ndevs, nfound;
	fido_dev_info_t *devlist = NULL, *devlist_set = NULL;
	int16_t vendor_id, product_id;
	fido_dev_io_t io;
	fido_dev_transport_t t;

	memset(&io, 0, sizeof(io));
	memset(&t, 0, sizeof(t));
	set_netlink_io_functions(fd_read, fd_write);
	set_wire_data(p->netlink_wiredata.body, p->netlink_wiredata.len);
	set_udev_parameters(p->uevent, &p->report_descriptor);

	ndevs = uniform_random(64);
	if ((devlist = fido_dev_info_new(ndevs)) == NULL ||
	    (devlist_set = fido_dev_info_new(1)) == NULL ||
	    fido_dev_info_manifest(devlist, ndevs, &nfound) != FIDO_OK)
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
		fido_dev_info_set(devlist_set, 0, fido_dev_info_path(di),
		    fido_dev_info_manufacturer_string(di),
		    fido_dev_info_product_string(di), &io, &t);
	}
out:
	fido_dev_info_free(&devlist, ndevs);
	fido_dev_info_free(&devlist_set, 1);
}

void
test(const struct param *p)
{
	prng_init((unsigned int)p->seed);
	fuzz_clock_reset();
	fido_init(FIDO_DEBUG);
	fido_set_log_handler(consume_str);

	get_usage(p);
	get_report_len(p);
	manifest(p);
}

void
mutate(struct param *p, unsigned int seed, unsigned int flags) NO_MSAN
{
	if (flags & MUTATE_SEED)
		p->seed = (int)seed;

	if (flags & MUTATE_PARAM) {
		mutate_blob(&p->report_descriptor);
		mutate_string(p->uevent);
	}

	if (flags & MUTATE_WIREDATA)
		mutate_blob(&p->netlink_wiredata);
}
