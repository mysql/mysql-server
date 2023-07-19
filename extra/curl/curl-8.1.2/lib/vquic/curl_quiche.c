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

#include "curl_setup.h"

#ifdef USE_QUICHE
#include <quiche.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include "bufq.h"
#include "urldata.h"
#include "cfilters.h"
#include "cf-socket.h"
#include "sendf.h"
#include "strdup.h"
#include "rand.h"
#include "strcase.h"
#include "multiif.h"
#include "connect.h"
#include "progress.h"
#include "strerror.h"
#include "http1.h"
#include "vquic.h"
#include "vquic_int.h"
#include "curl_quiche.h"
#include "transfer.h"
#include "vtls/openssl.h"
#include "vtls/keylog.h"

/* The last 3 #include files should be in this order */
#include "curl_printf.h"
#include "curl_memory.h"
#include "memdebug.h"

/* #define DEBUG_QUICHE */

#define QUIC_MAX_STREAMS              (100)
#define QUIC_IDLE_TIMEOUT        (5 * 1000) /* milliseconds */

#define H3_STREAM_WINDOW_SIZE  (128 * 1024)
#define H3_STREAM_CHUNK_SIZE    (16 * 1024)
/* The pool keeps spares around and half of a full stream windows
 * seems good. More does not seem to improve performance.
 * The benefit of the pool is that stream buffer to not keep
 * spares. So memory consumption goes down when streams run empty,
 * have a large upload done, etc. */
#define H3_STREAM_POOL_SPARES \
          (H3_STREAM_WINDOW_SIZE / H3_STREAM_CHUNK_SIZE ) / 2
/* Receive and Send max number of chunks just follows from the
 * chunk size and window size */
#define H3_STREAM_RECV_CHUNKS \
          (H3_STREAM_WINDOW_SIZE / H3_STREAM_CHUNK_SIZE)
#define H3_STREAM_SEND_CHUNKS \
          (H3_STREAM_WINDOW_SIZE / H3_STREAM_CHUNK_SIZE)

/*
 * Store quiche version info in this buffer.
 */
void Curl_quiche_ver(char *p, size_t len)
{
  (void)msnprintf(p, len, "quiche/%s", quiche_version());
}

static void keylog_callback(const SSL *ssl, const char *line)
{
  (void)ssl;
  Curl_tls_keylog_write_line(line);
}

static SSL_CTX *quic_ssl_ctx(struct Curl_easy *data)
{
  SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_method());

  SSL_CTX_set_alpn_protos(ssl_ctx,
                          (const uint8_t *)QUICHE_H3_APPLICATION_PROTOCOL,
                          sizeof(QUICHE_H3_APPLICATION_PROTOCOL) - 1);

  SSL_CTX_set_default_verify_paths(ssl_ctx);

  /* Open the file if a TLS or QUIC backend has not done this before. */
  Curl_tls_keylog_open();
  if(Curl_tls_keylog_enabled()) {
    SSL_CTX_set_keylog_callback(ssl_ctx, keylog_callback);
  }

  {
    struct connectdata *conn = data->conn;
    if(conn->ssl_config.verifypeer) {
      const char * const ssl_cafile = conn->ssl_config.CAfile;
      const char * const ssl_capath = conn->ssl_config.CApath;
      if(ssl_cafile || ssl_capath) {
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);
        /* tell OpenSSL where to find CA certificates that are used to verify
           the server's certificate. */
        if(!SSL_CTX_load_verify_locations(ssl_ctx, ssl_cafile, ssl_capath)) {
          /* Fail if we insist on successfully verifying the server. */
          failf(data, "error setting certificate verify locations:"
                "  CAfile: %s CApath: %s",
                ssl_cafile ? ssl_cafile : "none",
                ssl_capath ? ssl_capath : "none");
          return NULL;
        }
        infof(data, " CAfile: %s", ssl_cafile ? ssl_cafile : "none");
        infof(data, " CApath: %s", ssl_capath ? ssl_capath : "none");
      }
#ifdef CURL_CA_FALLBACK
      else {
        /* verifying the peer without any CA certificates won't work so
           use openssl's built-in default as fallback */
        SSL_CTX_set_default_verify_paths(ssl_ctx);
      }
#endif
    }
  }
  return ssl_ctx;
}

struct cf_quiche_ctx {
  struct cf_quic_ctx q;
  quiche_conn *qconn;
  quiche_config *cfg;
  quiche_h3_conn *h3c;
  quiche_h3_config *h3config;
  uint8_t scid[QUICHE_MAX_CONN_ID_LEN];
  SSL_CTX *sslctx;
  SSL *ssl;
  struct curltime started_at;        /* time the current attempt started */
  struct curltime handshake_at;      /* time connect handshake finished */
  struct curltime first_byte_at;     /* when first byte was recvd */
  struct curltime reconnect_at;      /* time the next attempt should start */
  struct bufc_pool stream_bufcp;     /* chunk pool for streams */
  curl_off_t data_recvd;
  size_t sends_on_hold;              /* # of streams with SEND_HOLD set */
  BIT(goaway);                       /* got GOAWAY from server */
  BIT(got_first_byte);               /* if first byte was received */
};

#ifdef DEBUG_QUICHE
static void quiche_debug_log(const char *line, void *argp)
{
  (void)argp;
  fprintf(stderr, "%s\n", line);
}
#endif

static void cf_quiche_ctx_clear(struct cf_quiche_ctx *ctx)
{
  if(ctx) {
    vquic_ctx_free(&ctx->q);
    if(ctx->qconn)
      quiche_conn_free(ctx->qconn);
    if(ctx->h3config)
      quiche_h3_config_free(ctx->h3config);
    if(ctx->h3c)
      quiche_h3_conn_free(ctx->h3c);
    if(ctx->cfg)
      quiche_config_free(ctx->cfg);
    Curl_bufcp_free(&ctx->stream_bufcp);
    memset(ctx, 0, sizeof(*ctx));
  }
}

/**
 * All about the H3 internals of a stream
 */
struct stream_ctx {
  int64_t id; /* HTTP/3 protocol stream identifier */
  struct bufq recvbuf; /* h3 response */
  uint64_t error3; /* HTTP/3 stream error code */
  curl_off_t upload_left; /* number of request bytes left to upload */
  bool closed; /* TRUE on stream close */
  bool reset;  /* TRUE on stream reset */
  bool send_closed; /* stream is locally closed */
  bool resp_hds_complete;  /* complete, final response has been received */
  bool resp_got_header; /* TRUE when h3 stream has recvd some HEADER */
};

#define H3_STREAM_CTX(d)    ((struct stream_ctx *)(((d) && (d)->req.p.http)? \
                             ((struct HTTP *)(d)->req.p.http)->h3_ctx \
                               : NULL))
#define H3_STREAM_LCTX(d)   ((struct HTTP *)(d)->req.p.http)->h3_ctx
#define H3_STREAM_ID(d)     (H3_STREAM_CTX(d)? \
                             H3_STREAM_CTX(d)->id : -2)

static bool stream_send_is_suspended(struct Curl_easy *data)
{
  return (data->req.keepon & KEEP_SEND_HOLD);
}

static void stream_send_suspend(struct Curl_cfilter *cf,
                                struct Curl_easy *data)
{
  struct cf_quiche_ctx *ctx = cf->ctx;

  if((data->req.keepon & KEEP_SENDBITS) == KEEP_SEND) {
    data->req.keepon |= KEEP_SEND_HOLD;
    ++ctx->sends_on_hold;
    if(H3_STREAM_ID(data) >= 0)
      DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"] suspend sending",
                    H3_STREAM_ID(data)));
    else
      DEBUGF(LOG_CF(data, cf, "[%s] suspend sending",
                    data->state.url));
  }
}

static void stream_send_resume(struct Curl_cfilter *cf,
                               struct Curl_easy *data)
{
  struct cf_quiche_ctx *ctx = cf->ctx;

  if(stream_send_is_suspended(data)) {
    data->req.keepon &= ~KEEP_SEND_HOLD;
    --ctx->sends_on_hold;
    if(H3_STREAM_ID(data) >= 0)
      DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"] resume sending",
                    H3_STREAM_ID(data)));
    else
      DEBUGF(LOG_CF(data, cf, "[%s] resume sending",
                    data->state.url));
    Curl_expire(data, 0, EXPIRE_RUN_NOW);
  }
}

static void check_resumes(struct Curl_cfilter *cf,
                          struct Curl_easy *data)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  struct Curl_easy *sdata;

  if(ctx->sends_on_hold) {
    DEBUGASSERT(data->multi);
    for(sdata = data->multi->easyp;
        sdata && ctx->sends_on_hold; sdata = sdata->next) {
      if(stream_send_is_suspended(sdata)) {
        stream_send_resume(cf, sdata);
      }
    }
  }
}

static CURLcode h3_data_setup(struct Curl_cfilter *cf,
                              struct Curl_easy *data)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  struct stream_ctx *stream = H3_STREAM_CTX(data);

  if(stream)
    return CURLE_OK;

  stream = calloc(1, sizeof(*stream));
  if(!stream)
    return CURLE_OUT_OF_MEMORY;

  H3_STREAM_LCTX(data) = stream;
  stream->id = -1;
  Curl_bufq_initp(&stream->recvbuf, &ctx->stream_bufcp,
                  H3_STREAM_RECV_CHUNKS, BUFQ_OPT_SOFT_LIMIT);
  DEBUGF(LOG_CF(data, cf, "data setup (easy %p)", (void *)data));
  return CURLE_OK;
}

static void h3_data_done(struct Curl_cfilter *cf, struct Curl_easy *data)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  struct stream_ctx *stream = H3_STREAM_CTX(data);

  (void)cf;
  if(stream) {
    DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"] easy handle is done",
                  stream->id));
    if(stream_send_is_suspended(data)) {
      data->req.keepon &= ~KEEP_SEND_HOLD;
      --ctx->sends_on_hold;
    }
    Curl_bufq_free(&stream->recvbuf);
    free(stream);
    H3_STREAM_LCTX(data) = NULL;
  }
}

static void drain_stream(struct Curl_cfilter *cf,
                         struct Curl_easy *data)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  unsigned char bits;

  (void)cf;
  bits = CURL_CSELECT_IN;
  if(stream && !stream->send_closed && stream->upload_left)
    bits |= CURL_CSELECT_OUT;
  if(data->state.dselect_bits != bits) {
    data->state.dselect_bits = bits;
    Curl_expire(data, 0, EXPIRE_RUN_NOW);
  }
}

static struct Curl_easy *get_stream_easy(struct Curl_cfilter *cf,
                                         struct Curl_easy *data,
                                         int64_t stream3_id)
{
  struct Curl_easy *sdata;

  (void)cf;
  if(H3_STREAM_ID(data) == stream3_id) {
    return data;
  }
  else {
    DEBUGASSERT(data->multi);
    for(sdata = data->multi->easyp; sdata; sdata = sdata->next) {
      if(H3_STREAM_ID(sdata) == stream3_id) {
        return sdata;
      }
    }
  }
  return NULL;
}

/*
 * write_resp_raw() copies response data in raw format to the `data`'s
  * receive buffer. If not enough space is available, it appends to the
 * `data`'s overflow buffer.
 */
static CURLcode write_resp_raw(struct Curl_cfilter *cf,
                               struct Curl_easy *data,
                               const void *mem, size_t memlen)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  CURLcode result = CURLE_OK;
  ssize_t nwritten;

  (void)cf;
  if(!stream)
    return CURLE_RECV_ERROR;
  nwritten = Curl_bufq_write(&stream->recvbuf, mem, memlen, &result);
  if(nwritten < 0)
    return result;

  if((size_t)nwritten < memlen) {
    /* This MUST not happen. Our recbuf is dimensioned to hold the
     * full max_stream_window and then some for this very reason. */
    DEBUGASSERT(0);
    return CURLE_RECV_ERROR;
  }
  return result;
}

struct cb_ctx {
  struct Curl_cfilter *cf;
  struct Curl_easy *data;
};

static int cb_each_header(uint8_t *name, size_t name_len,
                          uint8_t *value, size_t value_len,
                          void *argp)
{
  struct cb_ctx *x = argp;
  struct stream_ctx *stream = H3_STREAM_CTX(x->data);
  CURLcode result;

  (void)stream;
  if((name_len == 7) && !strncmp(HTTP_PSEUDO_STATUS, (char *)name, 7)) {
    result = write_resp_raw(x->cf, x->data, "HTTP/3 ", sizeof("HTTP/3 ") - 1);
    if(!result)
      result = write_resp_raw(x->cf, x->data, value, value_len);
    if(!result)
      result = write_resp_raw(x->cf, x->data, " \r\n", 3);
  }
  else {
    result = write_resp_raw(x->cf, x->data, name, name_len);
    if(!result)
      result = write_resp_raw(x->cf, x->data, ": ", 2);
    if(!result)
      result = write_resp_raw(x->cf, x->data, value, value_len);
    if(!result)
      result = write_resp_raw(x->cf, x->data, "\r\n", 2);
  }
  if(result) {
    DEBUGF(LOG_CF(x->data, x->cf,
                  "[h3sid=%"PRId64"][HEADERS][%.*s: %.*s] error %d",
                  stream? stream->id : -1, (int)name_len, name,
                  (int)value_len, value, result));
  }
  return result;
}

static ssize_t stream_resp_read(void *reader_ctx,
                                unsigned char *buf, size_t len,
                                CURLcode *err)
{
  struct cb_ctx *x = reader_ctx;
  struct cf_quiche_ctx *ctx = x->cf->ctx;
  struct stream_ctx *stream = H3_STREAM_CTX(x->data);
  ssize_t nread;

  if(!stream) {
    *err = CURLE_RECV_ERROR;
    return -1;
  }

  nread = quiche_h3_recv_body(ctx->h3c, ctx->qconn, stream->id,
                              buf, len);
  if(nread >= 0) {
    *err = CURLE_OK;
    return nread;
  }
  else if(nread < 0) {
    *err = CURLE_AGAIN;
    return -1;
  }
  else {
    *err = stream->resp_got_header? CURLE_PARTIAL_FILE : CURLE_RECV_ERROR;
    return -1;
  }
}

static CURLcode cf_recv_body(struct Curl_cfilter *cf,
                             struct Curl_easy *data)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  ssize_t nwritten;
  struct cb_ctx cb_ctx;
  CURLcode result = CURLE_OK;

  if(!stream)
    return CURLE_RECV_ERROR;

  if(!stream->resp_hds_complete) {
    result = write_resp_raw(cf, data, "\r\n", 2);
    if(result)
      return result;
    stream->resp_hds_complete = TRUE;
  }

  cb_ctx.cf = cf;
  cb_ctx.data = data;
  nwritten = Curl_bufq_slurp(&stream->recvbuf,
                             stream_resp_read, &cb_ctx, &result);

  if(nwritten < 0 && result != CURLE_AGAIN) {
    DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"] recv_body error %zd",
                  stream->id, nwritten));
    failf(data, "Error %zd in HTTP/3 response body for stream[%"PRId64"]",
          nwritten, stream->id);
    stream->closed = TRUE;
    stream->reset = TRUE;
    stream->send_closed = TRUE;
    streamclose(cf->conn, "Reset of stream");
    return result;
  }
  return CURLE_OK;
}

#ifdef DEBUGBUILD
static const char *cf_ev_name(quiche_h3_event *ev)
{
  switch(quiche_h3_event_type(ev)) {
  case QUICHE_H3_EVENT_HEADERS:
    return "HEADERS";
  case QUICHE_H3_EVENT_DATA:
    return "DATA";
  case QUICHE_H3_EVENT_RESET:
    return "RESET";
  case QUICHE_H3_EVENT_FINISHED:
    return "FINISHED";
  case QUICHE_H3_EVENT_GOAWAY:
    return "GOAWAY";
  default:
    return "Unknown";
  }
}
#else
#define cf_ev_name(x)   ""
#endif

static CURLcode h3_process_event(struct Curl_cfilter *cf,
                                 struct Curl_easy *data,
                                 int64_t stream3_id,
                                 quiche_h3_event *ev)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  struct cb_ctx cb_ctx;
  CURLcode result = CURLE_OK;
  int rc;

  if(!stream)
    return CURLE_OK;
  DEBUGASSERT(stream3_id == stream->id);
  switch(quiche_h3_event_type(ev)) {
  case QUICHE_H3_EVENT_HEADERS:
    stream->resp_got_header = TRUE;
    cb_ctx.cf = cf;
    cb_ctx.data = data;
    rc = quiche_h3_event_for_each_header(ev, cb_each_header, &cb_ctx);
    if(rc) {
      failf(data, "Error %d in HTTP/3 response header for stream[%"PRId64"]",
            rc, stream3_id);
      return CURLE_RECV_ERROR;
    }
    DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"][HEADERS]", stream3_id));
    break;

  case QUICHE_H3_EVENT_DATA:
    if(!stream->closed) {
      result = cf_recv_body(cf, data);
    }
    break;

  case QUICHE_H3_EVENT_RESET:
    DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"][RESET]", stream3_id));
    stream->closed = TRUE;
    stream->reset = TRUE;
    stream->send_closed = TRUE;
    streamclose(cf->conn, "Reset of stream");
    break;

  case QUICHE_H3_EVENT_FINISHED:
    DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"][FINISHED]", stream3_id));
    if(!stream->resp_hds_complete) {
      result = write_resp_raw(cf, data, "\r\n", 2);
      if(result)
        return result;
      stream->resp_hds_complete = TRUE;
    }
    stream->closed = TRUE;
    streamclose(cf->conn, "End of stream");
    break;

  case QUICHE_H3_EVENT_GOAWAY:
    DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"][GOAWAY]", stream3_id));
    break;

  default:
    DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"] recv, unhandled event %d",
                  stream3_id, quiche_h3_event_type(ev)));
    break;
  }
  return result;
}

static CURLcode cf_poll_events(struct Curl_cfilter *cf,
                               struct Curl_easy *data)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  struct Curl_easy *sdata;
  quiche_h3_event *ev;
  CURLcode result;

  /* Take in the events and distribute them to the transfers. */
  while(ctx->h3c) {
    int64_t stream3_id = quiche_h3_conn_poll(ctx->h3c, ctx->qconn, &ev);
    if(stream3_id == QUICHE_H3_ERR_DONE) {
      break;
    }
    else if(stream3_id < 0) {
      DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"] error poll: %"PRId64,
                    stream? stream->id : -1, stream3_id));
      return CURLE_HTTP3;
    }

    sdata = get_stream_easy(cf, data, stream3_id);
    if(!sdata) {
      DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"] discard event %s for "
                    "unknown [h3sid=%"PRId64"]",
                    stream? stream->id : -1, cf_ev_name(ev),
                    stream3_id));
    }
    else {
      result = h3_process_event(cf, sdata, stream3_id, ev);
      drain_stream(cf, sdata);
      if(result) {
        DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"] error processing event %s "
                      "for [h3sid=%"PRId64"] -> %d",
                      stream? stream->id : -1, cf_ev_name(ev),
                      stream3_id, result));
        quiche_h3_event_free(ev);
        return result;
      }
      quiche_h3_event_free(ev);
    }
  }
  return CURLE_OK;
}

struct recv_ctx {
  struct Curl_cfilter *cf;
  struct Curl_easy *data;
  int pkts;
};

static CURLcode recv_pkt(const unsigned char *pkt, size_t pktlen,
                         struct sockaddr_storage *remote_addr,
                         socklen_t remote_addrlen, int ecn,
                         void *userp)
{
  struct recv_ctx *r = userp;
  struct cf_quiche_ctx *ctx = r->cf->ctx;
  quiche_recv_info recv_info;
  ssize_t nread;

  (void)ecn;
  ++r->pkts;

  recv_info.to = (struct sockaddr *)&ctx->q.local_addr;
  recv_info.to_len = ctx->q.local_addrlen;
  recv_info.from = (struct sockaddr *)remote_addr;
  recv_info.from_len = remote_addrlen;

  nread = quiche_conn_recv(ctx->qconn, (unsigned char *)pkt, pktlen,
                           &recv_info);
  if(nread < 0) {
    if(QUICHE_ERR_DONE == nread) {
      DEBUGF(LOG_CF(r->data, r->cf, "ingress, quiche is DONE"));
      return CURLE_OK;
    }
    else if(QUICHE_ERR_TLS_FAIL == nread) {
      long verify_ok = SSL_get_verify_result(ctx->ssl);
      if(verify_ok != X509_V_OK) {
        failf(r->data, "SSL certificate problem: %s",
              X509_verify_cert_error_string(verify_ok));
        return CURLE_PEER_FAILED_VERIFICATION;
      }
    }
    else {
      failf(r->data, "quiche_conn_recv() == %zd", nread);
      return CURLE_RECV_ERROR;
    }
  }
  else if((size_t)nread < pktlen) {
    DEBUGF(LOG_CF(r->data, r->cf, "ingress, quiche only read %zd/%zd bytes",
                  nread, pktlen));
  }

  return CURLE_OK;
}

static CURLcode cf_process_ingress(struct Curl_cfilter *cf,
                                   struct Curl_easy *data)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  struct recv_ctx rctx;
  CURLcode result;

  DEBUGASSERT(ctx->qconn);
  rctx.cf = cf;
  rctx.data = data;
  rctx.pkts = 0;

  result = vquic_recv_packets(cf, data, &ctx->q, 1000, recv_pkt, &rctx);
  if(result)
    return result;

  if(rctx.pkts > 0) {
    /* quiche digested ingress packets. It might have opened flow control
     * windows again. */
    check_resumes(cf, data);
  }
  return cf_poll_events(cf, data);
}

struct read_ctx {
  struct Curl_cfilter *cf;
  struct Curl_easy *data;
  quiche_send_info send_info;
};

static ssize_t read_pkt_to_send(void *userp,
                                unsigned char *buf, size_t buflen,
                                CURLcode *err)
{
  struct read_ctx *x = userp;
  struct cf_quiche_ctx *ctx = x->cf->ctx;
  ssize_t nwritten;

  nwritten = quiche_conn_send(ctx->qconn, buf, buflen, &x->send_info);
  if(nwritten == QUICHE_ERR_DONE) {
    *err = CURLE_AGAIN;
    return -1;
  }

  if(nwritten < 0) {
    failf(x->data, "quiche_conn_send returned %zd", nwritten);
    *err = CURLE_SEND_ERROR;
    return -1;
  }
  *err = CURLE_OK;
  return nwritten;
}

/*
 * flush_egress drains the buffers and sends off data.
 * Calls failf() on errors.
 */
static CURLcode cf_flush_egress(struct Curl_cfilter *cf,
                                struct Curl_easy *data)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  ssize_t nread;
  CURLcode result;
  int64_t timeout_ns;
  struct read_ctx readx;
  size_t pkt_count, gsolen;

  result = vquic_flush(cf, data, &ctx->q);
  if(result) {
    if(result == CURLE_AGAIN) {
      Curl_expire(data, 1, EXPIRE_QUIC);
      return CURLE_OK;
    }
    return result;
  }

  readx.cf = cf;
  readx.data = data;
  memset(&readx.send_info, 0, sizeof(readx.send_info));
  pkt_count = 0;
  gsolen = quiche_conn_max_send_udp_payload_size(ctx->qconn);
  for(;;) {
    /* add the next packet to send, if any, to our buffer */
    nread = Curl_bufq_sipn(&ctx->q.sendbuf, 0,
                           read_pkt_to_send, &readx, &result);
    /* DEBUGF(LOG_CF(data, cf, "sip packet(maxlen=%zu) -> %zd, %d",
                  (size_t)0, nread, result)); */

    if(nread < 0) {
      if(result != CURLE_AGAIN)
        return result;
      /* Nothing more to add, flush and leave */
      result = vquic_send(cf, data, &ctx->q, gsolen);
      if(result) {
        if(result == CURLE_AGAIN) {
          Curl_expire(data, 1, EXPIRE_QUIC);
          return CURLE_OK;
        }
        return result;
      }
      goto out;
    }

    ++pkt_count;
    if((size_t)nread < gsolen || pkt_count >= MAX_PKT_BURST) {
      result = vquic_send(cf, data, &ctx->q, gsolen);
      if(result) {
        if(result == CURLE_AGAIN) {
          Curl_expire(data, 1, EXPIRE_QUIC);
          return CURLE_OK;
        }
        goto out;
      }
      pkt_count = 0;
    }
  }

out:
  timeout_ns = quiche_conn_timeout_as_nanos(ctx->qconn);
  if(timeout_ns % 1000000)
    timeout_ns += 1000000;
    /* expire resolution is milliseconds */
  Curl_expire(data, (timeout_ns / 1000000), EXPIRE_QUIC);
  return result;
}

static ssize_t recv_closed_stream(struct Curl_cfilter *cf,
                                  struct Curl_easy *data,
                                  CURLcode *err)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  ssize_t nread = -1;

  DEBUGASSERT(stream);
  if(stream->reset) {
    failf(data,
          "HTTP/3 stream %" PRId64 " reset by server", stream->id);
    *err = stream->resp_got_header? CURLE_PARTIAL_FILE : CURLE_RECV_ERROR;
    DEBUGF(LOG_CF(data, cf, "[h3sid=%" PRId64 "] cf_recv, was reset -> %d",
                  stream->id, *err));
  }
  else if(!stream->resp_got_header) {
    failf(data,
          "HTTP/3 stream %" PRId64 " was closed cleanly, but before getting"
          " all response header fields, treated as error",
          stream->id);
    /* *err = CURLE_PARTIAL_FILE; */
    *err = CURLE_RECV_ERROR;
    DEBUGF(LOG_CF(data, cf, "[h3sid=%" PRId64 "] cf_recv, closed incomplete"
                  " -> %d", stream->id, *err));
  }
  else {
    *err = CURLE_OK;
    nread = 0;
    DEBUGF(LOG_CF(data, cf, "[h3sid=%" PRId64 "] cf_recv, closed ok"
                  " -> %d", stream->id, *err));
  }
  return nread;
}

static ssize_t cf_quiche_recv(struct Curl_cfilter *cf, struct Curl_easy *data,
                              char *buf, size_t len, CURLcode *err)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  ssize_t nread = -1;
  CURLcode result;

  if(!stream) {
    *err = CURLE_RECV_ERROR;
    goto out;
  }

  if(!Curl_bufq_is_empty(&stream->recvbuf)) {
    nread = Curl_bufq_read(&stream->recvbuf,
                           (unsigned char *)buf, len, err);
    DEBUGF(LOG_CF(data, cf, "[h3sid=%" PRId64 "] read recvbuf(len=%zu) "
                  "-> %zd, %d", stream->id, len, nread, *err));
    if(nread < 0)
      goto out;
  }

  if(cf_process_ingress(cf, data)) {
    DEBUGF(LOG_CF(data, cf, "cf_recv, error on ingress"));
    *err = CURLE_RECV_ERROR;
    nread = -1;
    goto out;
  }

  /* recvbuf had nothing before, maybe after progressing ingress? */
  if(nread < 0 && !Curl_bufq_is_empty(&stream->recvbuf)) {
    nread = Curl_bufq_read(&stream->recvbuf,
                           (unsigned char *)buf, len, err);
    DEBUGF(LOG_CF(data, cf, "[h3sid=%" PRId64 "] read recvbuf(len=%zu) "
                  "-> %zd, %d", stream->id, len, nread, *err));
    if(nread < 0)
      goto out;
  }

  if(nread > 0) {
    if(stream->closed)
      drain_stream(cf, data);
  }
  else {
    if(stream->closed) {
      nread = recv_closed_stream(cf, data, err);
      goto out;
    }
    else if(quiche_conn_is_draining(ctx->qconn)) {
      failf(data, "QUIC connection is draining");
      *err = CURLE_HTTP3;
      nread = -1;
      goto out;
    }
    *err = CURLE_AGAIN;
    nread = -1;
  }

out:
  result = cf_flush_egress(cf, data);
  if(result) {
    DEBUGF(LOG_CF(data, cf, "cf_recv, flush egress failed"));
    *err = result;
    nread = -1;
  }
  if(nread > 0)
    ctx->data_recvd += nread;
  DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"] cf_recv(total=%zd) -> %zd, %d",
                stream->id, ctx->data_recvd, nread, *err));
  return nread;
}

/* Index where :authority header field will appear in request header
   field list. */
#define AUTHORITY_DST_IDX 3

static ssize_t h3_open_stream(struct Curl_cfilter *cf,
                              struct Curl_easy *data,
                              const void *buf, size_t len,
                              CURLcode *err)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  size_t nheader, i;
  int64_t stream3_id;
  struct h1_req_parser h1;
  struct dynhds h2_headers;
  quiche_h3_header *nva = NULL;
  ssize_t nwritten;

  if(!stream) {
    *err = h3_data_setup(cf, data);
    if(*err) {
      nwritten = -1;
      goto out;
    }
    stream = H3_STREAM_CTX(data);
    DEBUGASSERT(stream);
  }

  Curl_h1_req_parse_init(&h1, H1_PARSE_DEFAULT_MAX_LINE_LEN);
  Curl_dynhds_init(&h2_headers, 0, DYN_HTTP_REQUEST);

  DEBUGASSERT(stream);
  nwritten = Curl_h1_req_parse_read(&h1, buf, len, NULL, 0, err);
  if(nwritten < 0)
    goto out;
  DEBUGASSERT(h1.done);
  DEBUGASSERT(h1.req);

  *err = Curl_http_req_to_h2(&h2_headers, h1.req, data);
  if(*err) {
    nwritten = -1;
    goto out;
  }

  nheader = Curl_dynhds_count(&h2_headers);
  nva = malloc(sizeof(quiche_h3_header) * nheader);
  if(!nva) {
    *err = CURLE_OUT_OF_MEMORY;
    nwritten = -1;
    goto out;
  }

  for(i = 0; i < nheader; ++i) {
    struct dynhds_entry *e = Curl_dynhds_getn(&h2_headers, i);
    nva[i].name = (unsigned char *)e->name;
    nva[i].name_len = e->namelen;
    nva[i].value = (unsigned char *)e->value;
    nva[i].value_len = e->valuelen;
  }

  switch(data->state.httpreq) {
  case HTTPREQ_POST:
  case HTTPREQ_POST_FORM:
  case HTTPREQ_POST_MIME:
  case HTTPREQ_PUT:
    if(data->state.infilesize != -1)
      stream->upload_left = data->state.infilesize;
    else
      /* data sending without specifying the data amount up front */
      stream->upload_left = -1; /* unknown */
    break;
  default:
    stream->upload_left = 0; /* no request body */
    break;
  }

  if(stream->upload_left == 0)
    stream->send_closed = TRUE;

  stream3_id = quiche_h3_send_request(ctx->h3c, ctx->qconn, nva, nheader,
                                      stream->send_closed);
  if(stream3_id < 0) {
    if(QUICHE_H3_ERR_STREAM_BLOCKED == stream3_id) {
      /* quiche seems to report this error if the connection window is
       * exhausted. Which happens frequently and intermittent. */
      DEBUGF(LOG_CF(data, cf, "send_request(%s) rejected with BLOCKED",
                    data->state.url));
      stream_send_suspend(cf, data);
      *err = CURLE_AGAIN;
      nwritten = -1;
      goto out;
    }
    else {
      DEBUGF(LOG_CF(data, cf, "send_request(%s) -> %" PRId64,
                    data->state.url, stream3_id));
    }
    *err = CURLE_SEND_ERROR;
    nwritten = -1;
    goto out;
  }

  DEBUGASSERT(stream->id == -1);
  *err = CURLE_OK;
  stream->id = stream3_id;
  stream->closed = FALSE;
  stream->reset = FALSE;

  infof(data, "Using HTTP/3 Stream ID: %" PRId64 " (easy handle %p)",
        stream3_id, (void *)data);
  DEBUGF(LOG_CF(data, cf, "[h3sid=%" PRId64 "] opened for %s",
                stream3_id, data->state.url));

out:
  free(nva);
  Curl_h1_req_parse_free(&h1);
  Curl_dynhds_free(&h2_headers);
  return nwritten;
}

static ssize_t cf_quiche_send(struct Curl_cfilter *cf, struct Curl_easy *data,
                              const void *buf, size_t len, CURLcode *err)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  CURLcode result;
  ssize_t nwritten;

  *err = cf_process_ingress(cf, data);
  if(*err) {
    nwritten = -1;
    goto out;
  }

  if(!stream || stream->id < 0) {
    nwritten = h3_open_stream(cf, data, buf, len, err);
    if(nwritten < 0)
      goto out;
    stream = H3_STREAM_CTX(data);
  }
  else {
    bool eof = (stream->upload_left >= 0 &&
                (curl_off_t)len >= stream->upload_left);
    nwritten = quiche_h3_send_body(ctx->h3c, ctx->qconn, stream->id,
                                   (uint8_t *)buf, len, eof);
    if(nwritten == QUICHE_H3_ERR_DONE || (nwritten == 0 && len > 0)) {
      /* TODO: we seem to be blocked on flow control and should HOLD
       * sending. But when do we open again? */
      if(!quiche_conn_stream_writable(ctx->qconn, stream->id, len)) {
        DEBUGF(LOG_CF(data, cf, "[h3sid=%" PRId64 "] send_body(len=%zu) "
                      "-> window exhausted", stream->id, len));
        stream_send_suspend(cf, data);
      }
      *err = CURLE_AGAIN;
      nwritten = -1;
      goto out;
    }
    else if(nwritten == QUICHE_H3_TRANSPORT_ERR_FINAL_SIZE) {
      DEBUGF(LOG_CF(data, cf, "[h3sid=%" PRId64 "] send_body(len=%zu) "
                    "-> exceeds size", stream->id, len));
      *err = CURLE_SEND_ERROR;
      nwritten = -1;
      goto out;
    }
    else if(nwritten < 0) {
      DEBUGF(LOG_CF(data, cf, "[h3sid=%" PRId64 "] send_body(len=%zu) "
                    "-> quiche err %zd", stream->id, len, nwritten));
      *err = CURLE_SEND_ERROR;
      nwritten = -1;
      goto out;
    }
    else {
      /* quiche accepted all or at least a part of the buf */
      if(stream->upload_left > 0) {
        stream->upload_left = (nwritten < stream->upload_left)?
                              (stream->upload_left - nwritten) : 0;
      }
      if(stream->upload_left == 0)
        stream->send_closed = TRUE;

      DEBUGF(LOG_CF(data, cf, "[h3sid=%" PRId64 "] send body(len=%zu, "
                    "left=%zd) -> %zd",
                    stream->id, len, stream->upload_left, nwritten));
      *err = CURLE_OK;
    }
  }

out:
  result = cf_flush_egress(cf, data);
  if(result) {
    *err = result;
    nwritten = -1;
  }
  DEBUGF(LOG_CF(data, cf, "[h3sid=%" PRId64 "] cf_send(len=%zu) -> %zd, %d",
                stream? stream->id : -1, len, nwritten, *err));
  return nwritten;
}

static bool stream_is_writeable(struct Curl_cfilter *cf,
                                struct Curl_easy *data)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  struct stream_ctx *stream = H3_STREAM_CTX(data);

  return stream &&
         quiche_conn_stream_writable(ctx->qconn, (uint64_t)stream->id, 1);
}

static int cf_quiche_get_select_socks(struct Curl_cfilter *cf,
                                      struct Curl_easy *data,
                                      curl_socket_t *socks)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  struct SingleRequest *k = &data->req;
  int rv = GETSOCK_BLANK;

  socks[0] = ctx->q.sockfd;

  /* in an HTTP/3 connection we can basically always get a frame so we should
     always be ready for one */
  rv |= GETSOCK_READSOCK(0);

  /* we're still uploading or the HTTP/3 layer wants to send data */
  if(((k->keepon & KEEP_SENDBITS) == KEEP_SEND)
     && stream_is_writeable(cf, data))
    rv |= GETSOCK_WRITESOCK(0);

  return rv;
}

/*
 * Called from transfer.c:data_pending to know if we should keep looping
 * to receive more data from the connection.
 */
static bool cf_quiche_data_pending(struct Curl_cfilter *cf,
                                   const struct Curl_easy *data)
{
  const struct stream_ctx *stream = H3_STREAM_CTX(data);
  (void)cf;
  return stream && !Curl_bufq_is_empty(&stream->recvbuf);
}

static CURLcode h3_data_pause(struct Curl_cfilter *cf,
                              struct Curl_easy *data,
                              bool pause)
{
  /* TODO: there seems right now no API in quiche to shrink/enlarge
   * the streams windows. As we do in HTTP/2. */
  if(!pause) {
    drain_stream(cf, data);
    Curl_expire(data, 0, EXPIRE_RUN_NOW);
  }
  return CURLE_OK;
}

static CURLcode cf_quiche_data_event(struct Curl_cfilter *cf,
                                     struct Curl_easy *data,
                                     int event, int arg1, void *arg2)
{
  CURLcode result = CURLE_OK;

  (void)arg1;
  (void)arg2;
  switch(event) {
  case CF_CTRL_DATA_SETUP: {
    result = h3_data_setup(cf, data);
    break;
  }
  case CF_CTRL_DATA_PAUSE:
    result = h3_data_pause(cf, data, (arg1 != 0));
    break;
  case CF_CTRL_DATA_DONE: {
    h3_data_done(cf, data);
    break;
  }
  case CF_CTRL_DATA_DONE_SEND: {
    struct stream_ctx *stream = H3_STREAM_CTX(data);
    if(stream && !stream->send_closed) {
      unsigned char body[1];
      ssize_t sent;

      stream->send_closed = TRUE;
      stream->upload_left = 0;
      body[0] = 'X';
      sent = cf_quiche_send(cf, data, body, 0, &result);
      DEBUGF(LOG_CF(data, cf, "[h3sid=%"PRId64"] DONE_SEND -> %zd, %d",
                    stream->id, sent, result));
    }
    break;
  }
  case CF_CTRL_DATA_IDLE:
    result = cf_flush_egress(cf, data);
    if(result)
      DEBUGF(LOG_CF(data, cf, "data idle, flush egress -> %d", result));
    break;
  default:
    break;
  }
  return result;
}

static CURLcode cf_verify_peer(struct Curl_cfilter *cf,
                               struct Curl_easy *data)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  CURLcode result = CURLE_OK;

  cf->conn->bits.multiplex = TRUE; /* at least potentially multiplexed */
  cf->conn->httpversion = 30;
  cf->conn->bundle->multiuse = BUNDLE_MULTIPLEX;

  if(cf->conn->ssl_config.verifyhost) {
    X509 *server_cert;
    server_cert = SSL_get_peer_certificate(ctx->ssl);
    if(!server_cert) {
      result = CURLE_PEER_FAILED_VERIFICATION;
      goto out;
    }
    result = Curl_ossl_verifyhost(data, cf->conn, server_cert);
    X509_free(server_cert);
    if(result)
      goto out;
  }
  else
    DEBUGF(LOG_CF(data, cf, "Skipped certificate verification"));

  ctx->h3config = quiche_h3_config_new();
  if(!ctx->h3config) {
    result = CURLE_OUT_OF_MEMORY;
    goto out;
  }

  /* Create a new HTTP/3 connection on the QUIC connection. */
  ctx->h3c = quiche_h3_conn_new_with_transport(ctx->qconn, ctx->h3config);
  if(!ctx->h3c) {
    result = CURLE_OUT_OF_MEMORY;
    goto out;
  }
  if(data->set.ssl.certinfo)
    /* asked to gather certificate info */
    (void)Curl_ossl_certchain(data, ctx->ssl);

out:
  if(result) {
    if(ctx->h3config) {
      quiche_h3_config_free(ctx->h3config);
      ctx->h3config = NULL;
    }
    if(ctx->h3c) {
      quiche_h3_conn_free(ctx->h3c);
      ctx->h3c = NULL;
    }
  }
  return result;
}

static CURLcode cf_connect_start(struct Curl_cfilter *cf,
                                 struct Curl_easy *data)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  int rv;
  CURLcode result;
  const struct Curl_sockaddr_ex *sockaddr;

  DEBUGASSERT(ctx->q.sockfd != CURL_SOCKET_BAD);

#ifdef DEBUG_QUICHE
  /* initialize debug log callback only once */
  static int debug_log_init = 0;
  if(!debug_log_init) {
    quiche_enable_debug_logging(quiche_debug_log, NULL);
    debug_log_init = 1;
  }
#endif
  Curl_bufcp_init(&ctx->stream_bufcp, H3_STREAM_CHUNK_SIZE,
                  H3_STREAM_POOL_SPARES);
  ctx->data_recvd = 0;

  result = vquic_ctx_init(&ctx->q);
  if(result)
    return result;

  ctx->cfg = quiche_config_new(QUICHE_PROTOCOL_VERSION);
  if(!ctx->cfg) {
    failf(data, "can't create quiche config");
    return CURLE_FAILED_INIT;
  }
  quiche_config_enable_pacing(ctx->cfg, false);
  quiche_config_set_max_idle_timeout(ctx->cfg, QUIC_IDLE_TIMEOUT);
  quiche_config_set_initial_max_data(ctx->cfg, (1 * 1024 * 1024)
    /* (QUIC_MAX_STREAMS/2) * H3_STREAM_WINDOW_SIZE */);
  quiche_config_set_initial_max_streams_bidi(ctx->cfg, QUIC_MAX_STREAMS);
  quiche_config_set_initial_max_streams_uni(ctx->cfg, QUIC_MAX_STREAMS);
  quiche_config_set_initial_max_stream_data_bidi_local(ctx->cfg,
    H3_STREAM_WINDOW_SIZE);
  quiche_config_set_initial_max_stream_data_bidi_remote(ctx->cfg,
    H3_STREAM_WINDOW_SIZE);
  quiche_config_set_initial_max_stream_data_uni(ctx->cfg,
    H3_STREAM_WINDOW_SIZE);
  quiche_config_set_disable_active_migration(ctx->cfg, TRUE);

  quiche_config_set_max_connection_window(ctx->cfg,
    10 * QUIC_MAX_STREAMS * H3_STREAM_WINDOW_SIZE);
  quiche_config_set_max_stream_window(ctx->cfg, 10 * H3_STREAM_WINDOW_SIZE);
  quiche_config_set_application_protos(ctx->cfg,
                                       (uint8_t *)
                                       QUICHE_H3_APPLICATION_PROTOCOL,
                                       sizeof(QUICHE_H3_APPLICATION_PROTOCOL)
                                       - 1);

  DEBUGASSERT(!ctx->ssl);
  DEBUGASSERT(!ctx->sslctx);
  ctx->sslctx = quic_ssl_ctx(data);
  if(!ctx->sslctx)
    return CURLE_QUIC_CONNECT_ERROR;
  ctx->ssl = SSL_new(ctx->sslctx);
  if(!ctx->ssl)
    return CURLE_QUIC_CONNECT_ERROR;

  SSL_set_app_data(ctx->ssl, cf);
  SSL_set_tlsext_host_name(ctx->ssl, cf->conn->host.name);

  result = Curl_rand(data, ctx->scid, sizeof(ctx->scid));
  if(result)
    return result;

  Curl_cf_socket_peek(cf->next, data, &ctx->q.sockfd,
                      &sockaddr, NULL, NULL, NULL, NULL);
  ctx->q.local_addrlen = sizeof(ctx->q.local_addr);
  rv = getsockname(ctx->q.sockfd, (struct sockaddr *)&ctx->q.local_addr,
                   &ctx->q.local_addrlen);
  if(rv == -1)
    return CURLE_QUIC_CONNECT_ERROR;

  ctx->qconn = quiche_conn_new_with_tls((const uint8_t *)ctx->scid,
                                      sizeof(ctx->scid), NULL, 0,
                                      (struct sockaddr *)&ctx->q.local_addr,
                                      ctx->q.local_addrlen,
                                      &sockaddr->sa_addr, sockaddr->addrlen,
                                      ctx->cfg, ctx->ssl, false);
  if(!ctx->qconn) {
    failf(data, "can't create quiche connection");
    return CURLE_OUT_OF_MEMORY;
  }

  /* Known to not work on Windows */
#if !defined(WIN32) && defined(HAVE_QUICHE_CONN_SET_QLOG_FD)
  {
    int qfd;
    (void)Curl_qlogdir(data, ctx->scid, sizeof(ctx->scid), &qfd);
    if(qfd != -1)
      quiche_conn_set_qlog_fd(ctx->qconn, qfd,
                              "qlog title", "curl qlog");
  }
#endif

  /* we do not get a setup event for the initial transfer */
  result = h3_data_setup(cf, data);
  if(result)
    return result;

  result = cf_flush_egress(cf, data);
  if(result)
    return result;

  {
    unsigned char alpn_protocols[] = QUICHE_H3_APPLICATION_PROTOCOL;
    unsigned alpn_len, offset = 0;

    /* Replace each ALPN length prefix by a comma. */
    while(offset < sizeof(alpn_protocols) - 1) {
      alpn_len = alpn_protocols[offset];
      alpn_protocols[offset] = ',';
      offset += 1 + alpn_len;
    }

    DEBUGF(LOG_CF(data, cf, "Sent QUIC client Initial, ALPN: %s",
                   alpn_protocols + 1));
  }

  return CURLE_OK;
}

static CURLcode cf_quiche_connect(struct Curl_cfilter *cf,
                                  struct Curl_easy *data,
                                  bool blocking, bool *done)
{
  struct cf_quiche_ctx *ctx = cf->ctx;
  CURLcode result = CURLE_OK;
  struct curltime now;

  if(cf->connected) {
    *done = TRUE;
    return CURLE_OK;
  }

  /* Connect the UDP filter first */
  if(!cf->next->connected) {
    result = Curl_conn_cf_connect(cf->next, data, blocking, done);
    if(result || !*done)
      return result;
  }

  *done = FALSE;
  now = Curl_now();

  if(ctx->reconnect_at.tv_sec && Curl_timediff(now, ctx->reconnect_at) < 0) {
    /* Not time yet to attempt the next connect */
    DEBUGF(LOG_CF(data, cf, "waiting for reconnect time"));
    goto out;
  }

  if(!ctx->qconn) {
    result = cf_connect_start(cf, data);
    if(result)
      goto out;
    ctx->started_at = now;
    result = cf_flush_egress(cf, data);
    /* we do not expect to be able to recv anything yet */
    goto out;
  }

  result = cf_process_ingress(cf, data);
  if(result)
    goto out;

  result = cf_flush_egress(cf, data);
  if(result)
    goto out;

  if(quiche_conn_is_established(ctx->qconn)) {
    DEBUGF(LOG_CF(data, cf, "handshake complete after %dms",
           (int)Curl_timediff(now, ctx->started_at)));
    ctx->handshake_at = now;
    result = cf_verify_peer(cf, data);
    if(!result) {
      DEBUGF(LOG_CF(data, cf, "peer verified"));
      cf->connected = TRUE;
      cf->conn->alpn = CURL_HTTP_VERSION_3;
      *done = TRUE;
      connkeep(cf->conn, "HTTP/3 default");
    }
  }
  else if(quiche_conn_is_draining(ctx->qconn)) {
    /* When a QUIC server instance is shutting down, it may send us a
     * CONNECTION_CLOSE right away. Our connection then enters the DRAINING
     * state.
     * This may be a stopping of the service or it may be that the server
     * is reloading and a new instance will start serving soon.
     * In any case, we tear down our socket and start over with a new one.
     * We re-open the underlying UDP cf right now, but do not start
     * connecting until called again.
     */
    int reconn_delay_ms = 200;

    DEBUGF(LOG_CF(data, cf, "connect, remote closed, reconnect after %dms",
                  reconn_delay_ms));
    Curl_conn_cf_close(cf->next, data);
    cf_quiche_ctx_clear(ctx);
    result = Curl_conn_cf_connect(cf->next, data, FALSE, done);
    if(!result && *done) {
      *done = FALSE;
      ctx->reconnect_at = Curl_now();
      ctx->reconnect_at.tv_usec += reconn_delay_ms * 1000;
      Curl_expire(data, reconn_delay_ms, EXPIRE_QUIC);
      result = CURLE_OK;
    }
  }

out:
#ifndef CURL_DISABLE_VERBOSE_STRINGS
  if(result && result != CURLE_AGAIN) {
    const char *r_ip;
    int r_port;

    Curl_cf_socket_peek(cf->next, data, NULL, NULL,
                        &r_ip, &r_port, NULL, NULL);
    infof(data, "connect to %s port %u failed: %s",
          r_ip, r_port, curl_easy_strerror(result));
  }
#endif
  return result;
}

static void cf_quiche_close(struct Curl_cfilter *cf, struct Curl_easy *data)
{
  struct cf_quiche_ctx *ctx = cf->ctx;

  if(ctx) {
    if(ctx->qconn) {
      (void)quiche_conn_close(ctx->qconn, TRUE, 0, NULL, 0);
      /* flushing the egress is not a failsafe way to deliver all the
         outstanding packets, but we also don't want to get stuck here... */
      (void)cf_flush_egress(cf, data);
    }
    cf_quiche_ctx_clear(ctx);
  }
}

static void cf_quiche_destroy(struct Curl_cfilter *cf, struct Curl_easy *data)
{
  struct cf_quiche_ctx *ctx = cf->ctx;

  (void)data;
  cf_quiche_ctx_clear(ctx);
  free(ctx);
  cf->ctx = NULL;
}

static CURLcode cf_quiche_query(struct Curl_cfilter *cf,
                                struct Curl_easy *data,
                                int query, int *pres1, void *pres2)
{
  struct cf_quiche_ctx *ctx = cf->ctx;

  switch(query) {
  case CF_QUERY_MAX_CONCURRENT: {
    uint64_t max_streams = CONN_INUSE(cf->conn);
    if(!ctx->goaway) {
      max_streams += quiche_conn_peer_streams_left_bidi(ctx->qconn);
    }
    *pres1 = (max_streams > INT_MAX)? INT_MAX : (int)max_streams;
    DEBUGF(LOG_CF(data, cf, "query: MAX_CONCURRENT -> %d", *pres1));
    return CURLE_OK;
  }
  case CF_QUERY_CONNECT_REPLY_MS:
    if(ctx->got_first_byte) {
      timediff_t ms = Curl_timediff(ctx->first_byte_at, ctx->started_at);
      *pres1 = (ms < INT_MAX)? (int)ms : INT_MAX;
    }
    else
      *pres1 = -1;
    return CURLE_OK;
  case CF_QUERY_TIMER_CONNECT: {
    struct curltime *when = pres2;
    if(ctx->got_first_byte)
      *when = ctx->first_byte_at;
    return CURLE_OK;
  }
  case CF_QUERY_TIMER_APPCONNECT: {
    struct curltime *when = pres2;
    if(cf->connected)
      *when = ctx->handshake_at;
    return CURLE_OK;
  }
  default:
    break;
  }
  return cf->next?
    cf->next->cft->query(cf->next, data, query, pres1, pres2) :
    CURLE_UNKNOWN_OPTION;
}

static bool cf_quiche_conn_is_alive(struct Curl_cfilter *cf,
                                    struct Curl_easy *data,
                                    bool *input_pending)
{
  bool alive = TRUE;

  *input_pending = FALSE;
  if(!cf->next || !cf->next->cft->is_alive(cf->next, data, input_pending))
    return FALSE;

  if(*input_pending) {
    /* This happens before we've sent off a request and the connection is
       not in use by any other transfer, there shouldn't be any data here,
       only "protocol frames" */
    *input_pending = FALSE;
    Curl_attach_connection(data, cf->conn);
    if(cf_process_ingress(cf, data))
      alive = FALSE;
    else {
      alive = TRUE;
    }
    Curl_detach_connection(data);
  }

  return alive;
}

struct Curl_cftype Curl_cft_http3 = {
  "HTTP/3",
  CF_TYPE_IP_CONNECT | CF_TYPE_SSL | CF_TYPE_MULTIPLEX,
  0,
  cf_quiche_destroy,
  cf_quiche_connect,
  cf_quiche_close,
  Curl_cf_def_get_host,
  cf_quiche_get_select_socks,
  cf_quiche_data_pending,
  cf_quiche_send,
  cf_quiche_recv,
  cf_quiche_data_event,
  cf_quiche_conn_is_alive,
  Curl_cf_def_conn_keep_alive,
  cf_quiche_query,
};

CURLcode Curl_cf_quiche_create(struct Curl_cfilter **pcf,
                               struct Curl_easy *data,
                               struct connectdata *conn,
                               const struct Curl_addrinfo *ai)
{
  struct cf_quiche_ctx *ctx = NULL;
  struct Curl_cfilter *cf = NULL, *udp_cf = NULL;
  CURLcode result;

  (void)data;
  (void)conn;
  ctx = calloc(sizeof(*ctx), 1);
  if(!ctx) {
    result = CURLE_OUT_OF_MEMORY;
    goto out;
  }

  result = Curl_cf_create(&cf, &Curl_cft_http3, ctx);
  if(result)
    goto out;

  result = Curl_cf_udp_create(&udp_cf, data, conn, ai, TRNSPRT_QUIC);
  if(result)
    goto out;

  udp_cf->conn = cf->conn;
  udp_cf->sockindex = cf->sockindex;
  cf->next = udp_cf;

out:
  *pcf = (!result)? cf : NULL;
  if(result) {
    if(udp_cf)
      Curl_conn_cf_discard_sub(cf, udp_cf, data, TRUE);
    Curl_safefree(cf);
    Curl_safefree(ctx);
  }

  return result;
}

bool Curl_conn_is_quiche(const struct Curl_easy *data,
                         const struct connectdata *conn,
                         int sockindex)
{
  struct Curl_cfilter *cf = conn? conn->cfilter[sockindex] : NULL;

  (void)data;
  for(; cf; cf = cf->next) {
    if(cf->cft == &Curl_cft_http3)
      return TRUE;
    if(cf->cft->flags & CF_TYPE_IP_CONNECT)
      return FALSE;
  }
  return FALSE;
}

#endif
