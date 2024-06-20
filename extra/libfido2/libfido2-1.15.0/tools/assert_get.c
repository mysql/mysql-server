/*
 * Copyright (c) 2018-2023 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <fido.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "../openbsd-compat/openbsd-compat.h"
#include "extern.h"

struct toggle {
	fido_opt_t up;
	fido_opt_t uv;
	fido_opt_t pin;
};

static const char *
opt2str(fido_opt_t v)
{
	switch (v) {
	case FIDO_OPT_OMIT:
		return "omit";
	case FIDO_OPT_TRUE:
		return "true";
	case FIDO_OPT_FALSE:
		return "false";
	default:
		return "unknown";
	}
}

static void
parse_toggle(const char *str, struct toggle *opt)
{
	fido_opt_t *k;
	fido_opt_t  v;
	char *assignment;
	char *key;
	char *val;

	if ((assignment = strdup(str)) == NULL)
		err(1, "strdup");
	if ((val = strchr(assignment, '=')) == NULL)
		errx(1, "invalid assignment '%s'", assignment);

	key = assignment;
	*val++ = '\0';

	if (!strcmp(val, "true"))
		v = FIDO_OPT_TRUE;
	else if (!strcmp(val, "false"))
		v = FIDO_OPT_FALSE;
	else
		errx(1, "unknown value '%s'", val);

	if (!strcmp(key, "up"))
		k = &opt->up;
	else if (!strcmp(key, "uv"))
		k = &opt->uv;
	else if (!strcmp(key, "pin"))
		k = &opt->pin;
	else
		errx(1, "unknown key '%s'", key);

	free(assignment);

	*k = v;
}

static fido_assert_t *
prepare_assert(FILE *in_f, int flags, const struct toggle *opt)
{
	fido_assert_t *assert = NULL;
	struct blob cdh;
	struct blob id;
	struct blob hmac_salt;
	char *rpid = NULL;
	int r;

	memset(&cdh, 0, sizeof(cdh));
	memset(&id, 0, sizeof(id));
	memset(&hmac_salt, 0, sizeof(hmac_salt));

	r = base64_read(in_f, &cdh);
	r |= string_read(in_f, &rpid);
	if ((flags & FLAG_RK) == 0)
		r |= base64_read(in_f, &id);
	if (flags & FLAG_HMAC)
		r |= base64_read(in_f, &hmac_salt);
	if (r < 0)
		errx(1, "input error");

	if (flags & FLAG_DEBUG) {
		fprintf(stderr, "client data%s:\n",
			flags & FLAG_CD ? "" : " hash");
		xxd(cdh.ptr, cdh.len);
		fprintf(stderr, "relying party id: %s\n", rpid);
		if ((flags & FLAG_RK) == 0) {
			fprintf(stderr, "credential id:\n");
			xxd(id.ptr, id.len);
		}
		fprintf(stderr, "up=%s\n", opt2str(opt->up));
		fprintf(stderr, "uv=%s\n", opt2str(opt->uv));
		fprintf(stderr, "pin=%s\n", opt2str(opt->pin));
	}

	if ((assert = fido_assert_new()) == NULL)
		errx(1, "fido_assert_new");

	if (flags & FLAG_CD)
		r = fido_assert_set_clientdata(assert, cdh.ptr, cdh.len);
	else
		r = fido_assert_set_clientdata_hash(assert, cdh.ptr, cdh.len);

	if (r != FIDO_OK || (r = fido_assert_set_rp(assert, rpid)) != FIDO_OK)
		errx(1, "fido_assert_set: %s", fido_strerr(r));
	if ((r = fido_assert_set_up(assert, opt->up)) != FIDO_OK)
		errx(1, "fido_assert_set_up: %s", fido_strerr(r));
	if ((r = fido_assert_set_uv(assert, opt->uv)) != FIDO_OK)
		errx(1, "fido_assert_set_uv: %s", fido_strerr(r));

	if (flags & FLAG_HMAC) {
		if ((r = fido_assert_set_extensions(assert,
		    FIDO_EXT_HMAC_SECRET)) != FIDO_OK)
			errx(1, "fido_assert_set_extensions: %s",
			    fido_strerr(r));
		if ((r = fido_assert_set_hmac_salt(assert, hmac_salt.ptr,
		    hmac_salt.len)) != FIDO_OK)
			errx(1, "fido_assert_set_hmac_salt: %s",
			    fido_strerr(r));
	}
	if (flags & FLAG_LARGEBLOB) {
		if ((r = fido_assert_set_extensions(assert,
		    FIDO_EXT_LARGEBLOB_KEY)) != FIDO_OK)
			errx(1, "fido_assert_set_extensions: %s", fido_strerr(r));
	}
	if ((flags & FLAG_RK) == 0) {
		if ((r = fido_assert_allow_cred(assert, id.ptr,
		    id.len)) != FIDO_OK)
			errx(1, "fido_assert_allow_cred: %s", fido_strerr(r));
	}

	free(hmac_salt.ptr);
	free(cdh.ptr);
	free(id.ptr);
	free(rpid);

	return (assert);
}

static void
print_assert(FILE *out_f, const fido_assert_t *assert, size_t idx, int flags)
{
	char *cdh = NULL;
	char *authdata = NULL;
	char *sig = NULL;
	char *user_id = NULL;
	char *hmac_secret = NULL;
	char *key = NULL;
	int r;

	r = base64_encode(fido_assert_clientdata_hash_ptr(assert),
	    fido_assert_clientdata_hash_len(assert), &cdh);
	r |= base64_encode(fido_assert_authdata_ptr(assert, idx),
	    fido_assert_authdata_len(assert, 0), &authdata);
	r |= base64_encode(fido_assert_sig_ptr(assert, idx),
	    fido_assert_sig_len(assert, idx), &sig);
	if (flags & FLAG_RK)
		r |= base64_encode(fido_assert_user_id_ptr(assert, idx),
		    fido_assert_user_id_len(assert, idx), &user_id);
	if (flags & FLAG_HMAC)
		r |= base64_encode(fido_assert_hmac_secret_ptr(assert, idx),
		    fido_assert_hmac_secret_len(assert, idx), &hmac_secret);
	if (flags & FLAG_LARGEBLOB)
		r |= base64_encode(fido_assert_largeblob_key_ptr(assert, idx),
		    fido_assert_largeblob_key_len(assert, idx), &key);
	if (r < 0)
		errx(1, "output error");

	fprintf(out_f, "%s\n", cdh);
	fprintf(out_f, "%s\n", fido_assert_rp_id(assert));
	fprintf(out_f, "%s\n", authdata);
	fprintf(out_f, "%s\n", sig);
	if (flags & FLAG_RK)
		fprintf(out_f, "%s\n", user_id);
	if (hmac_secret) {
		fprintf(out_f, "%s\n", hmac_secret);
		explicit_bzero(hmac_secret, strlen(hmac_secret));
	}
	if (key) {
		fprintf(out_f, "%s\n", key);
		explicit_bzero(key, strlen(key));
	}

	free(key);
	free(hmac_secret);
	free(cdh);
	free(authdata);
	free(sig);
	free(user_id);
}

int
assert_get(int argc, char **argv)
{
	fido_dev_t *dev = NULL;
	fido_assert_t *assert = NULL;
	struct toggle opt;
	char prompt[1024];
	char pin[128];
	char *in_path = NULL;
	char *out_path = NULL;
	FILE *in_f = NULL;
	FILE *out_f = NULL;
	int flags = 0;
	int ch;
	int r;

	opt.up = opt.uv = opt.pin = FIDO_OPT_OMIT;

	while ((ch = getopt(argc, argv, "bdhi:o:prt:uvw")) != -1) {
		switch (ch) {
		case 'b':
			flags |= FLAG_LARGEBLOB;
			break;
		case 'd':
			flags |= FLAG_DEBUG;
			break;
		case 'h':
			flags |= FLAG_HMAC;
			break;
		case 'i':
			in_path = optarg;
			break;
		case 'o':
			out_path = optarg;
			break;
		case 'p':
			opt.up = FIDO_OPT_TRUE;
			break;
		case 'r':
			flags |= FLAG_RK;
			break;
		case 't' :
			parse_toggle(optarg, &opt);
			break;
		case 'u':
			flags |= FLAG_U2F;
			break;
		case 'v':
			/* -v implies both pin and uv for historical reasons */
			opt.pin = FIDO_OPT_TRUE;
			opt.uv = FIDO_OPT_TRUE;
			break;
		case 'w':
			flags |= FLAG_CD;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	in_f = open_read(in_path);
	out_f = open_write(out_path);

	fido_init((flags & FLAG_DEBUG) ? FIDO_DEBUG : 0);

	assert = prepare_assert(in_f, flags, &opt);

	dev = open_dev(argv[0]);
	if (flags & FLAG_U2F)
		fido_dev_force_u2f(dev);

	if (opt.pin == FIDO_OPT_TRUE) {
		r = snprintf(prompt, sizeof(prompt), "Enter PIN for %s: ",
		    argv[0]);
		if (r < 0 || (size_t)r >= sizeof(prompt))
			errx(1, "snprintf");
		if (!readpassphrase(prompt, pin, sizeof(pin), RPP_ECHO_OFF))
			errx(1, "readpassphrase");
		if (strlen(pin) < 4 || strlen(pin) > 63) {
			explicit_bzero(pin, sizeof(pin));
			errx(1, "invalid PIN length");
		}
		r = fido_dev_get_assert(dev, assert, pin);
	} else
		r = fido_dev_get_assert(dev, assert, NULL);

	explicit_bzero(pin, sizeof(pin));

	if (r != FIDO_OK)
		errx(1, "fido_dev_get_assert: %s", fido_strerr(r));

	if (flags & FLAG_RK) {
		for (size_t idx = 0; idx < fido_assert_count(assert); idx++)
			print_assert(out_f, assert, idx, flags);
	} else {
		if (fido_assert_count(assert) != 1)
			errx(1, "fido_assert_count: %zu",
			    fido_assert_count(assert));
		print_assert(out_f, assert, 0, flags);
	}

	fido_dev_close(dev);
	fido_dev_free(&dev);
	fido_assert_free(&assert);

	fclose(in_f);
	fclose(out_f);
	in_f = NULL;
	out_f = NULL;

	exit(0);
}
