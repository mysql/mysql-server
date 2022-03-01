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
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _MSC_VER
#include "../openbsd-compat/posix_win.h"
#endif
#include "../openbsd-compat/openbsd-compat.h"
#include "extern.h"

#ifdef SIGNAL_EXAMPLE
volatile sig_atomic_t got_signal = 0;

static void
signal_handler(int signo)
{
	(void)signo;
	got_signal = 1;
}

void
prepare_signal_handler(int signo)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));

	sigemptyset(&sa.sa_mask);
	sa.sa_handler = signal_handler;

	if (sigaction(signo, &sa, NULL) < 0)
		err(1, "sigaction");
}
#endif

int
base10(const char *str, long long *ll)
{
	char *ep;

	*ll = strtoll(str, &ep, 10);
	if (str == ep || *ep != '\0')
		return (-1);
	else if (*ll == LLONG_MIN && errno == ERANGE)
		return (-1);
	else if (*ll == LLONG_MAX && errno == ERANGE)
		return (-1);

	return (0);
}

int
write_blob(const char *path, const unsigned char *ptr, size_t len)
{
	int fd, ok = -1;
	ssize_t n;

	if ((fd = open(path, O_WRONLY | O_CREAT, 0600)) < 0) {
		warn("open %s", path);
		goto fail;
	}

	if ((n = write(fd, ptr, len)) < 0) {
		warn("write");
		goto fail;
	}
	if ((size_t)n != len) {
		warnx("write");
		goto fail;
	}

	ok = 0;
fail:
	if (fd != -1) {
		close(fd);
	}

	return (ok);
}

int
read_blob(const char *path, unsigned char **ptr, size_t *len)
{
	int fd, ok = -1;
	struct stat st;
	ssize_t n;

	*ptr = NULL;
	*len = 0;

	if ((fd = open(path, O_RDONLY)) < 0) {
		warn("open %s", path);
		goto fail;
	}
	if (fstat(fd, &st) < 0) {
		warn("stat %s", path);
		goto fail;
	}
	if (st.st_size < 0) {
		warnx("stat %s: invalid size", path);
		goto fail;
	}
	*len = (size_t)st.st_size;
	if ((*ptr = malloc(*len)) == NULL) {
		warn("malloc");
		goto fail;
	}
	if ((n = read(fd, *ptr, *len)) < 0) {
		warn("read");
		goto fail;
	}
	if ((size_t)n != *len) {
		warnx("read");
		goto fail;
	}

	ok = 0;
fail:
	if (fd != -1) {
		close(fd);
	}
	if (ok < 0) {
		free(*ptr);
		*ptr = NULL;
		*len = 0;
	}

	return (ok);
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
	if (fp != NULL) {
		fclose(fp);
	}
	if (pkey != NULL) {
		EVP_PKEY_free(pkey);
	}

	return (ec);
}

int
write_ec_pubkey(const char *path, const void *ptr, size_t len)
{
	FILE *fp = NULL;
	EVP_PKEY *pkey = NULL;
	es256_pk_t *pk = NULL;
	int fd = -1;
	int ok = -1;

	if ((pk = es256_pk_new()) == NULL) {
		warnx("es256_pk_new");
		goto fail;
	}

	if (es256_pk_from_ptr(pk, ptr, len) != FIDO_OK) {
		warnx("es256_pk_from_ptr");
		goto fail;
	}

	if ((fd = open(path, O_WRONLY | O_CREAT, 0644)) < 0) {
		warn("open %s", path);
		goto fail;
	}

	if ((fp = fdopen(fd, "w")) == NULL) {
		warn("fdopen");
		goto fail;
	}
	fd = -1; /* owned by fp now */

	if ((pkey = es256_pk_to_EVP_PKEY(pk)) == NULL) {
		warnx("es256_pk_to_EVP_PKEY");
		goto fail;
	}

	if (PEM_write_PUBKEY(fp, pkey) == 0) {
		warnx("PEM_write_PUBKEY");
		goto fail;
	}

	ok = 0;
fail:
	es256_pk_free(&pk);

	if (fp != NULL) {
		fclose(fp);
	}
	if (fd != -1) {
		close(fd);
	}
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
	if (fp != NULL) {
		fclose(fp);
	}
	if (pkey != NULL) {
		EVP_PKEY_free(pkey);
	}

	return (rsa);
}

int
write_rsa_pubkey(const char *path, const void *ptr, size_t len)
{
	FILE *fp = NULL;
	EVP_PKEY *pkey = NULL;
	rs256_pk_t *pk = NULL;
	int fd = -1;
	int ok = -1;

	if ((pk = rs256_pk_new()) == NULL) {
		warnx("rs256_pk_new");
		goto fail;
	}

	if (rs256_pk_from_ptr(pk, ptr, len) != FIDO_OK) {
		warnx("rs256_pk_from_ptr");
		goto fail;
	}

	if ((fd = open(path, O_WRONLY | O_CREAT, 0644)) < 0) {
		warn("open %s", path);
		goto fail;
	}

	if ((fp = fdopen(fd, "w")) == NULL) {
		warn("fdopen");
		goto fail;
	}
	fd = -1; /* owned by fp now */

	if ((pkey = rs256_pk_to_EVP_PKEY(pk)) == NULL) {
		warnx("rs256_pk_to_EVP_PKEY");
		goto fail;
	}

	if (PEM_write_PUBKEY(fp, pkey) == 0) {
		warnx("PEM_write_PUBKEY");
		goto fail;
	}

	ok = 0;
fail:
	rs256_pk_free(&pk);

	if (fp != NULL) {
		fclose(fp);
	}
	if (fd != -1) {
		close(fd);
	}
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
write_eddsa_pubkey(const char *path, const void *ptr, size_t len)
{
	FILE *fp = NULL;
	EVP_PKEY *pkey = NULL;
	eddsa_pk_t *pk = NULL;
	int fd = -1;
	int ok = -1;

	if ((pk = eddsa_pk_new()) == NULL) {
		warnx("eddsa_pk_new");
		goto fail;
	}

	if (eddsa_pk_from_ptr(pk, ptr, len) != FIDO_OK) {
		warnx("eddsa_pk_from_ptr");
		goto fail;
	}

	if ((fd = open(path, O_WRONLY | O_CREAT, 0644)) < 0) {
		warn("open %s", path);
		goto fail;
	}

	if ((fp = fdopen(fd, "w")) == NULL) {
		warn("fdopen");
		goto fail;
	}
	fd = -1; /* owned by fp now */

	if ((pkey = eddsa_pk_to_EVP_PKEY(pk)) == NULL) {
		warnx("eddsa_pk_to_EVP_PKEY");
		goto fail;
	}

	if (PEM_write_PUBKEY(fp, pkey) == 0) {
		warnx("PEM_write_PUBKEY");
		goto fail;
	}

	ok = 0;
fail:
	eddsa_pk_free(&pk);

	if (fp != NULL) {
		fclose(fp);
	}
	if (fd != -1) {
		close(fd);
	}
	if (pkey != NULL) {
		EVP_PKEY_free(pkey);
	}

	return (ok);
}
