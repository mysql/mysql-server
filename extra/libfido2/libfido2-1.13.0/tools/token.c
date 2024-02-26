/*
 * Copyright (c) 2018-2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

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

static void
format_flags(char *ret, size_t retlen, uint8_t flags)
{
	memset(ret, 0, retlen);

	if (flags & FIDO_CAP_WINK) {
		if (strlcat(ret, "wink,", retlen) >= retlen)
			goto toolong;
	} else {
		if (strlcat(ret, "nowink,", retlen) >= retlen)
			goto toolong;
	}

	if (flags & FIDO_CAP_CBOR) {
		if (strlcat(ret, " cbor,", retlen) >= retlen)
			goto toolong;
	} else {
		if (strlcat(ret, " nocbor,", retlen) >= retlen)
			goto toolong;
	}

	if (flags & FIDO_CAP_NMSG) {
		if (strlcat(ret, " nomsg", retlen) >= retlen)
			goto toolong;
	} else {
		if (strlcat(ret, " msg", retlen) >= retlen)
			goto toolong;
	}

	return;
toolong:
	strlcpy(ret, "toolong", retlen);
}

static void
print_attr(const fido_dev_t *dev)
{
	char flags_txt[128];

	printf("proto: 0x%02x\n", fido_dev_protocol(dev));
	printf("major: 0x%02x\n", fido_dev_major(dev));
	printf("minor: 0x%02x\n", fido_dev_minor(dev));
	printf("build: 0x%02x\n", fido_dev_build(dev));

	format_flags(flags_txt, sizeof(flags_txt), fido_dev_flags(dev));
	printf("caps: 0x%02x (%s)\n", fido_dev_flags(dev), flags_txt);
}

static void
print_str_array(const char *label, char * const *sa, size_t len)
{
	if (len == 0)
		return;

	printf("%s strings: ", label);

	for (size_t i = 0; i < len; i++)
		printf("%s%s", i > 0 ? ", " : "", sa[i]);

	printf("\n");
}

static void
print_opt_array(const char *label, char * const *name, const bool *value,
    size_t len)
{
	if (len == 0)
		return;

	printf("%s: ", label);

	for (size_t i = 0; i < len; i++)
		printf("%s%s%s", i > 0 ? ", " : "",
		    value[i] ? "" : "no", name[i]);

	printf("\n");
}

static void
print_cert_array(const char *label, char * const *name, const uint64_t *value,
    size_t len)
{
	if (len == 0)
		return;

	printf("%s: ", label);

	for (size_t i = 0; i < len; i++)
		printf("%s%s %llu", i > 0 ? ", " : "", name[i],
		    (unsigned long long)value[i]);

	printf("\n");
}

static void
print_algorithms(const fido_cbor_info_t *ci)
{
	const char *cose, *type;
	size_t len;

	if ((len = fido_cbor_info_algorithm_count(ci)) == 0)
		return;

	printf("algorithms: ");

	for (size_t i = 0; i < len; i++) {
		cose = type = "unknown";
		switch (fido_cbor_info_algorithm_cose(ci, i)) {
		case COSE_ES256:
			cose = "es256";
			break;
		case COSE_ES384:
			cose = "es384";
			break;
		case COSE_RS256:
			cose = "rs256";
			break;
		case COSE_EDDSA:
			cose = "eddsa";
			break;
		}
		if (fido_cbor_info_algorithm_type(ci, i) != NULL)
			type = fido_cbor_info_algorithm_type(ci, i);
		printf("%s%s (%s)", i > 0 ? ", " : "", cose, type);
	}

	printf("\n");
}

static void
print_aaguid(const unsigned char *buf, size_t buflen)
{
	printf("aaguid: ");

	while (buflen--)
		printf("%02x", *buf++);

	printf("\n");
}

static void
print_maxmsgsiz(uint64_t maxmsgsiz)
{
	printf("maxmsgsiz: %d\n", (int)maxmsgsiz);
}

static void
print_maxcredcntlst(uint64_t maxcredcntlst)
{
	printf("maxcredcntlst: %d\n", (int)maxcredcntlst);
}

static void
print_maxcredidlen(uint64_t maxcredidlen)
{
	printf("maxcredlen: %d\n", (int)maxcredidlen);
}

static void
print_maxlargeblob(uint64_t maxlargeblob)
{
	printf("maxlargeblob: %d\n", (int)maxlargeblob);
}

static void
print_maxrpid_minpinlen(uint64_t maxrpid)
{
	if (maxrpid > 0)
		printf("maxrpids in minpinlen: %d\n", (int)maxrpid);
}

static void
print_minpinlen(uint64_t minpinlen)
{
	if (minpinlen > 0)
		printf("minpinlen: %d\n", (int)minpinlen);
}

static void
print_uv_attempts(uint64_t uv_attempts)
{
	if (uv_attempts > 0)
		printf("platform uv attempt(s): %d\n", (int)uv_attempts);
}

static void
print_uv_modality(uint64_t uv_modality)
{
	uint64_t mode;
	bool printed = false;

	if (uv_modality == 0)
		return;

	printf("uv modality: 0x%x (", (int)uv_modality);

	for (size_t i = 0; i < 64; i++) {
		mode = 1ULL << i;
		if ((uv_modality & mode) == 0)
			continue;
		if (printed)
			printf(", ");
		switch (mode) {
		case FIDO_UV_MODE_TUP:
			printf("test of user presence");
			break;
		case FIDO_UV_MODE_FP:
			printf("fingerprint check");
			break;
		case FIDO_UV_MODE_PIN:
			printf("pin check");
			break;
		case FIDO_UV_MODE_VOICE:
			printf("voice recognition");
			break;
		case FIDO_UV_MODE_FACE:
			printf("face recognition");
			break;
		case FIDO_UV_MODE_LOCATION:
			printf("location check");
			break;
		case FIDO_UV_MODE_EYE:
			printf("eyeprint check");
			break;
		case FIDO_UV_MODE_DRAWN:
			printf("drawn pattern check");
			break;
		case FIDO_UV_MODE_HAND:
			printf("handprint verification");
			break;
		case FIDO_UV_MODE_NONE:
			printf("none");
			break;
		case FIDO_UV_MODE_ALL:
			printf("all required");
			break;
		case FIDO_UV_MODE_EXT_PIN:
			printf("external pin");
			break;
		case FIDO_UV_MODE_EXT_DRAWN:
			printf("external drawn pattern check");
			break;
		default:
			printf("unknown 0x%llx", (unsigned long long)mode);
			break;
		}
		printed = true;
	}

	printf(")\n");
}

static void
print_rk_remaining(int64_t rk_remaining)
{
	if (rk_remaining != -1)
		printf("remaining rk(s): %d\n", (int)rk_remaining);
}

static void
print_fwversion(uint64_t fwversion)
{
	printf("fwversion: 0x%x\n", (int)fwversion);
}

static void
print_byte_array(const char *label, const uint8_t *ba, size_t len)
{
	if (len == 0)
		return;

	printf("%s: ", label);

	for (size_t i = 0; i < len; i++)
		printf("%s%u", i > 0 ? ", " : "", (unsigned)ba[i]);

	printf("\n");
}

int
token_info(int argc, char **argv, char *path)
{
	char			*cred_id = NULL;
	char			*rp_id = NULL;
	fido_cbor_info_t	*ci = NULL;
	fido_dev_t		*dev = NULL;
	int			 ch;
	int			 credman = 0;
	int			 r;
	int			 retrycnt;

	optind = 1;

	while ((ch = getopt(argc, argv, TOKEN_OPT)) != -1) {
		switch (ch) {
		case 'c':
			credman = 1;
			break;
		case 'i':
			cred_id = optarg;
			break;
		case 'k':
			rp_id = optarg;
			break;
		default:
			break; /* ignore */
		}
	}

	if (path == NULL || (credman && (cred_id != NULL || rp_id != NULL)))
		usage();

	dev = open_dev(path);

	if (credman)
		return (credman_get_metadata(dev, path));
	if (cred_id && rp_id)
		return (credman_print_rk(dev, path, rp_id, cred_id));
	if (cred_id || rp_id)
		usage();

	print_attr(dev);

	if (fido_dev_is_fido2(dev) == false)
		goto end;
	if ((ci = fido_cbor_info_new()) == NULL)
		errx(1, "fido_cbor_info_new");
	if ((r = fido_dev_get_cbor_info(dev, ci)) != FIDO_OK)
		errx(1, "fido_dev_get_cbor_info: %s (0x%x)", fido_strerr(r), r);

	/* print supported protocol versions */
	print_str_array("version", fido_cbor_info_versions_ptr(ci),
	    fido_cbor_info_versions_len(ci));

	/* print supported extensions */
	print_str_array("extension", fido_cbor_info_extensions_ptr(ci),
	    fido_cbor_info_extensions_len(ci));

	/* print supported transports */
	print_str_array("transport", fido_cbor_info_transports_ptr(ci),
	    fido_cbor_info_transports_len(ci));

	/* print supported algorithms */
	print_algorithms(ci);

	/* print aaguid */
	print_aaguid(fido_cbor_info_aaguid_ptr(ci),
	    fido_cbor_info_aaguid_len(ci));

	/* print supported options */
	print_opt_array("options", fido_cbor_info_options_name_ptr(ci),
	    fido_cbor_info_options_value_ptr(ci),
	    fido_cbor_info_options_len(ci));

	/* print certifications */
	print_cert_array("certifications", fido_cbor_info_certs_name_ptr(ci),
	    fido_cbor_info_certs_value_ptr(ci),
	    fido_cbor_info_certs_len(ci));

	/* print firmware version */
	print_fwversion(fido_cbor_info_fwversion(ci));

	/* print maximum message size */
	print_maxmsgsiz(fido_cbor_info_maxmsgsiz(ci));

	/* print maximum number of credentials allowed in credential lists */
	print_maxcredcntlst(fido_cbor_info_maxcredcntlst(ci));

	/* print maximum length of a credential ID */
	print_maxcredidlen(fido_cbor_info_maxcredidlen(ci));

	/* print maximum length of serialized largeBlob array */
	print_maxlargeblob(fido_cbor_info_maxlargeblob(ci));

	/* print maximum number of RP IDs in fido_dev_set_pin_minlen_rpid() */
	print_maxrpid_minpinlen(fido_cbor_info_maxrpid_minpinlen(ci));

	/* print estimated number of resident credentials */
	print_rk_remaining(fido_cbor_info_rk_remaining(ci));

	/* print minimum pin length */
	print_minpinlen(fido_cbor_info_minpinlen(ci));

	/* print supported pin protocols */
	print_byte_array("pin protocols", fido_cbor_info_protocols_ptr(ci),
	    fido_cbor_info_protocols_len(ci));

	if (fido_dev_get_retry_count(dev, &retrycnt) != FIDO_OK)
		printf("pin retries: undefined\n");
	else
		printf("pin retries: %d\n", retrycnt);

	printf("pin change required: %s\n",
	    fido_cbor_info_new_pin_required(ci) ? "true" : "false");

	if (fido_dev_get_uv_retry_count(dev, &retrycnt) != FIDO_OK)
		printf("uv retries: undefined\n");
	else
		printf("uv retries: %d\n", retrycnt);

	/* print platform uv attempts */
	print_uv_attempts(fido_cbor_info_uv_attempts(ci));

	/* print supported uv mechanisms */
	print_uv_modality(fido_cbor_info_uv_modality(ci));

	bio_info(dev);

	fido_cbor_info_free(&ci);
end:
	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(0);
}

int
token_reset(char *path)
{
	fido_dev_t *dev = NULL;
	int r;

	if (path == NULL)
		usage();

	dev = open_dev(path);
	if ((r = fido_dev_reset(dev)) != FIDO_OK)
		errx(1, "fido_dev_reset: %s", fido_strerr(r));

	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(0);
}

int
token_get(int argc, char **argv, char *path)
{
	char	*id = NULL;
	char	*key = NULL;
	char	*name = NULL;
	int	 blob = 0;
	int	 ch;

	optind = 1;

	while ((ch = getopt(argc, argv, TOKEN_OPT)) != -1) {
		switch (ch) {
		case 'b':
			blob = 1;
			break;
		case 'i':
			id = optarg;
			break;
		case 'k':
			key = optarg;
			break;
		case 'n':
			name = optarg;
			break;
		default:
			break; /* ignore */
		}
	}

	argc -= optind;
	argv += optind;

	if (blob == 0 || argc != 2)
		usage();

	return blob_get(path, key, name, id, argv[0]);
}

int
token_set(int argc, char **argv, char *path)
{
	char	*id = NULL;
	char	*key = NULL;
	char	*len = NULL;
	char	*display_name = NULL;
	char	*name = NULL;
	char	*rpid = NULL;
	int	 blob = 0;
	int	 cred = 0;
	int	 ch;
	int	 enroll = 0;
	int	 ea = 0;
	int	 uv = 0;
	bool	 force = false;

	optind = 1;

	while ((ch = getopt(argc, argv, TOKEN_OPT)) != -1) {
		switch (ch) {
		case 'a':
			ea = 1;
			break;
		case 'b':
			blob = 1;
			break;
		case 'c':
			cred = 1;
			break;
		case 'e':
			enroll = 1;
			break;
		case 'f':
			force = true;
			break;
		case 'i':
			id = optarg;
			break;
		case 'k':
			key = optarg;
			break;
		case 'l':
			len = optarg;
			break;
		case 'p':
			display_name = optarg;
			break;
		case 'm':
			rpid = optarg;
			break;
		case 'n':
			name = optarg;
			break;
		case 'u':
			uv = 1;
			break;
		default:
			break; /* ignore */
		}
	}

	argc -= optind;
	argv += optind;

	if (path == NULL)
		usage();

	if (blob) {
		if (argc != 2)
			usage();
		return (blob_set(path, key, name, id, argv[0]));
	}

	if (cred) {
		if (!id || !key)
			usage();
		if (!name && !display_name)
			usage();
		return (credman_update_rk(path, key, id, name, display_name));
	}

	if (enroll) {
		if (ea || uv)
			usage();
		if (id && name)
			return (bio_set_name(path, id, name));
		if (!id && !name)
			return (bio_enroll(path));
		usage();
	}

	if (ea) {
		if (uv)
			usage();
		return (config_entattest(path));
	}

	if (len)
		return (config_pin_minlen(path, len));
	if (rpid)
		return (config_pin_minlen_rpid(path, rpid));
	if (force)
		return (config_force_pin_change(path));
	if (uv)
		return (config_always_uv(path, 1));

	return (pin_set(path));
}

int
token_list(int argc, char **argv, char *path)
{
	fido_dev_info_t *devlist;
	size_t ndevs;
	const char *rp_id = NULL;
	int blobs = 0;
	int enrolls = 0;
	int keys = 0;
	int rplist = 0;
	int ch;
	int r;

	optind = 1;

	while ((ch = getopt(argc, argv, TOKEN_OPT)) != -1) {
		switch (ch) {
		case 'b':
			blobs = 1;
			break;
		case 'e':
			enrolls = 1;
			break;
		case 'k':
			keys = 1;
			rp_id = optarg;
			break;
		case 'r':
			rplist = 1;
			break;
		default:
			break; /* ignore */
		}
	}

	if (blobs || enrolls || keys || rplist) {
		if (path == NULL)
			usage();
		if (blobs)
			return (blob_list(path));
		if (enrolls)
			return (bio_list(path));
		if (keys)
			return (credman_list_rk(path, rp_id));
		if (rplist)
			return (credman_list_rp(path));
		/* NOTREACHED */
	}

	if ((devlist = fido_dev_info_new(64)) == NULL)
		errx(1, "fido_dev_info_new");
	if ((r = fido_dev_info_manifest(devlist, 64, &ndevs)) != FIDO_OK)
		errx(1, "fido_dev_info_manifest: %s (0x%x)", fido_strerr(r), r);

	for (size_t i = 0; i < ndevs; i++) {
		const fido_dev_info_t *di = fido_dev_info_ptr(devlist, i);
		printf("%s: vendor=0x%04x, product=0x%04x (%s %s)\n",
		    fido_dev_info_path(di),
		    (uint16_t)fido_dev_info_vendor(di),
		    (uint16_t)fido_dev_info_product(di),
		    fido_dev_info_manufacturer_string(di),
		    fido_dev_info_product_string(di));
	}

	fido_dev_info_free(&devlist, ndevs);

	exit(0);
}

int
token_delete(int argc, char **argv, char *path)
{
	char		*id = NULL;
	char		*key = NULL;
	char		*name = NULL;
	int		 blob = 0;
	int		 ch;
	int		 enroll = 0;
	int		 uv = 0;

	optind = 1;

	while ((ch = getopt(argc, argv, TOKEN_OPT)) != -1) {
		switch (ch) {
		case 'b':
			blob = 1;
			break;
		case 'e':
			enroll = 1;
			break;
		case 'i':
			id = optarg;
			break;
		case 'k':
			key = optarg;
			break;
		case 'n':
			name = optarg;
			break;
		case 'u':
			uv = 1;
			break;
		default:
			break; /* ignore */
		}
	}

	if (path == NULL)
		usage();

	if (blob)
		return (blob_delete(path, key, name, id));

	if (id) {
		if (uv)
			usage();
		if (enroll == 0)
			return (credman_delete_rk(path, id));
		return (bio_delete(path, id));
	}

	if (uv == 0)
		usage();

	return (config_always_uv(path, 0));
}
