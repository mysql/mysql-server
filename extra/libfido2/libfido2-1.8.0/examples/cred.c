/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <errno.h>
#include <fido.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "../openbsd-compat/openbsd-compat.h"
#include "extern.h"

static const unsigned char cdh[32] = {
	0xf9, 0x64, 0x57, 0xe7, 0x2d, 0x97, 0xf6, 0xbb,
	0xdd, 0xd7, 0xfb, 0x06, 0x37, 0x62, 0xea, 0x26,
	0x20, 0x44, 0x8e, 0x69, 0x7c, 0x03, 0xf2, 0x31,
	0x2f, 0x99, 0xdc, 0xaf, 0x3e, 0x8a, 0x91, 0x6b,
};

static const unsigned char user_id[32] = {
	0x78, 0x1c, 0x78, 0x60, 0xad, 0x88, 0xd2, 0x63,
	0x32, 0x62, 0x2a, 0xf1, 0x74, 0x5d, 0xed, 0xb2,
	0xe7, 0xa4, 0x2b, 0x44, 0x89, 0x29, 0x39, 0xc5,
	0x56, 0x64, 0x01, 0x27, 0x0d, 0xbb, 0xc4, 0x49,
};

static void
usage(void)
{
	fprintf(stderr, "usage: cred [-t ecdsa|rsa|eddsa] [-k pubkey] "
	    "[-ei cred_id] [-P pin] [-T seconds] [-b blobkey] [-hruv] "
	    "<device>\n");
	exit(EXIT_FAILURE);
}

static void
verify_cred(int type, const char *fmt, const unsigned char *authdata_ptr,
    size_t authdata_len, const unsigned char *x509_ptr, size_t x509_len,
    const unsigned char *sig_ptr, size_t sig_len, bool rk, bool uv, int ext,
    const char *key_out, const char *id_out)
{
	fido_cred_t	*cred;
	int		 r;

	if ((cred = fido_cred_new()) == NULL)
		errx(1, "fido_cred_new");

	/* type */
	r = fido_cred_set_type(cred, type);
	if (r != FIDO_OK)
		errx(1, "fido_cred_set_type: %s (0x%x)", fido_strerr(r), r);

	/* client data hash */
	r = fido_cred_set_clientdata_hash(cred, cdh, sizeof(cdh));
	if (r != FIDO_OK)
		errx(1, "fido_cred_set_clientdata_hash: %s (0x%x)",
		    fido_strerr(r), r);

	/* relying party */
	r = fido_cred_set_rp(cred, "localhost", "sweet home localhost");
	if (r != FIDO_OK)
		errx(1, "fido_cred_set_rp: %s (0x%x)", fido_strerr(r), r);

	/* authdata */
	r = fido_cred_set_authdata(cred, authdata_ptr, authdata_len);
	if (r != FIDO_OK)
		errx(1, "fido_cred_set_authdata: %s (0x%x)", fido_strerr(r), r);

	/* extensions */
	r = fido_cred_set_extensions(cred, ext);
	if (r != FIDO_OK)
		errx(1, "fido_cred_set_extensions: %s (0x%x)", fido_strerr(r), r);

	/* resident key */
	if (rk && (r = fido_cred_set_rk(cred, FIDO_OPT_TRUE)) != FIDO_OK)
		errx(1, "fido_cred_set_rk: %s (0x%x)", fido_strerr(r), r);

	/* user verification */
	if (uv && (r = fido_cred_set_uv(cred, FIDO_OPT_TRUE)) != FIDO_OK)
		errx(1, "fido_cred_set_uv: %s (0x%x)", fido_strerr(r), r);

	/* fmt */
	r = fido_cred_set_fmt(cred, fmt);
	if (r != FIDO_OK)
		errx(1, "fido_cred_set_fmt: %s (0x%x)", fido_strerr(r), r);

	if (!strcmp(fido_cred_fmt(cred), "none")) {
		warnx("no attestation data, skipping credential verification");
		goto out;
	}

	/* x509 */
	r = fido_cred_set_x509(cred, x509_ptr, x509_len);
	if (r != FIDO_OK)
		errx(1, "fido_cred_set_x509: %s (0x%x)", fido_strerr(r), r);

	/* sig */
	r = fido_cred_set_sig(cred, sig_ptr, sig_len);
	if (r != FIDO_OK)
		errx(1, "fido_cred_set_sig: %s (0x%x)", fido_strerr(r), r);

	r = fido_cred_verify(cred);
	if (r != FIDO_OK)
		errx(1, "fido_cred_verify: %s (0x%x)", fido_strerr(r), r);

out:
	if (key_out != NULL) {
		/* extract the credential pubkey */
		if (type == COSE_ES256) {
			if (write_ec_pubkey(key_out, fido_cred_pubkey_ptr(cred),
			    fido_cred_pubkey_len(cred)) < 0)
				errx(1, "write_ec_pubkey");
		} else if (type == COSE_RS256) {
			if (write_rsa_pubkey(key_out, fido_cred_pubkey_ptr(cred),
			    fido_cred_pubkey_len(cred)) < 0)
				errx(1, "write_rsa_pubkey");
		} else if (type == COSE_EDDSA) {
			if (write_eddsa_pubkey(key_out, fido_cred_pubkey_ptr(cred),
			    fido_cred_pubkey_len(cred)) < 0)
				errx(1, "write_eddsa_pubkey");
		}
	}

	if (id_out != NULL) {
		/* extract the credential id */
		if (write_blob(id_out, fido_cred_id_ptr(cred),
		    fido_cred_id_len(cred)) < 0)
			errx(1, "write_blob");
	}

	fido_cred_free(&cred);
}

static fido_dev_t *
open_from_manifest(const fido_dev_info_t *dev_infos, size_t len,
    const char *path)
{
	size_t i;
	fido_dev_t *dev;

	for (i = 0; i < len; i++) {
		const fido_dev_info_t *curr = fido_dev_info_ptr(dev_infos, i);
		if (path == NULL ||
		    strcmp(path, fido_dev_info_path(curr)) == 0) {
			dev = fido_dev_new_with_info(curr);
			if (fido_dev_open_with_info(dev) == FIDO_OK)
				return (dev);
			fido_dev_free(&dev);
		}
	}

	return (NULL);
}

int
main(int argc, char **argv)
{
	bool		 rk = false;
	bool		 uv = false;
	bool		 u2f = false;
	fido_dev_t	*dev;
	fido_cred_t	*cred = NULL;
	const char	*pin = NULL;
	const char	*blobkey_out = NULL;
	const char	*key_out = NULL;
	const char	*id_out = NULL;
	const char	*path = NULL;
	unsigned char	*body = NULL;
	long long	 seconds = 0;
	size_t		 len;
	int		 type = COSE_ES256;
	int		 ext = 0;
	int		 ch;
	int		 r;
	fido_dev_info_t	*dev_infos = NULL;
	size_t		 dev_infos_len = 0;

	if ((cred = fido_cred_new()) == NULL)
		errx(1, "fido_cred_new");

	while ((ch = getopt(argc, argv, "P:T:b:e:hi:k:rt:uv")) != -1) {
		switch (ch) {
		case 'P':
			pin = optarg;
			break;
		case 'T':
#ifndef SIGNAL_EXAMPLE
			(void)seconds;
			errx(1, "-T not supported");
#else
			if (base10(optarg, &seconds) < 0)
				errx(1, "base10: %s", optarg);
			if (seconds <= 0 || seconds > 30)
				errx(1, "-T: %s must be in (0,30]", optarg);
			break;
#endif
		case 'b':
			ext |= FIDO_EXT_LARGEBLOB_KEY;
			blobkey_out = optarg;
			break;
		case 'e':
			if (read_blob(optarg, &body, &len) < 0)
				errx(1, "read_blob: %s", optarg);
			r = fido_cred_exclude(cred, body, len);
			if (r != FIDO_OK)
				errx(1, "fido_cred_exclude: %s (0x%x)",
				    fido_strerr(r), r);
			free(body);
			body = NULL;
			break;
		case 'h':
			ext |= FIDO_EXT_HMAC_SECRET;
			break;
		case 'i':
			id_out = optarg;
			break;
		case 'k':
			key_out = optarg;
			break;
		case 'r':
			rk = true;
			break;
		case 't':
			if (strcmp(optarg, "ecdsa") == 0)
				type = COSE_ES256;
			else if (strcmp(optarg, "rsa") == 0)
				type = COSE_RS256;
			else if (strcmp(optarg, "eddsa") == 0)
				type = COSE_EDDSA;
			else
				errx(1, "unknown type %s", optarg);
			break;
		case 'u':
			u2f = true;
			break;
		case 'v':
			uv = true;
			break;
		default:
			usage();
		}
	}

	fido_init(0);

	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage();
	dev_infos = fido_dev_info_new(16);
	fido_dev_info_manifest(dev_infos, 16, &dev_infos_len);
	if (argc == 1)
		path = argv[0];

	if ((dev = open_from_manifest(dev_infos, dev_infos_len, path)) == NULL)
		errx(1, "open_from_manifest");

	if (u2f)
		fido_dev_force_u2f(dev);

	/* type */
	r = fido_cred_set_type(cred, type);
	if (r != FIDO_OK)
		errx(1, "fido_cred_set_type: %s (0x%x)", fido_strerr(r), r);

	/* client data hash */
	r = fido_cred_set_clientdata_hash(cred, cdh, sizeof(cdh));
	if (r != FIDO_OK)
		errx(1, "fido_cred_set_clientdata_hash: %s (0x%x)",
		    fido_strerr(r), r);

	/* relying party */
	r = fido_cred_set_rp(cred, "localhost", "sweet home localhost");
	if (r != FIDO_OK)
		errx(1, "fido_cred_set_rp: %s (0x%x)", fido_strerr(r), r);

	/* user */
	r = fido_cred_set_user(cred, user_id, sizeof(user_id), "john smith",
	    "jsmith", NULL);
	if (r != FIDO_OK)
		errx(1, "fido_cred_set_user: %s (0x%x)", fido_strerr(r), r);

	/* extensions */
	r = fido_cred_set_extensions(cred, ext);
	if (r != FIDO_OK)
		errx(1, "fido_cred_set_extensions: %s (0x%x)", fido_strerr(r), r);

	/* resident key */
	if (rk && (r = fido_cred_set_rk(cred, FIDO_OPT_TRUE)) != FIDO_OK)
		errx(1, "fido_cred_set_rk: %s (0x%x)", fido_strerr(r), r);

	/* user verification */
	if (uv && (r = fido_cred_set_uv(cred, FIDO_OPT_TRUE)) != FIDO_OK)
		errx(1, "fido_cred_set_uv: %s (0x%x)", fido_strerr(r), r);

#ifdef SIGNAL_EXAMPLE
	prepare_signal_handler(SIGINT);
	if (seconds) {
		prepare_signal_handler(SIGALRM);
		alarm((unsigned)seconds);
	}
#endif

	r = fido_dev_make_cred(dev, cred, pin);
	if (r != FIDO_OK) {
#ifdef SIGNAL_EXAMPLE
		if (got_signal)
			fido_dev_cancel(dev);
#endif
		errx(1, "fido_makecred: %s (0x%x)", fido_strerr(r), r);
	}

	r = fido_dev_close(dev);
	if (r != FIDO_OK)
		errx(1, "fido_dev_close: %s (0x%x)", fido_strerr(r), r);

	fido_dev_free(&dev);

	/* when verifying, pin implies uv */
	if (pin)
		uv = true;

	verify_cred(type, fido_cred_fmt(cred), fido_cred_authdata_ptr(cred),
	    fido_cred_authdata_len(cred), fido_cred_x5c_ptr(cred),
	    fido_cred_x5c_len(cred), fido_cred_sig_ptr(cred),
	    fido_cred_sig_len(cred), rk, uv, ext, key_out, id_out);

	if (blobkey_out != NULL) {
		/* extract the "largeBlob" key */
		if (write_blob(blobkey_out, fido_cred_largeblob_key_ptr(cred),
		    fido_cred_largeblob_key_len(cred)) < 0)
			errx(1, "write_blob");
	}

	fido_cred_free(&cred);

	exit(0);
}
