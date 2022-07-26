/*
 * Copyright (c) 2020 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include <linux/nfc.h>

#include <errno.h>
#include <libudev.h>
#include <signal.h>
#include <unistd.h>

#include "fido.h"
#include "fido/param.h"
#include "netlink.h"
#include "iso7816.h"

#define TX_CHUNK_SIZE	240

static const uint8_t aid[] = { 0xa0, 0x00, 0x00, 0x06, 0x47, 0x2f, 0x00, 0x01 };
static const uint8_t v_u2f[] = { 'U', '2', 'F', '_', 'V', '2' };
static const uint8_t v_fido[] = { 'F', 'I', 'D', 'O', '_', '2', '_', '0' };

struct nfc_linux {
	int             fd;
	uint32_t        dev;
	uint32_t        target;
	sigset_t	sigmask;
	const sigset_t *sigmaskp;
	struct fido_nl *nl;
};

static int
tx_short_apdu(fido_dev_t *d, const iso7816_header_t *h, const uint8_t *payload,
    uint8_t payload_len, uint8_t cla_flags)
{
	uint8_t apdu[5 + UINT8_MAX + 1];
	uint8_t sw[2];
	size_t apdu_len;
	int ok = -1;

	memset(&apdu, 0, sizeof(apdu));
	apdu[0] = h->cla | cla_flags;
	apdu[1] = h->ins;
	apdu[2] = h->p1;
	apdu[3] = h->p2;
	apdu[4] = payload_len;
	memcpy(&apdu[5], payload, payload_len);
	apdu_len = (size_t)(5 + payload_len + 1);

	if (d->io.write(d->io_handle, apdu, apdu_len) < 0) {
		fido_log_debug("%s: write", __func__);
		goto fail;
	}

	if (cla_flags & 0x10) {
		if (d->io.read(d->io_handle, sw, sizeof(sw), -1) != 2) {
			fido_log_debug("%s: read", __func__);
			goto fail;
		}
		if ((sw[0] << 8 | sw[1]) != SW_NO_ERROR) {
			fido_log_debug("%s: unexpected sw", __func__);
			goto fail;
		}
	}

	ok = 0;
fail:
	explicit_bzero(apdu, sizeof(apdu));

	return (ok);
}

static int
nfc_do_tx(fido_dev_t *d, const uint8_t *apdu_ptr, size_t apdu_len)
{
	iso7816_header_t h;

	if (fido_buf_read(&apdu_ptr, &apdu_len, &h, sizeof(h)) < 0) {
		fido_log_debug("%s: header", __func__);
		return (-1);
	}
	if (apdu_len < 2) {
		fido_log_debug("%s: apdu_len %zu", __func__, apdu_len);
		return (-1);
	}

	apdu_len -= 2; /* trim le1 le2 */

	while (apdu_len > TX_CHUNK_SIZE) {
		if (tx_short_apdu(d, &h, apdu_ptr, TX_CHUNK_SIZE, 0x10) < 0) {
			fido_log_debug("%s: chain", __func__);
			return (-1);
		}
		apdu_ptr += TX_CHUNK_SIZE;
		apdu_len -= TX_CHUNK_SIZE;
	}

	if (tx_short_apdu(d, &h, apdu_ptr, (uint8_t)apdu_len, 0) < 0) {
		fido_log_debug("%s: tx_short_apdu", __func__);
		return (-1);
	}
 
	return (0);
}

int
fido_nfc_tx(fido_dev_t *d, uint8_t cmd, const unsigned char *buf, size_t count)
{
	iso7816_apdu_t *apdu = NULL;
	const uint8_t *ptr;
	size_t len;
	int ok = -1;

	switch (cmd) {
	case CTAP_CMD_INIT: /* select */
		if ((apdu = iso7816_new(0, 0xa4, 0x04, sizeof(aid))) == NULL ||
		    iso7816_add(apdu, aid, sizeof(aid)) < 0) {
			fido_log_debug("%s: iso7816", __func__);
			goto fail;
		}
		break;
	case CTAP_CMD_CBOR: /* wrap cbor */
		if (count > UINT16_MAX || (apdu = iso7816_new(0x80, 0x10, 0x80,
		    (uint16_t)count)) == NULL ||
		    iso7816_add(apdu, buf, count) < 0) {
			fido_log_debug("%s: iso7816", __func__);
			goto fail;
		}
		break;
	case CTAP_CMD_MSG: /* already an apdu */
		break;
	default:
		fido_log_debug("%s: cmd=%02x", __func__, cmd);
		goto fail;
	}

	if (apdu != NULL) {
		ptr = iso7816_ptr(apdu);
		len = iso7816_len(apdu);
	} else {
		ptr = buf;
		len = count;
	}

	if (nfc_do_tx(d, ptr, len) < 0) {
		fido_log_debug("%s: nfc_do_tx", __func__);
		goto fail;
	}

	ok = 0;
fail:
	iso7816_free(&apdu);

	return (ok);
}

static int
rx_init(fido_dev_t *d, unsigned char *buf, size_t count, int ms)
{
	fido_ctap_info_t *attr = (fido_ctap_info_t *)buf;
	uint8_t f[64];
	int n;

	if (count != sizeof(*attr)) {
		fido_log_debug("%s: count=%zu", __func__, count);
		return (-1);
	}

	memset(attr, 0, sizeof(*attr));

	if ((n = d->io.read(d->io_handle, f, sizeof(f), ms)) < 2 ||
	    (f[n - 2] << 8 | f[n - 1]) != SW_NO_ERROR) {
		fido_log_debug("%s: read", __func__);
		return (-1);
	}

	n -= 2;

	if (n == sizeof(v_u2f) && memcmp(f, v_u2f, sizeof(v_u2f)) == 0)
		attr->flags = FIDO_CAP_CBOR;
	else if (n == sizeof(v_fido) && memcmp(f, v_fido, sizeof(v_fido)) == 0)
		attr->flags = FIDO_CAP_CBOR | FIDO_CAP_NMSG;
	else {
		fido_log_debug("%s: unknown version string", __func__);
#ifdef FIDO_FUZZ
		attr->flags = FIDO_CAP_CBOR | FIDO_CAP_NMSG;
#else
		return (-1);
#endif
	}

	memcpy(&attr->nonce, &d->nonce, sizeof(attr->nonce)); /* XXX */

	return ((int)count);
}

static int
tx_get_response(fido_dev_t *d, uint8_t count)
{
	uint8_t apdu[5];

	memset(apdu, 0, sizeof(apdu));
	apdu[1] = 0xc0; /* GET_RESPONSE */
	apdu[4] = count;

	if (d->io.write(d->io_handle, apdu, sizeof(apdu)) < 0) {
		fido_log_debug("%s: write", __func__);
		return (-1);
	}

	return (0);
}

static int
rx_apdu(fido_dev_t *d, uint8_t sw[2], unsigned char **buf, size_t *count, int ms)
{
	uint8_t f[256 + 2];
	int n, ok = -1;

	if ((n = d->io.read(d->io_handle, f, sizeof(f), ms)) < 2) {
		fido_log_debug("%s: read", __func__);
		goto fail;
	}

	if (fido_buf_write(buf, count, f, (size_t)(n - 2)) < 0) {
		fido_log_debug("%s: fido_buf_write", __func__);
		goto fail;
	}

	memcpy(sw, f + n - 2, 2);

	ok = 0;
fail:
	explicit_bzero(f, sizeof(f));

	return (ok);
}

static int
rx_msg(fido_dev_t *d, unsigned char *buf, size_t count, int ms)
{
	uint8_t sw[2];
	const size_t bufsiz = count;

	if (rx_apdu(d, sw, &buf, &count, ms) < 0) {
		fido_log_debug("%s: preamble", __func__);
		return (-1);
	}

	while (sw[0] == SW1_MORE_DATA)
		if (tx_get_response(d, sw[1]) < 0 ||
		    rx_apdu(d, sw, &buf, &count, ms) < 0) {
			fido_log_debug("%s: chain", __func__);
			return (-1);
		}

	if (fido_buf_write(&buf, &count, sw, sizeof(sw)) < 0) {
		fido_log_debug("%s: sw", __func__);
		return (-1);
	}

	if (bufsiz - count > INT_MAX) {
		fido_log_debug("%s: bufsiz", __func__);
		return (-1);
	}

	return ((int)(bufsiz - count));
}

static int
rx_cbor(fido_dev_t *d, unsigned char *buf, size_t count, int ms)
{
	int r;

	if ((r = rx_msg(d, buf, count, ms)) < 2)
		return (-1);

	return (r - 2);
}

int
fido_nfc_rx(fido_dev_t *d, uint8_t cmd, unsigned char *buf, size_t count, int ms)
{
	switch (cmd) {
	case CTAP_CMD_INIT:
		return (rx_init(d, buf, count, ms));
	case CTAP_CMD_CBOR:
		return (rx_cbor(d, buf, count, ms));
	case CTAP_CMD_MSG:
		return (rx_msg(d, buf, count, ms));
	default:
		fido_log_debug("%s: cmd=%02x", __func__, cmd);
		return (-1);
	}
}

static char *
get_parent_attr(struct udev_device *dev, const char *subsystem,
    const char *devtype, const char *attr)
{
	struct udev_device *parent;
	const char *value;

	if ((parent = udev_device_get_parent_with_subsystem_devtype(dev,
	    subsystem, devtype)) == NULL || (value =
	    udev_device_get_sysattr_value(parent, attr)) == NULL)
		return (NULL);

	return (strdup(value));
}

static char *
get_usb_attr(struct udev_device *dev, const char *attr)
{
	return (get_parent_attr(dev, "usb", "usb_device", attr));
}

static int
to_int(const char *str, int base)
{
	char *ep;
	long long ll;

	ll = strtoll(str, &ep, base);
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

static int
copy_info(fido_dev_info_t *di, struct udev *udev,
    struct udev_list_entry *udev_entry)
{
	const char *name;
	char *str;
	struct udev_device *dev = NULL;
	int id, ok = -1;

	memset(di, 0, sizeof(*di));

	if ((name = udev_list_entry_get_name(udev_entry)) == NULL ||
	    (dev = udev_device_new_from_syspath(udev, name)) == NULL)
		goto fail;

	if ((di->path = strdup(name)) == NULL ||
	    (di->manufacturer = get_usb_attr(dev, "manufacturer")) == NULL ||
	    (di->product = get_usb_attr(dev, "product")) == NULL)
		goto fail;

	/* XXX assumes USB for vendor/product info */
	if ((str = get_usb_attr(dev, "idVendor")) != NULL &&
	    (id = to_int(str, 16)) > 0 && id <= UINT16_MAX)
		di->vendor_id = (int16_t)id;
	free(str);

	if ((str = get_usb_attr(dev, "idProduct")) != NULL &&
	    (id = to_int(str, 16)) > 0 && id <= UINT16_MAX)
		di->product_id = (int16_t)id;
	free(str);

	ok = 0;
fail:
	if (dev != NULL)
		udev_device_unref(dev);

	if (ok < 0) {
		free(di->path);
		free(di->manufacturer);
		free(di->product);
		explicit_bzero(di, sizeof(*di));
	}

	return (ok);
}

static int
sysnum_from_syspath(const char *path)
{
	struct udev *udev = NULL;
	struct udev_device *dev = NULL;
	const char *str;
	int idx;

	if ((udev = udev_new()) == NULL ||
	    (dev = udev_device_new_from_syspath(udev, path)) == NULL ||
	    (str = udev_device_get_sysnum(dev)) == NULL)
		idx = -1;
	else
		idx = to_int(str, 10);

	if (dev != NULL)
		udev_device_unref(dev);
	if (udev != NULL)
		udev_unref(udev);

	return (idx);
}

int
fido_nfc_manifest(fido_dev_info_t *devlist, size_t ilen, size_t *olen)
{
	struct udev *udev = NULL;
	struct udev_enumerate *udev_enum = NULL;
	struct udev_list_entry *udev_list;
	struct udev_list_entry *udev_entry;
	int r = FIDO_ERR_INTERNAL;

	*olen = 0;

	if (ilen == 0)
		return (FIDO_OK);

	if (devlist == NULL)
		return (FIDO_ERR_INVALID_ARGUMENT);

	if ((udev = udev_new()) == NULL ||
	    (udev_enum = udev_enumerate_new(udev)) == NULL)
		goto fail;

	if (udev_enumerate_add_match_subsystem(udev_enum, "nfc") < 0 ||
	    udev_enumerate_scan_devices(udev_enum) < 0)
		goto fail;

	if ((udev_list = udev_enumerate_get_list_entry(udev_enum)) == NULL) {
		r = FIDO_OK; /* zero nfc devices */
		goto fail;
	}

	udev_list_entry_foreach(udev_entry, udev_list) {
		if (copy_info(&devlist[*olen], udev, udev_entry) == 0) {
			devlist[*olen].io = (fido_dev_io_t) {
				fido_nfc_open,
				fido_nfc_close,
				fido_nfc_read,
				fido_nfc_write,
			};
			devlist[*olen].transport = (fido_dev_transport_t) {
				fido_nfc_rx,
				fido_nfc_tx,
			};
			if (++(*olen) == ilen)
				break;
		}
	}

	r = FIDO_OK;
fail:
	if (udev_enum != NULL)
		udev_enumerate_unref(udev_enum);
	if (udev != NULL)
		udev_unref(udev);

	return (r);
}

static int
nfc_target_connect(struct nfc_linux *ctx)
{
	struct sockaddr_nfc sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_family = AF_NFC;
	sa.dev_idx = ctx->dev;
	sa.target_idx = ctx->target;
	sa.nfc_protocol = NFC_PROTO_ISO14443;

	if ((ctx->fd = socket(AF_NFC, SOCK_SEQPACKET | SOCK_CLOEXEC,
	    NFC_SOCKPROTO_RAW)) == -1) {
		fido_log_error(errno, "%s: socket", __func__);
		return (-1);
	}
	if (connect(ctx->fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
		fido_log_error(errno, "%s: connect", __func__);
		if (close(ctx->fd) == -1)
			fido_log_error(errno, "%s: close", __func__);
		ctx->fd = -1;
		return (-1);
	}

	return (0);
}

static void
nfc_free(struct nfc_linux **ctx_p)
{
	struct nfc_linux *ctx;

	if (ctx_p == NULL || (ctx = *ctx_p) == NULL)
		return;
	if (ctx->fd != -1 && close(ctx->fd) == -1)
		fido_log_error(errno, "%s: close", __func__);
	if (ctx->nl != NULL)
		fido_nl_free(&ctx->nl);

	free(ctx);
	*ctx_p = NULL;
}

static struct nfc_linux *
nfc_new(uint32_t dev)
{
	struct nfc_linux *ctx;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL ||
	    (ctx->nl = fido_nl_new()) == NULL) {
		nfc_free(&ctx);
		return (NULL);
	}

	ctx->fd = -1;
	ctx->dev = dev;

	return (ctx);
}

void *
fido_nfc_open(const char *path)
{
	struct nfc_linux *ctx = NULL;
	int idx;

	if ((idx = sysnum_from_syspath(path)) < 0 ||
	    (ctx = nfc_new((uint32_t)idx)) == NULL) {
		fido_log_debug("%s: nfc_new", __func__);
		goto fail;
	}
	if (fido_nl_power_nfc(ctx->nl, ctx->dev) < 0 ||
	    fido_nl_get_nfc_target(ctx->nl, ctx->dev, &ctx->target) < 0 ||
	    nfc_target_connect(ctx) < 0) {
		fido_log_debug("%s: netlink", __func__);
		goto fail;
	}

	return (ctx);
fail:
	nfc_free(&ctx);
	return (NULL);
}

void
fido_nfc_close(void *handle)
{
	struct nfc_linux *ctx = handle;

	nfc_free(&ctx);
}

int
fido_nfc_set_sigmask(void *handle, const fido_sigset_t *sigmask)
{
	struct nfc_linux *ctx = handle;

	ctx->sigmask = *sigmask;
	ctx->sigmaskp = &ctx->sigmask;

	return (FIDO_OK);
}

int
fido_nfc_read(void *handle, unsigned char *buf, size_t len, int ms)
{
	struct nfc_linux *ctx = handle;
	struct iovec iov[2];
	uint8_t preamble;
	ssize_t	r;

	memset(&iov, 0, sizeof(iov));
	iov[0].iov_base = &preamble;
	iov[0].iov_len = sizeof(preamble);
	iov[1].iov_base = buf;
	iov[1].iov_len = len;

	if (fido_hid_unix_wait(ctx->fd, ms, ctx->sigmaskp) < 0) {
		fido_log_debug("%s: fido_hid_unix_wait", __func__);
		return (-1);
	}
	if ((r = readv(ctx->fd, iov, nitems(iov))) == -1) {
		fido_log_error(errno, "%s: read", __func__);
		return (-1);
	}
	if (r < 1) {
		fido_log_debug("%s: %zd < 1", __func__, r);
		return (-1);
	}
	if (preamble != 0x00) {
		fido_log_debug("%s: preamble", __func__);
		return (-1);
	}

	r--;
	fido_log_xxd(buf, (size_t)r, "%s", __func__);

	return ((int)r);
}

int
fido_nfc_write(void *handle, const unsigned char *buf, size_t len)
{
	struct nfc_linux *ctx = handle;
	ssize_t	r;

	fido_log_xxd(buf, len, "%s", __func__);

	if (len > INT_MAX) {
		fido_log_debug("%s: len", __func__);
		return (-1);
	}
	if ((r = write(ctx->fd, buf, len)) == -1) {
		fido_log_error(errno, "%s: write", __func__);
		return (-1);
	}
	if (r < 0 || (size_t)r != len) {
		fido_log_debug("%s: %zd != %zu", __func__, r, len);
		return (-1);
	}

	return ((int)r);
}
