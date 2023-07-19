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

#ifdef USE_MSH3

#include "urldata.h"
#include "timeval.h"
#include "multiif.h"
#include "sendf.h"
#include "curl_log.h"
#include "cfilters.h"
#include "cf-socket.h"
#include "connect.h"
#include "progress.h"
#include "http1.h"
#include "curl_msh3.h"
#include "socketpair.h"
#include "vquic/vquic.h"

/* The last 3 #include files should be in this order */
#include "curl_printf.h"
#include "curl_memory.h"
#include "memdebug.h"

#define H3_STREAM_WINDOW_SIZE (128 * 1024)
#define H3_STREAM_CHUNK_SIZE   (16 * 1024)
#define H3_STREAM_RECV_CHUNKS \
          (H3_STREAM_WINDOW_SIZE / H3_STREAM_CHUNK_SIZE)

#ifdef _WIN32
#define msh3_lock CRITICAL_SECTION
#define msh3_lock_initialize(lock) InitializeCriticalSection(lock)
#define msh3_lock_uninitialize(lock) DeleteCriticalSection(lock)
#define msh3_lock_acquire(lock) EnterCriticalSection(lock)
#define msh3_lock_release(lock) LeaveCriticalSection(lock)
#else /* !_WIN32 */
#include <pthread.h>
#define msh3_lock pthread_mutex_t
#define msh3_lock_initialize(lock) do { \
  pthread_mutexattr_t attr; \
  pthread_mutexattr_init(&attr); \
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); \
  pthread_mutex_init(lock, &attr); \
  pthread_mutexattr_destroy(&attr); \
}while(0)
#define msh3_lock_uninitialize(lock) pthread_mutex_destroy(lock)
#define msh3_lock_acquire(lock) pthread_mutex_lock(lock)
#define msh3_lock_release(lock) pthread_mutex_unlock(lock)
#endif /* _WIN32 */


static void MSH3_CALL msh3_conn_connected(MSH3_CONNECTION *Connection,
                                          void *IfContext);
static void MSH3_CALL msh3_conn_shutdown_complete(MSH3_CONNECTION *Connection,
                                          void *IfContext);
static void MSH3_CALL msh3_conn_new_request(MSH3_CONNECTION *Connection,
                                          void *IfContext,
                                          MSH3_REQUEST *Request);
static void MSH3_CALL msh3_header_received(MSH3_REQUEST *Request,
                                           void *IfContext,
                                           const MSH3_HEADER *Header);
static bool MSH3_CALL msh3_data_received(MSH3_REQUEST *Request,
                                        void *IfContext, uint32_t *Length,
                                        const uint8_t *Data);
static void MSH3_CALL msh3_complete(MSH3_REQUEST *Request, void *IfContext,
                                    bool Aborted, uint64_t AbortError);
static void MSH3_CALL msh3_shutdown_complete(MSH3_REQUEST *Request,
                                             void *IfContext);
static void MSH3_CALL msh3_data_sent(MSH3_REQUEST *Request,
                                     void *IfContext, void *SendContext);


void Curl_msh3_ver(char *p, size_t len)
{
  uint32_t v[4];
  MsH3Version(v);
  (void)msnprintf(p, len, "msh3/%d.%d.%d.%d", v[0], v[1], v[2], v[3]);
}

#define SP_LOCAL   0
#define SP_REMOTE  1

struct cf_msh3_ctx {
  MSH3_API *api;
  MSH3_CONNECTION *qconn;
  struct Curl_sockaddr_ex addr;
  curl_socket_t sock[2]; /* fake socket pair until we get support in msh3 */
  char l_ip[MAX_IPADR_LEN];          /* local IP as string */
  int l_port;                        /* local port number */
  struct cf_call_data call_data;
  struct curltime connect_started;   /* time the current attempt started */
  struct curltime handshake_at;      /* time connect handshake finished */
  /* Flags written by msh3/msquic thread */
  bool handshake_complete;
  bool handshake_succeeded;
  bool connected;
  /* Flags written by curl thread */
  BIT(verbose);
  BIT(active);
};

/* How to access `call_data` from a cf_msh3 filter */
#define CF_CTX_CALL_DATA(cf)  \
  ((struct cf_msh3_ctx *)(cf)->ctx)->call_data

/**
 * All about the H3 internals of a stream
 */
struct stream_ctx {
  struct MSH3_REQUEST *req;
  struct bufq recvbuf;   /* h3 response */
#ifdef _WIN32
  CRITICAL_SECTION recv_lock;
#else /* !_WIN32 */
  pthread_mutex_t recv_lock;
#endif /* _WIN32 */
  uint64_t error3; /* HTTP/3 stream error code */
  int status_code; /* HTTP status code */
  CURLcode recv_error;
  bool closed;
  bool reset;
  bool upload_done;
  bool firstheader;  /* FALSE until headers arrive */
  bool recv_header_complete;
};

#define H3_STREAM_CTX(d)    ((struct stream_ctx *)(((d) && (d)->req.p.http)? \
                             ((struct HTTP *)(d)->req.p.http)->h3_ctx \
                               : NULL))
#define H3_STREAM_LCTX(d)   ((struct HTTP *)(d)->req.p.http)->h3_ctx
#define H3_STREAM_ID(d)     (H3_STREAM_CTX(d)? \
                             H3_STREAM_CTX(d)->id : -2)


static CURLcode h3_data_setup(struct Curl_cfilter *cf,
                              struct Curl_easy *data)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);

  if(stream)
    return CURLE_OK;

  stream = calloc(1, sizeof(*stream));
  if(!stream)
    return CURLE_OUT_OF_MEMORY;

  H3_STREAM_LCTX(data) = stream;
  stream->req = ZERO_NULL;
  msh3_lock_initialize(&stream->recv_lock);
  Curl_bufq_init2(&stream->recvbuf, H3_STREAM_CHUNK_SIZE,
                  H3_STREAM_RECV_CHUNKS, BUFQ_OPT_SOFT_LIMIT);
  DEBUGF(LOG_CF(data, cf, "data setup (easy %p)", (void *)data));
  return CURLE_OK;
}

static void h3_data_done(struct Curl_cfilter *cf, struct Curl_easy *data)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);

  (void)cf;
  if(stream) {
    DEBUGF(LOG_CF(data, cf, "easy handle is done"));
    Curl_bufq_free(&stream->recvbuf);
    free(stream);
    H3_STREAM_LCTX(data) = NULL;
  }
}

static void drain_stream_from_other_thread(struct Curl_easy *data,
                                           struct stream_ctx *stream)
{
  unsigned char bits;

  /* risky */
  bits = CURL_CSELECT_IN;
  if(stream && !stream->upload_done)
    bits |= CURL_CSELECT_OUT;
  if(data->state.dselect_bits != bits) {
    data->state.dselect_bits = bits;
    /* cannot expire from other thread */
  }
}

static void drain_stream(struct Curl_cfilter *cf,
                         struct Curl_easy *data)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  unsigned char bits;

  (void)cf;
  bits = CURL_CSELECT_IN;
  if(stream && !stream->upload_done)
    bits |= CURL_CSELECT_OUT;
  if(data->state.dselect_bits != bits) {
    data->state.dselect_bits = bits;
    Curl_expire(data, 0, EXPIRE_RUN_NOW);
  }
}

static const MSH3_CONNECTION_IF msh3_conn_if = {
  msh3_conn_connected,
  msh3_conn_shutdown_complete,
  msh3_conn_new_request
};

static void MSH3_CALL msh3_conn_connected(MSH3_CONNECTION *Connection,
                                          void *IfContext)
{
  struct Curl_cfilter *cf = IfContext;
  struct cf_msh3_ctx *ctx = cf->ctx;
  struct Curl_easy *data = CF_DATA_CURRENT(cf);
  (void)Connection;

  DEBUGF(LOG_CF(data, cf, "[MSH3] connected"));
  ctx->handshake_succeeded = true;
  ctx->connected = true;
  ctx->handshake_complete = true;
}

static void MSH3_CALL msh3_conn_shutdown_complete(MSH3_CONNECTION *Connection,
                                          void *IfContext)
{
  struct Curl_cfilter *cf = IfContext;
  struct cf_msh3_ctx *ctx = cf->ctx;
  struct Curl_easy *data = CF_DATA_CURRENT(cf);

  (void)Connection;
  DEBUGF(LOG_CF(data, cf, "[MSH3] shutdown complete"));
  ctx->connected = false;
  ctx->handshake_complete = true;
}

static void MSH3_CALL msh3_conn_new_request(MSH3_CONNECTION *Connection,
                                          void *IfContext,
                                          MSH3_REQUEST *Request)
{
  (void)Connection;
  (void)IfContext;
  (void)Request;
}

static const MSH3_REQUEST_IF msh3_request_if = {
  msh3_header_received,
  msh3_data_received,
  msh3_complete,
  msh3_shutdown_complete,
  msh3_data_sent
};

/* Decode HTTP status code.  Returns -1 if no valid status code was
   decoded. (duplicate from http2.c) */
static int decode_status_code(const char *value, size_t len)
{
  int i;
  int res;

  if(len != 3) {
    return -1;
  }

  res = 0;

  for(i = 0; i < 3; ++i) {
    char c = value[i];

    if(c < '0' || c > '9') {
      return -1;
    }

    res *= 10;
    res += c - '0';
  }

  return res;
}

/*
 * write_resp_raw() copies response data in raw format to the `data`'s
  * receive buffer. If not enough space is available, it appends to the
 * `data`'s overflow buffer.
 */
static CURLcode write_resp_raw(struct Curl_easy *data,
                               const void *mem, size_t memlen)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  CURLcode result = CURLE_OK;
  ssize_t nwritten;

  if(!stream)
    return CURLE_RECV_ERROR;

  nwritten = Curl_bufq_write(&stream->recvbuf, mem, memlen, &result);
  if(nwritten < 0) {
    return result;
  }

  if((size_t)nwritten < memlen) {
    /* This MUST not happen. Our recbuf is dimensioned to hold the
     * full max_stream_window and then some for this very reason. */
    DEBUGASSERT(0);
    return CURLE_RECV_ERROR;
  }
  return result;
}

static void MSH3_CALL msh3_header_received(MSH3_REQUEST *Request,
                                           void *userp,
                                           const MSH3_HEADER *hd)
{
  struct Curl_easy *data = userp;
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  CURLcode result;
  (void)Request;

  if(!stream || stream->recv_header_complete) {
    return;
  }

  msh3_lock_acquire(&stream->recv_lock);

  if((hd->NameLength == 7) &&
     !strncmp(HTTP_PSEUDO_STATUS, (char *)hd->Name, 7)) {
    char line[14]; /* status line is always 13 characters long */
    size_t ncopy;

    DEBUGASSERT(!stream->firstheader);
    stream->status_code = decode_status_code(hd->Value, hd->ValueLength);
    DEBUGASSERT(stream->status_code != -1);
    ncopy = msnprintf(line, sizeof(line), "HTTP/3 %03d \r\n",
                      stream->status_code);
    result = write_resp_raw(data, line, ncopy);
    if(result)
      stream->recv_error = result;
    stream->firstheader = TRUE;
  }
  else {
    /* store as an HTTP1-style header */
    DEBUGASSERT(stream->firstheader);
    result = write_resp_raw(data, hd->Name, hd->NameLength);
    if(!result)
      result = write_resp_raw(data, ": ", 2);
    if(!result)
      result = write_resp_raw(data, hd->Value, hd->ValueLength);
    if(!result)
      result = write_resp_raw(data, "\r\n", 2);
    if(result) {
      stream->recv_error = result;
    }
  }

  drain_stream_from_other_thread(data, stream);
  msh3_lock_release(&stream->recv_lock);
}

static bool MSH3_CALL msh3_data_received(MSH3_REQUEST *Request,
                                         void *IfContext, uint32_t *buflen,
                                         const uint8_t *buf)
{
  struct Curl_easy *data = IfContext;
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  CURLcode result;
  bool rv = FALSE;

  /* TODO: we would like to limit the amount of data we are buffer here.
   * There seems to be no mechanism in msh3 to adjust flow control and
   * it is undocumented what happens if we return FALSE here or less
   * length (buflen is an inout parameter).
   */
  (void)Request;
  if(!stream)
    return FALSE;

  msh3_lock_acquire(&stream->recv_lock);

  if(!stream->recv_header_complete) {
    result = write_resp_raw(data, "\r\n", 2);
    if(result) {
      stream->recv_error = result;
      goto out;
    }
    stream->recv_header_complete = true;
  }

  result = write_resp_raw(data, buf, *buflen);
  if(result) {
    stream->recv_error = result;
  }
  rv = TRUE;

out:
  msh3_lock_release(&stream->recv_lock);
  return rv;
}

static void MSH3_CALL msh3_complete(MSH3_REQUEST *Request, void *IfContext,
                                    bool aborted, uint64_t error)
{
  struct Curl_easy *data = IfContext;
  struct stream_ctx *stream = H3_STREAM_CTX(data);

  (void)Request;
  if(!stream)
    return;
  msh3_lock_acquire(&stream->recv_lock);
  stream->closed = TRUE;
  stream->recv_header_complete = true;
  if(error)
    stream->error3 = error;
  if(aborted)
    stream->reset = TRUE;
  msh3_lock_release(&stream->recv_lock);
}

static void MSH3_CALL msh3_shutdown_complete(MSH3_REQUEST *Request,
                                             void *IfContext)
{
  struct Curl_easy *data = IfContext;
  struct stream_ctx *stream = H3_STREAM_CTX(data);

  if(!stream)
    return;
  (void)Request;
  (void)stream;
}

static void MSH3_CALL msh3_data_sent(MSH3_REQUEST *Request,
                                     void *IfContext, void *SendContext)
{
  struct Curl_easy *data = IfContext;
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  if(!stream)
    return;
  (void)Request;
  (void)stream;
  (void)SendContext;
}

static ssize_t recv_closed_stream(struct Curl_cfilter *cf,
                                  struct Curl_easy *data,
                                  CURLcode *err)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  ssize_t nread = -1;

  if(!stream) {
    *err = CURLE_RECV_ERROR;
    return -1;
  }
  (void)cf;
  if(stream->reset) {
    failf(data, "HTTP/3 stream reset by server");
    *err = CURLE_PARTIAL_FILE;
    DEBUGF(LOG_CF(data, cf, "cf_recv, was reset -> %d", *err));
    goto out;
  }
  else if(stream->error3) {
    failf(data, "HTTP/3 stream was not closed cleanly: (error %zd)",
          (ssize_t)stream->error3);
    *err = CURLE_HTTP3;
    DEBUGF(LOG_CF(data, cf, "cf_recv, closed uncleanly -> %d", *err));
    goto out;
  }
  else {
    DEBUGF(LOG_CF(data, cf, "cf_recv, closed ok -> %d", *err));
  }
  *err = CURLE_OK;
  nread = 0;

out:
  return nread;
}

static void set_quic_expire(struct Curl_cfilter *cf, struct Curl_easy *data)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);

  /* we have no indication from msh3 when it would be a good time
   * to juggle the connection again. So, we compromise by calling
   * us again every some milliseconds. */
  (void)cf;
  if(stream && stream->req && !stream->closed) {
    Curl_expire(data, 10, EXPIRE_QUIC);
  }
  else {
    Curl_expire(data, 50, EXPIRE_QUIC);
  }
}

static ssize_t cf_msh3_recv(struct Curl_cfilter *cf, struct Curl_easy *data,
                            char *buf, size_t len, CURLcode *err)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  ssize_t nread = -1;
  struct cf_call_data save;

  (void)cf;
  if(!stream) {
    *err = CURLE_RECV_ERROR;
    return -1;
  }
  CF_DATA_SAVE(save, cf, data);
  DEBUGF(LOG_CF(data, cf, "req: recv with %zu byte buffer", len));

  msh3_lock_acquire(&stream->recv_lock);

  if(stream->recv_error) {
    failf(data, "request aborted");
    *err = stream->recv_error;
    goto out;
  }

  *err = CURLE_OK;

  if(!Curl_bufq_is_empty(&stream->recvbuf)) {
    nread = Curl_bufq_read(&stream->recvbuf,
                           (unsigned char *)buf, len, err);
    DEBUGF(LOG_CF(data, cf, "read recvbuf(len=%zu) -> %zd, %d",
                  len, nread, *err));
    if(nread < 0)
      goto out;
    if(stream->closed)
      drain_stream(cf, data);
  }
  else if(stream->closed) {
    nread = recv_closed_stream(cf, data, err);
    goto out;
  }
  else {
    DEBUGF(LOG_CF(data, cf, "req: nothing here, call again"));
    *err = CURLE_AGAIN;
  }

out:
  msh3_lock_release(&stream->recv_lock);
  set_quic_expire(cf, data);
  CF_DATA_RESTORE(cf, save);
  return nread;
}

static ssize_t cf_msh3_send(struct Curl_cfilter *cf, struct Curl_easy *data,
                            const void *buf, size_t len, CURLcode *err)
{
  struct cf_msh3_ctx *ctx = cf->ctx;
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  struct h1_req_parser h1;
  struct dynhds h2_headers;
  MSH3_HEADER *nva = NULL;
  size_t nheader, i;
  ssize_t nwritten = -1;
  struct cf_call_data save;
  bool eos;

  CF_DATA_SAVE(save, cf, data);

  Curl_h1_req_parse_init(&h1, H1_PARSE_DEFAULT_MAX_LINE_LEN);
  Curl_dynhds_init(&h2_headers, 0, DYN_HTTP_REQUEST);

  /* Sizes must match for cast below to work" */
  DEBUGASSERT(stream);
  DEBUGF(LOG_CF(data, cf, "req: send %zu bytes", len));

  if(!stream->req) {
    /* The first send on the request contains the headers and possibly some
       data. Parse out the headers and create the request, then if there is
       any data left over go ahead and send it too. */
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
    nva = malloc(sizeof(MSH3_HEADER) * nheader);
    if(!nva) {
      *err = CURLE_OUT_OF_MEMORY;
      nwritten = -1;
      goto out;
    }

    for(i = 0; i < nheader; ++i) {
      struct dynhds_entry *e = Curl_dynhds_getn(&h2_headers, i);
      nva[i].Name = e->name;
      nva[i].NameLength = e->namelen;
      nva[i].Value = e->value;
      nva[i].ValueLength = e->valuelen;
    }

    switch(data->state.httpreq) {
    case HTTPREQ_POST:
    case HTTPREQ_POST_FORM:
    case HTTPREQ_POST_MIME:
    case HTTPREQ_PUT:
      /* known request body size or -1 */
      eos = FALSE;
      break;
    default:
      /* there is not request body */
      eos = TRUE;
      stream->upload_done = TRUE;
      break;
    }

    DEBUGF(LOG_CF(data, cf, "req: send %zu headers", nheader));
    stream->req = MsH3RequestOpen(ctx->qconn, &msh3_request_if, data,
                                  nva, nheader,
                                  eos ? MSH3_REQUEST_FLAG_FIN :
                                  MSH3_REQUEST_FLAG_NONE);
    if(!stream->req) {
      failf(data, "request open failed");
      *err = CURLE_SEND_ERROR;
      goto out;
    }
    *err = CURLE_OK;
    nwritten = len;
    goto out;
  }
  else {
    /* request is open */
    DEBUGF(LOG_CF(data, cf, "req: send %zd body bytes", len));
    if(len > 0xFFFFFFFF) {
      len = 0xFFFFFFFF;
    }

    if(!MsH3RequestSend(stream->req, MSH3_REQUEST_FLAG_NONE, buf,
                        (uint32_t)len, stream)) {
      *err = CURLE_SEND_ERROR;
      goto out;
    }

    /* TODO - msh3/msquic will hold onto this memory until the send complete
       event. How do we make sure curl doesn't free it until then? */
    *err = CURLE_OK;
    nwritten = len;
  }

out:
  set_quic_expire(cf, data);
  free(nva);
  Curl_h1_req_parse_free(&h1);
  Curl_dynhds_free(&h2_headers);
  CF_DATA_RESTORE(cf, save);
  return nwritten;
}

static int cf_msh3_get_select_socks(struct Curl_cfilter *cf,
                                    struct Curl_easy *data,
                                    curl_socket_t *socks)
{
  struct cf_msh3_ctx *ctx = cf->ctx;
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  int bitmap = GETSOCK_BLANK;
  struct cf_call_data save;

  CF_DATA_SAVE(save, cf, data);
  if(stream && ctx->sock[SP_LOCAL] != CURL_SOCKET_BAD) {
    socks[0] = ctx->sock[SP_LOCAL];

    if(stream->recv_error) {
      bitmap |= GETSOCK_READSOCK(0);
      drain_stream(cf, data);
    }
    else if(stream->req) {
      bitmap |= GETSOCK_READSOCK(0);
      drain_stream(cf, data);
    }
  }
  DEBUGF(LOG_CF(data, cf, "select_sock -> %d", bitmap));
  CF_DATA_RESTORE(cf, save);
  return bitmap;
}

static bool cf_msh3_data_pending(struct Curl_cfilter *cf,
                                 const struct Curl_easy *data)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  struct cf_call_data save;
  bool pending = FALSE;

  CF_DATA_SAVE(save, cf, data);

  (void)cf;
  if(stream && stream->req) {
    msh3_lock_acquire(&stream->recv_lock);
    DEBUGF(LOG_CF((struct Curl_easy *)data, cf, "data pending = %zu",
                  Curl_bufq_len(&stream->recvbuf)));
    pending = !Curl_bufq_is_empty(&stream->recvbuf);
    msh3_lock_release(&stream->recv_lock);
    if(pending)
      drain_stream(cf, (struct Curl_easy *)data);
  }

  CF_DATA_RESTORE(cf, save);
  return pending;
}

static void cf_msh3_active(struct Curl_cfilter *cf, struct Curl_easy *data)
{
  struct cf_msh3_ctx *ctx = cf->ctx;

  /* use this socket from now on */
  cf->conn->sock[cf->sockindex] = ctx->sock[SP_LOCAL];
  /* the first socket info gets set at conn and data */
  if(cf->sockindex == FIRSTSOCKET) {
    cf->conn->remote_addr = &ctx->addr;
  #ifdef ENABLE_IPV6
    cf->conn->bits.ipv6 = (ctx->addr.family == AF_INET6)? TRUE : FALSE;
  #endif
    Curl_persistconninfo(data, cf->conn, ctx->l_ip, ctx->l_port);
  }
  ctx->active = TRUE;
}

static CURLcode h3_data_pause(struct Curl_cfilter *cf,
                              struct Curl_easy *data,
                              bool pause)
{
  if(!pause) {
    drain_stream(cf, data);
    Curl_expire(data, 0, EXPIRE_RUN_NOW);
  }
  return CURLE_OK;
}

static CURLcode cf_msh3_data_event(struct Curl_cfilter *cf,
                                   struct Curl_easy *data,
                                   int event, int arg1, void *arg2)
{
  struct stream_ctx *stream = H3_STREAM_CTX(data);
  struct cf_call_data save;
  CURLcode result = CURLE_OK;

  CF_DATA_SAVE(save, cf, data);

  (void)arg1;
  (void)arg2;
  switch(event) {
  case CF_CTRL_DATA_SETUP:
    result = h3_data_setup(cf, data);
    break;
  case CF_CTRL_DATA_PAUSE:
    result = h3_data_pause(cf, data, (arg1 != 0));
    break;
  case CF_CTRL_DATA_DONE:
    h3_data_done(cf, data);
    break;
  case CF_CTRL_DATA_DONE_SEND:
    DEBUGF(LOG_CF(data, cf, "req: send done"));
    if(stream) {
      stream->upload_done = TRUE;
      if(stream->req) {
        char buf[1];
        if(!MsH3RequestSend(stream->req, MSH3_REQUEST_FLAG_FIN,
                            buf, 0, data)) {
          result = CURLE_SEND_ERROR;
        }
      }
    }
    break;
  case CF_CTRL_CONN_INFO_UPDATE:
    DEBUGF(LOG_CF(data, cf, "req: update info"));
    cf_msh3_active(cf, data);
    break;
  default:
    break;
  }

  CF_DATA_RESTORE(cf, save);
  return result;
}

static CURLcode cf_connect_start(struct Curl_cfilter *cf,
                                 struct Curl_easy *data)
{
  struct cf_msh3_ctx *ctx = cf->ctx;
  bool verify = !!cf->conn->ssl_config.verifypeer;
  MSH3_ADDR addr = {0};
  CURLcode result;

  memcpy(&addr, &ctx->addr.sa_addr, ctx->addr.addrlen);
  MSH3_SET_PORT(&addr, (uint16_t)cf->conn->remote_port);

  if(verify && (cf->conn->ssl_config.CAfile || cf->conn->ssl_config.CApath)) {
    /* TODO: need a way to provide trust anchors to MSH3 */
#ifdef DEBUGBUILD
    /* we need this for our test cases to run */
    DEBUGF(LOG_CF(data, cf, "non-standard CA not supported, "
                  "switching off verifypeer in DEBUG mode"));
    verify = 0;
#else
    DEBUGF(LOG_CF(data, cf, "non-standard CA not supported, "
                  "attempting with built-in verification"));
#endif
  }

  DEBUGF(LOG_CF(data, cf, "connecting to %s:%d (verify=%d)",
                cf->conn->host.name, (int)cf->conn->remote_port, verify));

  ctx->api = MsH3ApiOpen();
  if(!ctx->api) {
    failf(data, "can't create msh3 api");
    return CURLE_FAILED_INIT;
  }

  ctx->qconn = MsH3ConnectionOpen(ctx->api,
                                  &msh3_conn_if,
                                  cf,
                                  cf->conn->host.name,
                                  &addr,
                                  !verify);
  if(!ctx->qconn) {
    failf(data, "can't create msh3 connection");
    if(ctx->api) {
      MsH3ApiClose(ctx->api);
      ctx->api = NULL;
    }
    return CURLE_FAILED_INIT;
  }

  result = h3_data_setup(cf, data);
  if(result)
    return result;

  return CURLE_OK;
}

static CURLcode cf_msh3_connect(struct Curl_cfilter *cf,
                                struct Curl_easy *data,
                                bool blocking, bool *done)
{
  struct cf_msh3_ctx *ctx = cf->ctx;
  struct cf_call_data save;
  CURLcode result = CURLE_OK;

  (void)blocking;
  if(cf->connected) {
    *done = TRUE;
    return CURLE_OK;
  }

  CF_DATA_SAVE(save, cf, data);

  if(ctx->sock[SP_LOCAL] == CURL_SOCKET_BAD) {
    if(Curl_socketpair(AF_UNIX, SOCK_STREAM, 0, &ctx->sock[0]) < 0) {
      ctx->sock[SP_LOCAL] = CURL_SOCKET_BAD;
      ctx->sock[SP_REMOTE] = CURL_SOCKET_BAD;
      return CURLE_COULDNT_CONNECT;
    }
  }

  *done = FALSE;
  if(!ctx->qconn) {
    ctx->connect_started = Curl_now();
    result = cf_connect_start(cf, data);
    if(result)
      goto out;
  }

  if(ctx->handshake_complete) {
    ctx->handshake_at = Curl_now();
    if(ctx->handshake_succeeded) {
      DEBUGF(LOG_CF(data, cf, "handshake succeeded"));
      cf->conn->bits.multiplex = TRUE; /* at least potentially multiplexed */
      cf->conn->httpversion = 30;
      cf->conn->bundle->multiuse = BUNDLE_MULTIPLEX;
      cf->connected = TRUE;
      cf->conn->alpn = CURL_HTTP_VERSION_3;
      *done = TRUE;
      connkeep(cf->conn, "HTTP/3 default");
      Curl_pgrsTime(data, TIMER_APPCONNECT);
    }
    else {
      failf(data, "failed to connect, handshake failed");
      result = CURLE_COULDNT_CONNECT;
    }
  }

out:
  CF_DATA_RESTORE(cf, save);
  return result;
}

static void cf_msh3_close(struct Curl_cfilter *cf, struct Curl_easy *data)
{
  struct cf_msh3_ctx *ctx = cf->ctx;
  struct cf_call_data save;

  (void)data;
  CF_DATA_SAVE(save, cf, data);

  if(ctx) {
    DEBUGF(LOG_CF(data, cf, "destroying"));
    if(ctx->qconn) {
      MsH3ConnectionClose(ctx->qconn);
      ctx->qconn = NULL;
    }
    if(ctx->api) {
      MsH3ApiClose(ctx->api);
      ctx->api = NULL;
    }

    if(ctx->active) {
      /* We share our socket at cf->conn->sock[cf->sockindex] when active.
       * If it is no longer there, someone has stolen (and hopefully
       * closed it) and we just forget about it.
       */
      ctx->active = FALSE;
      if(ctx->sock[SP_LOCAL] == cf->conn->sock[cf->sockindex]) {
        DEBUGF(LOG_CF(data, cf, "cf_msh3_close(%d) active",
                      (int)ctx->sock[SP_LOCAL]));
        cf->conn->sock[cf->sockindex] = CURL_SOCKET_BAD;
      }
      else {
        DEBUGF(LOG_CF(data, cf, "cf_socket_close(%d) no longer at "
                      "conn->sock[], discarding", (int)ctx->sock[SP_LOCAL]));
        ctx->sock[SP_LOCAL] = CURL_SOCKET_BAD;
      }
      if(cf->sockindex == FIRSTSOCKET)
        cf->conn->remote_addr = NULL;
    }
    if(ctx->sock[SP_LOCAL] != CURL_SOCKET_BAD) {
      sclose(ctx->sock[SP_LOCAL]);
    }
    if(ctx->sock[SP_REMOTE] != CURL_SOCKET_BAD) {
      sclose(ctx->sock[SP_REMOTE]);
    }
    ctx->sock[SP_LOCAL] = CURL_SOCKET_BAD;
    ctx->sock[SP_REMOTE] = CURL_SOCKET_BAD;
  }
  CF_DATA_RESTORE(cf, save);
}

static void cf_msh3_destroy(struct Curl_cfilter *cf, struct Curl_easy *data)
{
  struct cf_call_data save;

  CF_DATA_SAVE(save, cf, data);
  cf_msh3_close(cf, data);
  free(cf->ctx);
  cf->ctx = NULL;
  /* no CF_DATA_RESTORE(cf, save); its gone */

}

static CURLcode cf_msh3_query(struct Curl_cfilter *cf,
                              struct Curl_easy *data,
                              int query, int *pres1, void *pres2)
{
  struct cf_msh3_ctx *ctx = cf->ctx;

  switch(query) {
  case CF_QUERY_MAX_CONCURRENT: {
    /* TODO: we do not have access to this so far, fake it */
    (void)ctx;
    *pres1 = 100;
    return CURLE_OK;
  }
  case CF_QUERY_TIMER_CONNECT: {
    struct curltime *when = pres2;
    /* we do not know when the first byte arrived */
    if(cf->connected)
      *when = ctx->handshake_at;
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

static bool cf_msh3_conn_is_alive(struct Curl_cfilter *cf,
                                  struct Curl_easy *data,
                                  bool *input_pending)
{
  struct cf_msh3_ctx *ctx = cf->ctx;

  (void)data;
  *input_pending = FALSE;
  return ctx && ctx->sock[SP_LOCAL] != CURL_SOCKET_BAD && ctx->qconn &&
         ctx->connected;
}

struct Curl_cftype Curl_cft_http3 = {
  "HTTP/3",
  CF_TYPE_IP_CONNECT | CF_TYPE_SSL | CF_TYPE_MULTIPLEX,
  0,
  cf_msh3_destroy,
  cf_msh3_connect,
  cf_msh3_close,
  Curl_cf_def_get_host,
  cf_msh3_get_select_socks,
  cf_msh3_data_pending,
  cf_msh3_send,
  cf_msh3_recv,
  cf_msh3_data_event,
  cf_msh3_conn_is_alive,
  Curl_cf_def_conn_keep_alive,
  cf_msh3_query,
};

CURLcode Curl_cf_msh3_create(struct Curl_cfilter **pcf,
                             struct Curl_easy *data,
                             struct connectdata *conn,
                             const struct Curl_addrinfo *ai)
{
  struct cf_msh3_ctx *ctx = NULL;
  struct Curl_cfilter *cf = NULL;
  CURLcode result;

  (void)data;
  (void)conn;
  (void)ai; /* TODO: msh3 resolves itself? */
  ctx = calloc(sizeof(*ctx), 1);
  if(!ctx) {
    result = CURLE_OUT_OF_MEMORY;
    goto out;
  }
  Curl_sock_assign_addr(&ctx->addr, ai, TRNSPRT_QUIC);
  ctx->sock[SP_LOCAL] = CURL_SOCKET_BAD;
  ctx->sock[SP_REMOTE] = CURL_SOCKET_BAD;

  result = Curl_cf_create(&cf, &Curl_cft_http3, ctx);

out:
  *pcf = (!result)? cf : NULL;
  if(result) {
    Curl_safefree(cf);
    Curl_safefree(ctx);
  }

  return result;
}

bool Curl_conn_is_msh3(const struct Curl_easy *data,
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

#endif /* USE_MSH3 */
