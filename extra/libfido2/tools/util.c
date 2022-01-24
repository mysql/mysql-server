/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <fido.h>
#include <fido/es256.h>
#include <fido/rs256.h>
#include <fido/eddsa.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../openbsd-compat/openbsd-compat.h"
#ifdef _MSC_VER
#include "../openbsd-compat/posix_win.h"
#endif

#include "extern.h"

void
read_pin(const char *path, char *buf, size_t len)
{
	char prompt[1024];
	int r;

	r = snprintf(prompt, sizeof(prompt), "Enter PIN for %s: ", path);
	if (r < 0 || (size_t)r >= sizeof(prompt))
		errx(1, "snprintf");
	if (!readpassphrase(prompt, buf, len, RPP_ECHO_OFF))
		errx(1, "readpassphrase");
}

FILE *
open_write(const char *file)
{
	int fd;
	FILE *f;

	if (file == NULL || strcmp(file, "-") == 0)
		return (stdout);
	if ((fd = open(file, O_WRONLY | O_CREAT, 0600)) < 0)
		err(1, "open %s", file);
	if ((f = fdopen(fd, "w")) == NULL)
		err(1, "fdopen %s", file);

	return (f);
}

FILE *
open_read(const char *file)
{
	int fd;
	FILE *f;

	if (file == NULL || strcmp(file, "-") == 0) {
#ifdef FIDO_FUZZ
		setvbuf(stdin, NULL, _IONBF, 0);
#endif
		return (stdin);
	}
	if ((fd = open(file, O_RDONLY)) < 0)
		err(1, "open %s", file);
	if ((f = fdopen(fd, "r")) == NULL)
		err(1, "fdopen %s", file);

	return (f);
}

int
base10(const char *str)
{
	char *ep;
	long long ll;

	ll = strtoll(str, &ep, 10);
	if (str == ep || *ep != '\0')
		return (-1);
	else if (ll == LLONG_MIN && errno == ERANGE)
		return (-1);
	else if (ll == LLONG_MAX && errno == ERANGE)
		return (-1);
	else if (ll < 0 || ll > INT_MAX)
		return (-1);

	return ((int)ll);
}

void
xxd(const void *buf, size_t count)
{
	const uint8_t	*ptr = buf;
	size_t		 i;

	fprintf(stderr, "  ");

	for (i = 0; i < count; i++) {
		fprintf(stderr, "%02x ", *ptr++);
		if ((i + 1) % 16 == 0 && i + 1 < count)
			fprintf(stderr, "\n  ");
	}

	fprintf(stderr, "\n");
	fflush(stderr);
}

int
string_read(FILE *f, char **out)
{
	char *line = NULL;
	size_t linesize = 0;
	ssize_t n;

	*out = NULL;

	if ((n = getline(&line, &linesize, f)) <= 0 ||
	    (size_t)n != strlen(line)) {
		free(line);
		return (-1);
	}

	line[n - 1] = '\0'; /* trim \n */
	*out = line;

	return (0);
}

fido_dev_t *
open_dev(const char *path)
{
	fido_dev_t *dev;
	int r;

	if ((dev = fido_dev_new()) == NULL)
		errx(1, "fido_dev_new");

	r = fido_dev_open(dev, path);
	if (r != FIDO_OK)
		errx(1, "fido_dev_open %s: %s", path, fido_strerr(r));

	return (dev);
}

EC_KEY *
read_ec_pubkey(const char *path)
{
	FILE *fp = NULL;
	EVP_PKEY *pkey = NULL;
	EC_KEY *ec = NULL;

	if ((fp = fopen(path, "r")) == NULL) {
		warn("fopen");
		goto fail;
	}

	if ((pkey = PEM_read_PUBKEY(fp, NULL, NULL, NULL)) == NULL) {
		warnx("PEM_read_PUBKEY");
		goto fail;
	}
	if ((ec = EVP_PKEY_get1_EC_KEY(pkey)) == NULL) {
		warnx("EVP_PKEY_get1_EC_KEY");
		goto fail;
	}

fail:
	if (fp) {
		fclose(fp);
	}
	if (pkey) {
		EVP_PKEY_free(pkey);
	}

	return (ec);
}

int
write_ec_pubkey(FILE *f, const void *ptr, size_t len)
{
	EVP_PKEY *pkey = NULL;
	es256_pk_t *pk = NULL;
	int ok = -1;

	if ((pk = es256_pk_new()) == NULL) {
		warnx("es256_pk_new");
		goto fail;
	}

	if (es256_pk_from_ptr(pk, ptr, len) != FIDO_OK) {
		warnx("es256_pk_from_ptr");
		goto fail;
	}

	if ((pkey = es256_pk_to_EVP_PKEY(pk)) == NULL) {
		warnx("es256_pk_to_EVP_PKEY");
		goto fail;
	}

	if (PEM_write_PUBKEY(f, pkey) == 0) {
		warnx("PEM_write_PUBKEY");
		goto fail;
	}

	ok = 0;
fail:
	es256_pk_free(&pk);

	if (pkey != NULL) {
		EVP_PKEY_free(pkey);
	}

	return (ok);
}

RSA *
read_rsa_pubkey(const char *path)
{
	FILE *fp = NULL;
	EVP_PKEY *pkey = NULL;
	RSA *rsa = NULL;

	if ((fp = fopen(path, "r")) == NULL) {
		warn("fopen");
		goto fail;
	}

	if ((pkey = PEM_read_PUBKEY(fp, NULL, NULL, NULL)) == NULL) {
		warnx("PEM_read_PUBKEY");
		goto fail;
	}
	if ((rsa = EVP_PKEY_get1_RSA(pkey)) == NULL) {
		warnx("EVP_PKEY_get1_RSA");
		goto fail;
	}

fail:
	if (fp) {
		fclose(fp);
	}
	if (pkey) {
		EVP_PKEY_free(pkey);
	}

	return (rsa);
}

int
write_rsa_pubkey(FILE *f, const void *ptr, size_t len)
{
	EVP_PKEY *pkey = NULL;
	rs256_pk_t *pk = NULL;
	int ok = -1;

	if ((pk = rs256_pk_new()) == NULL) {
		warnx("rs256_pk_new");
		goto fail;
	}

	if (rs256_pk_from_ptr(pk, ptr, len) != FIDO_OK) {
		warnx("rs256_pk_from_ptr");
		goto fail;
	}

	if ((pkey = rs256_pk_to_EVP_PKEY(pk)) == NULL) {
		warnx("rs256_pk_to_EVP_PKEY");
		goto fail;
	}

	if (PEM_write_PUBKEY(f, pkey) == 0) {
		warnx("PEM_write_PUBKEY");
		goto fail;
	}

	ok = 0;
fail:
	rs256_pk_free(&pk);

	if (pkey != NULL) {
		EVP_PKEY_free(pkey);
	}

	return (ok);
}

EVP_PKEY *
read_eddsa_pubkey(const char *path)
{
	FILE *fp = NULL;
	EVP_PKEY *pkey = NULL;

	if ((fp = fopen(path, "r")) == NULL) {
		warn("fopen");
		goto fail;
	}

	if ((pkey = PEM_read_PUBKEY(fp, NULL, NULL, NULL)) == NULL) {
		warnx("PEM_read_PUBKEY");
		goto fail;
	}

fail:
	if (fp) {
		fclose(fp);
	}

	return (pkey);
}

int
write_eddsa_pubkey(FILE *f, const void *ptr, size_t len)
{
	EVP_PKEY *pkey = NULL;
	eddsa_pk_t *pk = NULL;
	int ok = -1;

	if ((pk = eddsa_pk_new()) == NULL) {
		warnx("eddsa_pk_new");
		goto fail;
	}

	if (eddsa_pk_from_ptr(pk, ptr, len) != FIDO_OK) {
		warnx("eddsa_pk_from_ptr");
		goto fail;
	}

	if ((pkey = eddsa_pk_to_EVP_PKEY(pk)) == NULL) {
		warnx("eddsa_pk_to_EVP_PKEY");
		goto fail;
	}

	if (PEM_write_PUBKEY(f, pkey) == 0) {
		warnx("PEM_write_PUBKEY");
		goto fail;
	}

	ok = 0;
fail:
	eddsa_pk_free(&pk);

	if (pkey != NULL) {
		EVP_PKEY_free(pkey);
	}

	return (ok);
}

void
print_cred(FILE *out_f, int type, const fido_cred_t *cred)
{
	char *id;
	int r;

	r = base64_encode(fido_cred_id_ptr(cred), fido_cred_id_len(cred), &id);
	if (r < 0)
		errx(1, "output error");

	fprintf(out_f, "%s\n", id);

	if (type == COSE_ES256) {
		write_ec_pubkey(out_f, fido_cred_pubkey_ptr(cred),
		    fido_cred_pubkey_len(cred));
	} else if (type == COSE_RS256) {
		write_rsa_pubkey(out_f, fido_cred_pubkey_ptr(cred),
		    fido_cred_pubkey_len(cred));
	} else if (type == COSE_EDDSA) {
		write_eddsa_pubkey(out_f, fido_cred_pubkey_ptr(cred),
		    fido_cred_pubkey_len(cred));
	} else {
		errx(1, "print_cred: unknown type");
	}

	free(id);
}

int
cose_type(const char *str, int *type)
{
	if (strcmp(str, "es256") == 0)
		*type = COSE_ES256;
	else if (strcmp(str, "rs256") == 0)
		*type = COSE_RS256;
	else if (strcmp(str, "eddsa") == 0)
		*type = COSE_EDDSA;
	else {
		*type = 0;
		return (-1);
	}

	return (0);
}

const char *
cose_string(int type)
{
	switch (type) {
	case COSE_EDDSA:
		return ("eddsa");
	case COSE_ES256:
		return ("es256");
	case COSE_RS256:
		return ("rs256");
	default:
		return ("unknown");
	}
}

const char *
prot_string(int prot)
{
	switch (prot) {
	case FIDO_CRED_PROT_UV_OPTIONAL:
		return ("uvopt");
	case FIDO_CRED_PROT_UV_OPTIONAL_WITH_ID:
		return ("uvopt+id");
	case FIDO_CRED_PROT_UV_REQUIRED:
		return ("uvreq");
	default:
		return ("unknown");
	}
}
