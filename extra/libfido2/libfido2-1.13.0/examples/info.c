/*
 * Copyright (c) 2018-2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <fido.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../openbsd-compat/openbsd-compat.h"

/*
 * Pretty-print a device's capabilities flags and return the result.
 */
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

/*
 * Print a FIDO device's attributes on stdout.
 */
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

/*
 * Auxiliary function to print an array of strings on stdout.
 */
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

/*
 * Auxiliary function to print (char *, bool) pairs on stdout.
 */
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

/*
 * Auxiliary function to print (char *, uint64_t) pairs on stdout.
 */
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

/*
 * Auxiliary function to print a list of supported COSE algorithms on stdout.
 */
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

/*
 * Auxiliary function to print an authenticator's AAGUID on stdout.
 */
static void
print_aaguid(const unsigned char *buf, size_t buflen)
{
	printf("aaguid: ");

	while (buflen--)
		printf("%02x", *buf++);

	printf("\n");
}

/*
 * Auxiliary function to print an authenticator's maximum message size on
 * stdout.
 */
static void
print_maxmsgsiz(uint64_t maxmsgsiz)
{
	printf("maxmsgsiz: %d\n", (int)maxmsgsiz);
}

/*
 * Auxiliary function to print an authenticator's maximum number of credentials
 * in a credential list on stdout.
 */
static void
print_maxcredcntlst(uint64_t maxcredcntlst)
{
	printf("maxcredcntlst: %d\n", (int)maxcredcntlst);
}

/*
 * Auxiliary function to print an authenticator's maximum credential ID length
 * on stdout.
 */
static void
print_maxcredidlen(uint64_t maxcredidlen)
{
	printf("maxcredlen: %d\n", (int)maxcredidlen);
}

/*
 * Auxiliary function to print the maximum size of an authenticator's
 * serialized largeBlob array.
 */
static void
print_maxlargeblob(uint64_t maxlargeblob)
{
	printf("maxlargeblob: %d\n", (int)maxlargeblob);
}

/*
 * Auxiliary function to print the authenticator's estimated number of
 * remaining resident credentials.
 */
static void
print_rk_remaining(int64_t rk_remaining)
{
	printf("remaining rk(s): ");

	if (rk_remaining == -1)
		printf("undefined\n");
	else
		printf("%d\n", (int)rk_remaining);
}

/*
 * Auxiliary function to print the minimum pin length observed by the
 * authenticator.
 */
static void
print_minpinlen(uint64_t minpinlen)
{
	printf("minpinlen: %d\n", (int)minpinlen);
}

/*
 * Auxiliary function to print the authenticator's preferred (platform)
 * UV attempts.
 */
static void
print_uv_attempts(uint64_t uv_attempts)
{
	printf("platform uv attempt(s): %d\n", (int)uv_attempts);
}

/*
 * Auxiliary function to print an authenticator's firmware version on stdout.
 */
static void
print_fwversion(uint64_t fwversion)
{
	printf("fwversion: 0x%x\n", (int)fwversion);
}

/*
 * Auxiliary function to print an array of bytes on stdout.
 */
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

static void
getinfo(const char *path)
{
	fido_dev_t		*dev;
	fido_cbor_info_t	*ci;
	int			 r;

	fido_init(0);

	if ((dev = fido_dev_new()) == NULL)
		errx(1, "fido_dev_new");
	if ((r = fido_dev_open(dev, path)) != FIDO_OK)
		errx(1, "fido_dev_open: %s (0x%x)", fido_strerr(r), r);

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

	/* print maximum length of largeBlob array */
	print_maxlargeblob(fido_cbor_info_maxlargeblob(ci));

	/* print number of remaining resident credentials */
	print_rk_remaining(fido_cbor_info_rk_remaining(ci));

	/* print minimum pin length */
	print_minpinlen(fido_cbor_info_minpinlen(ci));

	/* print supported pin protocols */
	print_byte_array("pin protocols", fido_cbor_info_protocols_ptr(ci),
	    fido_cbor_info_protocols_len(ci));

	/* print whether a new pin is required */
	printf("pin change required: %s\n",
	    fido_cbor_info_new_pin_required(ci) ? "true" : "false");

	/* print platform uv attempts */
	print_uv_attempts(fido_cbor_info_uv_attempts(ci));

	fido_cbor_info_free(&ci);
end:
	if ((r = fido_dev_close(dev)) != FIDO_OK)
		errx(1, "fido_dev_close: %s (0x%x)", fido_strerr(r), r);

	fido_dev_free(&dev);
}

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: info <device>\n");
		exit(EXIT_FAILURE);
	}

	getinfo(argv[1]);

	exit(0);
}
