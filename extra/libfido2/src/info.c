/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <string.h>
#include "fido.h"

static int
decode_version(const cbor_item_t *item, void *arg)
{
	fido_str_array_t	*v = arg;
	const size_t		 i = v->len;

	/* keep ptr[x] and len consistent */
	if (cbor_string_copy(item, &v->ptr[i]) < 0) {
		fido_log_debug("%s: cbor_string_copy", __func__);
		return (-1);
	}

	v->len++;

	return (0);
}

static int
decode_versions(const cbor_item_t *item, fido_str_array_t *v)
{
	v->ptr = NULL;
	v->len = 0;

	if (cbor_isa_array(item) == false ||
	    cbor_array_is_definite(item) == false) {
		fido_log_debug("%s: cbor type", __func__);
		return (-1);
	}

	v->ptr = calloc(cbor_array_size(item), sizeof(char *));
	if (v->ptr == NULL)
		return (-1);

	if (cbor_array_iter(item, v, decode_version) < 0) {
		fido_log_debug("%s: decode_version", __func__);
		return (-1);
	}

	return (0);
}

static int
decode_extension(const cbor_item_t *item, void *arg)
{
	fido_str_array_t	*e = arg;
	const size_t		 i = e->len;

	/* keep ptr[x] and len consistent */
	if (cbor_string_copy(item, &e->ptr[i]) < 0) {
		fido_log_debug("%s: cbor_string_copy", __func__);
		return (-1);
	}

	e->len++;

	return (0);
}

static int
decode_extensions(const cbor_item_t *item, fido_str_array_t *e)
{
	e->ptr = NULL;
	e->len = 0;

	if (cbor_isa_array(item) == false ||
	    cbor_array_is_definite(item) == false) {
		fido_log_debug("%s: cbor type", __func__);
		return (-1);
	}

	e->ptr = calloc(cbor_array_size(item), sizeof(char *));
	if (e->ptr == NULL)
		return (-1);

	if (cbor_array_iter(item, e, decode_extension) < 0) {
		fido_log_debug("%s: decode_extension", __func__);
		return (-1);
	}

	return (0);
}

static int
decode_aaguid(const cbor_item_t *item, unsigned char *aaguid, size_t aaguid_len)
{
	if (cbor_isa_bytestring(item) == false ||
	    cbor_bytestring_is_definite(item) == false ||
	    cbor_bytestring_length(item) != aaguid_len) {
		fido_log_debug("%s: cbor type", __func__);
		return (-1);
	}

	memcpy(aaguid, cbor_bytestring_handle(item), aaguid_len);

	return (0);
}

static int
decode_option(const cbor_item_t *key, const cbor_item_t *val, void *arg)
{
	fido_opt_array_t	*o = arg;
	const size_t		 i = o->len;

	if (cbor_isa_float_ctrl(val) == false ||
	    cbor_float_get_width(val) != CBOR_FLOAT_0 ||
	    cbor_is_bool(val) == false) {
		fido_log_debug("%s: cbor type", __func__);
		return (0); /* ignore */
	}

	if (cbor_string_copy(key, &o->name[i]) < 0) {
		fido_log_debug("%s: cbor_string_copy", __func__);
		return (0); /* ignore */
	}

	/* keep name/value and len consistent */
	o->value[i] = cbor_ctrl_value(val) == CBOR_CTRL_TRUE;
	o->len++;

	return (0);
}

static int
decode_options(const cbor_item_t *item, fido_opt_array_t *o)
{
	o->name = NULL;
	o->value = NULL;
	o->len = 0;

	if (cbor_isa_map(item) == false ||
	    cbor_map_is_definite(item) == false) {
		fido_log_debug("%s: cbor type", __func__);
		return (-1);
	}

	o->name = calloc(cbor_map_size(item), sizeof(char *));
	o->value = calloc(cbor_map_size(item), sizeof(bool));
	if (o->name == NULL || o->value == NULL)
		return (-1);

	return (cbor_map_iter(item, o, decode_option));
}

static int
decode_protocol(const cbor_item_t *item, void *arg)
{
	fido_byte_array_t	*p = arg;
	const size_t		 i = p->len;

	if (cbor_isa_uint(item) == false ||
	    cbor_int_get_width(item) != CBOR_INT_8) {
		fido_log_debug("%s: cbor type", __func__);
		return (-1);
	}

	/* keep ptr[x] and len consistent */
	p->ptr[i] = cbor_get_uint8(item);
	p->len++;

	return (0);
}

static int
decode_protocols(const cbor_item_t *item, fido_byte_array_t *p)
{
	p->ptr = NULL;
	p->len = 0;

	if (cbor_isa_array(item) == false ||
	    cbor_array_is_definite(item) == false) {
		fido_log_debug("%s: cbor type", __func__);
		return (-1);
	}

	p->ptr = calloc(cbor_array_size(item), sizeof(uint8_t));
	if (p->ptr == NULL)
		return (-1);

	if (cbor_array_iter(item, p, decode_protocol) < 0) {
		fido_log_debug("%s: decode_protocol", __func__);
		return (-1);
	}

	return (0);
}

static int
parse_reply_element(const cbor_item_t *key, const cbor_item_t *val, void *arg)
{
	fido_cbor_info_t *ci = arg;

	if (cbor_isa_uint(key) == false ||
	    cbor_int_get_width(key) != CBOR_INT_8) {
		fido_log_debug("%s: cbor type", __func__);
		return (0); /* ignore */
	}

	switch (cbor_get_uint8(key)) {
	case 1: /* versions */
		return (decode_versions(val, &ci->versions));
	case 2: /* extensions */
		return (decode_extensions(val, &ci->extensions));
	case 3: /* aaguid */
		return (decode_aaguid(val, ci->aaguid, sizeof(ci->aaguid)));
	case 4: /* options */
		return (decode_options(val, &ci->options));
	case 5: /* maxMsgSize */
		return (cbor_decode_uint64(val, &ci->maxmsgsiz));
	case 6: /* pinProtocols */
		return (decode_protocols(val, &ci->protocols));
	case 7: /* maxCredentialCountInList */
		return (cbor_decode_uint64(val, &ci->maxcredcntlst));
	case 8: /* maxCredentialIdLength */
		return (cbor_decode_uint64(val, &ci->maxcredidlen));
	case 14: /* fwVersion */
		return (cbor_decode_uint64(val, &ci->fwversion));
	default: /* ignore */
		fido_log_debug("%s: cbor type", __func__);
		return (0);
	}
}

static int
fido_dev_get_cbor_info_tx(fido_dev_t *dev)
{
	const unsigned char cbor[] = { CTAP_CBOR_GETINFO };

	fido_log_debug("%s: dev=%p", __func__, (void *)dev);

	if (fido_tx(dev, CTAP_CMD_CBOR, cbor, sizeof(cbor)) < 0) {
		fido_log_debug("%s: fido_tx", __func__);
		return (FIDO_ERR_TX);
	}

	return (FIDO_OK);
}

static int
fido_dev_get_cbor_info_rx(fido_dev_t *dev, fido_cbor_info_t *ci, int ms)
{
	unsigned char	reply[FIDO_MAXMSG];
	int		reply_len;

	fido_log_debug("%s: dev=%p, ci=%p, ms=%d", __func__, (void *)dev,
	    (void *)ci, ms);

	memset(ci, 0, sizeof(*ci));

	if ((reply_len = fido_rx(dev, CTAP_CMD_CBOR, &reply, sizeof(reply),
	    ms)) < 0) {
		fido_log_debug("%s: fido_rx", __func__);
		return (FIDO_ERR_RX);
	}

	return (cbor_parse_reply(reply, (size_t)reply_len, ci,
	    parse_reply_element));
}

int
fido_dev_get_cbor_info_wait(fido_dev_t *dev, fido_cbor_info_t *ci, int ms)
{
	int r;

	if ((r = fido_dev_get_cbor_info_tx(dev)) != FIDO_OK ||
	    (r = fido_dev_get_cbor_info_rx(dev, ci, ms)) != FIDO_OK)
		return (r);

	return (FIDO_OK);
}

int
fido_dev_get_cbor_info(fido_dev_t *dev, fido_cbor_info_t *ci)
{
	return (fido_dev_get_cbor_info_wait(dev, ci, -1));
}

/*
 * get/set functions for fido_cbor_info_t; always at the end of the file
 */

fido_cbor_info_t *
fido_cbor_info_new(void)
{
	return (calloc(1, sizeof(fido_cbor_info_t)));
}

static void
free_str_array(fido_str_array_t *sa)
{
	for (size_t i = 0; i < sa->len; i++)
		free(sa->ptr[i]);

	free(sa->ptr);
	sa->ptr = NULL;
	sa->len = 0;
}

static void
free_opt_array(fido_opt_array_t *oa)
{
	for (size_t i = 0; i < oa->len; i++)
		free(oa->name[i]);

	free(oa->name);
	free(oa->value);
	oa->name = NULL;
	oa->value = NULL;
}

static void
free_byte_array(fido_byte_array_t *ba)
{
	free(ba->ptr);

	ba->ptr = NULL;
	ba->len = 0;
}

void
fido_cbor_info_free(fido_cbor_info_t **ci_p)
{
	fido_cbor_info_t *ci;

	if (ci_p == NULL || (ci = *ci_p) ==  NULL)
		return;

	free_str_array(&ci->versions);
	free_str_array(&ci->extensions);
	free_opt_array(&ci->options);
	free_byte_array(&ci->protocols);
	free(ci);

	*ci_p = NULL;
}

char **
fido_cbor_info_versions_ptr(const fido_cbor_info_t *ci)
{
	return (ci->versions.ptr);
}

size_t
fido_cbor_info_versions_len(const fido_cbor_info_t *ci)
{
	return (ci->versions.len);
}

char **
fido_cbor_info_extensions_ptr(const fido_cbor_info_t *ci)
{
	return (ci->extensions.ptr);
}

size_t
fido_cbor_info_extensions_len(const fido_cbor_info_t *ci)
{
	return (ci->extensions.len);
}

const unsigned char *
fido_cbor_info_aaguid_ptr(const fido_cbor_info_t *ci)
{
	return (ci->aaguid);
}

size_t
fido_cbor_info_aaguid_len(const fido_cbor_info_t *ci)
{
	return (sizeof(ci->aaguid));
}

char **
fido_cbor_info_options_name_ptr(const fido_cbor_info_t *ci)
{
	return (ci->options.name);
}

const bool *
fido_cbor_info_options_value_ptr(const fido_cbor_info_t *ci)
{
	return (ci->options.value);
}

size_t
fido_cbor_info_options_len(const fido_cbor_info_t *ci)
{
	return (ci->options.len);
}

uint64_t
fido_cbor_info_maxmsgsiz(const fido_cbor_info_t *ci)
{
	return (ci->maxmsgsiz);
}

uint64_t
fido_cbor_info_maxcredcntlst(const fido_cbor_info_t *ci)
{
	return (ci->maxcredcntlst);
}

uint64_t
fido_cbor_info_maxcredidlen(const fido_cbor_info_t *ci)
{
	return (ci->maxcredidlen);
}

uint64_t
fido_cbor_info_fwversion(const fido_cbor_info_t *ci)
{
	return (ci->fwversion);
}

const uint8_t *
fido_cbor_info_protocols_ptr(const fido_cbor_info_t *ci)
{
	return (ci->protocols.ptr);
}

size_t
fido_cbor_info_protocols_len(const fido_cbor_info_t *ci)
{
	return (ci->protocols.len);
}
