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

static fido_cred_t *
prepare_cred(FILE *in_f, int type, int flags)
{
	fido_cred_t *cred = NULL;
	struct blob cdh;
	struct blob uid;
	char *rpid = NULL;
	char *uname = NULL;
	int r;

	memset(&cdh, 0, sizeof(cdh));
	memset(&uid, 0, sizeof(uid));

	r = base64_read(in_f, &cdh);
	r |= string_read(in_f, &rpid);
	r |= string_read(in_f, &uname);
	r |= base64_read(in_f, &uid);
	if (r < 0)
		errx(1, "input error");

	if (flags & FLAG_DEBUG) {
		fprintf(stderr, "client data%s:\n",
			flags & FLAG_CD ? "" : " hash");
		xxd(cdh.ptr, cdh.len);
		fprintf(stderr, "relying party id: %s\n", rpid);
		fprintf(stderr, "user name: %s\n", uname);
		fprintf(stderr, "user id:\n");
		xxd(uid.ptr, uid.len);
	}

	if ((cred = fido_cred_new()) == NULL)
		errx(1, "fido_cred_new");


	if (flags & FLAG_CD)
		r = fido_cred_set_clientdata(cred, cdh.ptr, cdh.len);
	else
		r = fido_cred_set_clientdata_hash(cred, cdh.ptr, cdh.len);

	if (r != FIDO_OK || (r = fido_cred_set_type(cred, type)) != FIDO_OK ||
	    (r = fido_cred_set_rp(cred, rpid, NULL)) != FIDO_OK ||
	    (r = fido_cred_set_user(cred, uid.ptr, uid.len, uname, NULL,
	    NULL)) != FIDO_OK)
		errx(1, "fido_cred_set: %s", fido_strerr(r));

	if (flags & FLAG_RK) {
		if ((r = fido_cred_set_rk(cred, FIDO_OPT_TRUE)) != FIDO_OK)
			errx(1, "fido_cred_set_rk: %s", fido_strerr(r));
	}
	if (flags & FLAG_UV) {
		if ((r = fido_cred_set_uv(cred, FIDO_OPT_TRUE)) != FIDO_OK)
			errx(1, "fido_cred_set_uv: %s", fido_strerr(r));
	}
	if (flags & FLAG_HMAC) {
		if ((r = fido_cred_set_extensions(cred,
		    FIDO_EXT_HMAC_SECRET)) != FIDO_OK)
			errx(1, "fido_cred_set_extensions: %s", fido_strerr(r));
	}
	if (flags & FLAG_LARGEBLOB) {
		if ((r = fido_cred_set_extensions(cred,
		    FIDO_EXT_LARGEBLOB_KEY)) != FIDO_OK)
			errx(1, "fido_cred_set_extensions: %s", fido_strerr(r));
	}

	free(cdh.ptr);
	free(uid.ptr);
	free(rpid);
	free(uname);

	return (cred);
}

static void
print_attcred(FILE *out_f, const fido_cred_t *cred)
{
	char *cdh = NULL;
	char *authdata = NULL;
	char *id = NULL;
	char *sig = NULL;
	char *x5c = NULL;
	char *key = NULL;
	int r;

	r = base64_encode(fido_cred_clientdata_hash_ptr(cred),
	    fido_cred_clientdata_hash_len(cred), &cdh);
	r |= base64_encode(fido_cred_authdata_ptr(cred),
	    fido_cred_authdata_len(cred), &authdata);
	r |= base64_encode(fido_cred_id_ptr(cred), fido_cred_id_len(cred),
	    &id);
	r |= base64_encode(fido_cred_sig_ptr(cred), fido_cred_sig_len(cred),
	    &sig);
	if (fido_cred_x5c_ptr(cred) != NULL)
		r |= base64_encode(fido_cred_x5c_ptr(cred),
		    fido_cred_x5c_len(cred), &x5c);
	if (fido_cred_largeblob_key_ptr(cred) != NULL)
		r |= base64_encode(fido_cred_largeblob_key_ptr(cred),
		    fido_cred_largeblob_key_len(cred), &key);
	if (r < 0)
		errx(1, "output error");

	fprintf(out_f, "%s\n", cdh);
	fprintf(out_f, "%s\n", fido_cred_rp_id(cred));
	fprintf(out_f, "%s\n", fido_cred_fmt(cred));
	fprintf(out_f, "%s\n", authdata);
	fprintf(out_f, "%s\n", id);
	fprintf(out_f, "%s\n", sig);
	if (x5c != NULL)
		fprintf(out_f, "%s\n", x5c);
	if (key != NULL) {
		fprintf(out_f, "%s\n", key);
		explicit_bzero(key, strlen(key));
	}

	free(cdh);
	free(authdata);
	free(id);
	free(sig);
	free(x5c);
	free(key);
}

int
cred_make(int argc, char **argv)
{
	fido_dev_t *dev = NULL;
	fido_cred_t *cred = NULL;
	char prompt[1024];
	char pin[128];
	char *in_path = NULL;
	char *out_path = NULL;
	FILE *in_f = NULL;
	FILE *out_f = NULL;
	int type = COSE_ES256;
	int flags = 0;
	int cred_protect = -1;
	int ch;
	int r;

	while ((ch = getopt(argc, argv, "bc:dhi:o:qruvw")) != -1) {
		switch (ch) {
		case 'b':
			flags |= FLAG_LARGEBLOB;
			break;
		case 'c':
			if ((cred_protect = base10(optarg)) < 0)
				errx(1, "-c: invalid argument '%s'", optarg);
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
		case 'q':
			flags |= FLAG_QUIET;
			break;
		case 'r':
			flags |= FLAG_RK;
			break;
		case 'u':
			flags |= FLAG_U2F;
			break;
		case 'v':
			flags |= FLAG_UV;
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

	if (argc < 1 || argc > 2)
		usage();

	in_f = open_read(in_path);
	out_f = open_write(out_path);

	if (argc > 1 && cose_type(argv[1], &type) < 0)
		errx(1, "unknown type %s", argv[1]);

	fido_init((flags & FLAG_DEBUG) ? FIDO_DEBUG : 0);

	cred = prepare_cred(in_f, type, flags);

	dev = open_dev(argv[0]);
	if (flags & FLAG_U2F)
		fido_dev_force_u2f(dev);

	if (cred_protect > 0) {
		r = fido_cred_set_prot(cred, cred_protect);
		if (r != FIDO_OK) {
			errx(1, "fido_cred_set_prot: %s", fido_strerr(r));
		}
	}

	r = fido_dev_make_cred(dev, cred, NULL);
	if (r == FIDO_ERR_PIN_REQUIRED && !(flags & FLAG_QUIET)) {
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
		r = fido_dev_make_cred(dev, cred, pin);
	}

	explicit_bzero(pin, sizeof(pin));
	if (r != FIDO_OK)
		errx(1, "fido_dev_make_cred: %s", fido_strerr(r));
	print_attcred(out_f, cred);

	fido_dev_close(dev);
	fido_dev_free(&dev);
	fido_cred_free(&cred);

	fclose(in_f);
	fclose(out_f);
	in_f = NULL;
	out_f = NULL;

	exit(0);
}
