/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <fido.h>
#include <fido/bio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "../openbsd-compat/openbsd-compat.h"
#include "extern.h"

static int
print_template(const fido_bio_template_array_t *ta, size_t idx)
{
	const fido_bio_template_t *t = NULL;
	char *id = NULL;

	if ((t = fido_bio_template(ta, idx)) == NULL) {
		warnx("fido_bio_template");
		return -1;
	}
	if (base64_encode(fido_bio_template_id_ptr(t),
	    fido_bio_template_id_len(t), &id) < 0) {
		warnx("output error");
		return -1;
	}

	printf("%02u: %s %s\n", (unsigned)idx, id, fido_bio_template_name(t));
	free(id);

	return 0;
}

int
bio_list(const char *path)
{
	fido_bio_template_array_t *ta = NULL;
	fido_dev_t *dev = NULL;
	char *pin = NULL;
	int r, ok = 1;

	if ((ta = fido_bio_template_array_new()) == NULL)
		errx(1, "fido_bio_template_array_new");
	dev = open_dev(path);
	if ((pin = get_pin(path)) == NULL)
		goto out;
	r = fido_bio_dev_get_template_array(dev, ta, pin);
	freezero(pin, PINBUF_LEN);
	pin = NULL;
	if (r != FIDO_OK) {
		warnx("fido_bio_dev_get_template_array: %s", fido_strerr(r));
		goto out;
	}
	for (size_t i = 0; i < fido_bio_template_array_count(ta); i++)
		if (print_template(ta, i) < 0)
			goto out;

	ok = 0;
out:
	fido_bio_template_array_free(&ta);
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

int
bio_set_name(const char *path, const char *id, const char *name)
{
	fido_bio_template_t *t = NULL;
	fido_dev_t *dev = NULL;
	char *pin = NULL;
	void *id_blob_ptr = NULL;
	size_t id_blob_len = 0;
	int r, ok = 1;

	if ((t = fido_bio_template_new()) == NULL)
		errx(1, "fido_bio_template_new");
	if (base64_decode(id, &id_blob_ptr, &id_blob_len) < 0)
		errx(1, "base64_decode");
	if ((r = fido_bio_template_set_name(t, name)) != FIDO_OK)
		errx(1, "fido_bio_template_set_name: %s", fido_strerr(r));
	if ((r = fido_bio_template_set_id(t, id_blob_ptr,
	    id_blob_len)) != FIDO_OK)
		errx(1, "fido_bio_template_set_id: %s", fido_strerr(r));

	dev = open_dev(path);
	if ((pin = get_pin(path)) == NULL)
		goto out;
	r = fido_bio_dev_set_template_name(dev, t, pin);
	freezero(pin, PINBUF_LEN);
	pin = NULL;
	if (r != FIDO_OK) {
		warnx("fido_bio_dev_set_template_name: %s", fido_strerr(r));
		goto out;
	}

	ok = 0;
out:
	free(id_blob_ptr);
	fido_bio_template_free(&t);
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

static const char *
enroll_strerr(uint8_t n)
{
	switch (n) {
	case FIDO_BIO_ENROLL_FP_GOOD:
		return "Sample ok";
	case FIDO_BIO_ENROLL_FP_TOO_HIGH:
		return "Sample too high";
	case FIDO_BIO_ENROLL_FP_TOO_LOW:
		return "Sample too low";
	case FIDO_BIO_ENROLL_FP_TOO_LEFT:
		return "Sample too left";
	case FIDO_BIO_ENROLL_FP_TOO_RIGHT:
		return "Sample too right";
	case FIDO_BIO_ENROLL_FP_TOO_FAST:
		return "Sample too fast";
	case FIDO_BIO_ENROLL_FP_TOO_SLOW:
		return "Sample too slow";
	case FIDO_BIO_ENROLL_FP_POOR_QUALITY:
		return "Poor quality sample";
	case FIDO_BIO_ENROLL_FP_TOO_SKEWED:
		return "Sample too skewed";
	case FIDO_BIO_ENROLL_FP_TOO_SHORT:
		return "Sample too short";
	case FIDO_BIO_ENROLL_FP_MERGE_FAILURE:
		return "Sample merge failure";
	case FIDO_BIO_ENROLL_FP_EXISTS:
		return "Sample exists";
	case FIDO_BIO_ENROLL_FP_DATABASE_FULL:
		return "Fingerprint database full";
	case FIDO_BIO_ENROLL_NO_USER_ACTIVITY:
		return "No user activity";
	case FIDO_BIO_ENROLL_NO_USER_PRESENCE_TRANSITION:
		return "No user presence transition";
	default:
		return "Unknown error";
	}
}

int
bio_enroll(const char *path)
{
	fido_bio_template_t *t = NULL;
	fido_bio_enroll_t *e = NULL;
	fido_dev_t *dev = NULL;
	char *pin = NULL;
	int r, ok = 1;

	if ((t = fido_bio_template_new()) == NULL)
		errx(1, "fido_bio_template_new");
	if ((e = fido_bio_enroll_new()) == NULL)
		errx(1, "fido_bio_enroll_new");

	dev = open_dev(path);
	if ((pin = get_pin(path)) == NULL)
		goto out;
	printf("Touch your security key.\n");
	r = fido_bio_dev_enroll_begin(dev, t, e, 10000, pin);
	freezero(pin, PINBUF_LEN);
	pin = NULL;
	if (r != FIDO_OK) {
		warnx("fido_bio_dev_enroll_begin: %s", fido_strerr(r));
		goto out;
	}
	printf("%s.\n", enroll_strerr(fido_bio_enroll_last_status(e)));

	while (fido_bio_enroll_remaining_samples(e) > 0) {
		printf("Touch your security key (%u sample%s left).\n",
		    (unsigned)fido_bio_enroll_remaining_samples(e),
		    plural(fido_bio_enroll_remaining_samples(e)));
		if ((r = fido_bio_dev_enroll_continue(dev, t, e,
		    10000)) != FIDO_OK) {
			fido_dev_cancel(dev);
			warnx("fido_bio_dev_enroll_continue: %s",
			    fido_strerr(r));
			goto out;
		}
		printf("%s.\n", enroll_strerr(fido_bio_enroll_last_status(e)));
	}

	ok = 0;
out:
	fido_bio_template_free(&t);
	fido_bio_enroll_free(&e);
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

int
bio_delete(const char *path, const char *id)
{
	fido_bio_template_t *t = NULL;
	fido_dev_t *dev = NULL;
	char *pin = NULL;
	void *id_blob_ptr = NULL;
	size_t id_blob_len = 0;
	int r, ok = 1;

	if ((t = fido_bio_template_new()) == NULL)
		errx(1, "fido_bio_template_new");
	if (base64_decode(id, &id_blob_ptr, &id_blob_len) < 0)
		errx(1, "base64_decode");
	if ((r = fido_bio_template_set_id(t, id_blob_ptr,
	    id_blob_len)) != FIDO_OK)
		errx(1, "fido_bio_template_set_id: %s", fido_strerr(r));

	dev = open_dev(path);
	if ((pin = get_pin(path)) == NULL)
		goto out;
	r = fido_bio_dev_enroll_remove(dev, t, pin);
	freezero(pin, PINBUF_LEN);
	pin = NULL;
	if (r != FIDO_OK) {
		warnx("fido_bio_dev_enroll_remove: %s", fido_strerr(r));
		goto out;
	}

	ok = 0;
out:
	free(id_blob_ptr);
	fido_bio_template_free(&t);
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

static const char *
type_str(uint8_t t)
{
	switch (t) {
	case 1:
		return "touch";
	case 2:
		return "swipe";
	default:
		return "unknown";
	}
}

void
bio_info(fido_dev_t *dev)
{
	fido_bio_info_t	*i = NULL;

	if ((i = fido_bio_info_new()) == NULL) {
		warnx("fido_bio_info_new");
		return;
	}
	if (fido_bio_dev_get_info(dev, i) != FIDO_OK) {
		fido_bio_info_free(&i);
		return;
	}

	printf("sensor type: %u (%s)\n", (unsigned)fido_bio_info_type(i),
	    type_str(fido_bio_info_type(i)));
	printf("max samples: %u\n", (unsigned)fido_bio_info_max_samples(i));

	fido_bio_info_free(&i);
}
