/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <fido.h>
#include <fido/credman.h>

#include <stdbool.h>
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
	char pin[1024];
	int r;

	if ((metadata = fido_credman_metadata_new()) == NULL)
		errx(1, "fido_credman_metadata_new");

	read_pin(path, pin, sizeof(pin));
	r = fido_credman_get_dev_metadata(dev, metadata, pin);
	explicit_bzero(pin, sizeof(pin));

	if (r != FIDO_OK)
		errx(1, "fido_credman_get_dev_metadata: %s", fido_strerr(r));

	printf("existing rk(s): %u\n",
	    (unsigned)fido_credman_rk_existing(metadata));
	printf("remaining rk(s): %u\n",
	    (unsigned)fido_credman_rk_remaining(metadata));

	fido_credman_metadata_free(&metadata);
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(0);
}

static void
print_rp(fido_credman_rp_t *rp, size_t idx)
{
	char *rp_id_hash = NULL;

	if (base64_encode(fido_credman_rp_id_hash_ptr(rp, idx),
	    fido_credman_rp_id_hash_len(rp, idx), &rp_id_hash) < 0)
		errx(1, "output error");

	printf("%02u: %s %s\n", (unsigned)idx, rp_id_hash,
	    fido_credman_rp_id(rp, idx));

	free(rp_id_hash);
	rp_id_hash = NULL;
}

int
credman_list_rp(char *path)
{
	fido_dev_t *dev = NULL;
	fido_credman_rp_t *rp = NULL;
	char pin[1024];
	int r;

	if (path == NULL)
		usage();
	if ((rp = fido_credman_rp_new()) == NULL)
		errx(1, "fido_credman_rp_new");

	dev = open_dev(path);
	read_pin(path, pin, sizeof(pin));
	r = fido_credman_get_dev_rp(dev, rp, pin);
	explicit_bzero(pin, sizeof(pin));

	if (r != FIDO_OK)
		errx(1, "fido_credman_get_dev_rp: %s", fido_strerr(r));

	for (size_t i = 0; i < fido_credman_rp_count(rp); i++)
		print_rp(rp, i);

	fido_credman_rp_free(&rp);
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(0);
}

static void
print_rk(const fido_credman_rk_t *rk, size_t idx)
{
	const fido_cred_t *cred;
	char *id = NULL;
	char *user_id = NULL;
	const char *type;
	const char *prot;

	if ((cred = fido_credman_rk(rk, idx)) == NULL)
		errx(1, "fido_credman_rk");
	if (base64_encode(fido_cred_id_ptr(cred), fido_cred_id_len(cred),
	    &id) < 0 || base64_encode(fido_cred_user_id_ptr(cred),
	    fido_cred_user_id_len(cred), &user_id) < 0)
		errx(1, "output error");

	type = cose_string(fido_cred_type(cred));
	prot = prot_string(fido_cred_prot(cred));

	printf("%02u: %s %s %s %s %s\n", (unsigned)idx, id,
	    fido_cred_display_name(cred), user_id, type, prot);

	free(user_id);
	free(id);
	user_id = NULL;
	id = NULL;
}

int
credman_list_rk(char *path, const char *rp_id)
{
	fido_dev_t *dev = NULL;
	fido_credman_rk_t *rk = NULL;
	char pin[1024];
	int r;

	if (path == NULL)
		usage();
	if ((rk = fido_credman_rk_new()) == NULL)
		errx(1, "fido_credman_rk_new");

	dev = open_dev(path);
	read_pin(path, pin, sizeof(pin));
	r = fido_credman_get_dev_rk(dev, rp_id, rk, pin);
	explicit_bzero(pin, sizeof(pin));

	if (r != FIDO_OK)
		errx(1, "fido_credman_get_dev_rk: %s", fido_strerr(r));
	for (size_t i = 0; i < fido_credman_rk_count(rk); i++)
		print_rk(rk, i);

	fido_credman_rk_free(&rk);
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(0);
}

int
credman_print_rk(fido_dev_t *dev, const char *path, char *rp_id, char *cred_id)
{
	const fido_cred_t *cred = NULL;
	fido_credman_rk_t *rk = NULL;
	char pin[1024];
	void *cred_id_ptr = NULL;
	size_t cred_id_len = 0;
	int r;

	if ((rk = fido_credman_rk_new()) == NULL)
		errx(1, "fido_credman_rk_new");
	if (base64_decode(cred_id, &cred_id_ptr, &cred_id_len) < 0)
		errx(1, "base64_decode");

	read_pin(path, pin, sizeof(pin));
	r = fido_credman_get_dev_rk(dev, rp_id, rk, pin);
	explicit_bzero(pin, sizeof(pin));

	if (r != FIDO_OK)
		errx(1, "fido_credman_get_dev_rk: %s", fido_strerr(r));

	for (size_t i = 0; i < fido_credman_rk_count(rk); i++) {
		if ((cred = fido_credman_rk(rk, i)) == NULL ||
		    fido_cred_id_ptr(cred) == NULL)
			errx(1, "output error");
		if (cred_id_len != fido_cred_id_len(cred) ||
		    memcmp(cred_id_ptr, fido_cred_id_ptr(cred), cred_id_len))
			continue;
		print_cred(stdout, fido_cred_type(cred), cred);
		goto out;
	}

	errx(1, "credential not found");

out:
	free(cred_id_ptr);
	cred_id_ptr = NULL;

	fido_credman_rk_free(&rk);
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(0);
}

int
credman_delete_rk(fido_dev_t *dev, const char *path, char *id)
{
	char pin[1024];
	void *id_ptr = NULL;
	size_t id_len = 0;
	int r;

	if (base64_decode(id, &id_ptr, &id_len) < 0)
		errx(1, "base64_decode");

	read_pin(path, pin, sizeof(pin));
	r = fido_credman_del_dev_rk(dev, id_ptr, id_len, pin);
	explicit_bzero(pin, sizeof(pin));

	if (r != FIDO_OK)
		errx(1, "fido_credman_del_dev_rk: %s", fido_strerr(r));

	free(id_ptr);
	id_ptr = NULL;

	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(0);
}
