/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <openssl/sha.h>
#include "fido.h"

#ifndef TLS
#define TLS
#endif

typedef struct dev_manifest_func_node {
	dev_manifest_func_t manifest_func;
	struct dev_manifest_func_node *next;
} dev_manifest_func_node_t;

static TLS dev_manifest_func_node_t *manifest_funcs = NULL;
static TLS bool disable_u2f_fallback;

static void
find_manifest_func_node(dev_manifest_func_t f, dev_manifest_func_node_t **curr,
    dev_manifest_func_node_t **prev)
{
	*prev = NULL;
	*curr = manifest_funcs;

	while (*curr != NULL && (*curr)->manifest_func != f) {
		*prev = *curr;
		*curr = (*curr)->next;
	}
}

#ifdef FIDO_FUZZ
static void
set_random_report_len(fido_dev_t *dev)
{
	dev->rx_len = CTAP_MIN_REPORT_LEN +
	    uniform_random(CTAP_MAX_REPORT_LEN - CTAP_MIN_REPORT_LEN + 1);
	dev->tx_len = CTAP_MIN_REPORT_LEN +
	    uniform_random(CTAP_MAX_REPORT_LEN - CTAP_MIN_REPORT_LEN + 1);
}
#endif

static void
fido_dev_set_extension_flags(fido_dev_t *dev, const fido_cbor_info_t *info)
{
	char * const	*ptr = fido_cbor_info_extensions_ptr(info);
	size_t		 len = fido_cbor_info_extensions_len(info);

	for (size_t i = 0; i < len; i++)
		if (strcmp(ptr[i], "credProtect") == 0)
			dev->flags |= FIDO_DEV_CRED_PROT;
}

static void
fido_dev_set_option_flags(fido_dev_t *dev, const fido_cbor_info_t *info)
{
	char * const	*ptr = fido_cbor_info_options_name_ptr(info);
	const bool	*val = fido_cbor_info_options_value_ptr(info);
	size_t		 len = fido_cbor_info_options_len(info);

	for (size_t i = 0; i < len; i++)
		if (strcmp(ptr[i], "clientPin") == 0) {
			dev->flags |= val[i] ? FIDO_DEV_PIN_SET : FIDO_DEV_PIN_UNSET;
		} else if (strcmp(ptr[i], "credMgmt") == 0 ||
			   strcmp(ptr[i], "credentialMgmtPreview") == 0) {
			if (val[i])
				dev->flags |= FIDO_DEV_CREDMAN;
		} else if (strcmp(ptr[i], "uv") == 0) {
			dev->flags |= val[i] ? FIDO_DEV_UV_SET : FIDO_DEV_UV_UNSET;
		} else if (strcmp(ptr[i], "pinUvAuthToken") == 0) {
			if (val[i])
				dev->flags |= FIDO_DEV_TOKEN_PERMS;
		}
}

static void
fido_dev_set_protocol_flags(fido_dev_t *dev, const fido_cbor_info_t *info)
{
	const uint8_t	*ptr = fido_cbor_info_protocols_ptr(info);
	size_t		 len = fido_cbor_info_protocols_len(info);

	for (size_t i = 0; i < len; i++)
		switch (ptr[i]) {
		case CTAP_PIN_PROTOCOL1:
			dev->flags |= FIDO_DEV_PIN_PROTOCOL1;
			break;
		case CTAP_PIN_PROTOCOL2:
			dev->flags |= FIDO_DEV_PIN_PROTOCOL2;
			break;
		default:
			fido_log_debug("%s: unknown protocol %u", __func__,
			    ptr[i]);
			break;
		}
}

static void
fido_dev_set_flags(fido_dev_t *dev, const fido_cbor_info_t *info)
{
	fido_dev_set_extension_flags(dev, info);
	fido_dev_set_option_flags(dev, info);
	fido_dev_set_protocol_flags(dev, info);
}

static int
fido_dev_open_tx(fido_dev_t *dev, const char *path)
{
	int r;

	if (dev->io_handle != NULL) {
		fido_log_debug("%s: handle=%p", __func__, dev->io_handle);
		return (FIDO_ERR_INVALID_ARGUMENT);
	}

	if (dev->io.open == NULL || dev->io.close == NULL) {
		fido_log_debug("%s: NULL open/close", __func__);
		return (FIDO_ERR_INVALID_ARGUMENT);
	}

	if (dev->cid != CTAP_CID_BROADCAST) {
		fido_log_debug("%s: cid=0x%x", __func__, dev->cid);
		return (FIDO_ERR_INVALID_ARGUMENT);
	}

	if (fido_get_random(&dev->nonce, sizeof(dev->nonce)) < 0) {
		fido_log_debug("%s: fido_get_random", __func__);
		return (FIDO_ERR_INTERNAL);
	}

	if ((dev->io_handle = dev->io.open(path)) == NULL) {
		fido_log_debug("%s: dev->io.open", __func__);
		return (FIDO_ERR_INTERNAL);
	}

	if (dev->io_own) {
		dev->rx_len = CTAP_MAX_REPORT_LEN;
		dev->tx_len = CTAP_MAX_REPORT_LEN;
	} else {
		dev->rx_len = fido_hid_report_in_len(dev->io_handle);
		dev->tx_len = fido_hid_report_out_len(dev->io_handle);
	}

#ifdef FIDO_FUZZ
	set_random_report_len(dev);
#endif

	if (dev->rx_len < CTAP_MIN_REPORT_LEN ||
	    dev->rx_len > CTAP_MAX_REPORT_LEN) {
		fido_log_debug("%s: invalid rx_len %zu", __func__, dev->rx_len);
		r = FIDO_ERR_RX;
		goto fail;
	}

	if (dev->tx_len < CTAP_MIN_REPORT_LEN ||
	    dev->tx_len > CTAP_MAX_REPORT_LEN) {
		fido_log_debug("%s: invalid tx_len %zu", __func__, dev->tx_len);
		r = FIDO_ERR_TX;
		goto fail;
	}

	if (fido_tx(dev, CTAP_CMD_INIT, &dev->nonce, sizeof(dev->nonce)) < 0) {
		fido_log_debug("%s: fido_tx", __func__);
		r = FIDO_ERR_TX;
		goto fail;
	}

	return (FIDO_OK);
fail:
	dev->io.close(dev->io_handle);
	dev->io_handle = NULL;

	return (r);
}

static int
fido_dev_open_rx(fido_dev_t *dev, int ms)
{
	fido_cbor_info_t	*info = NULL;
	int			 reply_len;
	int			 r;

	if ((reply_len = fido_rx(dev, CTAP_CMD_INIT, &dev->attr,
	    sizeof(dev->attr), ms)) < 0) {
		fido_log_debug("%s: fido_rx", __func__);
		r = FIDO_ERR_RX;
		goto fail;
	}

#ifdef FIDO_FUZZ
	dev->attr.nonce = dev->nonce;
#endif

	if ((size_t)reply_len != sizeof(dev->attr) ||
	    dev->attr.nonce != dev->nonce) {
		fido_log_debug("%s: invalid nonce", __func__);
		r = FIDO_ERR_RX;
		goto fail;
	}

	dev->flags = 0;
	dev->cid = dev->attr.cid;

	if (fido_dev_is_fido2(dev)) {
		if ((info = fido_cbor_info_new()) == NULL) {
			fido_log_debug("%s: fido_cbor_info_new", __func__);
			r = FIDO_ERR_INTERNAL;
			goto fail;
		}
		if ((r = fido_dev_get_cbor_info_wait(dev, info,
		    ms)) != FIDO_OK) {
			fido_log_debug("%s: fido_dev_cbor_info_wait: %d",
			    __func__, r);
			if (disable_u2f_fallback)
				goto fail;
			fido_log_debug("%s: falling back to u2f", __func__);
			fido_dev_force_u2f(dev);
		} else {
			fido_dev_set_flags(dev, info);
		}
	}

	if (fido_dev_is_fido2(dev) && info != NULL) {
		dev->maxmsgsize = fido_cbor_info_maxmsgsiz(info);
		fido_log_debug("%s: FIDO_MAXMSG=%d, maxmsgsiz=%lu", __func__,
		    FIDO_MAXMSG, (unsigned long)dev->maxmsgsize);
	}

	r = FIDO_OK;
fail:
	fido_cbor_info_free(&info);

	if (r != FIDO_OK) {
		dev->io.close(dev->io_handle);
		dev->io_handle = NULL;
	}

	return (r);
}

static int
fido_dev_open_wait(fido_dev_t *dev, const char *path, int ms)
{
	int r;

#ifdef USE_WINHELLO
	if (strcmp(path, FIDO_WINHELLO_PATH) == 0)
		return (fido_winhello_open(dev));
#endif
	if ((r = fido_dev_open_tx(dev, path)) != FIDO_OK ||
	    (r = fido_dev_open_rx(dev, ms)) != FIDO_OK)
		return (r);

	return (FIDO_OK);
}

int
fido_dev_register_manifest_func(const dev_manifest_func_t f)
{
	dev_manifest_func_node_t *prev, *curr, *n;

	find_manifest_func_node(f, &curr, &prev);
	if (curr != NULL)
		return (FIDO_OK);

	if ((n = calloc(1, sizeof(*n))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		return (FIDO_ERR_INTERNAL);
	}

	n->manifest_func = f;
	n->next = manifest_funcs;
	manifest_funcs = n;

	return (FIDO_OK);
}

void
fido_dev_unregister_manifest_func(const dev_manifest_func_t f)
{
	dev_manifest_func_node_t *prev, *curr;

	find_manifest_func_node(f, &curr, &prev);
	if (curr == NULL)
		return;
	if (prev != NULL)
		prev->next = curr->next;
	else
		manifest_funcs = curr->next;

	free(curr);
}

int
fido_dev_info_manifest(fido_dev_info_t *devlist, size_t ilen, size_t *olen)
{
	dev_manifest_func_node_t	*curr = NULL;
	dev_manifest_func_t		 m_func;
	size_t				 curr_olen;
	int				 r;

	*olen = 0;

	if (fido_dev_register_manifest_func(fido_hid_manifest) != FIDO_OK)
		return (FIDO_ERR_INTERNAL);
#ifdef NFC_LINUX
	if (fido_dev_register_manifest_func(fido_nfc_manifest) != FIDO_OK)
		return (FIDO_ERR_INTERNAL);
#endif
#ifdef USE_WINHELLO
	if (fido_dev_register_manifest_func(fido_winhello_manifest) != FIDO_OK)
		return (FIDO_ERR_INTERNAL);
#endif

	for (curr = manifest_funcs; curr != NULL; curr = curr->next) {
		curr_olen = 0;
		m_func = curr->manifest_func;
		r = m_func(devlist + *olen, ilen - *olen, &curr_olen);
		if (r != FIDO_OK)
			return (r);
		*olen += curr_olen;
		if (*olen == ilen)
			break;
	}

	return (FIDO_OK);
}

int
fido_dev_open_with_info(fido_dev_t *dev)
{
	if (dev->path == NULL)
		return (FIDO_ERR_INVALID_ARGUMENT);

	return (fido_dev_open_wait(dev, dev->path, -1));
}

int
fido_dev_open(fido_dev_t *dev, const char *path)
{
#ifdef NFC_LINUX
	/*
	 * this is a hack to get existing applications up and running with nfc;
	 * it will *NOT* be part of a libfido2 release. to support nfc in your
	 * application, please change it to use fido_dev_open_with_info().
	 */
	if (strncmp(path, "/sys", strlen("/sys")) == 0 && strlen(path) > 4 &&
	    path[strlen(path) - 4] == 'n' && path[strlen(path) - 3] == 'f' &&
	    path[strlen(path) - 2] == 'c') {
		dev->io_own = true;
		dev->io = (fido_dev_io_t) {
			fido_nfc_open,
			fido_nfc_close,
			fido_nfc_read,
			fido_nfc_write,
		};
		dev->transport = (fido_dev_transport_t) {
			fido_nfc_rx,
			fido_nfc_tx,
		};
	}
#endif

	return (fido_dev_open_wait(dev, path, -1));
}

int
fido_dev_close(fido_dev_t *dev)
{
#ifdef USE_WINHELLO
	if (dev->flags & FIDO_DEV_WINHELLO)
		return (fido_winhello_close(dev));
#endif
	if (dev->io_handle == NULL || dev->io.close == NULL)
		return (FIDO_ERR_INVALID_ARGUMENT);

	dev->io.close(dev->io_handle);
	dev->io_handle = NULL;
	dev->cid = CTAP_CID_BROADCAST;

	return (FIDO_OK);
}

int
fido_dev_set_sigmask(fido_dev_t *dev, const fido_sigset_t *sigmask)
{
	if (dev->io_own || dev->io_handle == NULL || sigmask == NULL)
		return (FIDO_ERR_INVALID_ARGUMENT);

#ifdef NFC_LINUX
	if (dev->transport.rx == fido_nfc_rx)
		return (fido_nfc_set_sigmask(dev->io_handle, sigmask));
#endif
	return (fido_hid_set_sigmask(dev->io_handle, sigmask));
}

int
fido_dev_cancel(fido_dev_t *dev)
{
#ifdef USE_WINHELLO
	if (dev->flags & FIDO_DEV_WINHELLO)
		return (fido_winhello_cancel(dev));
#endif
	if (fido_dev_is_fido2(dev) == false)
		return (FIDO_ERR_INVALID_ARGUMENT);
	if (fido_tx(dev, CTAP_CMD_CANCEL, NULL, 0) < 0)
		return (FIDO_ERR_TX);

	return (FIDO_OK);
}

int
fido_dev_get_touch_begin(fido_dev_t *dev)
{
	fido_blob_t	 f;
	cbor_item_t	*argv[9];
	const char	*clientdata = FIDO_DUMMY_CLIENTDATA;
	const uint8_t	 user_id = FIDO_DUMMY_USER_ID;
	unsigned char	 cdh[SHA256_DIGEST_LENGTH];
	fido_rp_t	 rp;
	fido_user_t	 user;
	int		 r = FIDO_ERR_INTERNAL;

	memset(&f, 0, sizeof(f));
	memset(argv, 0, sizeof(argv));
	memset(cdh, 0, sizeof(cdh));
	memset(&rp, 0, sizeof(rp));
	memset(&user, 0, sizeof(user));

	if (fido_dev_is_fido2(dev) == false)
		return (u2f_get_touch_begin(dev));

	if (SHA256((const void *)clientdata, strlen(clientdata), cdh) != cdh) {
		fido_log_debug("%s: sha256", __func__);
		return (FIDO_ERR_INTERNAL);
	}

	if ((rp.id = strdup(FIDO_DUMMY_RP_ID)) == NULL ||
	    (user.name = strdup(FIDO_DUMMY_USER_NAME)) == NULL) {
		fido_log_debug("%s: strdup", __func__);
		goto fail;
	}

	if (fido_blob_set(&user.id, &user_id, sizeof(user_id)) < 0) {
		fido_log_debug("%s: fido_blob_set", __func__);
		goto fail;
	}

	if ((argv[0] = cbor_build_bytestring(cdh, sizeof(cdh))) == NULL ||
	    (argv[1] = cbor_encode_rp_entity(&rp)) == NULL ||
	    (argv[2] = cbor_encode_user_entity(&user)) == NULL ||
	    (argv[3] = cbor_encode_pubkey_param(COSE_ES256)) == NULL) {
		fido_log_debug("%s: cbor encode", __func__);
		goto fail;
	}

	if (fido_dev_supports_pin(dev)) {
		if ((argv[7] = cbor_new_definite_bytestring()) == NULL ||
		    (argv[8] = cbor_encode_pin_opt(dev)) == NULL) {
			fido_log_debug("%s: cbor encode", __func__);
			goto fail;
		}
	}

	if (cbor_build_frame(CTAP_CBOR_MAKECRED, argv, nitems(argv), &f) < 0 ||
	    fido_tx(dev, CTAP_CMD_CBOR, f.ptr, f.len) < 0) {
		fido_log_debug("%s: fido_tx", __func__);
		r = FIDO_ERR_TX;
		goto fail;
	}

	r = FIDO_OK;
fail:
	cbor_vector_free(argv, nitems(argv));
	free(f.ptr);
	free(rp.id);
	free(user.name);
	free(user.id.ptr);

	return (r);
}

int
fido_dev_get_touch_status(fido_dev_t *dev, int *touched, int ms)
{
	int r;

	*touched = 0;

	if (fido_dev_is_fido2(dev) == false)
		return (u2f_get_touch_status(dev, touched, ms));

	switch ((r = fido_rx_cbor_status(dev, ms))) {
	case FIDO_ERR_PIN_AUTH_INVALID:
	case FIDO_ERR_PIN_INVALID:
	case FIDO_ERR_PIN_NOT_SET:
	case FIDO_ERR_SUCCESS:
		*touched = 1;
		break;
	case FIDO_ERR_RX:
		/* ignore */
		break;
	default:
		fido_log_debug("%s: fido_rx_cbor_status", __func__);
		return (r);
	}

	return (FIDO_OK);
}

int
fido_dev_set_io_functions(fido_dev_t *dev, const fido_dev_io_t *io)
{
	if (dev->io_handle != NULL) {
		fido_log_debug("%s: non-NULL handle", __func__);
		return (FIDO_ERR_INVALID_ARGUMENT);
	}

	if (io == NULL || io->open == NULL || io->close == NULL ||
	    io->read == NULL || io->write == NULL) {
		fido_log_debug("%s: NULL function", __func__);
		return (FIDO_ERR_INVALID_ARGUMENT);
	}

	dev->io = *io;
	dev->io_own = true;

	return (FIDO_OK);
}

int
fido_dev_set_transport_functions(fido_dev_t *dev, const fido_dev_transport_t *t)
{
	if (dev->io_handle != NULL) {
		fido_log_debug("%s: non-NULL handle", __func__);
		return (FIDO_ERR_INVALID_ARGUMENT);
	}

	dev->transport = *t;
	dev->io_own = true;

	return (FIDO_OK);
}

void
fido_init(int flags)
{
	if (flags & FIDO_DEBUG || getenv("FIDO_DEBUG") != NULL)
		fido_log_init();

	disable_u2f_fallback = (flags & FIDO_DISABLE_U2F_FALLBACK);
}

fido_dev_t *
fido_dev_new(void)
{
	fido_dev_t *dev;

	if ((dev = calloc(1, sizeof(*dev))) == NULL)
		return (NULL);

	dev->cid = CTAP_CID_BROADCAST;
	dev->io = (fido_dev_io_t) {
		&fido_hid_open,
		&fido_hid_close,
		&fido_hid_read,
		&fido_hid_write,
	};

	return (dev);
}

fido_dev_t *
fido_dev_new_with_info(const fido_dev_info_t *di)
{
	fido_dev_t *dev;

	if ((dev = calloc(1, sizeof(*dev))) == NULL)
		return (NULL);

#if 0
	if (di->io.open == NULL || di->io.close == NULL ||
	    di->io.read == NULL || di->io.write == NULL) {
		fido_log_debug("%s: NULL function", __func__);
		fido_dev_free(&dev);
		return (NULL);
	}
#endif

	dev->io = di->io;
	dev->io_own = di->transport.tx != NULL || di->transport.rx != NULL;
	dev->transport = di->transport;
	dev->cid = CTAP_CID_BROADCAST;

	if ((dev->path = strdup(di->path)) == NULL) {
		fido_log_debug("%s: strdup", __func__);
		fido_dev_free(&dev);
		return (NULL);
	}

	return (dev);
}

void
fido_dev_free(fido_dev_t **dev_p)
{
	fido_dev_t *dev;

	if (dev_p == NULL || (dev = *dev_p) == NULL)
		return;

	free(dev->path);
	free(dev);

	*dev_p = NULL;
}

uint8_t
fido_dev_protocol(const fido_dev_t *dev)
{
	return (dev->attr.protocol);
}

uint8_t
fido_dev_major(const fido_dev_t *dev)
{
	return (dev->attr.major);
}

uint8_t
fido_dev_minor(const fido_dev_t *dev)
{
	return (dev->attr.minor);
}

uint8_t
fido_dev_build(const fido_dev_t *dev)
{
	return (dev->attr.build);
}

uint8_t
fido_dev_flags(const fido_dev_t *dev)
{
	return (dev->attr.flags);
}

bool
fido_dev_is_fido2(const fido_dev_t *dev)
{
	return (dev->attr.flags & FIDO_CAP_CBOR);
}

bool
fido_dev_is_winhello(const fido_dev_t *dev)
{
	return (dev->flags & FIDO_DEV_WINHELLO);
}

bool
fido_dev_supports_pin(const fido_dev_t *dev)
{
	return (dev->flags & (FIDO_DEV_PIN_SET|FIDO_DEV_PIN_UNSET));
}

bool
fido_dev_has_pin(const fido_dev_t *dev)
{
	return (dev->flags & FIDO_DEV_PIN_SET);
}

bool
fido_dev_supports_cred_prot(const fido_dev_t *dev)
{
	return (dev->flags & FIDO_DEV_CRED_PROT);
}

bool
fido_dev_supports_credman(const fido_dev_t *dev)
{
	return (dev->flags & FIDO_DEV_CREDMAN);
}

bool
fido_dev_supports_uv(const fido_dev_t *dev)
{
	return (dev->flags & (FIDO_DEV_UV_SET|FIDO_DEV_UV_UNSET));
}

bool
fido_dev_has_uv(const fido_dev_t *dev)
{
	return (dev->flags & FIDO_DEV_UV_SET);
}

bool
fido_dev_supports_permissions(const fido_dev_t *dev)
{
	return (dev->flags & FIDO_DEV_TOKEN_PERMS);
}

void
fido_dev_force_u2f(fido_dev_t *dev)
{
	dev->attr.flags &= (uint8_t)~FIDO_CAP_CBOR;
	dev->flags = 0;
}

void
fido_dev_force_fido2(fido_dev_t *dev)
{
	dev->attr.flags |= FIDO_CAP_CBOR;
}

uint8_t
fido_dev_get_pin_protocol(const fido_dev_t *dev)
{
	if (dev->flags & FIDO_DEV_PIN_PROTOCOL2)
		return (CTAP_PIN_PROTOCOL2);
	else if (dev->flags & FIDO_DEV_PIN_PROTOCOL1)
		return (CTAP_PIN_PROTOCOL1);

	return (0);
}

uint64_t
fido_dev_maxmsgsize(const fido_dev_t *dev)
{
	return (dev->maxmsgsize);
}
