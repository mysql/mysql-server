/*
 * Copyright (c) 2018-2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <fido.h>
#include <fido/es256.h>
#include <fido/es384.h>
#include <fido/rs256.h>
#include <fido/eddsa.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "../openbsd-compat/openbsd-compat.h"
#include "extern.h"

static const unsigned char cd[32] = {
	0xec, 0x8d, 0x8f, 0x78, 0x42, 0x4a, 0x2b, 0xb7,
	0x82, 0x34, 0xaa, 0xca, 0x07, 0xa1, 0xf6, 0x56,
	0x42, 0x1c, 0xb6, 0xf6, 0xb3, 0x00, 0x86, 0x52,
	0x35, 0x2d, 0xa2, 0x62, 0x4a, 0xbe, 0x89, 0x76,
};

static void
usage(void)
{
	fprintf(stderr, "usage: assert [-t es256|es384|rs256|eddsa] "
	    "[-a cred_id] [-h hmac_secret] [-s hmac_salt] [-P pin] "
	    "[-T seconds] [-b blobkey] [-puv] <pubkey> <device>\n");
	exit(EXIT_FAILURE);
}

static void
verify_assert(int type, const unsigned char *authdata_ptr, size_t authdata_len,
    const unsigned char *sig_ptr, size_t sig_len, bool up, bool uv, int ext,
    const char *key)
{
	fido_assert_t	*assert = NULL;
	EC_KEY		*ec = NULL;
	RSA		*rsa = NULL;
	EVP_PKEY	*eddsa = NULL;
	es256_pk_t	*es256_pk = NULL;
	es384_pk_t	*es384_pk = NULL;
	rs256_pk_t	*rs256_pk = NULL;
	eddsa_pk_t	*eddsa_pk = NULL;
	void		*pk;
	int		 r;

	/* credential pubkey */
	switch (type) {
	case COSE_ES256:
		if ((ec = read_ec_pubkey(key)) == NULL)
			errx(1, "read_ec_pubkey");

		if ((es256_pk = es256_pk_new()) == NULL)
			errx(1, "es256_pk_new");

		if (es256_pk_from_EC_KEY(es256_pk, ec) != FIDO_OK)
			errx(1, "es256_pk_from_EC_KEY");

		pk = es256_pk;
		EC_KEY_free(ec);
		ec = NULL;

		break;
	case COSE_ES384:
		if ((ec = read_ec_pubkey(key)) == NULL)
			errx(1, "read_ec_pubkey");

		if ((es384_pk = es384_pk_new()) == NULL)
			errx(1, "es384_pk_new");

		if (es384_pk_from_EC_KEY(es384_pk, ec) != FIDO_OK)
			errx(1, "es384_pk_from_EC_KEY");

		pk = es384_pk;
		EC_KEY_free(ec);
		ec = NULL;

		break;
	case COSE_RS256:
		if ((rsa = read_rsa_pubkey(key)) == NULL)
			errx(1, "read_rsa_pubkey");

		if ((rs256_pk = rs256_pk_new()) == NULL)
			errx(1, "rs256_pk_new");

		if (rs256_pk_from_RSA(rs256_pk, rsa) != FIDO_OK)
			errx(1, "rs256_pk_from_RSA");

		pk = rs256_pk;
		RSA_free(rsa);
		rsa = NULL;

		break;
	case COSE_EDDSA:
		if ((eddsa = read_eddsa_pubkey(key)) == NULL)
			errx(1, "read_eddsa_pubkey");

		if ((eddsa_pk = eddsa_pk_new()) == NULL)
			errx(1, "eddsa_pk_new");

		if (eddsa_pk_from_EVP_PKEY(eddsa_pk, eddsa) != FIDO_OK)
			errx(1, "eddsa_pk_from_EVP_PKEY");

		pk = eddsa_pk;
		EVP_PKEY_free(eddsa);
		eddsa = NULL;

		break;
	default:
		errx(1, "unknown credential type %d", type);
	}

	if ((assert = fido_assert_new()) == NULL)
		errx(1, "fido_assert_new");

	/* client data hash */
	r = fido_assert_set_clientdata(assert, cd, sizeof(cd));
	if (r != FIDO_OK)
		errx(1, "fido_assert_set_clientdata: %s (0x%x)", fido_strerr(r), r);

	/* relying party */
	r = fido_assert_set_rp(assert, "localhost");
	if (r != FIDO_OK)
		errx(1, "fido_assert_set_rp: %s (0x%x)", fido_strerr(r), r);

	/* authdata */
	r = fido_assert_set_count(assert, 1);
	if (r != FIDO_OK)
		errx(1, "fido_assert_set_count: %s (0x%x)", fido_strerr(r), r);
	r = fido_assert_set_authdata(assert, 0, authdata_ptr, authdata_len);
	if (r != FIDO_OK)
		errx(1, "fido_assert_set_authdata: %s (0x%x)", fido_strerr(r), r);

	/* extension */
	r = fido_assert_set_extensions(assert, ext);
	if (r != FIDO_OK)
		errx(1, "fido_assert_set_extensions: %s (0x%x)", fido_strerr(r),
		    r);

	/* user presence */
	if (up && (r = fido_assert_set_up(assert, FIDO_OPT_TRUE)) != FIDO_OK)
		errx(1, "fido_assert_set_up: %s (0x%x)", fido_strerr(r), r);

	/* user verification */
	if (uv && (r = fido_assert_set_uv(assert, FIDO_OPT_TRUE)) != FIDO_OK)
		errx(1, "fido_assert_set_uv: %s (0x%x)", fido_strerr(r), r);

	/* sig */
	r = fido_assert_set_sig(assert, 0, sig_ptr, sig_len);
	if (r != FIDO_OK)
		errx(1, "fido_assert_set_sig: %s (0x%x)", fido_strerr(r), r);

	r = fido_assert_verify(assert, 0, type, pk);
	if (r != FIDO_OK)
		errx(1, "fido_assert_verify: %s (0x%x)", fido_strerr(r), r);

	es256_pk_free(&es256_pk);
	es384_pk_free(&es384_pk);
	rs256_pk_free(&rs256_pk);
	eddsa_pk_free(&eddsa_pk);

	fido_assert_free(&assert);
}

int
main(int argc, char **argv)
{
	bool		 up = false;
	bool		 uv = false;
	bool		 u2f = false;
	fido_dev_t	*dev = NULL;
	fido_assert_t	*assert = NULL;
	const char	*pin = NULL;
	const char	*blobkey_out = NULL;
	const char	*hmac_out = NULL;
	unsigned char	*body = NULL;
	long long	 ms = 0;
	size_t		 len;
	int		 type = COSE_ES256;
	int		 ext = 0;
	int		 ch;
	int		 r;

	if ((assert = fido_assert_new()) == NULL)
		errx(1, "fido_assert_new");

	while ((ch = getopt(argc, argv, "P:T:a:b:h:ps:t:uv")) != -1) {
		switch (ch) {
		case 'P':
			pin = optarg;
			break;
		case 'T':
			if (base10(optarg, &ms) < 0)
				errx(1, "base10: %s", optarg);
			if (ms <= 0 || ms > 30)
				errx(1, "-T: %s must be in (0,30]", optarg);
			ms *= 1000; /* seconds to milliseconds */
			break;
		case 'a':
			if (read_blob(optarg, &body, &len) < 0)
				errx(1, "read_blob: %s", optarg);
			if ((r = fido_assert_allow_cred(assert, body,
			    len)) != FIDO_OK)
				errx(1, "fido_assert_allow_cred: %s (0x%x)",
				    fido_strerr(r), r);
			free(body);
			body = NULL;
			break;
		case 'b':
			ext |= FIDO_EXT_LARGEBLOB_KEY;
			blobkey_out = optarg;
			break;
		case 'h':
			hmac_out = optarg;
			break;
		case 'p':
			up = true;
			break;
		case 's':
			ext |= FIDO_EXT_HMAC_SECRET;
			if (read_blob(optarg, &body, &len) < 0)
				errx(1, "read_blob: %s", optarg);
			if ((r = fido_assert_set_hmac_salt(assert, body,
			    len)) != FIDO_OK)
				errx(1, "fido_assert_set_hmac_salt: %s (0x%x)",
				    fido_strerr(r), r);
			free(body);
			body = NULL;
			break;
		case 't':
			if (strcmp(optarg, "es256") == 0)
				type = COSE_ES256;
			else if (strcmp(optarg, "es384") == 0)
				type = COSE_ES384;
			else if (strcmp(optarg, "rs256") == 0)
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

	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	fido_init(0);

	if ((dev = fido_dev_new()) == NULL)
		errx(1, "fido_dev_new");

	r = fido_dev_open(dev, argv[1]);
	if (r != FIDO_OK)
		errx(1, "fido_dev_open: %s (0x%x)", fido_strerr(r), r);
	if (u2f)
		fido_dev_force_u2f(dev);

	/* client data hash */
	r = fido_assert_set_clientdata(assert, cd, sizeof(cd));
	if (r != FIDO_OK)
		errx(1, "fido_assert_set_clientdata: %s (0x%x)", fido_strerr(r), r);

	/* relying party */
	r = fido_assert_set_rp(assert, "localhost");
	if (r != FIDO_OK)
		errx(1, "fido_assert_set_rp: %s (0x%x)", fido_strerr(r), r);

	/* extensions */
	r = fido_assert_set_extensions(assert, ext);
	if (r != FIDO_OK)
		errx(1, "fido_assert_set_extensions: %s (0x%x)", fido_strerr(r),
		    r);

	/* user presence */
	if (up && (r = fido_assert_set_up(assert, FIDO_OPT_TRUE)) != FIDO_OK)
		errx(1, "fido_assert_set_up: %s (0x%x)", fido_strerr(r), r);

	/* user verification */
	if (uv && (r = fido_assert_set_uv(assert, FIDO_OPT_TRUE)) != FIDO_OK)
		errx(1, "fido_assert_set_uv: %s (0x%x)", fido_strerr(r), r);

	/* timeout */
	if (ms != 0 && (r = fido_dev_set_timeout(dev, (int)ms)) != FIDO_OK)
		errx(1, "fido_dev_set_timeout: %s (0x%x)", fido_strerr(r), r);

	if ((r = fido_dev_get_assert(dev, assert, pin)) != FIDO_OK) {
		fido_dev_cancel(dev);
		errx(1, "fido_dev_get_assert: %s (0x%x)", fido_strerr(r), r);
	}

	r = fido_dev_close(dev);
	if (r != FIDO_OK)
		errx(1, "fido_dev_close: %s (0x%x)", fido_strerr(r), r);

	fido_dev_free(&dev);

	if (fido_assert_count(assert) != 1)
		errx(1, "fido_assert_count: %d signatures returned",
		    (int)fido_assert_count(assert));

	/* when verifying, pin implies uv */
	if (pin)
		uv = true;

	verify_assert(type, fido_assert_authdata_ptr(assert, 0),
	    fido_assert_authdata_len(assert, 0), fido_assert_sig_ptr(assert, 0),
	    fido_assert_sig_len(assert, 0), up, uv, ext, argv[0]);

	if (hmac_out != NULL) {
		/* extract the hmac secret */
		if (write_blob(hmac_out, fido_assert_hmac_secret_ptr(assert, 0),
		    fido_assert_hmac_secret_len(assert, 0)) < 0)
			errx(1, "write_blob");
	}

	if (blobkey_out != NULL) {
		/* extract the hmac secret */
		if (write_blob(blobkey_out,
		    fido_assert_largeblob_key_ptr(assert, 0),
		    fido_assert_largeblob_key_len(assert, 0)) < 0)
			errx(1, "write_blob");
	}

	fido_assert_free(&assert);

	exit(0);
}
