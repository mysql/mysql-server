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
#include "dummy.h"

#include "../openbsd-compat/openbsd-compat.h"

/* Parameter set defining a FIDO2 credential management operation. */
struct param {
	char pin[MAXSTR];
	char name[MAXSTR];
	int seed;
	struct blob id;
	struct blob info_wire_data;
	struct blob enroll_wire_data;
	struct blob list_wire_data;
	struct blob set_name_wire_data;
	struct blob remove_wire_data;
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'getFingerprintSensorInfo' bio enrollment command.
 */
static const uint8_t dummy_info_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_BIO_INFO,
};

/*
 * Collection of HID reports from an authenticator issued with FIDO2
 * 'enrollBegin' + 'enrollCaptureNextSample' bio enrollment commands.
 */
static const uint8_t dummy_enroll_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_AUTHKEY,
	WIREDATA_CTAP_CBOR_PINTOKEN,
	WIREDATA_CTAP_CBOR_BIO_ENROLL,
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'enumerateEnrollments' bio enrollment command.
 */
static const uint8_t dummy_list_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_AUTHKEY,
	WIREDATA_CTAP_CBOR_PINTOKEN,
	WIREDATA_CTAP_CBOR_BIO_ENUM,
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'setFriendlyName' bio enrollment command.
 */
static const uint8_t dummy_set_name_wire_data[] = {
	WIREDATA_CTAP_INIT,
	WIREDATA_CTAP_CBOR_INFO,
	WIREDATA_CTAP_CBOR_AUTHKEY,
	WIREDATA_CTAP_CBOR_PINTOKEN,
	WIREDATA_CTAP_CBOR_STATUS,
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'removeEnrollment' bio enrollment command.
 */
static const uint8_t dummy_remove_wire_data[] = {
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
	    cbor_array_size(item) != 9 ||
	    (v = cbor_array_handle(item)) == NULL)
		goto fail;

	if (unpack_int(v[0], &p->seed) < 0 ||
	    unpack_string(v[1], p->pin) < 0 ||
	    unpack_string(v[2], p->name) < 0 ||
	    unpack_blob(v[3], &p->id) < 0 ||
	    unpack_blob(v[4], &p->info_wire_data) < 0 ||
	    unpack_blob(v[5], &p->enroll_wire_data) < 0 ||
	    unpack_blob(v[6], &p->list_wire_data) < 0 ||
	    unpack_blob(v[7], &p->set_name_wire_data) < 0 ||
	    unpack_blob(v[8], &p->remove_wire_data) < 0)
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
	    (argv[1] = pack_string(p->pin)) == NULL ||
	    (argv[2] = pack_string(p->name)) == NULL ||
	    (argv[3] = pack_blob(&p->id)) == NULL ||
	    (argv[4] = pack_blob(&p->info_wire_data)) == NULL ||
	    (argv[5] = pack_blob(&p->enroll_wire_data)) == NULL ||
	    (argv[6] = pack_blob(&p->list_wire_data)) == NULL ||
	    (argv[7] = pack_blob(&p->set_name_wire_data)) == NULL ||
	    (argv[8] = pack_blob(&p->remove_wire_data)) == NULL)
		goto fail;

	for (size_t i = 0; i < 9; i++)
		if (cbor_array_push(array, argv[i]) == false)
			goto fail;

	if ((cbor_len = cbor_serialize_alloc(array, &cbor,
	    &cbor_alloc_len)) > len) {
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
	uint8_t	blob[4096];
	size_t blob_len;

	memset(&dummy, 0, sizeof(dummy));

	strlcpy(dummy.pin, dummy_pin, sizeof(dummy.pin));
	strlcpy(dummy.name, dummy_name, sizeof(dummy.name));

	dummy.info_wire_data.len = sizeof(dummy_info_wire_data);
	dummy.enroll_wire_data.len = sizeof(dummy_enroll_wire_data);
	dummy.list_wire_data.len = sizeof(dummy_list_wire_data);
	dummy.set_name_wire_data.len = sizeof(dummy_set_name_wire_data);
	dummy.remove_wire_data.len = sizeof(dummy_remove_wire_data);
	dummy.id.len = sizeof(dummy_id);

	memcpy(&dummy.info_wire_data.body, &dummy_info_wire_data,
	    dummy.info_wire_data.len);
	memcpy(&dummy.enroll_wire_data.body, &dummy_enroll_wire_data,
	    dummy.enroll_wire_data.len);
	memcpy(&dummy.list_wire_data.body, &dummy_list_wire_data,
	    dummy.list_wire_data.len);
	memcpy(&dummy.set_name_wire_data.body, &dummy_set_name_wire_data,
	    dummy.set_name_wire_data.len);
	memcpy(&dummy.remove_wire_data.body, &dummy_remove_wire_data,
	    dummy.remove_wire_data.len);
	memcpy(&dummy.id.body, &dummy_id, dummy.id.len);

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
	x = fido_dev_supports_pin(dev);
	consume(&x, sizeof(x));
	x = fido_dev_has_pin(dev);
	consume(&x, sizeof(x));
	x = fido_dev_supports_uv(dev);
	consume(&x, sizeof(x));
	x = fido_dev_has_uv(dev);
	consume(&x, sizeof(x));

	return dev;
}

static void
get_info(const struct param *p)
{
	fido_dev_t *dev = NULL;
	fido_bio_info_t *i = NULL;
	uint8_t type;
	uint8_t max_samples;
	int r;

	set_wire_data(p->info_wire_data.body, p->info_wire_data.len);

	if ((dev = prepare_dev()) == NULL || (i = fido_bio_info_new()) == NULL)
		goto done;

	r = fido_bio_dev_get_info(dev, i);
	consume_str(fido_strerr(r));

	type = fido_bio_info_type(i);
	max_samples = fido_bio_info_max_samples(i);
	consume(&type, sizeof(type));
	consume(&max_samples, sizeof(max_samples));

done:
	if (dev)
		fido_dev_close(dev);

	fido_dev_free(&dev);
	fido_bio_info_free(&i);
}

static void
consume_template(const fido_bio_template_t *t)
{
	consume_str(fido_bio_template_name(t));
	consume(fido_bio_template_id_ptr(t), fido_bio_template_id_len(t));
}

static void
consume_enroll(fido_bio_enroll_t *e)
{
	uint8_t last_status;
	uint8_t remaining_samples;

	last_status = fido_bio_enroll_last_status(e);
	remaining_samples = fido_bio_enroll_remaining_samples(e);
	consume(&last_status, sizeof(last_status));
	consume(&remaining_samples, sizeof(remaining_samples));
}

static void
enroll(const struct param *p)
{
	fido_dev_t *dev = NULL;
	fido_bio_template_t *t = NULL;
	fido_bio_enroll_t *e = NULL;
	size_t cnt = 0;

	set_wire_data(p->enroll_wire_data.body, p->enroll_wire_data.len);

	if ((dev = prepare_dev()) == NULL ||
	    (t = fido_bio_template_new()) == NULL ||
	    (e = fido_bio_enroll_new()) == NULL)
		goto done;

	fido_bio_dev_enroll_begin(dev, t, e, (uint32_t)p->seed, p->pin);

	consume_template(t);
	consume_enroll(e);

	while (fido_bio_enroll_remaining_samples(e) > 0 && cnt++ < 5) {
		fido_bio_dev_enroll_continue(dev, t, e, p->seed);
		consume_template(t);
		consume_enroll(e);
	}

done:
	if (dev)
		fido_dev_close(dev);

	fido_dev_free(&dev);
	fido_bio_template_free(&t);
	fido_bio_enroll_free(&e);
}

static void
list(const struct param *p)
{
	fido_dev_t *dev = NULL;
	fido_bio_template_array_t *ta = NULL;
	const fido_bio_template_t *t = NULL;

	set_wire_data(p->list_wire_data.body, p->list_wire_data.len);

	if ((dev = prepare_dev()) == NULL ||
	    (ta = fido_bio_template_array_new()) == NULL)
		goto done;

	fido_bio_dev_get_template_array(dev, ta, p->pin);

	/* +1 on purpose */
	for (size_t i = 0; i < fido_bio_template_array_count(ta) + 1; i++)
		if ((t = fido_bio_template(ta, i)) != NULL)
			consume_template(t);

done:
	if (dev)
		fido_dev_close(dev);

	fido_dev_free(&dev);
	fido_bio_template_array_free(&ta);
}

static void
set_name(const struct param *p)
{
	fido_dev_t *dev = NULL;
	fido_bio_template_t *t = NULL;

	set_wire_data(p->set_name_wire_data.body, p->set_name_wire_data.len);

	if ((dev = prepare_dev()) == NULL ||
	    (t = fido_bio_template_new()) == NULL)
		goto done;

	fido_bio_template_set_name(t, p->name);
	fido_bio_template_set_id(t, p->id.body, p->id.len);
	consume_template(t);

	fido_bio_dev_set_template_name(dev, t, p->pin);

done:
	if (dev)
		fido_dev_close(dev);

	fido_dev_free(&dev);
	fido_bio_template_free(&t);
}

static void
del(const struct param *p)
{
	fido_dev_t *dev = NULL;
	fido_bio_template_t *t = NULL;
	int r;

	set_wire_data(p->remove_wire_data.body, p->remove_wire_data.len);

	if ((dev = prepare_dev()) == NULL ||
	    (t = fido_bio_template_new()) == NULL)
		goto done;

	r = fido_bio_template_set_id(t, p->id.body, p->id.len);
	consume_template(t);
	consume_str(fido_strerr(r));

	fido_bio_dev_enroll_remove(dev, t, p->pin);

done:
	if (dev)
		fido_dev_close(dev);

	fido_dev_free(&dev);
	fido_bio_template_free(&t);
}

void
test(const struct param *p)
{
	prng_init((unsigned int)p->seed);
	fido_init(FIDO_DEBUG);
	fido_set_log_handler(consume_str);

	get_info(p);
	enroll(p);
	list(p);
	set_name(p);
	del(p);
}

void
mutate(struct param *p, unsigned int seed, unsigned int flags) NO_MSAN
{
	if (flags & MUTATE_SEED)
		p->seed = (int)seed;

	if (flags & MUTATE_PARAM) {
		mutate_blob(&p->id);
		mutate_string(p->pin);
		mutate_string(p->name);
	}

	if (flags & MUTATE_WIREDATA) {
		mutate_blob(&p->info_wire_data);
		mutate_blob(&p->enroll_wire_data);
		mutate_blob(&p->list_wire_data);
		mutate_blob(&p->set_name_wire_data);
		mutate_blob(&p->remove_wire_data);
	}
}
