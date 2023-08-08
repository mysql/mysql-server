/*
 * Copyright (c) 2020-2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <fido.h>
#include <fido/credman.h>

#include <cbor.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <zlib.h>

#include "../openbsd-compat/openbsd-compat.h"
#include "extern.h"

#define BOUND (1024UL * 1024UL)

struct rkmap {
	fido_credman_rp_t  *rp; /* known rps */
	fido_credman_rk_t **rk; /* rk per rp */
};

static void
free_rkmap(struct rkmap *map)
{
	if (map->rp != NULL) {
		for (size_t i = 0; i < fido_credman_rp_count(map->rp); i++)
			fido_credman_rk_free(&map->rk[i]);
		fido_credman_rp_free(&map->rp);
	}
	free(map->rk);
}

static int
map_known_rps(fido_dev_t *dev, const char *path, struct rkmap *map)
{
	const char *rp_id;
	char *pin = NULL;
	size_t n;
	int r, ok = -1;

	if ((map->rp = fido_credman_rp_new()) == NULL) {
		warnx("%s: fido_credman_rp_new", __func__);
		goto out;
	}
	if ((pin = get_pin(path)) == NULL)
		goto out;
	if ((r = fido_credman_get_dev_rp(dev, map->rp, pin)) != FIDO_OK) {
		warnx("fido_credman_get_dev_rp: %s", fido_strerr(r));
		goto out;
	}
	if ((n = fido_credman_rp_count(map->rp)) > UINT8_MAX) {
		warnx("%s: fido_credman_rp_count > UINT8_MAX", __func__);
		goto out;
	}
	if ((map->rk = calloc(n, sizeof(*map->rk))) == NULL) {
		warnx("%s: calloc", __func__);
		goto out;
	}
	for (size_t i = 0; i < n; i++) {
		if ((rp_id = fido_credman_rp_id(map->rp, i)) == NULL) {
			warnx("%s: fido_credman_rp_id %zu", __func__, i);
			goto out;
		}
		if ((map->rk[i] = fido_credman_rk_new()) == NULL) {
			warnx("%s: fido_credman_rk_new", __func__);
			goto out;
		}
		if ((r = fido_credman_get_dev_rk(dev, rp_id, map->rk[i],
		    pin)) != FIDO_OK) {
			warnx("%s: fido_credman_get_dev_rk %s: %s", __func__,
			    rp_id, fido_strerr(r));
			goto out;
		}
	}

	ok = 0;
out:
	freezero(pin, PINBUF_LEN);

	return ok;
}

static int
lookup_key(const char *path, fido_dev_t *dev, const char *rp_id,
    const struct blob *cred_id, char **pin, struct blob *key)
{
	fido_credman_rk_t *rk = NULL;
	const fido_cred_t *cred = NULL;
	size_t i, n;
	int r, ok = -1;

	if ((rk = fido_credman_rk_new()) == NULL) {
		warnx("%s: fido_credman_rk_new", __func__);
		goto out;
	}
	if ((r = fido_credman_get_dev_rk(dev, rp_id, rk, *pin)) != FIDO_OK &&
	    *pin == NULL && should_retry_with_pin(dev, r)) {
		if ((*pin = get_pin(path)) == NULL)
			goto out;
		r = fido_credman_get_dev_rk(dev, rp_id, rk, *pin);
	}
	if (r != FIDO_OK) {
		warnx("%s: fido_credman_get_dev_rk: %s", __func__,
		    fido_strerr(r));
		goto out;
	}
	if ((n = fido_credman_rk_count(rk)) == 0) {
		warnx("%s: rp id not found", __func__);
		goto out;
	}
	if (n == 1 && cred_id->len == 0) {
		/* use the credential we found */
		cred = fido_credman_rk(rk, 0);
	} else {
		if (cred_id->len == 0) {
			warnx("%s: multiple credentials found", __func__);
			goto out;
		}
		for (i = 0; i < n; i++) {
			const fido_cred_t *x = fido_credman_rk(rk, i);
			if (fido_cred_id_len(x) <= cred_id->len &&
			    !memcmp(fido_cred_id_ptr(x), cred_id->ptr,
			    fido_cred_id_len(x))) {
				cred = x;
				break;
			}
		}
	}
	if (cred == NULL) {
		warnx("%s: credential not found", __func__);
		goto out;
	}
	if (fido_cred_largeblob_key_ptr(cred) == NULL) {
		warnx("%s: no associated blob key", __func__);
		goto out;
	}
	key->len = fido_cred_largeblob_key_len(cred);
	if ((key->ptr = malloc(key->len)) == NULL) {
		warnx("%s: malloc", __func__);
		goto out;
	}
	memcpy(key->ptr, fido_cred_largeblob_key_ptr(cred), key->len);

	ok = 0;
out:
	fido_credman_rk_free(&rk);

	return ok;
}

static int
load_key(const char *keyf, const char *cred_id64, const char *rp_id,
    const char *path, fido_dev_t *dev, char **pin, struct blob *key)
{
	struct blob cred_id;
	FILE *fp;
	int r;

	memset(&cred_id, 0, sizeof(cred_id));

	if (keyf != NULL) {
		if (rp_id != NULL || cred_id64 != NULL)
			usage();
		fp = open_read(keyf);
		if ((r = base64_read(fp, key)) < 0)
			warnx("%s: base64_read %s", __func__, keyf);
		fclose(fp);
		return r;
	}
	if (rp_id == NULL)
		usage();
	if (cred_id64 != NULL && base64_decode(cred_id64, (void *)&cred_id.ptr,
	    &cred_id.len) < 0) {
		warnx("%s: base64_decode %s", __func__, cred_id64);
		return -1;
	}
	r = lookup_key(path, dev, rp_id, &cred_id, pin, key);
	free(cred_id.ptr);

	return r;
}

int
blob_set(const char *path, const char *keyf, const char *rp_id,
    const char *cred_id64, const char *blobf)
{
	fido_dev_t *dev;
	struct blob key, blob;
	char *pin = NULL;
	int r, ok = 1;

	dev = open_dev(path);
	memset(&key, 0, sizeof(key));
	memset(&blob, 0, sizeof(blob));

	if (read_file(blobf, &blob.ptr, &blob.len) < 0 ||
	    load_key(keyf, cred_id64, rp_id, path, dev, &pin, &key) < 0)
		goto out;
	if ((r = fido_dev_largeblob_set(dev, key.ptr, key.len, blob.ptr,
	    blob.len, pin)) != FIDO_OK && should_retry_with_pin(dev, r)) {
		if ((pin = get_pin(path)) == NULL)
			goto out;
		r = fido_dev_largeblob_set(dev, key.ptr, key.len, blob.ptr,
		    blob.len, pin);
	}
	if (r != FIDO_OK) {
		warnx("fido_dev_largeblob_set: %s", fido_strerr(r));
		goto out;
	}

	ok = 0; /* success */
out:
	freezero(key.ptr, key.len);
	freezero(blob.ptr, blob.len);
	freezero(pin, PINBUF_LEN);

	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

int
blob_get(const char *path, const char *keyf, const char *rp_id,
    const char *cred_id64, const char *blobf)
{
	fido_dev_t *dev;
	struct blob key, blob;
	char *pin = NULL;
	int r, ok = 1;

	dev = open_dev(path);
	memset(&key, 0, sizeof(key));
	memset(&blob, 0, sizeof(blob));

	if (load_key(keyf, cred_id64, rp_id, path, dev, &pin, &key) < 0)
		goto out;
	if ((r = fido_dev_largeblob_get(dev, key.ptr, key.len, &blob.ptr,
	    &blob.len)) != FIDO_OK) {
		warnx("fido_dev_largeblob_get: %s", fido_strerr(r));
		goto out;
	}
	if (write_file(blobf, blob.ptr, blob.len) < 0)
		goto out;

	ok = 0; /* success */
out:
	freezero(key.ptr, key.len);
	freezero(blob.ptr, blob.len);
	freezero(pin, PINBUF_LEN);

	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

int
blob_delete(const char *path, const char *keyf, const char *rp_id,
    const char *cred_id64)
{
	fido_dev_t *dev;
	struct blob key;
	char *pin = NULL;
	int r, ok = 1;

	dev = open_dev(path);
	memset(&key, 0, sizeof(key));

	if (load_key(keyf, cred_id64, rp_id, path, dev, &pin, &key) < 0)
		goto out;
	if ((r = fido_dev_largeblob_remove(dev, key.ptr, key.len,
	    pin)) != FIDO_OK && should_retry_with_pin(dev, r)) {
		if ((pin = get_pin(path)) == NULL)
			goto out;
		r = fido_dev_largeblob_remove(dev, key.ptr, key.len, pin);
	}
	if (r != FIDO_OK) {
		warnx("fido_dev_largeblob_remove: %s", fido_strerr(r));
		goto out;
	}

	ok = 0; /* success */
out:
	freezero(key.ptr, key.len);
	freezero(pin, PINBUF_LEN);

	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}

static int
try_decompress(const struct blob *in, uint64_t origsiz, int wbits)
{
	struct blob out;
	z_stream zs;
	u_int ilen, olen;
	int ok = -1;

	memset(&zs, 0, sizeof(zs));
	memset(&out, 0, sizeof(out));

	if (in->len > UINT_MAX || (ilen = (u_int)in->len) > BOUND)
		return -1;
	if (origsiz > SIZE_MAX || origsiz > UINT_MAX ||
	    (olen = (u_int)origsiz) > BOUND)
		return -1;
	if (inflateInit2(&zs, wbits) != Z_OK)
		return -1;

	if ((out.ptr = calloc(1, olen)) == NULL)
		goto fail;

	out.len = olen;
	zs.next_in = in->ptr;
	zs.avail_in = ilen;
	zs.next_out = out.ptr;
	zs.avail_out = olen;

	if (inflate(&zs, Z_FINISH) != Z_STREAM_END)
		goto fail;
	if (zs.avail_out != 0)
		goto fail;

	ok = 0;
fail:
	if (inflateEnd(&zs) != Z_OK)
		ok = -1;

	freezero(out.ptr, out.len);

	return ok;
}

static int
decompress(const struct blob *plaintext, uint64_t origsiz)
{
	if (try_decompress(plaintext, origsiz, MAX_WBITS) == 0) /* rfc1950 */
		return 0;
	return try_decompress(plaintext, origsiz, -MAX_WBITS); /* rfc1951 */
}

static int
decode(const struct blob *ciphertext, const struct blob *nonce,
    uint64_t origsiz, const fido_cred_t *cred)
{
	uint8_t aad[4 + sizeof(uint64_t)];
	EVP_CIPHER_CTX *ctx = NULL;
	const EVP_CIPHER *cipher;
	struct blob plaintext;
	uint64_t tmp;
	int ok = -1;

	memset(&plaintext, 0, sizeof(plaintext));

	if (nonce->len != 12)
		return -1;
	if (cred == NULL ||
	    fido_cred_largeblob_key_ptr(cred) == NULL ||
	    fido_cred_largeblob_key_len(cred) != 32)
		return -1;
	if (ciphertext->len > UINT_MAX ||
	    ciphertext->len > SIZE_MAX - 16 ||
	    ciphertext->len < 16)
		return -1;
	plaintext.len = ciphertext->len - 16;
	if ((plaintext.ptr = calloc(1, plaintext.len)) == NULL)
		return -1;
	if ((ctx = EVP_CIPHER_CTX_new()) == NULL ||
	    (cipher = EVP_aes_256_gcm()) == NULL ||
	    EVP_CipherInit(ctx, cipher, fido_cred_largeblob_key_ptr(cred),
	    nonce->ptr, 0) == 0)
		goto out;
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16,
	    ciphertext->ptr + ciphertext->len - 16) == 0)
		goto out;
	aad[0] = 0x62; /* b */
	aad[1] = 0x6c; /* l */
	aad[2] = 0x6f; /* o */
	aad[3] = 0x62; /* b */
	tmp = htole64(origsiz);
	memcpy(&aad[4], &tmp, sizeof(uint64_t));
	if (EVP_Cipher(ctx, NULL, aad, (u_int)sizeof(aad)) < 0 ||
	    EVP_Cipher(ctx, plaintext.ptr, ciphertext->ptr,
	    (u_int)plaintext.len) < 0 ||
	    EVP_Cipher(ctx, NULL, NULL, 0) < 0)
		goto out;
	if (decompress(&plaintext, origsiz) < 0)
		goto out;

	ok = 0;
out:
	freezero(plaintext.ptr, plaintext.len);

	if (ctx != NULL)
		EVP_CIPHER_CTX_free(ctx);

	return ok;
}

static const fido_cred_t *
try_rp(const fido_credman_rk_t *rk, const struct blob *ciphertext,
    const struct blob *nonce, uint64_t origsiz)
{
	const fido_cred_t *cred;

	for (size_t i = 0; i < fido_credman_rk_count(rk); i++)
		if ((cred = fido_credman_rk(rk, i)) != NULL &&
		    decode(ciphertext, nonce, origsiz, cred) == 0)
			return cred;

	return NULL;
}

static int
decode_cbor_blob(struct blob *out, const cbor_item_t *item)
{
	if (out->ptr != NULL ||
	    cbor_isa_bytestring(item) == false ||
	    cbor_bytestring_is_definite(item) == false)
		return -1;
	out->len = cbor_bytestring_length(item);
	if ((out->ptr = malloc(out->len)) == NULL)
		return -1;
	memcpy(out->ptr, cbor_bytestring_handle(item), out->len);

	return 0;
}

static int
decode_blob_entry(const cbor_item_t *item, struct blob *ciphertext,
    struct blob *nonce, uint64_t *origsiz)
{
	struct cbor_pair *v;

	if (item == NULL)
		return -1;
	if (cbor_isa_map(item) == false ||
	    cbor_map_is_definite(item) == false ||
	    (v = cbor_map_handle(item)) == NULL)
		return -1;
	if (cbor_map_size(item) > UINT8_MAX)
		return -1;

	for (size_t i = 0; i < cbor_map_size(item); i++) {
		if (cbor_isa_uint(v[i].key) == false ||
		    cbor_int_get_width(v[i].key) != CBOR_INT_8)
			continue; /* ignore */
		switch (cbor_get_uint8(v[i].key)) {
		case 1: /* ciphertext */
			if (decode_cbor_blob(ciphertext, v[i].value) < 0)
				return -1;
			break;
		case 2: /* nonce */
			if (decode_cbor_blob(nonce, v[i].value) < 0)
				return -1;
			break;
		case 3: /* origSize */
			if (*origsiz != 0 ||
			    cbor_isa_uint(v[i].value) == false ||
			    (*origsiz = cbor_get_int(v[i].value)) > SIZE_MAX)
				return -1;
		}
	}
	if (ciphertext->ptr == NULL || nonce->ptr == NULL || *origsiz == 0)
		return -1;

	return 0;
}

static void
print_blob_entry(size_t idx, const cbor_item_t *item, const struct rkmap *map)
{
	struct blob ciphertext, nonce;
	const fido_cred_t *cred = NULL;
	const char *rp_id = NULL;
	char *cred_id = NULL;
	uint64_t origsiz = 0;

	memset(&ciphertext, 0, sizeof(ciphertext));
	memset(&nonce, 0, sizeof(nonce));

	if (decode_blob_entry(item, &ciphertext, &nonce, &origsiz) < 0) {
		printf("%02zu: <skipped: bad cbor>\n", idx);
		goto out;
	}
	for (size_t i = 0; i < fido_credman_rp_count(map->rp); i++) {
		if ((cred = try_rp(map->rk[i], &ciphertext, &nonce,
		    origsiz)) != NULL) {
			rp_id = fido_credman_rp_id(map->rp, i);
			break;
		}
	}
	if (cred == NULL) {
		if ((cred_id = strdup("<unknown>")) == NULL) {
			printf("%02zu: <skipped: strdup failed>\n", idx);
			goto out;
		}
	} else {
		if (base64_encode(fido_cred_id_ptr(cred),
		    fido_cred_id_len(cred), &cred_id) < 0) {
			printf("%02zu: <skipped: base64_encode failed>\n", idx);
			goto out;
		}
	}
	if (rp_id == NULL)
		rp_id = "<unknown>";

	printf("%02zu: %4zu %4zu %s %s\n", idx, ciphertext.len,
	    (size_t)origsiz, cred_id, rp_id);
out:
	free(ciphertext.ptr);
	free(nonce.ptr);
	free(cred_id);
}

static cbor_item_t *
get_cbor_array(fido_dev_t *dev)
{
	struct cbor_load_result cbor_result;
	cbor_item_t *item = NULL;
	u_char *cbor_ptr = NULL;
	size_t cbor_len;
	int r, ok = -1;

	if ((r = fido_dev_largeblob_get_array(dev, &cbor_ptr,
	    &cbor_len)) != FIDO_OK) {
		warnx("%s: fido_dev_largeblob_get_array: %s", __func__,
		    fido_strerr(r));
		goto out;
	}
	if ((item = cbor_load(cbor_ptr, cbor_len, &cbor_result)) == NULL) {
		warnx("%s: cbor_load", __func__);
		goto out;
	}
	if (cbor_result.read != cbor_len) {
		warnx("%s: cbor_result.read (%zu) != cbor_len (%zu)", __func__,
		    cbor_result.read, cbor_len);
		/* continue */
	}
	if (cbor_isa_array(item) == false ||
	    cbor_array_is_definite(item) == false) {
		warnx("%s: cbor type", __func__);
		goto out;
	}
	if (cbor_array_size(item) > UINT8_MAX) {
		warnx("%s: cbor_array_size > UINT8_MAX", __func__);
		goto out;
	}
	if (cbor_array_size(item) == 0) {
		ok = 0; /* nothing to do */
		goto out;
	}

	printf("total map size: %zu byte%s\n", cbor_len, plural(cbor_len)); 

	ok = 0;
out:
	if (ok < 0 && item != NULL) {
		cbor_decref(&item);
		item = NULL;
	}
	free(cbor_ptr);

	return item;
}

int
blob_list(const char *path)
{
	struct rkmap map;
	fido_dev_t *dev = NULL;
	cbor_item_t *item = NULL, **v;
	int ok = 1;

	memset(&map, 0, sizeof(map));
	dev = open_dev(path);
	if (map_known_rps(dev, path, &map) < 0 ||
	    (item = get_cbor_array(dev)) == NULL)
		goto out;
	if (cbor_array_size(item) == 0) {
		ok = 0; /* nothing to do */
		goto out;
	}
	if ((v = cbor_array_handle(item)) == NULL) {
		warnx("%s: cbor_array_handle", __func__);
		goto out;
	}
	for (size_t i = 0; i < cbor_array_size(item); i++)
		print_blob_entry(i, v[i], &map);

	ok = 0; /* success */
out:
	free_rkmap(&map);

	if (item != NULL)
		cbor_decref(&item);

	fido_dev_close(dev);
	fido_dev_free(&dev);

	exit(ok);
}
