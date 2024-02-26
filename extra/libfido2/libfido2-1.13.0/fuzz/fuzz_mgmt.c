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
#include "dummy.h"

#include "../openbsd-compat/openbsd-compat.h"

#define MAXRPID	64

struct param {
	char pin1[MAXSTR];
	char pin2[MAXSTR];
	struct blob reset_wire_data;
	struct blob info_wire_data;
	struct blob set_pin_wire_data;
	struct blob change_pin_wire_data;
	struct blob retry_wire_data;
	struct blob config_wire_data;
	int seed;
};

static const uint8_t dummy_reset_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_KEEPALIVE,
	WIREDATA_CTAP_KEEPALIVE,
	WIREDATA_CTAP_KEEPALIVE,
	WIREDATA_CTAP_CBOR_STATUS,
};

static const uint8_t dummy_info_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_INFO,
};

static const uint8_t dummy_set_pin_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_AUTHKEY,
	WIREDATA_CTAP_CBOR_STATUS,
};

static const uint8_t dummy_change_pin_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_AUTHKEY,
	WIREDATA_CTAP_CBOR_STATUS,
};

static const uint8_t dummy_retry_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_RETRIES,
};

static const uint8_t dummy_config_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
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
	    cbor_array_size(item) != 9 ||
	    (v = cbor_array_handle(item)) == NULL)
		goto fail;

	if (unpack_int(v[0], &p->seed) < 0 ||
	    unpack_string(v[1], p->pin1) < 0 ||
	    unpack_string(v[2], p->pin2) < 0 ||
	    unpack_blob(v[3], &p->reset_wire_data) < 0 ||
	    unpack_blob(v[4], &p->info_wire_data) < 0 ||
	    unpack_blob(v[5], &p->set_pin_wire_data) < 0 ||
	    unpack_blob(v[6], &p->change_pin_wire_data) < 0 ||
	    unpack_blob(v[7], &p->retry_wire_data) < 0 ||
	    unpack_blob(v[8], &p->config_wire_data) < 0)
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
	cbor_item_t *argv[9], *array = NULL;
	size_t cbor_alloc_len, cbor_len = 0;
	unsigned char *cbor = NULL;

	memset(argv, 0, sizeof(argv));

	if ((array = cbor_new_definite_array(9)) == NULL ||
	    (argv[0] = pack_int(p->seed)) == NULL ||
	    (argv[1] = pack_string(p->pin1)) == NULL ||
	    (argv[2] = pack_string(p->pin2)) == NULL ||
	    (argv[3] = pack_blob(&p->reset_wire_data)) == NULL ||
	    (argv[4] = pack_blob(&p->info_wire_data)) == NULL ||
	    (argv[5] = pack_blob(&p->set_pin_wire_data)) == NULL ||
	    (argv[6] = pack_blob(&p->change_pin_wire_data)) == NULL ||
	    (argv[7] = pack_blob(&p->retry_wire_data)) == NULL ||
	    (argv[8] = pack_blob(&p->config_wire_data)) == NULL)
		goto fail;

	for (size_t i = 0; i < 9; i++)
		if (cbor_array_push(array, argv[i]) == false)
			goto fail;

	if ((cbor_len = cbor_serialize_alloc(array, &cbor,
	    &cbor_alloc_len)) == 0 || cbor_len > len) {
		cbor_len = 0;
		goto fail;
	}

	memcpy(ptr, cbor, cbor_len);
fail:
	for (size_t i = 0; i < 9; i++)
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

	strlcpy(dummy.pin1, dummy_pin1, sizeof(dummy.pin1));
	strlcpy(dummy.pin2, dummy_pin2, sizeof(dummy.pin2));

	dummy.reset_wire_data.len = sizeof(dummy_reset_wire_data);
	dummy.info_wire_data.len = sizeof(dummy_info_wire_data);
	dummy.set_pin_wire_data.len = sizeof(dummy_set_pin_wire_data);
	dummy.change_pin_wire_data.len = sizeof(dummy_change_pin_wire_data);
	dummy.retry_wire_data.len = sizeof(dummy_retry_wire_data);
	dummy.config_wire_data.len = sizeof(dummy_config_wire_data);

	memcpy(&dummy.reset_wire_data.body, &dummy_reset_wire_data,
	    dummy.reset_wire_data.len);
	memcpy(&dummy.info_wire_data.body, &dummy_info_wire_data,
	    dummy.info_wire_data.len);
	memcpy(&dummy.set_pin_wire_data.body, &dummy_set_pin_wire_data,
	    dummy.set_pin_wire_data.len);
	memcpy(&dummy.change_pin_wire_data.body, &dummy_change_pin_wire_data,
	    dummy.change_pin_wire_data.len);
	memcpy(&dummy.retry_wire_data.body, &dummy_retry_wire_data,
	    dummy.retry_wire_data.len);
	memcpy(&dummy.config_wire_data.body, &dummy_config_wire_data,
	    dummy.config_wire_data.len);

	assert((blob_len = pack(blob, sizeof(blob), &dummy)) != 0);

	if (blob_len > len) {
		memcpy(ptr, blob, len);
		return len;
	}

	memcpy(ptr, blob, blob_len);

	return blob_len;
}

static void
dev_reset(const struct param *p)
{
	fido_dev_t *dev;

	set_wire_data(p->reset_wire_data.body, p->reset_wire_data.len);

	if ((dev = open_dev(0)) == NULL)
		return;

	fido_dev_reset(dev);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
dev_get_cbor_info(const struct param *p)
{
	fido_dev_t *dev;
	fido_cbor_info_t *ci;
	uint64_t n;
	uint8_t proto, major, minor, build, flags;
	bool v;

	set_wire_data(p->info_wire_data.body, p->info_wire_data.len);

	if ((dev = open_dev(0)) == NULL)
		return;

	proto = fido_dev_protocol(dev);
	major = fido_dev_major(dev);
	minor = fido_dev_minor(dev);
	build = fido_dev_build(dev);
	flags = fido_dev_flags(dev);

	consume(&proto, sizeof(proto));
	consume(&major, sizeof(major));
	consume(&minor, sizeof(minor));
	consume(&build, sizeof(build));
	consume(&flags, sizeof(flags));

	if ((ci = fido_cbor_info_new()) == NULL)
		goto out;

	fido_dev_get_cbor_info(dev, ci);

	for (size_t i = 0; i < fido_cbor_info_versions_len(ci); i++) {
		char * const *sa = fido_cbor_info_versions_ptr(ci);
		consume(sa[i], strlen(sa[i]));
	}

	for (size_t i = 0; i < fido_cbor_info_extensions_len(ci); i++) {
		char * const *sa = fido_cbor_info_extensions_ptr(ci);
		consume(sa[i], strlen(sa[i]));
	}

	for (size_t i = 0; i < fido_cbor_info_transports_len(ci); i++) {
		char * const *sa = fido_cbor_info_transports_ptr(ci);
		consume(sa[i], strlen(sa[i]));
	}

	for (size_t i = 0; i < fido_cbor_info_options_len(ci); i++) {
		char * const *sa = fido_cbor_info_options_name_ptr(ci);
		const bool *va = fido_cbor_info_options_value_ptr(ci);
		consume(sa[i], strlen(sa[i]));
		consume(&va[i], sizeof(va[i]));
	}

	/* +1 on purpose */
	for (size_t i = 0; i <= fido_cbor_info_algorithm_count(ci); i++) {
		const char *type = fido_cbor_info_algorithm_type(ci, i);
		int cose = fido_cbor_info_algorithm_cose(ci, i);
		consume_str(type);
		consume(&cose, sizeof(cose));
	}

	for (size_t i = 0; i < fido_cbor_info_certs_len(ci); i++) {
		char * const *na = fido_cbor_info_certs_name_ptr(ci);
		const uint64_t *va = fido_cbor_info_certs_value_ptr(ci);
		consume(na[i], strlen(na[i]));
		consume(&va[i], sizeof(va[i]));
	}

	n = fido_cbor_info_maxmsgsiz(ci);
	consume(&n, sizeof(n));
	n = fido_cbor_info_maxcredbloblen(ci);
	consume(&n, sizeof(n));
	n = fido_cbor_info_maxcredcntlst(ci);
	consume(&n, sizeof(n));
	n = fido_cbor_info_maxcredidlen(ci);
	consume(&n, sizeof(n));
	n = fido_cbor_info_maxlargeblob(ci);
	consume(&n, sizeof(n));
	n = fido_cbor_info_fwversion(ci);
	consume(&n, sizeof(n));
	n = fido_cbor_info_minpinlen(ci);
	consume(&n, sizeof(n));
	n = fido_cbor_info_maxrpid_minpinlen(ci);
	consume(&n, sizeof(n));
	n = fido_cbor_info_uv_attempts(ci);
	consume(&n, sizeof(n));
	n = fido_cbor_info_uv_modality(ci);
	consume(&n, sizeof(n));
	n = (uint64_t)fido_cbor_info_rk_remaining(ci);
	consume(&n, sizeof(n));

	consume(fido_cbor_info_aaguid_ptr(ci), fido_cbor_info_aaguid_len(ci));
	consume(fido_cbor_info_protocols_ptr(ci),
	    fido_cbor_info_protocols_len(ci));

	v = fido_cbor_info_new_pin_required(ci);
	consume(&v, sizeof(v));

out:
	fido_dev_close(dev);
	fido_dev_free(&dev);

	fido_cbor_info_free(&ci);
}

static void
dev_set_pin(const struct param *p)
{
	fido_dev_t *dev;

	set_wire_data(p->set_pin_wire_data.body, p->set_pin_wire_data.len);

	if ((dev = open_dev(0)) == NULL)
		return;

	fido_dev_set_pin(dev, p->pin1, NULL);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
dev_change_pin(const struct param *p)
{
	fido_dev_t *dev;

	set_wire_data(p->change_pin_wire_data.body, p->change_pin_wire_data.len);

	if ((dev = open_dev(0)) == NULL)
		return;

	fido_dev_set_pin(dev, p->pin2, p->pin1);
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
dev_get_retry_count(const struct param *p)
{
	fido_dev_t *dev;
	int n = 0;

	set_wire_data(p->retry_wire_data.body, p->retry_wire_data.len);

	if ((dev = open_dev(0)) == NULL)
		return;

	fido_dev_get_retry_count(dev, &n);
	consume(&n, sizeof(n));
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
dev_get_uv_retry_count(const struct param *p)
{
	fido_dev_t *dev;
	int n = 0;

	set_wire_data(p->retry_wire_data.body, p->retry_wire_data.len);

	if ((dev = open_dev(0)) == NULL)
		return;

	fido_dev_get_uv_retry_count(dev, &n);
	consume(&n, sizeof(n));
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
dev_enable_entattest(const struct param *p)
{
	fido_dev_t *dev;
	const char *pin;
	int r;

	set_wire_data(p->config_wire_data.body, p->config_wire_data.len);
	if ((dev = open_dev(0)) == NULL)
		return;
	pin = p->pin1;
	if (strlen(pin) == 0)
		pin = NULL;
	r = fido_dev_enable_entattest(dev, pin);
	consume_str(fido_strerr(r));
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
dev_toggle_always_uv(const struct param *p)
{
	fido_dev_t *dev;
	const char *pin;
	int r;

	set_wire_data(p->config_wire_data.body, p->config_wire_data.len);
	if ((dev = open_dev(0)) == NULL)
		return;
	pin = p->pin1;
	if (strlen(pin) == 0)
		pin = NULL;
	r = fido_dev_toggle_always_uv(dev, pin);
	consume_str(fido_strerr(r));
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
dev_force_pin_change(const struct param *p)
{
	fido_dev_t *dev;
	const char *pin;
	int r;

	set_wire_data(p->config_wire_data.body, p->config_wire_data.len);
	if ((dev = open_dev(0)) == NULL)
		return;
	pin = p->pin1;
	if (strlen(pin) == 0)
		pin = NULL;
	r = fido_dev_force_pin_change(dev, pin);
	consume_str(fido_strerr(r));
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
dev_set_pin_minlen(const struct param *p)
{
	fido_dev_t *dev;
	const char *pin;
	int r;

	set_wire_data(p->config_wire_data.body, p->config_wire_data.len);
	if ((dev = open_dev(0)) == NULL)
		return;
	pin = p->pin1;
	if (strlen(pin) == 0)
		pin = NULL;
	r = fido_dev_set_pin_minlen(dev, strlen(p->pin2), pin);
	consume_str(fido_strerr(r));
	fido_dev_close(dev);
	fido_dev_free(&dev);
}

static void
dev_set_pin_minlen_rpid(const struct param *p)
{
	fido_dev_t *dev;
	const char *rpid[MAXRPID];
	const char *pin;
	size_t n;
	int r;

	set_wire_data(p->config_wire_data.body, p->config_wire_data.len);
	if ((dev = open_dev(0)) == NULL)
		return;
	n = uniform_random(MAXRPID);
	for (size_t i = 0; i < n; i++)
		rpid[i] = dummy_rp_id;
	pin = p->pin1;
	if (strlen(pin) == 0)
		pin = NULL;
	r = fido_dev_set_pin_minlen_rpid(dev, rpid, n, pin);
	consume_str(fido_strerr(r));
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

	dev_reset(p);
	dev_get_cbor_info(p);
	dev_set_pin(p);
	dev_change_pin(p);
	dev_get_retry_count(p);
	dev_get_uv_retry_count(p);
	dev_enable_entattest(p);
	dev_toggle_always_uv(p);
	dev_force_pin_change(p);
	dev_set_pin_minlen(p);
	dev_set_pin_minlen_rpid(p);
}

void
mutate(struct param *p, unsigned int seed, unsigned int flags) NO_MSAN
{
	if (flags & MUTATE_SEED)
		p->seed = (int)seed;

	if (flags & MUTATE_PARAM) {
		mutate_string(p->pin1);
		mutate_string(p->pin2);
	}

	if (flags & MUTATE_WIREDATA) {
		mutate_blob(&p->reset_wire_data);
		mutate_blob(&p->info_wire_data);
		mutate_blob(&p->set_pin_wire_data);
		mutate_blob(&p->change_pin_wire_data);
		mutate_blob(&p->retry_wire_data);
	}
}
