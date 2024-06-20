/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <fido.h>
#include <fido/credman.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "../openbsd-compat/openbsd-compat.h"
#include "extern.h"

int
credman_get_metadata(fido_dev_t *dev, const char *path)
{
	fido_credman_metadata_t *metadata = NULL;
	char *pin = NULL;
	int r, ok = 1;

	if ((metadata = fido_credman_metadata_new()) == NULL) {
		warnx("fido_credman_metadata_new");
		goto out;
	}
	if ((r = fido_credman_get_dev_metadata(dev, metadata,
	    NULL)) != FIDO_OK && should_retry_with_pin(dev, r)) {
		if ((pin = get_pin(path)) == NULL)
			goto out;
		r = fido_credman_get_dev_metadata(dev, metadata, pin);
		freezero(pin, PINBUF_LEN);
		pin = NULL;
	}
	if (r != FIDO_OK) {
		warnx("fido_credman_get_dev_metadata: %s", fido_strerr(r));
		goto out;
	}

	printf("existing rk(s): %u\n",
	    (unsigned)fido_credman_rk_existing(metadata));
	printf("remaining rk(s): %u\n",
	    (unsigned)fido_credman_rk_remaining(metadata));

	ok = 0;
out:
	fido_credman_metadata_free(&metadata);
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

static int
print_rp(fido_credman_rp_t *rp, size_t idx)
{
	char *rp_id_hash = NULL;

	if (base64_encode(fido_credman_rp_id_hash_ptr(rp, idx),
	    fido_credman_rp_id_hash_len(rp, idx), &rp_id_hash) < 0) {
		warnx("output error");
		return -1;
	}
	printf("%02u: %s %s\n", (unsigned)idx, rp_id_hash,
	    fido_credman_rp_id(rp, idx));
	free(rp_id_hash);

	return 0;
}

int
credman_list_rp(const char *path)
{
	fido_credman_rp_t *rp = NULL;
	fido_dev_t *dev = NULL;
	char *pin = NULL;
	int r, ok = 1;

	dev = open_dev(path);
	if ((rp = fido_credman_rp_new()) == NULL) {
		warnx("fido_credman_rp_new");
		goto out;
	}
	if ((r = fido_credman_get_dev_rp(dev, rp, NULL)) != FIDO_OK &&
	    should_retry_with_pin(dev, r)) {
		if ((pin = get_pin(path)) == NULL)
			goto out;
		r = fido_credman_get_dev_rp(dev, rp, pin);
		freezero(pin, PINBUF_LEN);
		pin = NULL;
	}
	if (r != FIDO_OK) {
		warnx("fido_credman_get_dev_rp: %s", fido_strerr(r));
		goto out;
	}
	for (size_t i = 0; i < fido_credman_rp_count(rp); i++)
		if (print_rp(rp, i) < 0)
			goto out;

	ok = 0;
out:
	fido_credman_rp_free(&rp);
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

static int
print_rk(const fido_credman_rk_t *rk, size_t idx)
{
	const fido_cred_t *cred;
	char *id = NULL;
	char *user_id = NULL;
	const char *type;
	const char *prot;
	int r = -1;

	if ((cred = fido_credman_rk(rk, idx)) == NULL) {
		warnx("fido_credman_rk");
		return -1;
	}
	if (base64_encode(fido_cred_id_ptr(cred), fido_cred_id_len(cred),
	    &id) < 0 || base64_encode(fido_cred_user_id_ptr(cred),
	    fido_cred_user_id_len(cred), &user_id) < 0) {
		warnx("output error");
		goto out;
	}

	type = cose_string(fido_cred_type(cred));
	prot = prot_string(fido_cred_prot(cred));

	printf("%02u: %s %s %s %s %s\n", (unsigned)idx, id,
	    fido_cred_display_name(cred), user_id, type, prot);

	r = 0;
out:
	free(user_id);
	free(id);

	return r;
}

int
credman_list_rk(const char *path, const char *rp_id)
{
	fido_dev_t *dev = NULL;
	fido_credman_rk_t *rk = NULL;
	char *pin = NULL;
	int r, ok = 1;

	dev = open_dev(path);
	if ((rk = fido_credman_rk_new()) == NULL) {
		warnx("fido_credman_rk_new");
		goto out;
	}
	if ((r = fido_credman_get_dev_rk(dev, rp_id, rk, NULL)) != FIDO_OK &&
	    should_retry_with_pin(dev, r)) {
		if ((pin = get_pin(path)) == NULL)
			goto out;
		r = fido_credman_get_dev_rk(dev, rp_id, rk, pin);
		freezero(pin, PINBUF_LEN);
		pin = NULL;
	}
	if (r != FIDO_OK) {
		warnx("fido_credman_get_dev_rk: %s", fido_strerr(r));
		goto out;
	}
	for (size_t i = 0; i < fido_credman_rk_count(rk); i++)
		if (print_rk(rk, i) < 0)
			goto out;

	ok = 0;
out:
	fido_credman_rk_free(&rk);
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

int
credman_print_rk(fido_dev_t *dev, const char *path, const char *rp_id,
    const char *cred_id)
{
	fido_credman_rk_t *rk = NULL;
	const fido_cred_t *cred = NULL;
	char *pin = NULL;
	void *cred_id_ptr = NULL;
	size_t cred_id_len = 0;
	int r, ok = 1;

	if ((rk = fido_credman_rk_new()) == NULL) {
		warnx("fido_credman_rk_new");
		goto out;
	}
	if (base64_decode(cred_id, &cred_id_ptr, &cred_id_len) < 0) {
		warnx("base64_decode");
		goto out;
	}
	if ((r = fido_credman_get_dev_rk(dev, rp_id, rk, NULL)) != FIDO_OK &&
	    should_retry_with_pin(dev, r)) {
		if ((pin = get_pin(path)) == NULL)
			goto out;
		r = fido_credman_get_dev_rk(dev, rp_id, rk, pin);
		freezero(pin, PINBUF_LEN);
		pin = NULL;
	}
	if (r != FIDO_OK) {
		warnx("fido_credman_get_dev_rk: %s", fido_strerr(r));
		goto out;
	}

	for (size_t i = 0; i < fido_credman_rk_count(rk); i++) {
		if ((cred = fido_credman_rk(rk, i)) == NULL ||
		    fido_cred_id_ptr(cred) == NULL) {
			warnx("output error");
			goto out;
		}
		if (cred_id_len != fido_cred_id_len(cred) ||
		    memcmp(cred_id_ptr, fido_cred_id_ptr(cred), cred_id_len))
			continue;
		print_cred(stdout, fido_cred_type(cred), cred);
		ok = 0;
		goto out;
	}

	warnx("credential not found");
out:
	free(cred_id_ptr);
	fido_credman_rk_free(&rk);
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

int
credman_delete_rk(const char *path, const char *id)
{
	fido_dev_t *dev = NULL;
	char *pin = NULL;
	void *id_ptr = NULL;
	size_t id_len = 0;
	int r, ok = 1;

	dev = open_dev(path);
	if (base64_decode(id, &id_ptr, &id_len) < 0) {
		warnx("base64_decode");
		goto out;
	}
	if ((r = fido_credman_del_dev_rk(dev, id_ptr, id_len,
	    NULL)) != FIDO_OK && should_retry_with_pin(dev, r)) {
		if ((pin = get_pin(path)) == NULL)
			goto out;
		r = fido_credman_del_dev_rk(dev, id_ptr, id_len, pin);
		freezero(pin, PINBUF_LEN);
		pin = NULL;
	}
	if (r != FIDO_OK) {
		warnx("fido_credman_del_dev_rk: %s", fido_strerr(r));
		goto out;
	}

	ok = 0;
out:
	free(id_ptr);
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

int
credman_update_rk(const char *path, const char *user_id, const char *cred_id,
    const char *name, const char *display_name)
{
	fido_dev_t *dev = NULL;
	fido_cred_t *cred = NULL;
	char *pin = NULL;
	void *user_id_ptr = NULL;
	void *cred_id_ptr = NULL;
	size_t user_id_len = 0;
	size_t cred_id_len = 0;
	int r, ok = 1;

	dev = open_dev(path);
	if (base64_decode(user_id, &user_id_ptr, &user_id_len) < 0 ||
	    base64_decode(cred_id, &cred_id_ptr, &cred_id_len) < 0) {
		warnx("base64_decode");
		goto out;
	}
	if ((cred = fido_cred_new()) == NULL) {
		warnx("fido_cred_new");
		goto out;
	}
	if ((r = fido_cred_set_id(cred, cred_id_ptr, cred_id_len)) != FIDO_OK) { 
		warnx("fido_cred_set_id: %s",  fido_strerr(r));
		goto out;
	}
	if ((r = fido_cred_set_user(cred, user_id_ptr, user_id_len, name,
	    display_name, NULL)) != FIDO_OK) {
		warnx("fido_cred_set_user: %s", fido_strerr(r));
		goto out;
	}
	if ((r = fido_credman_set_dev_rk(dev, cred, NULL)) != FIDO_OK &&
	    should_retry_with_pin(dev, r)) {
		if ((pin = get_pin(path)) == NULL)
			goto out;
		r = fido_credman_set_dev_rk(dev, cred, pin);
		freezero(pin, PINBUF_LEN);
		pin = NULL;
	}
	if (r != FIDO_OK) {
		warnx("fido_credman_set_dev_rk: %s", fido_strerr(r));
		goto out;
	}

	ok = 0;
out:
	free(user_id_ptr);
	free(cred_id_ptr);
	fido_dev_close(dev);
	fido_dev_free(&dev);
	fido_cred_free(&cred);

	exit(ok);
}
