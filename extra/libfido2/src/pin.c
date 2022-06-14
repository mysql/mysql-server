/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <string.h>

#include "fido.h"
#include "fido/es256.h"

static int
parse_pintoken(const cbor_item_t *key, const cbor_item_t *val, void *arg)
{
	fido_blob_t *token = arg;

	if (cbor_isa_uint(key) == false ||
	    cbor_int_get_width(key) != CBOR_INT_8 ||
	    cbor_get_uint8(key) != 2) {
		fido_log_debug("%s: cbor type", __func__);
		return (0); /* ignore */
	}

	return (fido_blob_decode(val, token));
}

#ifdef FIDO_UVTOKEN
static int
parse_uvtoken(const cbor_item_t *key, const cbor_item_t *val, void *arg)
{
	return (parse_pintoken(key, val, arg));
}
#endif /* FIDO_UVTOKEN */

static int
fido_dev_get_pin_token_tx(fido_dev_t *dev, const char *pin,
    const fido_blob_t *ecdh, const es256_pk_t *pk)
{
	fido_blob_t	 f;
	fido_blob_t	*p = NULL;
	cbor_item_t	*argv[6];
	int		 r;

	memset(&f, 0, sizeof(f));
	memset(argv, 0, sizeof(argv));

	if ((p = fido_blob_new()) == NULL || fido_blob_set(p,
	    (const unsigned char *)pin, strlen(pin)) < 0) {
		fido_log_debug("%s: fido_blob_set", __func__);
		r = FIDO_ERR_INVALID_ARGUMENT;
		goto fail;
	}

	if ((argv[0] = cbor_build_uint8(1)) == NULL ||
	    (argv[1] = cbor_build_uint8(5)) == NULL ||
	    (argv[2] = es256_pk_encode(pk, 1)) == NULL ||
	    (argv[5] = cbor_encode_pin_hash_enc(ecdh, p)) == NULL) {
		fido_log_debug("%s: cbor encode", __func__);
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	if (cbor_build_frame(CTAP_CBOR_CLIENT_PIN, argv, nitems(argv),
	    &f) < 0 || fido_tx(dev, CTAP_CMD_CBOR, f.ptr, f.len) < 0) {
		fido_log_debug("%s: fido_tx", __func__);
		r = FIDO_ERR_TX;
		goto fail;
	}

	r = FIDO_OK;
fail:
	cbor_vector_free(argv, nitems(argv));
	fido_blob_free(&p);
	free(f.ptr);

	return (r);
}

#ifdef FIDO_UVTOKEN
static int
fido_dev_get_uv_token_tx(fido_dev_t *dev, const es256_pk_t *pk)
{
	fido_blob_t	 f;
	cbor_item_t	*argv[3];
	int		 r;

	memset(&f, 0, sizeof(f));
	memset(argv, 0, sizeof(argv));

	if ((argv[0] = cbor_build_uint8(1)) == NULL ||
	    (argv[1] = cbor_build_uint8(6)) == NULL ||
	    (argv[2] = es256_pk_encode(pk, 1)) == NULL) {
		fido_log_debug("%s: cbor encode", __func__);
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	if (cbor_build_frame(CTAP_CBOR_CLIENT_PIN, argv, nitems(argv),
	    &f) < 0 || fido_tx(dev, CTAP_CMD_CBOR, f.ptr, f.len) < 0) {
		fido_log_debug("%s:  fido_tx", __func__);
		r = FIDO_ERR_TX;
		goto fail;
	}

	r = FIDO_OK;
fail:
	cbor_vector_free(argv, nitems(argv));
	free(f.ptr);

	return (r);
}
#endif /* FIDO_UVTOKEN */

static int
fido_dev_get_pin_token_rx(fido_dev_t *dev, const fido_blob_t *ecdh,
    fido_blob_t *token, int ms)
{
	fido_blob_t	*aes_token = NULL;
	unsigned char	 reply[FIDO_MAXMSG];
	int		 reply_len;
	int		 r;

	if ((aes_token = fido_blob_new()) == NULL) {
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	if ((reply_len = fido_rx(dev, CTAP_CMD_CBOR, &reply, sizeof(reply),
	    ms)) < 0) {
		fido_log_debug("%s: fido_rx", __func__);
		r = FIDO_ERR_RX;
		goto fail;
	}

	if ((r = cbor_parse_reply(reply, (size_t)reply_len, aes_token,
	    parse_pintoken)) != FIDO_OK) {
		fido_log_debug("%s: parse_pintoken", __func__);
		goto fail;
	}

	if  (aes256_cbc_dec(ecdh, aes_token, token) < 0) {
		fido_log_debug("%s: aes256_cbc_dec", __func__);
		r = FIDO_ERR_RX;
		goto fail;
	}

	r = FIDO_OK;
fail:
	fido_blob_free(&aes_token);

	return (r);
}

#ifdef FIDO_UVTOKEN
static int
fido_dev_get_uv_token_rx(fido_dev_t *dev, const  fido_blob_t *ecdh,
    fido_blob_t *token, int ms)
{
	fido_blob_t	*aes_token = NULL;
	unsigned char	 reply[FIDO_MAXMSG];
	int		 reply_len;
	int		 r;

	if ((aes_token = fido_blob_new()) == NULL) {
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	if ((reply_len = fido_rx(dev, CTAP_CMD_CBOR, &reply, sizeof(reply),
	    ms)) < 0) {
		fido_log_debug("%s: fido_rx", __func__);
		r = FIDO_ERR_RX;
		goto fail;
	}

	if ((r = cbor_parse_reply(reply, (size_t)reply_len, aes_token,
	    parse_uvtoken)) != FIDO_OK) {
		fido_log_debug("%s: parse_uvtoken", __func__);
		goto fail;
	}

	if (aes256_cbc_dec(ecdh, aes_token, token) < 0) {
		fido_log_debug("%s: aes256_cbc_dec", __func__);
		r = FIDO_ERR_RX;
		goto fail;
	}

	r = FIDO_OK;
fail:
	fido_blob_free(&aes_token);

	return (r);
}
#endif /* FIDO_UVTOKEN */

static int
fido_dev_get_pin_token_wait(fido_dev_t *dev, const char *pin,
    const fido_blob_t *ecdh, const es256_pk_t *pk, fido_blob_t *token, int ms)
{
	int r;

#ifdef FIDO_UVTOKEN
	if (getenv("FIDO_UVTOKEN") != NULL) {
		if ((r = fido_dev_get_uv_token_tx(dev, pk)) != FIDO_OK ||
		    (r = fido_dev_get_uv_token_rx(dev, ecdh, token, ms)) != FIDO_OK)
			return (r);
	} else {
		if ((r = fido_dev_get_pin_token_tx(dev, pin, ecdh, pk)) != FIDO_OK ||
		    (r = fido_dev_get_pin_token_rx(dev, ecdh, token, ms)) != FIDO_OK)
			return (r);
	}
#else
	if ((r = fido_dev_get_pin_token_tx(dev, pin, ecdh, pk)) != FIDO_OK ||
	    (r = fido_dev_get_pin_token_rx(dev, ecdh, token, ms)) != FIDO_OK)
		return (r);
#endif

	return (FIDO_OK);
}

int
fido_dev_get_pin_token(fido_dev_t *dev, const char *pin,
    const fido_blob_t *ecdh, const es256_pk_t *pk, fido_blob_t *token)
{
	return (fido_dev_get_pin_token_wait(dev, pin, ecdh, pk, token, -1));
}

static int
pad64(const char *pin, fido_blob_t **ppin)
{
	size_t	pin_len;
	size_t	ppin_len;

	pin_len = strlen(pin);
	if (pin_len < 4 || pin_len > 255) {
		fido_log_debug("%s: invalid pin length", __func__);
		return (FIDO_ERR_PIN_POLICY_VIOLATION);
	}

	if ((*ppin = fido_blob_new()) == NULL)
		return (FIDO_ERR_INTERNAL);

	ppin_len = (pin_len + 63U) & ~63U;
	if (ppin_len < pin_len || ((*ppin)->ptr = calloc(1, ppin_len)) == NULL) {
		fido_blob_free(ppin);
		return (FIDO_ERR_INTERNAL);
	}

	memcpy((*ppin)->ptr, pin, pin_len);
	(*ppin)->len = ppin_len;

	return (FIDO_OK);
}

static int
fido_dev_change_pin_tx(fido_dev_t *dev, const char *pin, const char *oldpin)
{
	fido_blob_t	 f;
	fido_blob_t	*ppin = NULL;
	fido_blob_t	*ecdh = NULL;
	fido_blob_t	*opin = NULL;
	cbor_item_t	*argv[6];
	es256_pk_t	*pk = NULL;
	int r;

	memset(&f, 0, sizeof(f));
	memset(argv, 0, sizeof(argv));

	if ((opin = fido_blob_new()) == NULL || fido_blob_set(opin,
	    (const unsigned char *)oldpin, strlen(oldpin)) < 0) {
		fido_log_debug("%s: fido_blob_set", __func__);
		r = FIDO_ERR_INVALID_ARGUMENT;
		goto fail;
	}

	if ((r = pad64(pin, &ppin)) != FIDO_OK) {
		fido_log_debug("%s: pad64", __func__);
		goto fail;
	}

	if ((r = fido_do_ecdh(dev, &pk, &ecdh)) != FIDO_OK) {
		fido_log_debug("%s: fido_do_ecdh", __func__);
		goto fail;
	}

	if ((argv[0] = cbor_build_uint8(1)) == NULL ||
	    (argv[1] = cbor_build_uint8(4)) == NULL ||
	    (argv[2] = es256_pk_encode(pk, 1)) == NULL ||
	    (argv[3] = cbor_encode_change_pin_auth(ecdh, ppin, opin)) == NULL ||
	    (argv[4] = cbor_encode_pin_enc(ecdh, ppin)) == NULL ||
	    (argv[5] = cbor_encode_pin_hash_enc(ecdh, opin)) == NULL) {
		fido_log_debug("%s: cbor encode", __func__);
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	if (cbor_build_frame(CTAP_CBOR_CLIENT_PIN, argv, nitems(argv),
	    &f) < 0 || fido_tx(dev, CTAP_CMD_CBOR, f.ptr, f.len) < 0) {
		fido_log_debug("%s: fido_tx", __func__);
		r = FIDO_ERR_TX;
		goto fail;
	}

	r = FIDO_OK;
fail:
	cbor_vector_free(argv, nitems(argv));
	es256_pk_free(&pk);
	fido_blob_free(&ppin);
	fido_blob_free(&ecdh);
	fido_blob_free(&opin);
	free(f.ptr);

	return (r);

}

static int
fido_dev_set_pin_tx(fido_dev_t *dev, const char *pin)
{
	fido_blob_t	 f;
	fido_blob_t	*ppin = NULL;
	fido_blob_t	*ecdh = NULL;
	cbor_item_t	*argv[5];
	es256_pk_t	*pk = NULL;
	int		 r;

	memset(&f, 0, sizeof(f));
	memset(argv, 0, sizeof(argv));

	if ((r = pad64(pin, &ppin)) != FIDO_OK) {
		fido_log_debug("%s: pad64", __func__);
		goto fail;
	}

	if ((r = fido_do_ecdh(dev, &pk, &ecdh)) != FIDO_OK) {
		fido_log_debug("%s: fido_do_ecdh", __func__);
		goto fail;
	}

	if ((argv[0] = cbor_build_uint8(1)) == NULL ||
	    (argv[1] = cbor_build_uint8(3)) == NULL ||
	    (argv[2] = es256_pk_encode(pk, 1)) == NULL ||
	    (argv[3] = cbor_encode_set_pin_auth(ecdh, ppin)) == NULL ||
	    (argv[4] = cbor_encode_pin_enc(ecdh, ppin)) == NULL) {
		fido_log_debug("%s: cbor encode", __func__);
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	if (cbor_build_frame(CTAP_CBOR_CLIENT_PIN, argv, nitems(argv),
	    &f) < 0 || fido_tx(dev, CTAP_CMD_CBOR, f.ptr, f.len) < 0) {
		fido_log_debug("%s: fido_tx", __func__);
		r = FIDO_ERR_TX;
		goto fail;
	}

	r = FIDO_OK;
fail:
	cbor_vector_free(argv, nitems(argv));
	es256_pk_free(&pk);
	fido_blob_free(&ppin);
	fido_blob_free(&ecdh);
	free(f.ptr);

	return (r);
}

static int
fido_dev_set_pin_wait(fido_dev_t *dev, const char *pin, const char *oldpin,
    int ms)
{
	int r;

	if (oldpin != NULL) {
		if ((r = fido_dev_change_pin_tx(dev, pin, oldpin)) != FIDO_OK) {
			fido_log_debug("%s: fido_dev_change_pin_tx", __func__);
			return (r);
		}
	} else {
		if ((r = fido_dev_set_pin_tx(dev, pin)) != FIDO_OK) {
			fido_log_debug("%s: fido_dev_set_pin_tx", __func__);
			return (r);
		}
	}

	if ((r = fido_rx_cbor_status(dev, ms)) != FIDO_OK) {
		fido_log_debug("%s: fido_rx_cbor_status", __func__);
		return (r);
	}

	return (FIDO_OK);
}

int
fido_dev_set_pin(fido_dev_t *dev, const char *pin, const char *oldpin)
{
	return (fido_dev_set_pin_wait(dev, pin, oldpin, -1));
}

static int
parse_retry_count(const cbor_item_t *key, const cbor_item_t *val, void *arg)
{
	int		*retries = arg;
	uint64_t	 n;

	if (cbor_isa_uint(key) == false ||
	    cbor_int_get_width(key) != CBOR_INT_8 ||
	    cbor_get_uint8(key) != 3) {
		fido_log_debug("%s: cbor type", __func__);
		return (0); /* ignore */
	}

	if (cbor_decode_uint64(val, &n) < 0 || n > INT_MAX) {
		fido_log_debug("%s: cbor_decode_uint64", __func__);
		return (-1);
	}

	*retries = (int)n;

	return (0);
}

static int
fido_dev_get_retry_count_tx(fido_dev_t *dev)
{
	fido_blob_t	 f;
	cbor_item_t	*argv[2];
	int		 r;

	memset(&f, 0, sizeof(f));
	memset(argv, 0, sizeof(argv));

	if ((argv[0] = cbor_build_uint8(1)) == NULL ||
	    (argv[1] = cbor_build_uint8(1)) == NULL) {
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	if (cbor_build_frame(CTAP_CBOR_CLIENT_PIN, argv, nitems(argv),
	    &f) < 0 || fido_tx(dev, CTAP_CMD_CBOR, f.ptr, f.len) < 0) {
		fido_log_debug("%s: fido_tx", __func__);
		r = FIDO_ERR_TX;
		goto fail;
	}

	r = FIDO_OK;
fail:
	cbor_vector_free(argv, nitems(argv));
	free(f.ptr);

	return (r);
}

static int
fido_dev_get_retry_count_rx(fido_dev_t *dev, int *retries, int ms)
{
	unsigned char	reply[FIDO_MAXMSG];
	int		reply_len;
	int		r;

	*retries = 0;

	if ((reply_len = fido_rx(dev, CTAP_CMD_CBOR, &reply, sizeof(reply),
	    ms)) < 0) {
		fido_log_debug("%s: fido_rx", __func__);
		return (FIDO_ERR_RX);
	}

	if ((r = cbor_parse_reply(reply, (size_t)reply_len, retries,
	    parse_retry_count)) != FIDO_OK) {
		fido_log_debug("%s: parse_retry_count", __func__);
		return (r);
	}

	return (FIDO_OK);
}

static int
fido_dev_get_retry_count_wait(fido_dev_t *dev, int *retries, int ms)
{
	int r;

	if ((r = fido_dev_get_retry_count_tx(dev)) != FIDO_OK ||
	    (r = fido_dev_get_retry_count_rx(dev, retries, ms)) != FIDO_OK)
		return (r);

	return (FIDO_OK);
}

int
fido_dev_get_retry_count(fido_dev_t *dev, int *retries)
{
	return (fido_dev_get_retry_count_wait(dev, retries, -1));
}

int
cbor_add_pin_params(fido_dev_t *dev, const fido_blob_t *hmac_data,
    const es256_pk_t *pk, const fido_blob_t *ecdh, const char *pin,
    cbor_item_t **auth, cbor_item_t **opt)
{
	fido_blob_t	*token = NULL;
	int		 r;

	if ((token = fido_blob_new()) == NULL) {
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	if ((r = fido_dev_get_pin_token(dev, pin, ecdh, pk, token)) != FIDO_OK) {
		fido_log_debug("%s: fido_dev_get_pin_token", __func__);
		goto fail;
	}

	if ((*auth = cbor_encode_pin_auth(token, hmac_data)) == NULL ||
	    (*opt = cbor_encode_pin_opt()) == NULL) {
		fido_log_debug("%s: cbor encode", __func__);
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	r = FIDO_OK;
fail:
	fido_blob_free(&token);

	return (r);
}
