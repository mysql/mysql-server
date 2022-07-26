/*
 * Copyright (c) 2020 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <sys/socket.h>

#include <linux/genetlink.h>
#include <linux/netlink.h>
#include <linux/nfc.h>

#include <errno.h>
#include <limits.h>

#include "fido.h"
#include "netlink.h"

#ifdef FIDO_FUZZ
static ssize_t (*fuzz_read)(int, void *, size_t);
static ssize_t (*fuzz_write)(int, const void *, size_t);
# define READ	fuzz_read
# define WRITE	fuzz_write
#else
# define READ	read
# define WRITE	write
#endif

#ifndef SOL_NETLINK
#define SOL_NETLINK	270
#endif

/* XXX avoid signed NLA_ALIGNTO */
#undef NLA_HDRLEN
#define NLA_HDRLEN	NLMSG_ALIGN(sizeof(struct nlattr))

typedef struct nlmsgbuf {
	size_t         siz; /* alloc size */
	size_t         len; /* of payload */
	unsigned char *ptr; /* in payload */
	union {
		struct nlmsghdr   nlmsg;
		char              buf[NLMSG_HDRLEN]; /* align */
	}              u;
	unsigned char  payload[];
} nlmsgbuf_t;

typedef struct genlmsgbuf {
	union {
		struct genlmsghdr genl;
		char              buf[GENL_HDRLEN];  /* align */
	}              u;
} genlmsgbuf_t;

typedef struct nlamsgbuf {
	size_t         siz; /* alloc size */
	size_t         len; /* of payload */
	unsigned char *ptr; /* in payload */
	union {
		struct nlattr     nla;
		char              buf[NLA_HDRLEN];   /* align */
	}              u;
	unsigned char  payload[];
} nlamsgbuf_t;

typedef struct nl_family {
	uint16_t id;
	uint32_t mcastgrp;
} nl_family_t;

typedef struct nl_poll {
	uint32_t     dev;
	unsigned int eventcnt;
} nl_poll_t;

typedef struct nl_target {
	int       found;
	uint32_t *value;
} nl_target_t;

static const void *
nlmsg_ptr(const nlmsgbuf_t *m)
{
	return (&m->u.nlmsg);
}

static size_t
nlmsg_len(const nlmsgbuf_t *m)
{
	return (m->u.nlmsg.nlmsg_len);
}

static uint16_t
nlmsg_type(const nlmsgbuf_t *m)
{
	return (m->u.nlmsg.nlmsg_type);
}

static nlmsgbuf_t *
nlmsg_new(uint16_t type, uint16_t flags, size_t len)
{
	nlmsgbuf_t *m;
	size_t siz;

	if (len > SIZE_MAX - sizeof(*m) ||
	    (siz = sizeof(*m) + len) > UINT16_MAX ||
	    (m = calloc(1, siz)) == NULL)
		return (NULL);

	m->siz = siz;
	m->len = len;
	m->ptr = m->payload;
	m->u.nlmsg.nlmsg_type = type;
	m->u.nlmsg.nlmsg_flags = NLM_F_REQUEST | flags;
	m->u.nlmsg.nlmsg_len = NLMSG_HDRLEN;

	return (m);
}

static nlamsgbuf_t *
nla_from_buf(const unsigned char **ptr, size_t *len)
{
	nlamsgbuf_t h, *a;
	size_t nlalen, skip;

	if (*len < sizeof(h.u))
		return (NULL);

	memset(&h, 0, sizeof(h));
	memcpy(&h.u, *ptr, sizeof(h.u));

	if ((nlalen = h.u.nla.nla_len) < sizeof(h.u) || nlalen > *len ||
	    nlalen - sizeof(h.u) > UINT16_MAX ||
	    nlalen > SIZE_MAX - sizeof(*a) ||
	    (skip = NLMSG_ALIGN(nlalen)) > *len ||
	    (a = calloc(1, sizeof(*a) + nlalen - sizeof(h.u))) == NULL)
		return (NULL);

	memcpy(&a->u, *ptr, nlalen);
	a->siz = sizeof(*a) + nlalen - sizeof(h.u);
	a->ptr = a->payload;
	a->len = nlalen - sizeof(h.u);
	*ptr += skip;
	*len -= skip;

	return (a);
}

static nlamsgbuf_t *
nla_getattr(nlamsgbuf_t *a)
{
	return (nla_from_buf((void *)&a->ptr, &a->len));
}

static uint16_t
nla_type(const nlamsgbuf_t *a)
{
	return (a->u.nla.nla_type);
}

static nlamsgbuf_t *
nlmsg_getattr(nlmsgbuf_t *m)
{
	return (nla_from_buf((void *)&m->ptr, &m->len));
}

static int
nla_read(nlamsgbuf_t *a, void *buf, size_t cnt)
{
	if (cnt > a->u.nla.nla_len ||
	    fido_buf_read((void *)&a->ptr, &a->len, buf, cnt) < 0)
		return (-1);

	a->u.nla.nla_len = (uint16_t)(a->u.nla.nla_len - cnt);

	return (0);
}

static nlmsgbuf_t *
nlmsg_from_buf(const unsigned char **ptr, size_t *len)
{
	nlmsgbuf_t h, *m;
	size_t msglen, skip;

	if (*len < sizeof(h.u))
		return (NULL);

	memset(&h, 0, sizeof(h));
	memcpy(&h.u, *ptr, sizeof(h.u));

	if ((msglen = h.u.nlmsg.nlmsg_len) < sizeof(h.u) || msglen > *len ||
	    msglen - sizeof(h.u) > UINT16_MAX ||
	    (skip = NLMSG_ALIGN(msglen)) > *len ||
	    (m = nlmsg_new(0, 0, msglen - sizeof(h.u))) == NULL)
		return (NULL);

	memcpy(&m->u, *ptr, msglen);
	*ptr += skip;
	*len -= skip;

	return (m);
}

static int
nlmsg_read(nlmsgbuf_t *m, void *buf, size_t cnt)
{
	if (cnt > m->u.nlmsg.nlmsg_len ||
	    fido_buf_read((void *)&m->ptr, &m->len, buf, cnt) < 0)
		return (-1);

	m->u.nlmsg.nlmsg_len = (uint32_t)(m->u.nlmsg.nlmsg_len - cnt);

	return (0);
}

static int
nlmsg_write(nlmsgbuf_t *m, const void *buf, size_t cnt)
{
	if (cnt > UINT32_MAX - m->u.nlmsg.nlmsg_len ||
	    fido_buf_write(&m->ptr, &m->len, buf, cnt) < 0)
		return (-1);

	m->u.nlmsg.nlmsg_len = (uint32_t)(m->u.nlmsg.nlmsg_len + cnt);

	return (0);
}

static int
nlmsg_set_genl(nlmsgbuf_t *m, uint8_t cmd)
{
	genlmsgbuf_t g;

	memset(&g, 0, sizeof(g));
	g.u.genl.cmd = cmd;
	g.u.genl.version = NFC_GENL_VERSION;

	return (nlmsg_write(m, &g, sizeof(g)));
}

static int
nlmsg_get_genl(nlmsgbuf_t *m, uint8_t cmd)
{
	genlmsgbuf_t g;

	memset(&g, 0, sizeof(g));

	if (nlmsg_read(m, &g, sizeof(g)) < 0 || g.u.genl.cmd != cmd)
		return (-1);

	return (0);
}

static int
nlmsg_get_status(nlmsgbuf_t *m)
{
	int status;

	if (nlmsg_read(m, &status, sizeof(status)) < 0 || status == INT_MIN)
		return (-1);
	if (status < 0)
		status = -status;

	return (status);
}

static int
nlmsg_setattr(nlmsgbuf_t *m, uint16_t type, const void *ptr, size_t len)
{
	int r;
	char *padding;
	size_t skip;
	nlamsgbuf_t a;

	if ((skip = NLMSG_ALIGN(len)) > UINT16_MAX - sizeof(a.u) ||
	    skip < len || (padding = calloc(1, skip - len)) == NULL)
		return (-1);

	memset(&a, 0, sizeof(a));
	a.u.nla.nla_type = type;
	a.u.nla.nla_len = (uint16_t)(len + sizeof(a.u));
	r = nlmsg_write(m, &a.u, sizeof(a.u)) < 0 ||
	    nlmsg_write(m, ptr, len) < 0 ||
	    nlmsg_write(m, padding, skip - len) < 0 ? -1 : 0;

	free(padding);

	return (r);
}

static int
nlmsg_set_u16(nlmsgbuf_t *m, uint16_t type, uint16_t val)
{
	return (nlmsg_setattr(m, type, &val, sizeof(val)));
}

static int
nlmsg_set_u32(nlmsgbuf_t *m, uint16_t type, uint32_t val)
{
	return (nlmsg_setattr(m, type, &val, sizeof(val)));
}

static int
nlmsg_set_str(nlmsgbuf_t *m, uint16_t type, const char *val)
{
	return (nlmsg_setattr(m, type, val, strlen(val) + 1));
}

static int
nla_get_u16(nlamsgbuf_t *a, uint16_t *v)
{
	return (nla_read(a, v, sizeof(*v)));
}

static int
nla_get_u32(nlamsgbuf_t *a, uint32_t *v)
{
	return (nla_read(a, v, sizeof(*v)));
}

static char *
nla_get_str(nlamsgbuf_t *a)
{
	size_t n;
	char *s = NULL;

	if ((n = a->len) < 1 || a->ptr[n - 1] != '\0' ||
	    (s = calloc(1, n)) == NULL || nla_read(a, s, n) < 0) {
		free(s);
		return (NULL);
	}
	s[n - 1] = '\0';

	return (s);
}

static int
nlmsg_tx(int fd, const nlmsgbuf_t *m)
{
	ssize_t r;

	if ((r = WRITE(fd, nlmsg_ptr(m), nlmsg_len(m))) == -1) {
		fido_log_error(errno, "%s: write", __func__);
		return (-1);
	}
	if (r < 0 || (size_t)r != nlmsg_len(m)) {
		fido_log_debug("%s: %zd != %zu", __func__, r, nlmsg_len(m));
		return (-1);
	}
	fido_log_xxd(nlmsg_ptr(m), nlmsg_len(m), "%s", __func__);

	return (0);
}

static ssize_t
nlmsg_rx(int fd, unsigned char *ptr, size_t len, int ms)
{
	ssize_t r;

	if (len > SSIZE_MAX) {
		fido_log_debug("%s: len", __func__);
		return (-1);
	}
	if (fido_hid_unix_wait(fd, ms, NULL) < 0) {
		fido_log_debug("%s: fido_hid_unix_wait", __func__);
		return (-1);
	}
	if ((r = READ(fd, ptr, len)) == -1) {
		fido_log_error(errno, "%s: read %zd", __func__, r);
		return (-1);
	}
	fido_log_xxd(ptr, (size_t)r, "%s", __func__);

	return (r);
}

static int
nlmsg_iter(nlmsgbuf_t *m, void *arg, int (*parser)(nlamsgbuf_t *, void *))
{
	nlamsgbuf_t *a;
	int r;

	while ((a = nlmsg_getattr(m)) != NULL) {
		r = parser(a, arg);
		free(a);
		if (r < 0) {
			fido_log_debug("%s: parser", __func__);
			return (-1);
		}
	}

	return (0);
}

static int
nla_iter(nlamsgbuf_t *g, void *arg, int (*parser)(nlamsgbuf_t *, void *))
{
	nlamsgbuf_t *a;
	int r;

	while ((a = nla_getattr(g)) != NULL) {
		r = parser(a, arg);
		free(a);
		if (r < 0) {
			fido_log_debug("%s: parser", __func__);
			return (-1);
		}
	}

	return (0);
}

static int
nl_parse_reply(const uint8_t *blob, size_t blob_len, uint16_t msg_type,
    uint8_t genl_cmd, void *arg, int (*parser)(nlamsgbuf_t *, void *))
{
	nlmsgbuf_t *m;
	int r;

	while (blob_len) {
		if ((m = nlmsg_from_buf(&blob, &blob_len)) == NULL) {
			fido_log_debug("%s: nlmsg", __func__);
			return (-1);
		}
		if (nlmsg_type(m) == NLMSG_ERROR) {
			r = nlmsg_get_status(m);
			free(m);
			return (r);
		}
		if (nlmsg_type(m) != msg_type ||
		    nlmsg_get_genl(m, genl_cmd) < 0) {
			fido_log_debug("%s: skipping", __func__);
			free(m);
			continue;
		}
		if (parser != NULL && nlmsg_iter(m, arg, parser) < 0) {
			fido_log_debug("%s: nlmsg_iter", __func__);
			free(m);
			return (-1);
		}
		free(m);
	}

	return (0);
}

static int
parse_mcastgrp(nlamsgbuf_t *a, void *arg)
{
	nl_family_t *family = arg;
	char *name;

	switch (nla_type(a)) {
	case CTRL_ATTR_MCAST_GRP_NAME:
		if ((name = nla_get_str(a)) == NULL ||
		    strcmp(name, NFC_GENL_MCAST_EVENT_NAME) != 0) {
			free(name);
			return (-1); /* XXX skip? */
		}
		free(name);
		return (0);
	case CTRL_ATTR_MCAST_GRP_ID:
		if (family->mcastgrp)
			break;
		if (nla_get_u32(a, &family->mcastgrp) < 0) {
			fido_log_debug("%s: group", __func__);
			return (-1);
		}
		return (0);
	}

	fido_log_debug("%s: ignoring nla 0x%x", __func__, nla_type(a));

	return (0);
}

static int
parse_mcastgrps(nlamsgbuf_t *a, void *arg)
{
	return (nla_iter(a, arg, parse_mcastgrp));
}

static int
parse_family(nlamsgbuf_t *a, void *arg)
{
	nl_family_t *family = arg;

	switch (nla_type(a)) {
	case CTRL_ATTR_FAMILY_ID:
		if (family->id)
			break;
		if (nla_get_u16(a, &family->id) < 0) {
			fido_log_debug("%s: id", __func__);
			return (-1);
		}
		return (0);
	case CTRL_ATTR_MCAST_GROUPS:
		return (nla_iter(a, family, parse_mcastgrps));
	}

	fido_log_debug("%s: ignoring nla 0x%x", __func__, nla_type(a));

	return (0);
}

static int
nl_get_nfc_family(int fd, uint16_t *type, uint32_t *mcastgrp)
{
	nlmsgbuf_t *m;
	uint8_t reply[512];
	nl_family_t family;
	ssize_t r;
	int ok;

	if ((m = nlmsg_new(GENL_ID_CTRL, 0, 64)) == NULL ||
	    nlmsg_set_genl(m, CTRL_CMD_GETFAMILY) < 0 ||
	    nlmsg_set_u16(m, CTRL_ATTR_FAMILY_ID, GENL_ID_CTRL) < 0 ||
	    nlmsg_set_str(m, CTRL_ATTR_FAMILY_NAME, NFC_GENL_NAME) < 0 ||
	    nlmsg_tx(fd, m) < 0) {
		free(m);
		return (-1);
	}
	free(m);
	memset(&family, 0, sizeof(family));
	if ((r = nlmsg_rx(fd, reply, sizeof(reply), -1)) < 0) {
		fido_log_debug("%s: nlmsg_rx", __func__);
		return (-1);
	}
	if ((ok = nl_parse_reply(reply, (size_t)r, GENL_ID_CTRL,
	    CTRL_CMD_NEWFAMILY, &family, parse_family)) != 0) {
		fido_log_debug("%s: nl_parse_reply: %d", __func__, ok);
		return (-1);
	}
	if (family.id == 0 || family.mcastgrp == 0) {
		fido_log_debug("%s: missing attr", __func__);
		return (-1);
	}
	*type = family.id;
	*mcastgrp = family.mcastgrp;

	return (0);
}

static int
parse_target(nlamsgbuf_t *a, void *arg)
{
	nl_target_t *t = arg;

	if (t->found || nla_type(a) != NFC_ATTR_TARGET_INDEX) {
		fido_log_debug("%s: ignoring nla 0x%x", __func__, nla_type(a));
		return (0);
	}
	if (nla_get_u32(a, t->value) < 0) {
		fido_log_debug("%s: target", __func__);
		return (-1);
	}
	t->found = 1;

	return (0);
}

int
fido_nl_power_nfc(fido_nl_t *nl, uint32_t dev)
{
	nlmsgbuf_t *m;
	uint8_t reply[512];
	ssize_t r;
	int ok;

	if ((m = nlmsg_new(nl->nfc_type, NLM_F_ACK, 64)) == NULL ||
	    nlmsg_set_genl(m, NFC_CMD_DEV_UP) < 0 ||
	    nlmsg_set_u32(m, NFC_ATTR_DEVICE_INDEX, dev) < 0 ||
	    nlmsg_tx(nl->fd, m) < 0) {
		free(m);
		return (-1);
	}
	free(m);
	if ((r = nlmsg_rx(nl->fd, reply, sizeof(reply), -1)) < 0) {
		fido_log_debug("%s: nlmsg_rx", __func__);
		return (-1);
	}
	if ((ok = nl_parse_reply(reply, (size_t)r, nl->nfc_type,
	    NFC_CMD_DEV_UP, NULL, NULL)) != 0 && ok != EALREADY) {
		fido_log_debug("%s: nl_parse_reply: %d", __func__, ok);
		return (-1);
	}

	return (0);
}

static int
nl_nfc_poll(fido_nl_t *nl, uint32_t dev)
{
	nlmsgbuf_t *m;
	uint8_t reply[512];
	ssize_t r;
	int ok;

	if ((m = nlmsg_new(nl->nfc_type, NLM_F_ACK, 64)) == NULL ||
	    nlmsg_set_genl(m, NFC_CMD_START_POLL) < 0 ||
	    nlmsg_set_u32(m, NFC_ATTR_DEVICE_INDEX, dev) < 0 ||
	    nlmsg_set_u32(m, NFC_ATTR_PROTOCOLS, NFC_PROTO_ISO14443_MASK) < 0 ||
	    nlmsg_tx(nl->fd, m) < 0) {
		free(m);
		return (-1);
	}
	free(m);
	if ((r = nlmsg_rx(nl->fd, reply, sizeof(reply), -1)) < 0) {
		fido_log_debug("%s: nlmsg_rx", __func__);
		return (-1);
	}
	if ((ok = nl_parse_reply(reply, (size_t)r, nl->nfc_type,
	    NFC_CMD_START_POLL, NULL, NULL)) != 0) {
		fido_log_debug("%s: nl_parse_reply: %d", __func__, ok);
		return (-1);
	}

	return (0);
}

static int
nl_dump_nfc_target(fido_nl_t *nl, uint32_t dev, uint32_t *target, int ms)
{
	nlmsgbuf_t *m;
	nl_target_t t;
	uint8_t reply[512];
	ssize_t r;
	int ok;

	if ((m = nlmsg_new(nl->nfc_type, NLM_F_DUMP, 64)) == NULL ||
	    nlmsg_set_genl(m, NFC_CMD_GET_TARGET) < 0 ||
	    nlmsg_set_u32(m, NFC_ATTR_DEVICE_INDEX, dev) < 0 ||
	    nlmsg_tx(nl->fd, m) < 0) {
		free(m);
		return (-1);
	}
	free(m);
	if ((r = nlmsg_rx(nl->fd, reply, sizeof(reply), ms)) < 0) {
		fido_log_debug("%s: nlmsg_rx", __func__);
		return (-1);
	}
	memset(&t, 0, sizeof(t));
	t.value = target;
	if ((ok = nl_parse_reply(reply, (size_t)r, nl->nfc_type,
	    NFC_CMD_GET_TARGET, &t, parse_target)) != 0) {
		fido_log_debug("%s: nl_parse_reply: %d", __func__, ok);
		return (-1);
	}
	if (!t.found) {
		fido_log_debug("%s: target not found", __func__);
		return (-1);
	}

	return (0);
}

static int
parse_nfc_event(nlamsgbuf_t *a, void *arg)
{
	nl_poll_t *ctx = arg;
	uint32_t dev;

	if (nla_type(a) != NFC_ATTR_DEVICE_INDEX) {
		fido_log_debug("%s: ignoring nla 0x%x", __func__, nla_type(a));
		return (0);
	}
	if (nla_get_u32(a, &dev) < 0) {
		fido_log_debug("%s: dev", __func__);
		return (-1);
	}
	if (dev == ctx->dev)
		ctx->eventcnt++;
	else
		fido_log_debug("%s: ignoring dev 0x%x", __func__, dev);

	return (0);
}

int
fido_nl_get_nfc_target(fido_nl_t *nl, uint32_t dev, uint32_t *target)
{
	uint8_t reply[512];
	nl_poll_t ctx;
	ssize_t r;
	int ok;

	if (nl_nfc_poll(nl, dev) < 0) {
		fido_log_debug("%s: nl_nfc_poll", __func__);
		return (-1);
	}
#ifndef FIDO_FUZZ
	if (setsockopt(nl->fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
	    &nl->nfc_mcastgrp, sizeof(nl->nfc_mcastgrp)) == -1) {
		fido_log_error(errno, "%s: setsockopt add", __func__);
		return (-1);
	}
#endif
	r = nlmsg_rx(nl->fd, reply, sizeof(reply), -1);
#ifndef FIDO_FUZZ
	if (setsockopt(nl->fd, SOL_NETLINK, NETLINK_DROP_MEMBERSHIP,
	    &nl->nfc_mcastgrp, sizeof(nl->nfc_mcastgrp)) == -1) {
		fido_log_error(errno, "%s: setsockopt drop", __func__);
		return (-1);
	}
#endif
	if (r < 0) {
		fido_log_debug("%s: nlmsg_rx", __func__);
		return (-1);
	}
	memset(&ctx, 0, sizeof(ctx));
	ctx.dev = dev;
	if ((ok = nl_parse_reply(reply, (size_t)r, nl->nfc_type,
	    NFC_EVENT_TARGETS_FOUND, &ctx, parse_nfc_event)) != 0) {
		fido_log_debug("%s: nl_parse_reply: %d", __func__, ok);
		return (-1);
	}
	if (ctx.eventcnt == 0) {
		fido_log_debug("%s: dev 0x%x not observed", __func__, dev);
		return (-1);
	}
	if (nl_dump_nfc_target(nl, dev, target, -1) < 0) {
		fido_log_debug("%s: nl_dump_nfc_target", __func__);
		return (-1);
	}

	return (0);
}

void
fido_nl_free(fido_nl_t **nlp)
{
	fido_nl_t *nl;

	if (nlp == NULL || (nl = *nlp) == NULL)
		return;
	if (nl->fd != -1 && close(nl->fd) == -1)
		fido_log_error(errno, "%s: close", __func__);

	free(nl);
	*nlp = NULL;
}

fido_nl_t *
fido_nl_new(void)
{
	fido_nl_t *nl;
	int ok = -1;

	if ((nl = calloc(1, sizeof(*nl))) == NULL)
		return (NULL);
	if ((nl->fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC,
	    NETLINK_GENERIC)) == -1) {
		fido_log_error(errno, "%s: socket", __func__);
		goto fail;
	}
	nl->saddr.nl_family = AF_NETLINK;
	if (bind(nl->fd, (struct sockaddr *)&nl->saddr,
	    sizeof(nl->saddr)) == -1) {
		fido_log_error(errno, "%s: bind", __func__);
		goto fail;
	}
	if (nl_get_nfc_family(nl->fd, &nl->nfc_type, &nl->nfc_mcastgrp) < 0) {
		fido_log_debug("%s: nl_get_nfc_family", __func__);
		goto fail;
	}

	ok = 0;
fail:
	if (ok < 0)
		fido_nl_free(&nl);

	return (nl);
}

#ifdef FIDO_FUZZ
void
set_netlink_io_functions(ssize_t (*read_f)(int, void *, size_t),
    ssize_t (*write_f)(int, const void *, size_t))
{
	fuzz_read = read_f;
	fuzz_write = write_f;
}
#endif
