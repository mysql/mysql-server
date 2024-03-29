/*
 * Copyright (c) 2019-2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <openssl/sha.h>

#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mutator_aux.h"

extern int fuzz_save_corpus;

static bool debug;
static unsigned int flags = MUTATE_ALL;
static unsigned long long test_fail;
static unsigned long long test_total;
static unsigned long long mutate_fail;
static unsigned long long mutate_total;

int LLVMFuzzerInitialize(int *, char ***);
int LLVMFuzzerTestOneInput(const uint8_t *, size_t);
size_t LLVMFuzzerCustomMutator(uint8_t *, size_t, size_t, unsigned int);

static int
save_seed(const char *opt)
{
	const char *path;
	int fd = -1, status = 1;
	void *buf = NULL;
	const size_t buflen = MAXCORPUS;
	size_t n;
	struct param *p = NULL;

	if ((path = strchr(opt, '=')) == NULL || strlen(++path) == 0) {
		warnx("usage: --fido-save-seed=<path>");
		goto fail;
	}

	if ((fd = open(path, O_CREAT|O_TRUNC|O_WRONLY, 0644)) == -1) {
		warn("open %s", path);
		goto fail;
	}

	if ((buf = malloc(buflen)) == NULL) {
		warn("malloc");
		goto fail;
	}

	n = pack_dummy(buf, buflen);

	if ((p = unpack(buf, n)) == NULL) {
		warnx("unpack");
		goto fail;
	}

	if (write(fd, buf, n) != (ssize_t)n) {
		warn("write %s", path);
		goto fail;
	}

	status = 0;
fail:
	if (fd != -1)
		close(fd);
	free(buf);
	free(p);

	return status;
}

static int
save_corpus(const struct param *p)
{
	uint8_t blob[MAXCORPUS], dgst[SHA256_DIGEST_LENGTH];
	size_t blob_len;
	char path[PATH_MAX];
	int r, fd;

	if ((blob_len = pack(blob, sizeof(blob), p)) == 0 ||
	    blob_len > sizeof(blob)) {
		warnx("pack");
		return -1;
	}

	if (SHA256(blob, blob_len, dgst) != dgst) {
		warnx("sha256");
		return -1;
	}

	if ((r = snprintf(path, sizeof(path), "saved_corpus_%02x%02x%02x%02x"
	    "%02x%02x%02x%02x", dgst[0], dgst[1], dgst[2], dgst[3], dgst[4],
	    dgst[5], dgst[6], dgst[7])) < 0 || (size_t)r >= sizeof(path)) {
		warnx("snprintf");
		return -1;
	}

	if ((fd = open(path, O_CREAT|O_TRUNC|O_WRONLY, 0644)) == -1) {
		warn("open %s", path);
		return -1;
	}

	if (write(fd, blob, blob_len) != (ssize_t)blob_len) {
		warn("write");
		r = -1;
	} else {
		warnx("wrote %s", path);
		r = 0;
	}

	close(fd);

	return r;
}

static void
parse_mutate_flags(const char *opt, unsigned int *mutate_flags)
{
	const char *f;

	if ((f = strchr(opt, '=')) == NULL || strlen(++f) == 0)
		errx(1, "usage: --fido-mutate=<flag>");

	if (strcmp(f, "seed") == 0)
		*mutate_flags |= MUTATE_SEED;
	else if (strcmp(f, "param") == 0)
		*mutate_flags |= MUTATE_PARAM;
	else if (strcmp(f, "wiredata") == 0)
		*mutate_flags |= MUTATE_WIREDATA;
	else
		errx(1, "--fido-mutate: unknown flag '%s'", f);
}

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
	unsigned int mutate_flags = 0;

	for (int i = 0; i < *argc; i++)
		if (strcmp((*argv)[i], "--fido-debug") == 0) {
			debug = 1;
		} else if (strncmp((*argv)[i], "--fido-save-seed=", 17) == 0) {
			exit(save_seed((*argv)[i]));
		} else if (strncmp((*argv)[i], "--fido-mutate=", 14) == 0) {
			parse_mutate_flags((*argv)[i], &mutate_flags);
		}

	if (mutate_flags)
		flags = mutate_flags;

	return 0;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct param *p;

	if (size > MAXCORPUS)
		return 0;

	if (++test_total % 100000 == 0 && debug) {
		double r = (double)test_fail/(double)test_total * 100.0;
		fprintf(stderr, "%s: %llu/%llu (%.2f%%)\n", __func__,
		    test_fail, test_total, r);
	}

	if ((p = unpack(data, size)) == NULL)
		test_fail++;
	else {
		fuzz_save_corpus = 0;
		test(p);
		if (fuzz_save_corpus && save_corpus(p) < 0)
			fprintf(stderr, "%s: failed to save corpus\n",
			    __func__);
		free(p);
	}

	return 0;
}

size_t
LLVMFuzzerCustomMutator(uint8_t *data, size_t size, size_t maxsize,
    unsigned int seed) NO_MSAN
{
	struct param *p;
	uint8_t blob[MAXCORPUS];
	size_t blob_len;

	memset(&p, 0, sizeof(p));

#ifdef WITH_MSAN
	__msan_unpoison(data, maxsize);
#endif

	if (++mutate_total % 100000 == 0 && debug) {
		double r = (double)mutate_fail/(double)mutate_total * 100.0;
		fprintf(stderr, "%s: %llu/%llu (%.2f%%)\n", __func__,
		    mutate_fail, mutate_total, r);
	}

	if ((p = unpack(data, size)) == NULL) {
		mutate_fail++;
		return pack_dummy(data, maxsize);
	}

	mutate(p, seed, flags);

	if ((blob_len = pack(blob, sizeof(blob), p)) == 0 ||
	    blob_len > sizeof(blob) || blob_len > maxsize) {
		mutate_fail++;
		free(p);
		return 0;
	}

	free(p);

	memcpy(data, blob, blob_len);

	return blob_len;
}
