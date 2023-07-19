/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/

/* WIP, experimental: use recvmmsg() on linux
 * we have no configure check, yet
 * and also it is only available for _GNU_SOURCE, which
 * we do not use otherwise.
#define HAVE_SENDMMSG
 */
#if defined(HAVE_SENDMMSG)
#define _GNU_SOURCE
#include <sys/socket.h>
#undef _GNU_SOURCE
#endif

#include "curl_setup.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "urldata.h"
#include "bufq.h"
#include "dynbuf.h"
#include "cfilters.h"
#include "curl_log.h"
#include "curl_msh3.h"
#include "curl_ngtcp2.h"
#include "curl_quiche.h"
#include "vquic.h"
#include "vquic_int.h"

/* The last 3 #include files should be in this order */
#include "curl_printf.h"
#include "curl_memory.h"
#include "memdebug.h"


#ifdef ENABLE_QUIC

#ifdef O_BINARY
#define QLOGMODE O_WRONLY|O_CREAT|O_BINARY
#else
#define QLOGMODE O_WRONLY|O_CREAT
#endif

#define NW_CHUNK_SIZE     (64 * 1024)
#define NW_SEND_CHUNKS    2


void Curl_quic_ver(char *p, size_t len)
{
#if defined(USE_NGTCP2) && defined(USE_NGHTTP3)
  Curl_ngtcp2_ver(p, len);
#elif defined(USE_QUICHE)
  Curl_quiche_ver(p, len);
#elif defined(USE_MSH3)
  Curl_msh3_ver(p, len);
#endif
}

CURLcode vquic_ctx_init(struct cf_quic_ctx *qctx)
{
  Curl_bufq_init2(&qctx->sendbuf, NW_CHUNK_SIZE, NW_SEND_CHUNKS,
                  BUFQ_OPT_SOFT_LIMIT);
#if defined(__linux__) && defined(UDP_SEGMENT) && defined(HAVE_SENDMSG)
  qctx->no_gso = FALSE;
#else
  qctx->no_gso = TRUE;
#endif

  return CURLE_OK;
}

void vquic_ctx_free(struct cf_quic_ctx *qctx)
{
  Curl_bufq_free(&qctx->sendbuf);
}

static CURLcode send_packet_no_gso(struct Curl_cfilter *cf,
                                   struct Curl_easy *data,
                                   struct cf_quic_ctx *qctx,
                                   const uint8_t *pkt, size_t pktlen,
                                   size_t gsolen, size_t *psent);

static CURLcode do_sendmsg(struct Curl_cfilter *cf,
                           struct Curl_easy *data,
                           struct cf_quic_ctx *qctx,
                           const uint8_t *pkt, size_t pktlen, size_t gsolen,
                           size_t *psent)
{
#ifdef HAVE_SENDMSG
  struct iovec msg_iov;
  struct msghdr msg = {0};
  ssize_t sent;
#if defined(__linux__) && defined(UDP_SEGMENT)
  uint8_t msg_ctrl[32];
  struct cmsghdr *cm;
#endif

  *psent = 0;
  msg_iov.iov_base = (uint8_t *)pkt;
  msg_iov.iov_len = pktlen;
  msg.msg_iov = &msg_iov;
  msg.msg_iovlen = 1;

#if defined(__linux__) && defined(UDP_SEGMENT)
  if(pktlen > gsolen) {
    /* Only set this, when we need it. macOS, for example,
     * does not seem to like a msg_control of length 0. */
    msg.msg_control = msg_ctrl;
    assert(sizeof(msg_ctrl) >= CMSG_SPACE(sizeof(uint16_t)));
    msg.msg_controllen = CMSG_SPACE(sizeof(uint16_t));
    cm = CMSG_FIRSTHDR(&msg);
    cm->cmsg_level = SOL_UDP;
    cm->cmsg_type = UDP_SEGMENT;
    cm->cmsg_len = CMSG_LEN(sizeof(uint16_t));
    *(uint16_t *)(void *)CMSG_DATA(cm) = gsolen & 0xffff;
  }
#endif


  while((sent = sendmsg(qctx->sockfd, &msg, 0)) == -1 && SOCKERRNO == EINTR)
    ;

  if(sent == -1) {
    switch(SOCKERRNO) {
    case EAGAIN:
#if EAGAIN != EWOULDBLOCK
    case EWOULDBLOCK:
#endif
      return CURLE_AGAIN;
    case EMSGSIZE:
      /* UDP datagram is too large; caused by PMTUD. Just let it be lost. */
      break;
    case EIO:
      if(pktlen > gsolen) {
        /* GSO failure */
        failf(data, "sendmsg() returned %zd (errno %d); disable GSO", sent,
              SOCKERRNO);
        qctx->no_gso = TRUE;
        return send_packet_no_gso(cf, data, qctx, pkt, pktlen, gsolen, psent);
      }
      /* FALLTHROUGH */
    default:
      failf(data, "sendmsg() returned %zd (errno %d)", sent, SOCKERRNO);
      return CURLE_SEND_ERROR;
    }
  }
  else {
    assert(pktlen == (size_t)sent);
  }
#else
  ssize_t sent;
  (void)gsolen;

  *psent = 0;

  while((sent = send(qctx->sockfd,
                     (const char *)pkt, (SEND_TYPE_ARG3)pktlen, 0)) == -1 &&
        SOCKERRNO == EINTR)
    ;

  if(sent == -1) {
    if(SOCKERRNO == EAGAIN || SOCKERRNO == EWOULDBLOCK) {
      return CURLE_AGAIN;
    }
    else {
      failf(data, "send() returned %zd (errno %d)", sent, SOCKERRNO);
      if(SOCKERRNO != EMSGSIZE) {
        return CURLE_SEND_ERROR;
      }
      /* UDP datagram is too large; caused by PMTUD. Just let it be
         lost. */
    }
  }
#endif
  (void)cf;
  *psent = pktlen;

  return CURLE_OK;
}

static CURLcode send_packet_no_gso(struct Curl_cfilter *cf,
                                   struct Curl_easy *data,
                                   struct cf_quic_ctx *qctx,
                                   const uint8_t *pkt, size_t pktlen,
                                   size_t gsolen, size_t *psent)
{
  const uint8_t *p, *end = pkt + pktlen;
  size_t sent;

  *psent = 0;

  for(p = pkt; p < end; p += gsolen) {
    size_t len = CURLMIN(gsolen, (size_t)(end - p));
    CURLcode curlcode = do_sendmsg(cf, data, qctx, p, len, len, &sent);
    if(curlcode != CURLE_OK) {
      return curlcode;
    }
    *psent += sent;
  }

  return CURLE_OK;
}

static CURLcode vquic_send_packets(struct Curl_cfilter *cf,
                                   struct Curl_easy *data,
                                   struct cf_quic_ctx *qctx,
                                   const uint8_t *pkt, size_t pktlen,
                                   size_t gsolen, size_t *psent)
{
  if(qctx->no_gso && pktlen > gsolen) {
    return send_packet_no_gso(cf, data, qctx, pkt, pktlen, gsolen, psent);
  }

  return do_sendmsg(cf, data, qctx, pkt, pktlen, gsolen, psent);
}

CURLcode vquic_flush(struct Curl_cfilter *cf, struct Curl_easy *data,
                     struct cf_quic_ctx *qctx)
{
  const unsigned char *buf;
  size_t blen, sent;
  CURLcode result;
  size_t gsolen;

  while(Curl_bufq_peek(&qctx->sendbuf, &buf, &blen)) {
    gsolen = qctx->gsolen;
    if(qctx->split_len) {
      gsolen = qctx->split_gsolen;
      if(blen > qctx->split_len)
        blen = qctx->split_len;
    }

    DEBUGF(LOG_CF(data, cf, "vquic_send(len=%zu, gso=%zu)",
                  blen, gsolen));
    result = vquic_send_packets(cf, data, qctx, buf, blen, gsolen, &sent);
    DEBUGF(LOG_CF(data, cf, "vquic_send(len=%zu, gso=%zu) -> %d, sent=%zu",
                  blen, gsolen, result, sent));
    if(result) {
      if(result == CURLE_AGAIN) {
        Curl_bufq_skip(&qctx->sendbuf, sent);
        if(qctx->split_len)
          qctx->split_len -= sent;
      }
      return result;
    }
    Curl_bufq_skip(&qctx->sendbuf, sent);
    if(qctx->split_len)
      qctx->split_len -= sent;
  }
  return CURLE_OK;
}

CURLcode vquic_send(struct Curl_cfilter *cf, struct Curl_easy *data,
                        struct cf_quic_ctx *qctx, size_t gsolen)
{
  qctx->gsolen = gsolen;
  return vquic_flush(cf, data, qctx);
}

CURLcode vquic_send_tail_split(struct Curl_cfilter *cf, struct Curl_easy *data,
                               struct cf_quic_ctx *qctx, size_t gsolen,
                               size_t tail_len, size_t tail_gsolen)
{
  DEBUGASSERT(Curl_bufq_len(&qctx->sendbuf) > tail_len);
  qctx->split_len = Curl_bufq_len(&qctx->sendbuf) - tail_len;
  qctx->split_gsolen = gsolen;
  qctx->gsolen = tail_gsolen;
  DEBUGF(LOG_CF(data, cf, "vquic_send_tail_split: [%zu gso=%zu][%zu gso=%zu]",
                qctx->split_len, qctx->split_gsolen,
                tail_len, qctx->gsolen));
  return vquic_flush(cf, data, qctx);
}

#ifdef HAVE_SENDMMSG
static CURLcode recvmmsg_packets(struct Curl_cfilter *cf,
                                 struct Curl_easy *data,
                                 struct cf_quic_ctx *qctx,
                                 size_t max_pkts,
                                 vquic_recv_pkt_cb *recv_cb, void *userp)
{
#define MMSG_NUM  64
  struct iovec msg_iov[MMSG_NUM];
  struct mmsghdr mmsg[MMSG_NUM];
  uint8_t bufs[MMSG_NUM][2*1024];
  struct sockaddr_storage remote_addr[MMSG_NUM];
  size_t total_nread, pkts;
  int mcount, i, n;
  CURLcode result = CURLE_OK;

  DEBUGASSERT(max_pkts > 0);
  pkts = 0;
  total_nread = 0;
  while(pkts < max_pkts) {
    n = (int)CURLMIN(MMSG_NUM, max_pkts);
    memset(&mmsg, 0, sizeof(mmsg));
    for(i = 0; i < n; ++i) {
      msg_iov[i].iov_base = bufs[i];
      msg_iov[i].iov_len = (int)sizeof(bufs[i]);
      mmsg[i].msg_hdr.msg_iov = &msg_iov[i];
      mmsg[i].msg_hdr.msg_iovlen = 1;
      mmsg[i].msg_hdr.msg_name = &remote_addr[i];
      mmsg[i].msg_hdr.msg_namelen = sizeof(remote_addr[i]);
    }

    while((mcount = recvmmsg(qctx->sockfd, mmsg, n, 0, NULL)) == -1 &&
          SOCKERRNO == EINTR)
      ;
    if(mcount == -1) {
      if(SOCKERRNO == EAGAIN || SOCKERRNO == EWOULDBLOCK) {
        DEBUGF(LOG_CF(data, cf, "ingress, recvmmsg -> EAGAIN"));
        goto out;
      }
      if(!cf->connected && SOCKERRNO == ECONNREFUSED) {
        const char *r_ip;
        int r_port;
        Curl_cf_socket_peek(cf->next, data, NULL, NULL,
                            &r_ip, &r_port, NULL, NULL);
        failf(data, "QUIC: connection to %s port %u refused",
              r_ip, r_port);
        result = CURLE_COULDNT_CONNECT;
        goto out;
      }
      failf(data, "QUIC: recvmsg() unexpectedly returned %d (errno=%d)",
                  mcount, SOCKERRNO);
      result = CURLE_RECV_ERROR;
      goto out;
    }

    DEBUGF(LOG_CF(data, cf, "recvmmsg() -> %d packets", mcount));
    pkts += mcount;
    for(i = 0; i < mcount; ++i) {
      total_nread += mmsg[i].msg_len;
      result = recv_cb(bufs[i], mmsg[i].msg_len,
                       mmsg[i].msg_hdr.msg_name, mmsg[i].msg_hdr.msg_namelen,
                       0, userp);
      if(result)
        goto out;
    }
  }

out:
  DEBUGF(LOG_CF(data, cf, "recvd %zu packets with %zd bytes -> %d",
                pkts, total_nread, result));
  return result;
}

#elif defined(HAVE_SENDMSG)
static CURLcode recvmsg_packets(struct Curl_cfilter *cf,
                                struct Curl_easy *data,
                                struct cf_quic_ctx *qctx,
                                size_t max_pkts,
                                vquic_recv_pkt_cb *recv_cb, void *userp)
{
  struct iovec msg_iov;
  struct msghdr msg;
  uint8_t buf[64*1024];
  struct sockaddr_storage remote_addr;
  size_t total_nread, pkts;
  ssize_t nread;
  CURLcode result = CURLE_OK;

  msg_iov.iov_base = buf;
  msg_iov.iov_len = (int)sizeof(buf);

  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &msg_iov;
  msg.msg_iovlen = 1;

  DEBUGASSERT(max_pkts > 0);
  for(pkts = 0, total_nread = 0; pkts < max_pkts;) {
    msg.msg_name = &remote_addr;
    msg.msg_namelen = sizeof(remote_addr);
    while((nread = recvmsg(qctx->sockfd, &msg, 0)) == -1 &&
          SOCKERRNO == EINTR)
      ;
    if(nread == -1) {
      if(SOCKERRNO == EAGAIN || SOCKERRNO == EWOULDBLOCK) {
        goto out;
      }
      if(!cf->connected && SOCKERRNO == ECONNREFUSED) {
        const char *r_ip;
        int r_port;
        Curl_cf_socket_peek(cf->next, data, NULL, NULL,
                            &r_ip, &r_port, NULL, NULL);
        failf(data, "QUIC: connection to %s port %u refused",
              r_ip, r_port);
        result = CURLE_COULDNT_CONNECT;
        goto out;
      }
      failf(data, "QUIC: recvmsg() unexpectedly returned %zd (errno=%d)",
                  nread, SOCKERRNO);
      result = CURLE_RECV_ERROR;
      goto out;
    }

    ++pkts;
    total_nread += (size_t)nread;
    result = recv_cb(buf, (size_t)nread, msg.msg_name, msg.msg_namelen,
                     0, userp);
    if(result)
      goto out;
  }

out:
  DEBUGF(LOG_CF(data, cf, "recvd %zu packets with %zd bytes -> %d",
                pkts, total_nread, result));
  return result;
}

#else /* HAVE_SENDMMSG || HAVE_SENDMSG */
static CURLcode recvfrom_packets(struct Curl_cfilter *cf,
                                 struct Curl_easy *data,
                                 struct cf_quic_ctx *qctx,
                                 size_t max_pkts,
                                 vquic_recv_pkt_cb *recv_cb, void *userp)
{
  uint8_t buf[64*1024];
  int bufsize = (int)sizeof(buf);
  struct sockaddr_storage remote_addr;
  socklen_t remote_addrlen = sizeof(remote_addr);
  size_t total_nread, pkts;
  ssize_t nread;
  CURLcode result = CURLE_OK;

  DEBUGASSERT(max_pkts > 0);
  for(pkts = 0, total_nread = 0; pkts < max_pkts;) {
    while((nread = recvfrom(qctx->sockfd, (char *)buf, bufsize, 0,
                            (struct sockaddr *)&remote_addr,
                            &remote_addrlen)) == -1 &&
          SOCKERRNO == EINTR)
      ;
    if(nread == -1) {
      if(SOCKERRNO == EAGAIN || SOCKERRNO == EWOULDBLOCK) {
        DEBUGF(LOG_CF(data, cf, "ingress, recvfrom -> EAGAIN"));
        goto out;
      }
      if(!cf->connected && SOCKERRNO == ECONNREFUSED) {
        const char *r_ip;
        int r_port;
        Curl_cf_socket_peek(cf->next, data, NULL, NULL,
                            &r_ip, &r_port, NULL, NULL);
        failf(data, "QUIC: connection to %s port %u refused",
              r_ip, r_port);
        result = CURLE_COULDNT_CONNECT;
        goto out;
      }
      failf(data, "QUIC: recvfrom() unexpectedly returned %zd (errno=%d)",
                  nread, SOCKERRNO);
      result = CURLE_RECV_ERROR;
      goto out;
    }

    ++pkts;
    total_nread += (size_t)nread;
    result = recv_cb(buf, (size_t)nread, &remote_addr, remote_addrlen,
                     0, userp);
    if(result)
      goto out;
  }

out:
  DEBUGF(LOG_CF(data, cf, "recvd %zu packets with %zd bytes -> %d",
                pkts, total_nread, result));
  return result;
}
#endif /* !HAVE_SENDMMSG && !HAVE_SENDMSG */

CURLcode vquic_recv_packets(struct Curl_cfilter *cf,
                            struct Curl_easy *data,
                            struct cf_quic_ctx *qctx,
                            size_t max_pkts,
                            vquic_recv_pkt_cb *recv_cb, void *userp)
{
#if defined(HAVE_SENDMMSG)
  return recvmmsg_packets(cf, data, qctx, max_pkts, recv_cb, userp);
#elif defined(HAVE_SENDMSG)
  return recvmsg_packets(cf, data, qctx, max_pkts, recv_cb, userp);
#else
  return recvfrom_packets(cf, data, qctx, max_pkts, recv_cb, userp);
#endif
}

/*
 * If the QLOGDIR environment variable is set, open and return a file
 * descriptor to write the log to.
 *
 * This function returns error if something failed outside of failing to
 * create the file. Open file success is deemed by seeing if the returned fd
 * is != -1.
 */
CURLcode Curl_qlogdir(struct Curl_easy *data,
                      unsigned char *scid,
                      size_t scidlen,
                      int *qlogfdp)
{
  const char *qlog_dir = getenv("QLOGDIR");
  *qlogfdp = -1;
  if(qlog_dir) {
    struct dynbuf fname;
    CURLcode result;
    unsigned int i;
    Curl_dyn_init(&fname, DYN_QLOG_NAME);
    result = Curl_dyn_add(&fname, qlog_dir);
    if(!result)
      result = Curl_dyn_add(&fname, "/");
    for(i = 0; (i < scidlen) && !result; i++) {
      char hex[3];
      msnprintf(hex, 3, "%02x", scid[i]);
      result = Curl_dyn_add(&fname, hex);
    }
    if(!result)
      result = Curl_dyn_add(&fname, ".sqlog");

    if(!result) {
      int qlogfd = open(Curl_dyn_ptr(&fname), QLOGMODE,
                        data->set.new_file_perms);
      if(qlogfd != -1)
        *qlogfdp = qlogfd;
    }
    Curl_dyn_free(&fname);
    if(result)
      return result;
  }

  return CURLE_OK;
}

CURLcode Curl_cf_quic_create(struct Curl_cfilter **pcf,
                             struct Curl_easy *data,
                             struct connectdata *conn,
                             const struct Curl_addrinfo *ai,
                             int transport)
{
  (void)transport;
  DEBUGASSERT(transport == TRNSPRT_QUIC);
#if defined(USE_NGTCP2) && defined(USE_NGHTTP3)
  return Curl_cf_ngtcp2_create(pcf, data, conn, ai);
#elif defined(USE_QUICHE)
  return Curl_cf_quiche_create(pcf, data, conn, ai);
#elif defined(USE_MSH3)
  return Curl_cf_msh3_create(pcf, data, conn, ai);
#else
  *pcf = NULL;
  (void)data;
  (void)conn;
  (void)ai;
  return CURLE_NOT_BUILT_IN;
#endif
}

bool Curl_conn_is_http3(const struct Curl_easy *data,
                        const struct connectdata *conn,
                        int sockindex)
{
#if defined(USE_NGTCP2) && defined(USE_NGHTTP3)
  return Curl_conn_is_ngtcp2(data, conn, sockindex);
#elif defined(USE_QUICHE)
  return Curl_conn_is_quiche(data, conn, sockindex);
#elif defined(USE_MSH3)
  return Curl_conn_is_msh3(data, conn, sockindex);
#else
  return ((conn->handler->protocol & PROTO_FAMILY_HTTP) &&
          (conn->httpversion == 30));
#endif
}

CURLcode Curl_conn_may_http3(struct Curl_easy *data,
                             const struct connectdata *conn)
{
  if(conn->transport == TRNSPRT_UNIX) {
    /* cannot do QUIC over a unix domain socket */
    return CURLE_QUIC_CONNECT_ERROR;
  }
  if(!(conn->handler->flags & PROTOPT_SSL)) {
    failf(data, "HTTP/3 requested for non-HTTPS URL");
    return CURLE_URL_MALFORMAT;
  }
#ifndef CURL_DISABLE_PROXY
  if(conn->bits.socksproxy) {
    failf(data, "HTTP/3 is not supported over a SOCKS proxy");
    return CURLE_URL_MALFORMAT;
  }
  if(conn->bits.httpproxy && conn->bits.tunnel_proxy) {
    failf(data, "HTTP/3 is not supported over a HTTP proxy");
    return CURLE_URL_MALFORMAT;
  }
#endif

  return CURLE_OK;
}

#else /* ENABLE_QUIC */

CURLcode Curl_conn_may_http3(struct Curl_easy *data,
                             const struct connectdata *conn)
{
  (void)conn;
  (void)data;
  DEBUGF(infof(data, "QUIC is not supported in this build"));
  return CURLE_NOT_BUILT_IN;
}

#endif /* !ENABLE_QUIC */
