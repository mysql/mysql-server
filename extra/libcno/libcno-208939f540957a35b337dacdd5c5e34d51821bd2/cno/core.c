#include <ctype.h>
#include <stdio.h>

#include "core.h"
#include "picohttpparser/picohttpparser.h"

static inline uint32_t read4(const void *v) {
  const uint8_t *p = (const uint8_t *)v;
  return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

#define PACK(...)                                          \
  ((struct cno_buffer_t){(char *)(uint8_t[]){__VA_ARGS__}, \
                         sizeof((uint8_t[]){__VA_ARGS__})})
#define I8(x) (x)
#define I16(x) (x) >> 8, (x)
#define I24(x) (x) >> 16, (x) >> 8, (x)
#define I32(x) (x) >> 24, (x) >> 16, (x) >> 8, (x)

#define CNO_FIRE(ob, cb, ...)        \
  (ob->cb_code && ob->cb_code->cb && \
   ob->cb_code->cb(ob->cb_data, ##__VA_ARGS__))

#define CNO_WRITEV(c, ...)                                     \
  CNO_FIRE(c, on_writev, (struct cno_buffer_t[]){__VA_ARGS__}, \
           sizeof((struct cno_buffer_t[]){__VA_ARGS__}) /      \
               sizeof(struct cno_buffer_t))

enum CNO_CONNECTION_STATE {
  CNO_STATE_CLOSED,
  CNO_STATE_H2_INIT,
  CNO_STATE_H2_PREFACE,
  CNO_STATE_H2_SETTINGS,
  CNO_STATE_H2_FRAME,
  CNO_STATE_H1_HEAD,
  CNO_STATE_H1_UPGRADED,
  CNO_STATE_H1_BODY,
  CNO_STATE_H1_TAIL,
  CNO_STATE_H1_TRAILERS,
};

enum CNO_STREAM_STATE {
  CNO_STREAM_HEADERS,
  CNO_STREAM_DATA,
  CNO_STREAM_CLOSED,
};

struct cno_stream_t {
  struct cno_stream_t *next;  // in hashmap bucket
  uint32_t id;
  uint8_t refs;  // max. 4 = 1 from hash map + 1 from read + 1 from write + 1
                 // for http1 mode
  uint8_t /* enum CNO_STREAM_STATE */ r_state : 3;
  uint8_t /* enum CNO_STREAM_STATE */ w_state : 3;
  uint8_t reading_chunked : 1;
  uint8_t writing_chunked : 1;
  uint8_t head_response : 1;
  int64_t window[2];
  uint64_t remaining_payload;
};

static int cno_is_informational(int code) { return 100 <= code && code < 200; }

static uint64_t cno_parse_uint(struct cno_buffer_t value) {
  uint64_t prev = 0, ret = 0;
  for (const char *ptr = value.data, *end = ptr + value.size; ptr != end;
       ptr++, prev = ret)
    if (*ptr < '0' || '9' < *ptr || (ret = prev * 10 + (*ptr - '0')) < prev)
      return (uint64_t)-1;
  return ret;
}

// Each character that is valid in a header's name is mapped to its lowercase
// version. All other characters are mapped to 0. (Headers names are ASCII-only.
// Should be Latin-1, but screw that.)
static const char CNO_HEADER_TRANSFORM[256] =
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0!\0#$%&'"
    "\0\0*+\0-.\0"
    "0123456789\0\0\0\0\0\0\0abcdefghijklmnopqrstuvwxyz\0\0\0^_`"
    "abcdefghijklmnopqrstuvwxyz"
    "\0|\0~"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0";

static int cno_check_no_ctl(const struct cno_buffer_t buf) {
  for (uint8_t *p = (uint8_t *)buf.data, *e = p + buf.size; p != e; p++)
    if (*p < ' ')
      return CNO_ERROR(ASSERTION, "invalid byte '%d' in header value", *p);
  return CNO_OK;
}

// Even streams are server-intitiated (pushes), odd streams are client-initiated
// (requests).
static inline int cno_stream_is_local(const struct cno_connection_t *c,
                                      uint32_t sid) {
  return sid % 2 == c->client;
}

// Streams are reference-counted in order to allow concurrent read and write
// operations. For example, a write of data to stream can block in `on_writev`,
// and a concurrent `cno_consume` might handle an RST_STREAM frame that ends the
// stream. Reference counting guarantees that a woken `on_writev` will not
// use-after-free.
static inline void cno_stream_decref(struct cno_stream_t **sp) {
  if (*sp && !--(*sp)->refs) free(*sp);
}

#define CNO_STREAM_REF ATTRIBUTE((cleanup(cno_stream_decref)))

// Remove the stream from the map. Further write operations will fail with
// INVALID_STREAM. Unless recorded otherwise through `cno_stream_end_by_write`,
// receiving a frame on this stream is a protocol error.
static int cno_stream_end(struct cno_connection_t *c, struct cno_stream_t *s,
                          uint32_t code, enum CNO_PEER_KIND side) {
  // Read the stream ID before removing the map's reference in case the caller
  // does not hold one.
  uint32_t sid = s->id;
  struct cno_stream_t **sp = &c->streams[sid % CNO_STREAM_BUCKETS];
  while (*sp && *sp != s) sp = &(*sp)->next;
  if (!*sp) return CNO_OK;  // already ended
  *sp = s->next;
  cno_stream_decref(&s);
  c->stream_count[cno_stream_is_local(c, sid)]--;
  if (CNO_FIRE(c, on_stream_end, sid, code, side)) return CNO_ERROR_UP();
  return c->goaway[CNO_REMOTE] == (uint32_t)-1 &&
                 c->goaway[CNO_LOCAL] == (uint32_t)-1
             ? CNO_OK
             // Seems to be a recurring pattern...
             : c->stream_count[CNO_LOCAL] + c->stream_count[CNO_REMOTE]
                   ? CNO_OK
                   : CNO_FIRE(c, on_close);
}

// Insert a new stream into the map, return a valid reference. May return
// WOULD_BLOCK if attempting to create a new request stream as a client or h1
// server when the limit on concurrent streams has been reached; in that case,
// wait for any stream's termination.
static struct cno_stream_t *cno_stream_new(struct cno_connection_t *c,
                                           uint32_t sid, int local) {
  if (cno_stream_is_local(c, sid) != local)
    return (local ? CNO_ERROR(ASSERTION, "incorrect stream id parity")
                  : CNO_ERROR(PROTOCOL, "incorrect stream id parity")),
           NULL;
  if (sid <= c->last_stream[local])
    return (local ? CNO_ERROR(ASSERTION, "nonmonotonic stream id")
                  : CNO_ERROR(PROTOCOL, "nonmonotonic stream id")),
           NULL;
  if (c->stream_count[local] >= c->settings[!local].max_concurrent_streams)
    // In h1 mode, the client is not aware of the limit, so we simply stop
    // reading for now.
    return (local || c->mode != CNO_HTTP2
                ? CNO_ERROR(WOULD_BLOCK, "wait for on_stream_end")
                : CNO_ERROR(PROTOCOL, "peer exceeded stream limit")),
           NULL;

  struct cno_stream_t *s = malloc(sizeof(struct cno_stream_t));
  if (!s)
    return CNO_ERROR(NO_MEMORY, "%zu bytes", sizeof(struct cno_stream_t)), NULL;
  *s = (struct cno_stream_t){
      .id = c->last_stream[local] = sid,
      .next = c->streams[sid % CNO_STREAM_BUCKETS],
      .refs =
          2,  // 1 from the hash map, 1 from this function (returned to caller).
      .r_state = sid % 2 || !local ? CNO_STREAM_HEADERS : CNO_STREAM_CLOSED,
      .w_state = sid % 2 || local ? CNO_STREAM_HEADERS : CNO_STREAM_CLOSED,
  };

  // Gotta love C for not having any standard library to speak of.
  c->streams[sid % CNO_STREAM_BUCKETS] = s;
  c->stream_count[local]++;
  if (CNO_FIRE(c, on_stream_start, sid)) {
    cno_stream_end(c, s, CNO_RST_INTERNAL_ERROR, CNO_LOCAL);
    cno_stream_decref(&s);
    return (void)CNO_ERROR_UP(), NULL;
  }
  return s;
}

// Return a new reference to a stream if it's still in the map, NULL otherwise.
static struct cno_stream_t *cno_stream_find(const struct cno_connection_t *c,
                                            uint32_t sid) {
  struct cno_stream_t *s = c->streams[sid % CNO_STREAM_BUCKETS];
  while (s && s->id != sid) s = s->next;
  if (s) s->refs++;
  return s;
}

// Call `on_writev` with a single h2 frame; do not check the size or attempt to
// split.
static int cno_frame_write(struct cno_connection_t *c,
                           const struct cno_frame_t *f) {
  return CNO_WRITEV(c,
                    PACK(I24(f->payload.size), I8(f->type), I8(f->flags),
                         I32(f->stream)),
                    f->payload) ||
         CNO_FIRE(c, on_frame_send, f);
}

// Call `on_writev` with a DATA frame, splitting it into multiple frames if over
// the size limit.
static int cno_frame_write_data(struct cno_connection_t *c,
                                const struct cno_frame_t *f) {
  struct cno_buffer_t part = f->payload;
  // Assume no two concurrent `cno_write_data` on one stream - atomicity not
  // required. `limit` intentionally reloaded in case `on_writev` blocks and a
  // new SETTINGS is ACKed.
  for (size_t limit;
       part.size > (limit = c->settings[CNO_REMOTE].max_frame_size);)
    if (CNO_WRITEV(c, PACK(I24(limit), I8(f->type), I8(0), I32(f->stream)),
                   cno_buffer_cut(&part, limit)))
      return CNO_ERROR_UP();
  return CNO_WRITEV(
             c, PACK(I24(part.size), I8(f->type), I8(f->flags), I32(f->stream)),
             part) ||
         CNO_FIRE(c, on_frame_send, f);
}

// Call `on_writev` with a HEADERS/PUSH_PROMISE frame, adding CONTINUATIONs if
// over the size limit. Requires that the headers fit in CNO_MAX_CONTINUATIONS+1
// frames.
static int cno_frame_write_head(struct cno_connection_t *c,
                                const struct cno_frame_t *f) {
  size_t limit = c->settings[CNO_REMOTE].max_frame_size;
  if (f->payload.size <= limit) return cno_frame_write(c, f);
  // CONTINUATIONs have to follow HEADERS/PUSH_PROMISE with no frames in
  // between, so the write must be atomic.
  if (f->payload.size > limit * (CNO_MAX_CONTINUATIONS + 1))
    return CNO_ERROR(
        ASSERTION,
        "HEADERS too big to write (> CNO_MAX_CONTINUATIONS + 1 frames)");
  struct cno_buffer_t split[2 * CNO_MAX_CONTINUATIONS + 2];
  struct cno_buffer_t part = f->payload;
  size_t i = 0;
  split[i++] = PACK(I24(limit), I8(f->type),
                    I8(f->flags & ~CNO_FLAG_END_HEADERS), I32(f->stream));
  split[i++] = cno_buffer_cut(&part, limit);
  while (part.size > limit) {
    split[i++] =
        PACK(I24(limit), I8(CNO_FRAME_CONTINUATION), I8(0), I32(f->stream));
    split[i++] = cno_buffer_cut(&part, limit);
  }
  split[i++] = PACK(I24(part.size), I8(CNO_FRAME_CONTINUATION),
                    I8(f->flags & CNO_FLAG_END_HEADERS), I32(f->stream));
  split[i++] = part;
  return CNO_FIRE(c, on_writev, split, i) || CNO_FIRE(c, on_frame_send, f);
}

// Call `on_writev` with a SETTINGS frame containing the difference between two
// objects.
static int cno_h2_write_settings(struct cno_connection_t *c,
                                 const struct cno_settings_t *old,
                                 const struct cno_settings_t *new) {
  uint8_t payload[CNO_SETTINGS_UNDEFINED - 1][6];
  uint8_t(*ptr)[6] = payload;
  for (size_t i = 0; i + 1 < CNO_SETTINGS_UNDEFINED; i++) {
    if (old->array[i] == new->array[i]) continue;
    struct cno_buffer_t buf = PACK(I16(i + 1), I32(new->array[i]));
    memcpy(ptr++, buf.data, buf.size);
  }
  struct cno_frame_t f = {
      CNO_FRAME_SETTINGS, 0, 0, {(char *)payload, (ptr - payload) * 6}};
  struct cno_frame_t w = {
      CNO_FRAME_WINDOW_UPDATE, 0, 0,
      PACK(I32(new->initial_window_size - old->initial_window_size))};
  if (cno_frame_write(c, &f)) return CNO_ERROR_UP();
  if (new->initial_window_size > old->initial_window_size &&
      cno_frame_write(c, &w))
    return CNO_ERROR_UP();
  c->window[CNO_LOCAL] += (new->initial_window_size - old->initial_window_size);
  return CNO_OK;
}

// Call `on_writev` with a GOAWAY frame containing the specified code.
static int cno_h2_goaway(struct cno_connection_t *c,
                         uint32_t /* enum CNO_RST_STREAM_CODE */ code) {
  if (c->goaway[CNO_LOCAL] > c->last_stream[CNO_REMOTE])
    c->goaway[CNO_LOCAL] = c->last_stream[CNO_REMOTE];
  struct cno_frame_t error = {CNO_FRAME_GOAWAY, 0, 0,
                              PACK(I32(c->goaway[CNO_LOCAL]), I32(code))};
  return cno_frame_write(c, &error);
}

// Write a PROTOCOL_ERROR GOAWAY frame and return a PROTOCOL error.
#define cno_h2_fatal(c, code, ...) \
  (cno_h2_goaway(c, code) ? CNO_ERROR_UP() : CNO_ERROR(PROTOCOL, __VA_ARGS__))

// Remember a stream so that HEADERS, DATA, WINDOW_UPDATE, and RST_STREAM frames
// may still be accepted on it even if it is removed from the map. This is
// needed because there is a delay between us sending a stream-closing frame and
// the peer receiving it.
static void cno_h2_just_ended(struct cno_connection_t *c, uint32_t sid,
                              uint8_t r_state) {
  // XXX what if id has bit 30 set? (31 is reserved)
  c->recently_reset[c->recently_reset_next++] = sid << 2 | r_state;
  c->recently_reset_next %= CNO_STREAM_RESET_HISTORY;
}

// Switch a stream to closed write-state, close it if it is in closed
// read-state, but still accept RST_STREAMs afterwards.
static int cno_stream_end_by_write(struct cno_connection_t *c,
                                   struct cno_stream_t *s) {
  s->w_state = CNO_STREAM_CLOSED;
  if (c->state == CNO_STATE_H1_UPGRADED)
    // The stream *is* the connection.
    return CNO_FIRE(c, on_close);
  if (!c->client && c->mode != CNO_HTTP2)
    // Allow to respond to the next pipelined request. See the comment in
    // `cno_when_h1_head` on the use of `last_stream` in h1 mode.
    c->last_stream[CNO_LOCAL] += 2;  // current value must have been `s->id`
  if (s->r_state != CNO_STREAM_CLOSED) return CNO_OK;
  cno_h2_just_ended(c, s->id, s->r_state);
  return cno_stream_end(c, s, CNO_RST_NO_ERROR, CNO_LOCAL);
}

// Inverse of `cno_stream_end_by_write`: switch to closed read-state, close if
// also in closed write-state. This implies that the peer is aware of stream
// closure, so it is not recorded, and further frames on this stream are
// protocol errors.
static int cno_stream_end_by_read(struct cno_connection_t *c,
                                  struct cno_stream_t *s) {
  s->r_state = CNO_STREAM_CLOSED;
  if (c->client && c->mode != CNO_HTTP2)
    // Expect a response to the next pipelined request.
    c->last_stream[CNO_REMOTE] += 2;
  if (s->w_state != CNO_STREAM_CLOSED) return CNO_OK;
  return cno_stream_end(c, s, CNO_RST_NO_ERROR, CNO_REMOTE);
}

// Call `on_writev` with an RST_STREAM frame for a stream that is not in the
// map.
static int cno_h2_rst_by_id(struct cno_connection_t *c, uint32_t sid,
                            uint32_t code, uint8_t r_state) {
  cno_h2_just_ended(c, sid, r_state);
  struct cno_frame_t error = {CNO_FRAME_RST_STREAM, 0, sid, PACK(I32(code))};
  return cno_frame_write(c, &error);
}

// Call `on_writev` with an RST_STREAM frame for a currently open stream.
static int cno_h2_rst(struct cno_connection_t *c, struct cno_stream_t *s,
                      uint32_t code) {
  return cno_h2_rst_by_id(c, s->id, code, s->r_state) ||
         cno_stream_end(c, s, code, CNO_LOCAL);
}

// When receiving a frame on a stream that is not in the map, check if it was
// recently closed by a frame we sent. If not, or the received frame was not
// acceptable in the stream's old read-state, return a PROTOCOL error.
static int cno_h2_on_invalid_stream(struct cno_connection_t *c,
                                    struct cno_frame_t *f) {
  if (f->stream &&
      f->stream <= c->last_stream[cno_stream_is_local(c, f->stream)]) {
    for (uint8_t i = 0; i < CNO_STREAM_RESET_HISTORY; i++) {
      if (c->recently_reset[i] >> 2 != f->stream) continue;
      // HEADERS is acceptable in data read-state (that's trailers).
      if ((f->type == CNO_FRAME_HEADERS &&
           c->recently_reset[i] == (f->stream << 2 | CNO_STREAM_CLOSED)) ||
          (f->type == CNO_FRAME_DATA &&
           c->recently_reset[i] != (f->stream << 2 | CNO_STREAM_DATA)))
        break;
      if (f->type == CNO_FRAME_RST_STREAM)
        c->recently_reset[i] =
            0;  // don't expect any more frames on that stream
      else if (f->type == CNO_FRAME_HEADERS ||
               (f->type == CNO_FRAME_DATA && f->flags & CNO_FLAG_END_STREAM))
        c->recently_reset[i]++;  // update read state: headers -> data, data ->
                                 // closed
      return CNO_OK;
    }
  }
  return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR, "invalid stream");
}

// Handle an END_STREAM flag, valid on HEADERS and DATA frames.
static int cno_h2_on_end_stream(struct cno_connection_t *c,
                                struct cno_stream_t *s,
                                struct cno_message_t *m) {
  // FIXME what if the sum of DATA's frames length is 1 greater than
  // content-length?
  if (s->remaining_payload && s->remaining_payload != (uint64_t)-1)
    // Had content-length which does not match up to the sum of data frame's
    // lengths.
    return cno_h2_rst(c, s, CNO_RST_PROTOCOL_ERROR);
  if (CNO_FIRE(c, on_message_tail, s->id,
               m ? &((struct cno_tail_t){m->headers, m->headers_len}) : NULL))
    return CNO_ERROR_UP();
  return cno_stream_end_by_read(c, s);
}

// Check a message for validity, parse method/path/code into `cno_message_t`'s
// fields, switch the stream's read-state as appropriate.
static int cno_h2_on_message(struct cno_connection_t *c, struct cno_stream_t *s,
                             struct cno_frame_t *f, struct cno_message_t *m) {
  int is_response = c->client && f->type != CNO_FRAME_PUSH_PROMISE;

  struct cno_header_t *it = m->headers;
  struct cno_header_t *end = m->headers + m->headers_len;
  // >HTTP/2 uses special pseudo-header fields beginning with ':' character
  // (ASCII 0x3a)
  for (; it != end &&
         cno_buffer_startswith(it->name, CNO_BUFFER_CONST_STRING(":"));
       it++)
    // >Pseudo-header fields MUST NOT appear in trailers.
    if (f->type == CNO_FRAME_HEADERS && s->r_state != CNO_STREAM_HEADERS)
      return cno_h2_rst(c, s, CNO_RST_PROTOCOL_ERROR);
  // Trailers do not contain any connection-specific headers, and `te: trailers`
  // or `content-length` are also out. So nothing more to do.
  if (f->type == CNO_FRAME_HEADERS && s->r_state != CNO_STREAM_HEADERS)
    // Already checked for `CNO_FLAG_END_STREAM` in `cno_h2_on_headers`.
    return cno_h2_on_end_stream(c, s, m);

  struct cno_header_t *first_non_pseudo = it;
  // Pseudo-headers are checked in reverse order because those that are used to
  // fill fields of `cno_message_t` are then erased, and shifting the two
  // remaining headers up is cheaper than moving all the normal headers down.
  int has_scheme = 0;
  int has_authority = 0;
  int has_protocol = 0;
  for (struct cno_header_t *h = it; h-- != m->headers;) {
    if (cno_check_no_ctl(h->value))
      return cno_h2_rst(c, s, CNO_RST_PROTOCOL_ERROR);
    if (is_response) {
      if (cno_buffer_eq(h->name, CNO_BUFFER_CONST_STRING(":status")) &&
          !m->code)
        if ((m->code = cno_parse_uint(h->value)) <
            0x10000)  // kind of an arbitrary limit, really
          continue;
      // >Endpoints MUST NOT generate pseudo-header fields other than those
      // defined in this document. (Also, duplicates are not allowed.)
      return cno_h2_rst(c, s, CNO_RST_PROTOCOL_ERROR);
    } else if (cno_buffer_eq(h->name, CNO_BUFFER_CONST_STRING(":path")) &&
               !m->path.data) {
      m->path = h->value;
      continue;
    } else if (cno_buffer_eq(h->name, CNO_BUFFER_CONST_STRING(":method")) &&
               !m->method.data) {
      m->method = h->value;
      continue;
    } else if (cno_buffer_eq(h->name, CNO_BUFFER_CONST_STRING(":authority")) &&
               !has_authority) {
      has_authority = 1;
    } else if (cno_buffer_eq(h->name, CNO_BUFFER_CONST_STRING(":scheme")) &&
               !has_scheme) {
      has_scheme = 1;
    } else if (c->settings[CNO_LOCAL].enable_connect_protocol &&
               cno_buffer_eq(h->name, CNO_BUFFER_CONST_STRING(":protocol")) &&
               !has_protocol) {
      has_protocol = 1;
    } else {
      return cno_h2_rst(c, s, CNO_RST_PROTOCOL_ERROR);
    }
    struct cno_header_t tmp = *--it;
    *it = *h;
    *h = tmp;
  }
  m->headers = it;
  m->headers_len = end - it;

  s->remaining_payload = (uint64_t)-1;
  for (it = first_non_pseudo; it != end; ++it) {
    if (cno_check_no_ctl(it->value))
      return cno_h2_rst(c, s, CNO_RST_PROTOCOL_ERROR);
    // >All pseudo-header fields MUST appear in the header block before regular
    // >header fields. [...] However, header field names MUST be converted
    // >to lowercase prior to their encoding in HTTP/2.
    for (uint8_t *p = (uint8_t *)it->name.data, *e = p + it->name.size; p != e;
         p++)
      if (CNO_HEADER_TRANSFORM[*p] !=
          *p)  // this also rejects invalid symbols, incl. `:`
        return cno_h2_rst(c, s, CNO_RST_PROTOCOL_ERROR);
    // >HTTP/2 does not use the Connection header field to indicate
    // >connection-specific header fields.
    if (cno_buffer_eq(it->name, CNO_BUFFER_CONST_STRING("connection")))
      return cno_h2_rst(c, s, CNO_RST_PROTOCOL_ERROR);
    // >The only exception to this is the TE header field, which MAY be present
    // in >an HTTP/2 request; when it is, it MUST NOT contain any value other
    // than "trailers".
    if (cno_buffer_eq(it->name, CNO_BUFFER_CONST_STRING("te")) &&
        !cno_buffer_eq(it->value, CNO_BUFFER_CONST_STRING("trailers")))
      return cno_h2_rst(c, s, CNO_RST_PROTOCOL_ERROR);
    // See content-length checking logic in `cno_h2_on_stream_end`.
    if (cno_buffer_eq(it->name, CNO_BUFFER_CONST_STRING("content-length")))
      if ((s->remaining_payload = cno_parse_uint(it->value)) == (uint64_t)-1)
        return cno_h2_rst(c, s, CNO_RST_PROTOCOL_ERROR);
  }
  // >All HTTP/2 requests MUST include exactly one valid value for the :method,
  // :scheme, >and :path pseudo-header fields, unless it is a CONNECT request
  // (Section 8.3). Except CONNECT with :protocol is a normal request, like GET
  // with upgrade in h1.
  int has_req_pseudo = m->path.size && m->method.size && has_scheme;
  int is_connect = cno_buffer_eq(m->method, CNO_BUFFER_CONST_STRING("CONNECT"));
  if (is_response ? !m->code
                  : is_connect ? (has_protocol && !has_req_pseudo)
                               : (has_protocol || !has_req_pseudo))
    return cno_h2_rst(c, s, CNO_RST_PROTOCOL_ERROR);

  if (!is_response)
    // Don't send a payload in the response to this request.
    s->head_response =
        cno_buffer_eq(m->method, CNO_BUFFER_CONST_STRING("HEAD"));
  else if (m->code == 204 || m->code == 304 || s->head_response)
    // 204 No Content never has payload; for 304 Not Modified and responses to
    // HEAD, the content-length describes the entity that would've been sent,
    // but there is no body.
    s->remaining_payload = 0;

  if (f->type == CNO_FRAME_PUSH_PROMISE)
    // The stream is already in valid (closed) write-state, set by
    // `cno_stream_new`. Read-state describes the pushed *response*, and should
    // stay the same.
    return CNO_FIRE(c, on_message_push, s->id, m, f->stream);

  if (!cno_is_informational(m->code))
    s->r_state = CNO_STREAM_DATA;
  else if (f->flags & CNO_FLAG_END_STREAM ||
           s->remaining_payload != (uint64_t)-1)
    // Informational responses should be followed by more headers.
    return cno_h2_rst(c, s, CNO_RST_PROTOCOL_ERROR);

  if (CNO_FIRE(c, on_message_head, s->id, m)) return CNO_ERROR_UP();
  if (f->flags & CNO_FLAG_END_STREAM && cno_h2_on_end_stream(c, s, NULL))
    return CNO_ERROR_UP();
  return CNO_OK;
}

// Handle an END_HEADERS flag, valid on HEADERS and PUSH_PROMISE frames. (Also
// CONTINNUATION, but these are merged into the preceding HEADERS/PUSH_PROMISE
// by `cno_when_h2_frame`.)
static int cno_h2_on_end_headers(struct cno_connection_t *c,
                                 struct cno_stream_t *s,
                                 struct cno_frame_t *f) {
  struct cno_header_t headers[CNO_MAX_HEADERS];
  struct cno_message_t m = {0, {0}, {0}, headers, CNO_MAX_HEADERS};
  if (cno_hpack_decode(&c->decoder, f->payload, headers, &m.headers_len))
    // XXX cno_h2_goaway does not necessarily keep the old error on success...
    return cno_h2_goaway(c, CNO_RST_COMPRESSION_ERROR), CNO_ERROR_UP();
  // Value changed by `cno_h2_on_message` (it removes some pseudo-headers).
  const size_t nheaders = m.headers_len;
  // Just ignore the message if the stream has already been reset.
  int ret = s ? cno_h2_on_message(c, s, f, &m) : CNO_OK;
  for (size_t i = 0; i < nheaders; i++) cno_hpack_free_header(&headers[i]);
  return ret;
}

// Handle a PADDED flag, valid on HEADERS, PUSH_PROMISE, and DATA frames. In the
// latter case, the payload length *before* a call to this function should be
// used for flow control!
static int cno_h2_on_padding(struct cno_connection_t *c,
                             struct cno_frame_t *f) {
  if (f->flags & CNO_FLAG_PADDED) {
    if (f->payload.size == 0)
      return cno_h2_fatal(c, CNO_RST_FRAME_SIZE_ERROR, "no padding found");
    size_t padding = ((const uint8_t *)f->payload.data)[0] + 1;
    if (padding > f->payload.size)
      return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR, "more padding than data");
    f->payload.data += 1;
    f->payload.size -= padding;
  }
  return CNO_OK;
}

// Handle a PRIORITY frame or the same flag on a HEADERS frame.
static int cno_h2_on_priority(struct cno_connection_t *c,
                              struct cno_stream_t *s, struct cno_frame_t *f) {
  if ((f->flags & CNO_FLAG_PRIORITY) || f->type == CNO_FRAME_PRIORITY) {
    if (f->payload.size < 5 ||
        (f->type == CNO_FRAME_PRIORITY && f->payload.size != 5))
      return cno_h2_fatal(c, CNO_RST_FRAME_SIZE_ERROR,
                          "PRIORITY of invalid size");
    if (!f->stream)
      return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR, "PRIORITY on stream 0");
    if (f->stream == (read4(f->payload.data) & 0x7FFFFFFFUL))
      return s ? cno_h2_rst(c, s, CNO_RST_PROTOCOL_ERROR)
               : cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR,
                              "PRIORITY depends on itself");
    // TODO implement prioritization
    f->payload = cno_buffer_shift(f->payload, 5);
  }
  return CNO_OK;
}

static int cno_h2_on_headers(struct cno_connection_t *c, struct cno_stream_t *s,
                             struct cno_frame_t *f) {
  struct cno_stream_t *CNO_STREAM_REF sref = NULL;
  if (cno_h2_on_padding(c, f)) return CNO_ERROR_UP();
  if (cno_h2_on_priority(c, s, f)) return CNO_ERROR_UP();
  if (s == NULL) {
    if (c->client || f->stream <= c->last_stream[CNO_REMOTE]) {
      if (cno_h2_on_invalid_stream(c, f)) return CNO_ERROR_UP();
      // Frame must be decompressed to sync HPACK state, but otherwise ignored.
    } else if (f->stream > c->goaway[CNO_LOCAL] ||
               c->stream_count[CNO_REMOTE] >=
                   c->settings[CNO_LOCAL].max_concurrent_streams) {
      c->last_stream[CNO_REMOTE] = f->stream;
      int r_state =
          f->flags & CNO_FLAG_END_STREAM ? CNO_STREAM_CLOSED : CNO_STREAM_DATA;
      if (cno_h2_rst_by_id(c, f->stream, CNO_RST_REFUSED_STREAM, r_state))
        return CNO_ERROR_UP();
    } else if ((s = sref = cno_stream_new(c, f->stream, CNO_REMOTE)) == NULL) {
      return CNO_ERROR_UP();
    }
  } else if (s->r_state == CNO_STREAM_DATA &&
             !(f->flags & CNO_FLAG_END_STREAM)) {
    return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR,
                        "trailers without END_STREAM");
  } else if (s->r_state == CNO_STREAM_CLOSED) {
    return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR, "unexpected HEADERS");
  }
  return cno_h2_on_end_headers(c, s, f);
}

static int cno_h2_on_push(struct cno_connection_t *c, struct cno_stream_t *s,
                          struct cno_frame_t *f) {
  if (cno_h2_on_padding(c, f)) return CNO_ERROR_UP();
  if (f->payload.size < 4)
    return cno_h2_fatal(c, CNO_RST_FRAME_SIZE_ERROR, "PUSH_PROMISE too short");

  // XXX stream may have been reset by us, in which case should RST_STREAM with
  // CANCEL.
  if (!c->settings[CNO_LOCAL].enable_push ||
      !cno_stream_is_local(c, f->stream) || !s ||
      s->r_state == CNO_STREAM_CLOSED)
    return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR, "unexpected PUSH_PROMISE");

  struct cno_stream_t *CNO_STREAM_REF child =
      cno_stream_new(c, read4(f->payload.data), CNO_REMOTE);
  if (child == NULL) return CNO_ERROR_UP();
  f->payload = cno_buffer_shift(f->payload, 4);
  return cno_h2_on_end_headers(c, child, f);
}

static int cno_h2_on_continuation(struct cno_connection_t *c,
                                  struct cno_stream_t *s ATTRIBUTE((unused)),
                                  struct cno_frame_t *f ATTRIBUTE((unused))) {
  // There were no HEADERS (else `cno_when_h2_frame` would've merge the frames).
  return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR, "unexpected CONTINUATION");
}

static int cno_h2_on_data(struct cno_connection_t *c, struct cno_stream_t *s,
                          struct cno_frame_t *f) {
  // For purposes of flow control, padding counts.
  uint32_t flow = f->payload.size;
  if (cno_h2_on_padding(c, f)) return CNO_ERROR_UP();

  // Frames on invalid streams still count against the connection-wide flow
  // control window.
  // TODO allow manual connection flow control?
  if (flow && flow > c->window[CNO_LOCAL])
    return cno_h2_fatal(c, CNO_RST_FLOW_CONTROL_ERROR,
                        "connection-wide overflow");
  if (flow && cno_frame_write(c, &(struct cno_frame_t){CNO_FRAME_WINDOW_UPDATE,
                                                       0, 0, PACK(I32(flow))}))
    return CNO_ERROR_UP();

  if (!s) return cno_h2_on_invalid_stream(c, f) ? CNO_ERROR_UP() : CNO_OK;
  if (s->r_state != CNO_STREAM_DATA)
    return cno_h2_rst(c, s, CNO_RST_STREAM_CLOSED);
  if (flow &&
      flow > s->window[CNO_LOCAL] + c->settings[CNO_LOCAL].initial_window_size)
    return cno_h2_rst(c, s, CNO_RST_FLOW_CONTROL_ERROR);

  if (s->remaining_payload != (uint64_t)-1)
    s->remaining_payload -= f->payload.size;

  if (f->payload.size &&
      CNO_FIRE(c, on_message_data, f->stream, f->payload.data, f->payload.size))
    return CNO_ERROR_UP();
  if (f->flags & CNO_FLAG_END_STREAM) return cno_h2_on_end_stream(c, s, NULL);

  if (c->manual_flow_control) {
    s->window[CNO_LOCAL] -= f->payload.size;
    // If there was padding, increase the window by its length right now anyway.
    flow -= f->payload.size;
  }
  struct cno_frame_t update = {CNO_FRAME_WINDOW_UPDATE, 0, s->id,
                               PACK(I32(flow))};
  return flow ? cno_frame_write(c, &update) : CNO_OK;
}

static int cno_h2_on_ping(struct cno_connection_t *c,
                          struct cno_stream_t *s ATTRIBUTE((unused)),
                          struct cno_frame_t *f) {
  if (f->stream)
    return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR, "PING on a stream");
  if (f->payload.size != 8)
    return cno_h2_fatal(c, CNO_RST_FRAME_SIZE_ERROR, "bad PING frame");
  if (f->flags & CNO_FLAG_ACK) return CNO_FIRE(c, on_pong, f->payload.data);
  struct cno_frame_t response = {CNO_FRAME_PING, CNO_FLAG_ACK, 0, f->payload};
  return cno_frame_write(c, &response);
}

static int cno_h2_on_goaway(struct cno_connection_t *c,
                            struct cno_stream_t *s ATTRIBUTE((unused)),
                            struct cno_frame_t *f) {
  if (f->stream)
    return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR, "GOAWAY on a stream");
  if (f->payload.size < 8)
    return cno_h2_fatal(c, CNO_RST_FRAME_SIZE_ERROR, "bad GOAWAY");
  const uint32_t lstid = read4(f->payload.data);
  const uint32_t error = read4(f->payload.data + 4);
  if (lstid > c->goaway[CNO_REMOTE])
    return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR,
                        "GOAWAY with higher stream id");
  if (!cno_stream_is_local(c, lstid))
    return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR,
                        "GOAWAY specifies peer's stream id");
  if (error != CNO_RST_NO_ERROR)
    return cno_h2_fatal(c, CNO_RST_NO_ERROR, "disconnected with error %u",
                        error);
  c->goaway[CNO_REMOTE] = lstid;
  for (size_t i = 0; i < CNO_STREAM_BUCKETS; i++) {
    // XXX assuming calling `on_stream_end` cannot result in a concurrent
    // destruction
    //     of another stream.
    for (struct cno_stream_t **sp = &c->streams[i]; *sp;) {
      if (!cno_stream_is_local(c, (*sp)->id) || (*sp)->id <= lstid) {
        sp = &(*sp)->next;
      } else {
        (*sp)->r_state = CNO_STREAM_CLOSED;
        // Should still accept RST_STREAM (most likely with error
        // REFUSED_STREAM) on it.
        if (cno_stream_end_by_write(c, *sp)) return CNO_ERROR_UP();
      }
    }
  }
  return c->stream_count[CNO_LOCAL] + c->stream_count[CNO_REMOTE]
             ? CNO_OK
             : CNO_FIRE(c, on_close);
}

static int cno_h2_on_rst(struct cno_connection_t *c, struct cno_stream_t *s,
                         struct cno_frame_t *f) {
  if (f->payload.size != 4)
    return cno_h2_fatal(c, CNO_RST_FRAME_SIZE_ERROR, "bad RST_STREAM");
  if (!s) return cno_h2_on_invalid_stream(c, f) ? CNO_ERROR_UP() : CNO_OK;
  return cno_stream_end(c, s, read4(f->payload.data), CNO_REMOTE);
}

static int cno_h2_on_settings(struct cno_connection_t *c,
                              struct cno_stream_t *s ATTRIBUTE((unused)),
                              struct cno_frame_t *f) {
  if (f->stream)
    return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR, "SETTINGS on a stream");
  if (f->payload.size % 6)
    return cno_h2_fatal(c, CNO_RST_FRAME_SIZE_ERROR, "bad SETTINGS");
  if (f->flags & CNO_FLAG_ACK)
    // XXX should use the previous SETTINGS (except for stream limit) before
    // receiving this
    return f->payload.size
               ? cno_h2_fatal(c, CNO_RST_FRAME_SIZE_ERROR, "bad SETTINGS ack")
               : CNO_OK;

  struct cno_settings_t *cfg = &c->settings[CNO_REMOTE];
  const uint32_t old_window = cfg->initial_window_size;
  for (const char *p = f->payload.data, *e = p + f->payload.size; p != e;
       p += 6) {
    uint16_t setting = read4(p) >> 16;
    // Not sure about ignoring the 0 setting, but whatever.
    if (setting && setting < CNO_SETTINGS_UNDEFINED)
      cfg->array[setting - 1] = read4(p + 2);
  }

  if (cfg->enable_push != 0 && (c->client || cfg->enable_push != 1))
    return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR, "enable_push out of bounds");
  if (cfg->initial_window_size > 0x7FFFFFFFL)
    return cno_h2_fatal(c, CNO_RST_FLOW_CONTROL_ERROR,
                        "initial_window_size too big");
  if (cfg->max_frame_size < 16384 || cfg->max_frame_size > 16777215)
    return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR,
                        "max_frame_size out of bounds");
  if (cfg->enable_connect_protocol > 1)
    return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR,
                        "enable_connect_protocol out of bounds");
  if (cfg->initial_window_size > old_window && CNO_FIRE(c, on_flow_increase, 0))
    return CNO_ERROR_UP();

  size_t limit = c->encoder.limit_upper = cfg->header_table_size;
  if (limit > c->settings[CNO_LOCAL].header_table_size)
    limit = c->settings[CNO_LOCAL].header_table_size;
  if (cno_hpack_setlimit(&c->encoder, limit)) return CNO_ERROR_UP();

  struct cno_frame_t ack = {CNO_FRAME_SETTINGS, CNO_FLAG_ACK, 0, {0}};
  return cno_frame_write(c, &ack) || CNO_FIRE(c, on_settings);
}

static int cno_h2_on_window_update(struct cno_connection_t *c,
                                   struct cno_stream_t *s,
                                   struct cno_frame_t *f) {
  if (f->payload.size != 4)
    return cno_h2_fatal(c, CNO_RST_FRAME_SIZE_ERROR, "bad WINDOW_UPDATE");
  const uint32_t delta = read4(f->payload.data);
  if (delta == 0 || delta > 0x7FFFFFFFL)
    return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR,
                        "window increment out of bounds");
  if (f->stream && !s)
    return cno_h2_on_invalid_stream(c, f) ? CNO_ERROR_UP() : CNO_OK;
  if (!s && (c->window[CNO_REMOTE] += delta) > 0x7FFFFFFFL)
    return cno_h2_fatal(c, CNO_RST_FLOW_CONTROL_ERROR,
                        "window increment too big");
  if (s && (s->window[CNO_REMOTE] += delta) +
                   c->settings[CNO_REMOTE].initial_window_size >
               0x7FFFFFFFL)
    return cno_h2_rst(c, s, CNO_RST_FLOW_CONTROL_ERROR);
  return CNO_FIRE(c, on_flow_increase, f->stream);
}

typedef int cno_frame_handler_t(struct cno_connection_t *,
                                struct cno_stream_t *, struct cno_frame_t *);

static cno_frame_handler_t *const CNO_FRAME_HANDLERS[] = {
    // Should be synced to enum CNO_FRAME_TYPE.
    &cno_h2_on_data,         &cno_h2_on_headers,  &cno_h2_on_priority,
    &cno_h2_on_rst,          &cno_h2_on_settings, &cno_h2_on_push,
    &cno_h2_on_ping,         &cno_h2_on_goaway,   &cno_h2_on_window_update,
    &cno_h2_on_continuation,
};

// Standard-defined pre-initial-SETTINGS values
static const struct cno_settings_t CNO_SETTINGS_STANDARD = {{{
    .header_table_size = 4096,
    .enable_push = 1,
    .max_concurrent_streams = -1,
    .initial_window_size = 65535,
    .max_frame_size = 16384,
    .max_header_list_size = -1,
}}};

// A somewhat more conservative version assumed to be used by the remote side at
// first. (In case we want to send some frames before ACK-ing the remote
// settings, but don't want to get told.)
static const struct cno_settings_t CNO_SETTINGS_CONSERVATIVE = {{{
    .header_table_size = 4096,
    .enable_push = 0,
    .max_concurrent_streams = 100,
    .initial_window_size = 65535,
    .max_frame_size = 16384,
    .max_header_list_size = -1,
}}};

// Actual values to send in the first SETTINGS frame.
static const struct cno_settings_t CNO_SETTINGS_INITIAL = {{{
    .header_table_size = 4096,
    .enable_push = 1,
    .max_concurrent_streams = 1024,
    .initial_window_size = 65535,
    .max_frame_size = 16384,
    .max_header_list_size = -1,  // actually (CNO_MAX_CONTINUATIONS *
                                 // max_frame_size - 32 * CNO_MAX_HEADERS)
}}};

void cno_init(struct cno_connection_t *c, enum CNO_CONNECTION_KIND kind) {
  *c = (struct cno_connection_t){
      .client = CNO_CLIENT == kind,
      .goaway = {(uint32_t)-1, (uint32_t)-1},
      .window = {CNO_SETTINGS_STANDARD.initial_window_size,
                 CNO_SETTINGS_STANDARD.initial_window_size},
      .settings = {CNO_SETTINGS_CONSERVATIVE, CNO_SETTINGS_INITIAL},
      .disallow_h2_upgrade = 1,
  };
  c->settings[CNO_LOCAL].enable_push &=
      c->client && c->cb_code && c->cb_code->on_message_push;
  cno_hpack_init(&c->decoder, CNO_SETTINGS_INITIAL.header_table_size);
  cno_hpack_init(&c->encoder, CNO_SETTINGS_STANDARD.header_table_size);
}

void cno_fini(struct cno_connection_t *c) {
  cno_buffer_dyn_clear(&c->buffer);
  cno_hpack_clear(&c->encoder);
  cno_hpack_clear(&c->decoder);
  for (size_t i = 0; i < CNO_STREAM_BUCKETS; i++)
    for (struct cno_stream_t *s; (s = c->streams[i]); free(s))
      c->streams[i] = s->next;
}

int cno_configure(struct cno_connection_t *c,
                  const struct cno_settings_t *settings) {
  if (settings->enable_push != 0 && settings->enable_push != 1)
    return CNO_ERROR(ASSERTION, "enable_push neither 0 nor 1");
  if (settings->max_frame_size < 16384 || settings->max_frame_size > 16777215)
    return CNO_ERROR(ASSERTION,
                     "maximum frame size out of bounds (2^14..2^24-1)");
  // If not yet in HTTP2 mode, `cno_when_h2_init` will send the SETTINGS frame.
  if (c->mode == CNO_HTTP2 &&
      cno_h2_write_settings(c, &c->settings[CNO_LOCAL], settings))
    return CNO_ERROR_UP();
  c->decoder.limit_upper = settings->header_table_size;
  memcpy(&c->settings[CNO_LOCAL], settings, sizeof(*settings));
  return CNO_OK;
}

// NOTE these functions have tri-state returns now: negative for errors, 0
// (CNO_OK)
//      to wait for more data, and positive (CNO_CONNECTION_STATE) to switch to
//      another state.
static int cno_when_closed(struct cno_connection_t *c ATTRIBUTE((unused))) {
  return CNO_ERROR(DISCONNECT, "connection closed");
}

static const struct cno_buffer_t CNO_PREFACE = {
    "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", 24};

static int cno_when_h2_init(struct cno_connection_t *c) {
  c->mode = CNO_HTTP2;
  if (c->client && CNO_FIRE(c, on_writev, &CNO_PREFACE, 1))
    return CNO_ERROR_UP();
  if (cno_h2_write_settings(c, &CNO_SETTINGS_STANDARD, &c->settings[CNO_LOCAL]))
    return CNO_ERROR_UP();
  return CNO_STATE_H2_PREFACE;
}

static int cno_when_h2_preface(struct cno_connection_t *c) {
  if (!c->client) {
    if (strncmp(c->buffer.data, CNO_PREFACE.data, c->buffer.size))
      return CNO_ERROR(PROTOCOL, "invalid HTTP 2 client preface");
    if (c->buffer.size < CNO_PREFACE.size) return CNO_OK;
    cno_buffer_dyn_shift(&c->buffer, CNO_PREFACE.size);
  }
  return CNO_STATE_H2_SETTINGS;
}

static int cno_when_h2_settings(struct cno_connection_t *c) {
  if (c->buffer.size < 5) return CNO_OK;
  if (c->buffer.data[3] != CNO_FRAME_SETTINGS || c->buffer.data[4] != 0)
    return CNO_ERROR(PROTOCOL, "invalid HTTP 2 preface: no initial SETTINGS");
  size_t len = read4(c->buffer.data) >> 8;
  if (len > CNO_SETTINGS_INITIAL
                .max_frame_size)  // couldn't have ACKed our settings yet!
    return CNO_ERROR(PROTOCOL,
                     "invalid HTTP 2 preface: initial SETTINGS too big");
  if (c->buffer.size < 9 + len) return CNO_OK;
  // Now that we know the *actual* values, they should be applied as deltas to
  // this.
  c->settings[CNO_REMOTE] = CNO_SETTINGS_INITIAL;
  c->settings[CNO_REMOTE].enable_push &= !c->client;
  return CNO_STATE_H2_FRAME;
}

static int cno_when_h2_frame(struct cno_connection_t *c) {
  while (c->buffer.size >= 9) {
    uint8_t *base = (uint8_t *)c->buffer.data;
    size_t offset = (read4(base) >> 8) + 9;
    if (c->settings[CNO_LOCAL].max_frame_size < offset - 9)
      return cno_h2_fatal(c, CNO_RST_FRAME_SIZE_ERROR, "frame too big");

    struct cno_frame_t f = {base[3],
                            base[4],
                            read4(&base[5]) & 0x7FFFFFFFUL,
                            {c->buffer.data + 9, offset - 9}};
    if (f.type == CNO_FRAME_HEADERS || f.type == CNO_FRAME_PUSH_PROMISE) {
      for (size_t size, i = 0; !(f.flags & CNO_FLAG_END_HEADERS);
           offset += size + 9) {
        if (++i > CNO_MAX_CONTINUATIONS)
          return cno_h2_fatal(c, CNO_RST_ENHANCE_YOUR_CALM,
                              "too many CONTINUATIONs");
        if (c->buffer.size < offset + 9) return CNO_OK;
        size = read4(&base[offset]) >> 8;
        if (size > c->settings[CNO_LOCAL].max_frame_size)
          return cno_h2_fatal(c, CNO_RST_FRAME_SIZE_ERROR, "frame too big");
        if (base[offset + 3] != CNO_FRAME_CONTINUATION)
          return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR,
                              "expected CONTINUATION");
        if (base[offset + 4] & ~CNO_FLAG_END_HEADERS)
          return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR,
                              "invalid CONTINUATION flags");
        if ((read4(&base[offset + 5]) & 0x7FFFFFFFUL) != f.stream)
          return cno_h2_fatal(c, CNO_RST_PROTOCOL_ERROR,
                              "invalid CONTINUATION stream");
        f.flags |= base[offset + 4];
      }
      if (c->buffer.size < offset) return CNO_OK;
      // XXX might be better to concatenate right after validating, but that
      // could trip up
      //     the single frame length check if more data is needed afterwards.
      for (size_t size, p = f.payload.size + 9; p < offset;
           p += size + 9, f.payload.size += size)
        memmove(&base[f.payload.size + 9], &base[p + 9],
                size = read4(&base[p]) >> 8);
    } else if (c->buffer.size < offset) {
      if (f.type != CNO_FRAME_DATA || f.flags & CNO_FLAG_PADDED ||
          c->buffer.size == 9)
        return CNO_OK;
      // Have *some* data, might as well split the frame in two.
      struct cno_buffer_t h = PACK(I24(offset - c->buffer.size), I8(f.type),
                                   I8(f.flags), I32(f.stream));
      memmove(base, &base[9], offset = c->buffer.size - 9);
      memmove(&base[offset], h.data, h.size);
      f.flags &= ~CNO_FLAG_END_STREAM;
      f.payload = (struct cno_buffer_t){c->buffer.data, offset};
    }

    if (CNO_FIRE(c, on_frame, &f)) return CNO_ERROR_UP();
    struct cno_stream_t *CNO_STREAM_REF s = cno_stream_find(c, f.stream);
    // >Implementations MUST ignore and discard any frame that has a type that
    // is unknown.
    if (f.type < CNO_FRAME_UNKNOWN && CNO_FRAME_HANDLERS[f.type](c, s, &f))
      return CNO_ERROR_UP();
    cno_buffer_dyn_shift(&c->buffer, offset);
  }
  return CNO_OK;
}

static size_t cno_remove_chunked_te(struct cno_buffer_t *buf) {
  // assuming the request is valid, chunked can only be the last
  // transfer-encoding
  if (cno_buffer_endswith(*buf, CNO_BUFFER_CONST_STRING("chunked"))) {
    buf->size -= 7;
    while (buf->size && buf->data[buf->size - 1] == ' ') buf->size--;
    if (buf->size && buf->data[buf->size - 1] == ',') buf->size--;
  }
  return buf->size;
}

static int cno_when_h1_head(struct cno_connection_t *c) {
  if (!c->buffer.size) return CNO_OK;

  // In h1 mode, both counters are used to track client-initiated streams, since
  // there's no other kind. Client-side stream counter is the ID of the current
  // request, while server-side stream counter is the ID of the current
  // response. This is used for pipelining, as the server may fall behind.
  struct cno_stream_t *CNO_STREAM_REF s =
      cno_stream_find(c, c->last_stream[CNO_REMOTE]);
  if (!s || s->r_state != CNO_STREAM_HEADERS) {
    if (c->client)
      return CNO_ERROR(
          PROTOCOL,
          "server sent an HTTP/1.x response, but there was no request");
    // Only allow upgrading with prior knowledge if no h1 requests have yet been
    // received.
    if (!c->disallow_h2_prior_knowledge && c->last_stream[CNO_REMOTE] == 0)
      if (!strncmp(c->buffer.data, CNO_PREFACE.data, c->buffer.size))
        return c->buffer.size < CNO_PREFACE.size ? CNO_OK : CNO_STATE_H2_INIT;
    // Do not accept new requests if shutting down.
    if (c->last_stream[CNO_REMOTE] >= c->goaway[CNO_LOCAL])
      return c->stream_count[CNO_REMOTE]
                 ? CNO_ERROR(
                       WOULD_BLOCK,
                       "shutting down; wait until existing streams are done")
                 : CNO_ERROR(DISCONNECT, "already shut down");
    if (s) cno_stream_decref(&s);
    // This is allowed to return WOULD_BLOCK if the pipelining limit
    // has been reached. (It's not a protocol error since there are no
    // SETTINGS.)
    if (!(s = cno_stream_new(c, (c->last_stream[CNO_REMOTE] + 1) | 1,
                             CNO_REMOTE)))
      return CNO_ERROR_UP();
  }

  struct cno_header_t headers[CNO_MAX_HEADERS + 2];  // + :scheme and :authority
  struct cno_message_t m = {0, {0}, {0}, headers, CNO_MAX_HEADERS};
  struct phr_header headers_phr[CNO_MAX_HEADERS];

  int minor = 0;
  int ok =
      c->client
          ? phr_parse_response(c->buffer.data, c->buffer.size, &minor, &m.code,
                               &m.method.data, &m.method.size, headers_phr,
                               &m.headers_len, 1)
          : phr_parse_request(c->buffer.data, c->buffer.size, &m.method.data,
                              &m.method.size, &m.path.data, &m.path.size,
                              &minor, headers_phr, &m.headers_len, 1);
  if (ok == -1) return CNO_ERROR(PROTOCOL, "HTTP/1.x message invalid");
  if (ok == -2 && c->buffer.size >= (CNO_MAX_CONTINUATIONS + 1) *
                                        c->settings[CNO_LOCAL].max_frame_size)
    return CNO_ERROR(PROTOCOL, "HTTP/1.x message too big");
  if (ok == -2) return CNO_OK;
  if (minor != 0 && minor != 1)
    // HTTP/1.0 is probably not really supported either tbh.
    return CNO_ERROR(PROTOCOL, "HTTP/1.%d not supported", minor);

  int finalRequest = (minor == 0);
  int upgrade = 0;
  int upgradeToH2 = 0;
  int closeDelimited = c->client && !cno_is_informational(m.code);
  struct cno_header_t *it = headers;
  if (!c->client) {
    *it++ = (struct cno_header_t){CNO_BUFFER_CONST_STRING(":scheme"),
                                  CNO_BUFFER_CONST_STRING("unknown"), 0};
    *it++ = (struct cno_header_t){CNO_BUFFER_CONST_STRING(":authority"),
                                  CNO_BUFFER_CONST_STRING("unknown"), 0};
  }
  for (size_t i = 0; i < m.headers_len; i++) {
    if (!headers_phr[i].name)
      return CNO_ERROR(PROTOCOL, "HTTP/1.x line folding rejected");
    *it = (struct cno_header_t){
        .name = {headers_phr[i].name, headers_phr[i].name_len},
        .value = {headers_phr[i].value, headers_phr[i].value_len},
    };
    for (uint8_t *p = (uint8_t *)it->name.data, *e = p + it->name.size; p != e;
         p++)
      if (!(/* *p = */ CNO_HEADER_TRANSFORM[*p]))
        return CNO_ERROR(PROTOCOL, "invalid character in h1 header");

    if (!c->client &&
        cno_buffer_eq(it->name, CNO_BUFFER_CONST_STRING("host"))) {
      headers[1].value = it->value;
      continue;
    } else if (cno_buffer_eq(it->name,
                             CNO_BUFFER_CONST_STRING("http2-settings"))) {
      // TODO decode & emit on_frame
      continue;
    } else if (!c->client &&
               cno_buffer_eq(it->name, CNO_BUFFER_CONST_STRING("upgrade"))) {
      if (cno_buffer_eq(it->value, CNO_BUFFER_CONST_STRING("h2c"))) {
        // TODO client-side h2 upgrade
        if (!c->disallow_h2_upgrade && s->id == 1 && !upgrade) upgradeToH2 = 1;
        continue;
      } else if (upgradeToH2) {
        continue;  // If upgrading to h2c, don't notify the application of any
                   // upgrades.
      }
      upgrade = 1;
    } else if (cno_buffer_eq(it->name,
                             CNO_BUFFER_CONST_STRING("content-length"))) {
      if (s->reading_chunked) continue;
      if (s->remaining_payload)
        return CNO_ERROR(PROTOCOL, "multiple content-lengths");
      if ((s->remaining_payload = cno_parse_uint(it->value)) == (uint64_t)-1)
        return CNO_ERROR(PROTOCOL, "invalid content-length");
      closeDelimited = 0;
    } else if (cno_buffer_eq(it->name,
                             CNO_BUFFER_CONST_STRING("transfer-encoding"))) {
      if (cno_buffer_eq(it->value, CNO_BUFFER_CONST_STRING("identity")))
        continue;  // (This value is probably not actually allowed.)
      // Any non-identity transfer-encoding requires chunked (which should also
      // be listed). (This part is a bit non-compatible with h2. Proxies should
      // probably decode TEs.)
      s->remaining_payload = (uint64_t)-1;
      s->reading_chunked = 1;
      closeDelimited = 0;
      if (!cno_remove_chunked_te(&it->value)) continue;
    } else if (cno_buffer_eq(it->name, CNO_BUFFER_CONST_STRING("connection"))) {
      // XXX actually supposed to be a comma-separated list; does anyone
      //     use anything other than "close", "keep-alive", and "upgrade"?
      if (cno_buffer_eq(it->value, CNO_BUFFER_CONST_STRING("close"))) {
        finalRequest = 1;
        // continue;
      } else if (cno_buffer_eq(it->value,
                               CNO_BUFFER_CONST_STRING("keep-alive"))) {
        finalRequest = 0;
        // continue;
      }
    }
    it++;
  }
  m.headers_len = it - m.headers;

  if (finalRequest && c->goaway[CNO_REMOTE] > c->last_stream[CNO_REMOTE])
    // Call `on_close` when this stream terminates.
    c->goaway[CNO_REMOTE] = c->last_stream[CNO_REMOTE];

  if (c->client) {
    if (s->remaining_payload && cno_is_informational(m.code))
      return CNO_ERROR(PROTOCOL, "informational response with a payload");
    if (m.code == 204 || m.code == 304 || s->head_response)
      // See equivalent code in `cno_h2_on_message`.
      s->remaining_payload = closeDelimited = 0;
  } else {
    // Upgrading in response to a request with payload requires reading h1 while
    // writing h2, which interacts poorly with cancellation (i.e. not at all;
    // client should continue uploading).
    if (upgradeToH2 && !s->remaining_payload) {
      // Technically, server should refuse if HTTP2-Settings are not present.
      // We'll let this slide.
      if (CNO_WRITEV(c, CNO_BUFFER_CONST_STRING(
                            "HTTP/1.1 101 Switching Protocols\r\nconnection: "
                            "upgrade\r\nupgrade: h2c\r\n\r\n")) ||
          cno_when_h2_init(c) < 0)
        return CNO_ERROR_UP();
    } else if (!c->last_stream[CNO_LOCAL]) {
      // Allow writing responses.
      c->last_stream[CNO_LOCAL] = s->id;
    }
    s->head_response = cno_buffer_eq(m.method, CNO_BUFFER_CONST_STRING("HEAD"));
  }

  // If on_message_head triggers asynchronous handling, this is expected to
  // block until either 101 has been sent or the server decides not to upgrade.
  if (CNO_FIRE(c, on_message_head, s->id, &m) ||
      (upgrade && CNO_FIRE(c, on_upgrade, s->id)))
    return CNO_ERROR_UP();

  cno_buffer_dyn_shift(&c->buffer, (size_t)ok);
  if (cno_is_informational(m.code) && m.code != 101) return CNO_STATE_H1_HEAD;

  s->r_state = CNO_STREAM_DATA;
  return c->state == CNO_STATE_H1_UPGRADED || m.code == 101 || closeDelimited
             ? CNO_STATE_H1_UPGRADED
             : s->remaining_payload ? CNO_STATE_H1_BODY : CNO_STATE_H1_TAIL;
}

static int cno_when_h1_upgraded(struct cno_connection_t *c) {
  if (c->buffer.size && CNO_FIRE(c, on_message_data, c->last_stream[CNO_REMOTE],
                                 c->buffer.data, c->buffer.size))
    return CNO_ERROR_UP();
  cno_buffer_dyn_shift(&c->buffer, c->buffer.size);
  return CNO_OK;
}

static int cno_when_h1_body(struct cno_connection_t *c) {
  struct cno_stream_t *CNO_STREAM_REF s =
      cno_stream_find(c, c->last_stream[CNO_REMOTE]);
  if (!s)
    // Stream should exist here because the only way to switch its read-state
    // to closed from the write side is a h2 RST_STREAM, but a payload-carrying
    // request could not have upgraded to h2c.
    return CNO_ERROR(ASSERTION, "waiting for h1 payload but no streams exist");
  while (1) {
    if (s->reading_chunked && s->remaining_payload == (uint64_t)-1) {
      const char *p = c->buffer.data;
      const char *e = memchr(p, '\n', c->buffer.size);
      if (e == NULL)
        return c->buffer.size >= c->settings[CNO_LOCAL].max_frame_size
                   ? CNO_ERROR(PROTOCOL, "too many h1 chunk extensions")
                   : CNO_OK;
      size_t length = 0;
      do {
        int digit = '0' <= *p && *p <= '9'
                        ? *p - '0'
                        : 'A' <= *p && *p <= 'F'
                              ? *p - 'A' + 10
                              : 'a' <= *p && *p <= 'f' ? *p - 'a' + 10 : -1;
        if (digit == -1 || length * 16 < length)
          return CNO_ERROR(PROTOCOL, "invalid h1 chunk length");
        length = length * 16 + digit;
      } while (*++p != '\r' && *p != '\n' && *p != ';');
      if (p[0] == '\r' && p[1] != '\n')
        return CNO_ERROR(PROTOCOL, "invalid h1 chunk length separator");
      cno_buffer_dyn_shift(&c->buffer, e - c->buffer.data + 1);
      s->remaining_payload = length;
      if (!length) return CNO_STATE_H1_TRAILERS;
    }

    while (s->remaining_payload) {
      if (!c->buffer.size) return CNO_OK;
      struct cno_buffer_t b = CNO_BUFFER_VIEW(c->buffer);
      if (b.size > s->remaining_payload) b.size = s->remaining_payload;
      if (CNO_FIRE(c, on_message_data, s->id, b.data, b.size))
        return CNO_ERROR_UP();
      cno_buffer_dyn_shift(&c->buffer, b.size);
      s->remaining_payload -= b.size;
    }

    if (!s->reading_chunked) return CNO_STATE_H1_TAIL;

    size_t expect_crlf = c->buffer.size > 0 && c->buffer.data[0] == '\r';
    if (c->buffer.size < 1 + expect_crlf) return CNO_OK;
    if (c->buffer.data[expect_crlf] != '\n')
      return CNO_ERROR(PROTOCOL, "invalid h1 chunk terminator");
    s->remaining_payload = (uint64_t)-1;
    cno_buffer_dyn_shift(&c->buffer, 1 + expect_crlf);
  }
}

static int cno_when_h1_tail(struct cno_connection_t *c) {
  struct cno_stream_t *CNO_STREAM_REF s =
      cno_stream_find(c, c->last_stream[CNO_REMOTE]);
  // We could have upgraded to h2c then reset the stream while in
  // on_message_head, destroying it before entering this function.
  if (s && (CNO_FIRE(c, on_message_tail, s->id, NULL) ||
            cno_stream_end_by_read(c, s)))
    return CNO_ERROR_UP();
  return c->mode == CNO_HTTP2 ? CNO_STATE_H2_PREFACE : CNO_STATE_H1_HEAD;
}

static int cno_when_h1_trailers(struct cno_connection_t *c) {
  struct cno_header_t headers[CNO_MAX_HEADERS];
  struct cno_tail_t t = {headers, CNO_MAX_HEADERS};
  struct phr_header headers_phr[CNO_MAX_HEADERS];
  int ok = phr_parse_headers(c->buffer.data, c->buffer.size, headers_phr,
                             &t.headers_len, 0);
  if (ok == -1) return CNO_ERROR(PROTOCOL, "HTTP/1.x trailers invalid");
  if (ok == -2 && c->buffer.size >= (CNO_MAX_CONTINUATIONS + 1) *
                                        c->settings[CNO_LOCAL].max_frame_size)
    return CNO_ERROR(PROTOCOL, "HTTP/1.x message too big");
  if (ok == -2) return CNO_OK;
  for (size_t i = 0; i < t.headers_len; i++) {
    if (!headers_phr[i].name)
      return CNO_ERROR(PROTOCOL, "HTTP/1.x line folding rejected");
    headers[i] = (struct cno_header_t){
        .name = {headers_phr[i].name, headers_phr[i].name_len},
        .value = {headers_phr[i].value, headers_phr[i].value_len},
    };
  }
  struct cno_stream_t *CNO_STREAM_REF s =
      cno_stream_find(c, c->last_stream[CNO_REMOTE]);
  // See comment on stream existence in `cno_when_h1_body`.
  if (CNO_FIRE(c, on_message_tail, s->id, &t) || cno_stream_end_by_read(c, s))
    return CNO_ERROR_UP();
  cno_buffer_dyn_shift(&c->buffer, (size_t)ok);
  return CNO_STATE_H1_HEAD;
}

typedef int cno_state_handler_t(struct cno_connection_t *);

static cno_state_handler_t *const CNO_STATE_MACHINE[] = {
    // Should be synced to enum CNO_CONNECTION_STATE.
    &cno_when_closed,      &cno_when_h2_init,  &cno_when_h2_preface,
    &cno_when_h2_settings, &cno_when_h2_frame, &cno_when_h1_head,
    &cno_when_h1_upgraded, &cno_when_h1_body,  &cno_when_h1_tail,
    &cno_when_h1_trailers,
};

int cno_begin(struct cno_connection_t *c, enum CNO_HTTP_VERSION version) {
  if (c->state != CNO_STATE_CLOSED)
    return CNO_ERROR(ASSERTION, "called connection_made twice");
  c->state = (version == CNO_HTTP2 ? CNO_STATE_H2_INIT : CNO_STATE_H1_HEAD);
  return cno_consume(c, NULL, 0);
}

int cno_consume(struct cno_connection_t *c, const char *data, size_t size) {
  if (cno_buffer_dyn_concat(&c->buffer, (struct cno_buffer_t){data, size}))
    return CNO_ERROR_UP();
  for (int r; (r = CNO_STATE_MACHINE[c->state](c)) != 0; c->state = r)
    if (r < 0) return CNO_ERROR_UP();
  return CNO_OK;
}

int cno_shutdown(struct cno_connection_t *c) {
  return cno_write_reset(c, 0, CNO_RST_NO_ERROR);
}

int cno_eof(struct cno_connection_t *c) {
  int upgraded = (c->state == CNO_STATE_H1_UPGRADED);
  if (upgraded && cno_when_h1_tail(c) < 0) return CNO_ERROR_UP();
  c->state = CNO_STATE_CLOSED;
  int wantR = 0, wantW = 0;
  for (size_t i = 0; i < CNO_STREAM_BUCKETS; i++) {
    while (c->streams[i]) {
      wantR += (c->streams[i]->r_state != CNO_STREAM_CLOSED);
      wantW += (c->streams[i]->w_state != CNO_STREAM_CLOSED);
      if (cno_stream_end(c, c->streams[i], CNO_RST_CANCEL, CNO_LOCAL))
        return CNO_ERROR_UP();
    }
  }
  return (wantR || wantW) && !upgraded
             ? CNO_ERROR(PROTOCOL,
                         "%d read-open + %d write-open streams on EOF", wantR,
                         wantW)
             : CNO_FIRE(c, on_close);
}

uint32_t cno_next_stream(const struct cno_connection_t *c) {
  uint32_t last = c->last_stream[CNO_LOCAL];
  return c->client ? (last + 1) | 1 : last + 2;
}

int cno_write_reset(struct cno_connection_t *c, uint32_t sid,
                    enum CNO_RST_STREAM_CODE code) {
  if (!sid || c->mode != CNO_HTTP2) {
    if (c->goaway[CNO_LOCAL] > c->last_stream[CNO_REMOTE])
      c->goaway[CNO_LOCAL] = c->last_stream[CNO_REMOTE];
    if (c->mode == CNO_HTTP2 && cno_h2_goaway(c, code)) return CNO_ERROR_UP();
    return c->stream_count[CNO_LOCAL] + c->stream_count[CNO_REMOTE]
               ? CNO_OK
               : CNO_FIRE(c, on_close);
  }
  struct cno_stream_t *CNO_STREAM_REF s = cno_stream_find(c, sid);
  return s ? cno_h2_rst(c, s, code)
           : CNO_OK;  // assume idle streams have already been reset
}

int cno_write_push(struct cno_connection_t *c, uint32_t sid,
                   const struct cno_message_t *m) {
  if (c->state == CNO_STATE_CLOSED)
    return CNO_ERROR(DISCONNECT, "connection closed");
  if (c->client) return CNO_ERROR(ASSERTION, "clients can't push");
  if (c->mode != CNO_HTTP2 || !c->settings[CNO_REMOTE].enable_push ||
      cno_stream_is_local(c, sid) ||
      c->last_stream[CNO_LOCAL] >= c->goaway[CNO_REMOTE])
    return CNO_OK;

  struct cno_stream_t *CNO_STREAM_REF s = cno_stream_find(c, sid);
  if (!s || s->w_state == CNO_STREAM_CLOSED)
    return CNO_OK;  // pushed requests are safe, so whether we send one doesn't
                    // matter

  struct cno_stream_t *CNO_STREAM_REF sc =
      cno_stream_new(c, cno_next_stream(c), CNO_LOCAL);
  if (sc == NULL) return CNO_ERROR_UP();

  struct cno_buffer_dyn_t enc = {0};
  struct cno_header_t head[2] = {
      {CNO_BUFFER_CONST_STRING(":method"), m->method, 0},
      {CNO_BUFFER_CONST_STRING(":path"), m->path, 0},
  };
  if (cno_buffer_dyn_concat(&enc, PACK(I32(sc->id))) ||
      cno_hpack_encode(&c->encoder, &enc, head, 2) ||
      cno_hpack_encode(&c->encoder, &enc, m->headers, m->headers_len) ||
      cno_frame_write_head(
          c, &(struct cno_frame_t){CNO_FRAME_PUSH_PROMISE, CNO_FLAG_END_HEADERS,
                                   sid, CNO_BUFFER_VIEW(enc)}))
    // irrecoverable (compression state desync), don't bother destroying the
    // stream.
    // FIXME: the possible errors are NO_MEMORY or something from on_writev.
    //        one case is when `on_writev` is cancelled before writing anything
    //        due to parent stream being reset; this should not be a connection
    //        error.
    // FIXME: should make next `cno_consume` fail.
    return cno_buffer_dyn_clear(&enc), CNO_ERROR_UP();
  cno_buffer_dyn_clear(&enc);
  return CNO_FIRE(c, on_message_head, sc->id, m) ||
         CNO_FIRE(c, on_message_tail, sc->id, NULL);
}

static struct cno_buffer_t cno_fmt_uint(char *b, size_t s, unsigned n) {
  char *q = b + s;
  do
    *--q = '0' + (n % 10);
  while (n /= 10);
  return (struct cno_buffer_t){q, b + s - q};
}

static struct cno_buffer_t cno_fmt_chunk_length(char *b, size_t s, size_t n) {
  static const char hex[] = "0123456789ABCDEF";
  char *q = b + s;
  *--q = '\n';
  *--q = '\r';
  do
    *--q = hex[n % 16];
  while (n /= 16);
  return (struct cno_buffer_t){q, b + s - q};
}

static int cno_h1_write_head(struct cno_connection_t *c, struct cno_stream_t *s,
                             const struct cno_message_t *m, int final) {
  struct cno_buffer_t iov[33];
  size_t iovcnt = 0;
  char codebuf[12];
  if (c->client) {
    iov[iovcnt++] = m->method;
    iov[iovcnt++] = CNO_BUFFER_CONST_STRING(" ");
    iov[iovcnt++] = m->path;
    iov[iovcnt++] = CNO_BUFFER_CONST_STRING(" HTTP/1.1\r\n");
  } else if (!m->method.size)
    switch (m->code) {
#define CODE_DEF(n, msg)                                                    \
  case n:                                                                   \
    iov[iovcnt++] = CNO_BUFFER_CONST_STRING("HTTP/1.1 " #n " " msg "\r\n"); \
    break
      CODE_DEF(100, "Continue");
      CODE_DEF(101, "Switching Protocols");
      CODE_DEF(200, "OK");
      CODE_DEF(201, "Created");
      CODE_DEF(202, "Accepted");
      CODE_DEF(203, "Non-Authoritative Information");
      CODE_DEF(204, "No Content");
      CODE_DEF(205, "Reset Content");
      CODE_DEF(206, "Partial Content");
      CODE_DEF(300, "Multiple Choices");
      CODE_DEF(301, "Moved Permanently");
      CODE_DEF(302, "Found");
      CODE_DEF(303, "See Other");
      CODE_DEF(304, "Not Modified");
      CODE_DEF(305, "Use Proxy");
      CODE_DEF(307, "Temporary Redirect");
      CODE_DEF(400, "Bad Request");
      CODE_DEF(401, "Unauthorized");
      CODE_DEF(402, "Payment Required");
      CODE_DEF(403, "Forbidden");
      CODE_DEF(404, "Not Found");
      CODE_DEF(405, "Method Not Allowed");
      CODE_DEF(406, "Not Acceptable");
      CODE_DEF(407, "Proxy Authentication Required");
      CODE_DEF(408, "Request Time-out");
      CODE_DEF(409, "Conflict");
      CODE_DEF(410, "Gone");
      CODE_DEF(411, "Length Required");
      CODE_DEF(412, "Precondition Failed");
      CODE_DEF(413, "Request Entity Too Large");
      CODE_DEF(414, "Request-URI Too Large");
      CODE_DEF(415, "Unsupported Media Type");
      CODE_DEF(416, "Requested range not satisfiable");
      CODE_DEF(417, "Expectation Failed");
      CODE_DEF(500, "Internal Server Error");
      CODE_DEF(501, "Not Implemented");
      CODE_DEF(502, "Bad Gateway");
      CODE_DEF(503, "Service Unavailable");
      CODE_DEF(504, "Gateway Time-out");
      CODE_DEF(505, "HTTP Version not supported");
#undef CODE_DEF
      default:
        goto unknown_code;
    }
  else
  unknown_code : {
    iov[iovcnt++] = CNO_BUFFER_CONST_STRING("HTTP/1.1 ");
    iov[iovcnt++] = cno_fmt_uint(codebuf, 12, m->code);
    iov[iovcnt++] = CNO_BUFFER_CONST_STRING(" ");
    iov[iovcnt++] =
        m->method.size ? m->method : CNO_BUFFER_CONST_STRING("No Reason");
    iov[iovcnt++] = CNO_BUFFER_CONST_STRING("\r\n");
  }

    s->writing_chunked = !cno_is_informational(m->code) && !final;
  for (const struct cno_header_t *it = m->headers, *end = it + m->headers_len;
       it != end; ++it) {
    struct cno_header_t h = *it;
    if (cno_buffer_eq(h.name, CNO_BUFFER_CONST_STRING(":authority"))) {
      h.name = CNO_BUFFER_CONST_STRING("host");
    } else if (cno_buffer_startswith(h.name, CNO_BUFFER_CONST_STRING(":"))) {
      continue;  // :scheme, probably
    } else if (cno_buffer_eq(h.name,
                             CNO_BUFFER_CONST_STRING("content-length")) ||
               cno_buffer_eq(h.name, CNO_BUFFER_CONST_STRING("upgrade"))) {
      // XXX not writing chunked on `upgrade` is a hack so that `GET` with final
      // = 0 still works.
      s->writing_chunked = 0;
    } else if (cno_buffer_eq(h.name,
                             CNO_BUFFER_CONST_STRING("transfer-encoding"))) {
      // Either CNO_STREAM_H1_WRITING_CHUNKED is set, there's no body at all, or
      // message is invalid because it contains both content-length and
      // transfer-encoding.
      if (!cno_remove_chunked_te(&h.value)) continue;
    }
    // Reserve one for the trailing CRLF below.
    if (iovcnt > 28) {
      if (CNO_FIRE(c, on_writev, iov, iovcnt)) return CNO_ERROR_UP();
      iovcnt = 0;
    }
    iov[iovcnt++] = h.name;
    iov[iovcnt++] = CNO_BUFFER_CONST_STRING(": ");
    iov[iovcnt++] = h.value;
    iov[iovcnt++] = CNO_BUFFER_CONST_STRING("\r\n");
  }
  iov[iovcnt++] =
      s->writing_chunked
          ? CNO_BUFFER_CONST_STRING("transfer-encoding: chunked\r\n\r\n")
          : CNO_BUFFER_CONST_STRING("\r\n");
  return CNO_FIRE(c, on_writev, iov, iovcnt);
}

static int cno_h2_write_head(struct cno_connection_t *c, struct cno_stream_t *s,
                             const struct cno_message_t *m, int final) {
  int flags = (final ? CNO_FLAG_END_STREAM : 0) | CNO_FLAG_END_HEADERS;
  struct cno_buffer_dyn_t enc = {0};
  struct cno_header_t head[] = {
      {CNO_BUFFER_CONST_STRING(":status"),
       cno_fmt_uint((char[12]){0}, 12, m->code), 0},
      {CNO_BUFFER_CONST_STRING(":method"), m->method, 0},
      {CNO_BUFFER_CONST_STRING(":path"), m->path, 0},
  };
  if (cno_hpack_encode(&c->encoder, &enc, c->client ? head + 1 : head,
                       c->client ? 2 : 1) ||
      cno_hpack_encode(&c->encoder, &enc, m->headers, m->headers_len) ||
      cno_frame_write_head(
          c, &(struct cno_frame_t){CNO_FRAME_HEADERS, flags, s->id,
                                   CNO_BUFFER_VIEW(enc)}))
    // Irrecoverable (compression state desync). FIXME: see `cno_write_push`.
    return cno_buffer_dyn_clear(&enc), CNO_ERROR_UP();
  return cno_buffer_dyn_clear(&enc), CNO_OK;
}

static int cno_check_headers(const struct cno_header_t *h, size_t size,
                             int trailers) {
  for (; size--; h++) {
    if (h->name.size == 0)
      return CNO_ERROR(ASSERTION, "attempting to send a header with no name");
    if (trailers && h->name.data[0] == ':')
      return CNO_ERROR(ASSERTION, "pseudo-header in trailers");
    // The code is too strict for HTTP1
    // for (uint8_t *p = (uint8_t *) h->name.data, *e = p + h->name.size;
    // p != e; p++)
    //    if (*p != ':' && CNO_HEADER_TRANSFORM[*p] != *p)
    //        // Only in h2, actually; h1 is case-insensitive. Better be
    //        conservative, though.
    //        // (Also, there are some case-sensitive comparisons in
    //        `cno_h1_write_head`.) return CNO_ERROR(ASSERTION, "invalid
    //        character '%c' in header name; should be a lowercase letter", *p);
    if (cno_check_no_ctl(h->value)) return CNO_ERROR_UP();
  }
  return CNO_OK;
}

int cno_write_head(struct cno_connection_t *c, uint32_t sid,
                   const struct cno_message_t *m, int final) {
  if (c->state == CNO_STATE_CLOSED ||
      sid > c->goaway[!cno_stream_is_local(c, sid)])
    return CNO_ERROR(DISCONNECT, "connection closed");

  if (c->client ? !!m->code : !!m->path.size)
    return CNO_ERROR(
        ASSERTION, c->client ? "request with a code" : "response with a path");
  if (cno_is_informational(m->code) && final)
    // These codes always leave the stream in CNO_STREAM_HEADERS write-state.
    return CNO_ERROR(ASSERTION, "1xx codes cannot end the stream");
  if (m->code == 101 && c->mode == CNO_HTTP2)
    return CNO_ERROR(ASSERTION,
                     "cannot switch protocols over an http2 connection");
  if (cno_check_no_ctl(m->method) || cno_check_no_ctl(m->path) ||
      cno_check_headers(m->headers, m->headers_len, 0))
    return CNO_ERROR_UP();

  struct cno_stream_t *CNO_STREAM_REF s = cno_stream_find(c, sid);
  if (c->client && !s && !(s = cno_stream_new(c, sid, CNO_LOCAL)))
    return CNO_ERROR_UP();
  if (!s) return CNO_ERROR(INVALID_STREAM, "this stream is closed");
  if (s->w_state != CNO_STREAM_HEADERS)
    return CNO_ERROR(ASSERTION, "already sent headers on this stream");
  if (c->client)
    // See comment in `cno_h2_on_message`.
    s->head_response =
        cno_buffer_eq(m->method, CNO_BUFFER_CONST_STRING("HEAD"));

  if (c->mode == CNO_HTTP2) {
    if (cno_h2_write_head(c, s, m, final)) return CNO_ERROR_UP();
  } else {
    if (!c->client) {
      if (c->last_stream[CNO_LOCAL] != s->id)
        return CNO_ERROR(WOULD_BLOCK, "not head of line");
    } else if (c->last_stream[CNO_REMOTE] != 0 &&
               c->last_stream[CNO_REMOTE] != s->id) {
      // Response has not been received yet, but maybe we've uploaded the
      // request already? Check `last[CNO_LOCAL] - 2`, which should exist and be
      // the previous request's stream.
      struct cno_stream_t *CNO_STREAM_REF prev =
          cno_stream_find(c, c->last_stream[CNO_LOCAL] - 2);
      if (prev && prev->w_state != CNO_STREAM_CLOSED)
        // FIXME this makes the user wait until another stream ends, while it is
        // enough
        //       to wait until `cno_write_*` is called with final = 1.
        return CNO_ERROR(WOULD_BLOCK, "not head of line");
    }
    if (m->code == 101) {
      // Only handle upgrades if still in on_message_head/on_upgrade.
      if (c->last_stream[CNO_REMOTE] != s->id ||
          c->state != CNO_STATE_H1_HEAD || s->r_state == CNO_STREAM_CLOSED)
        return CNO_ERROR(
            ASSERTION,
            "accepted a h1 upgrade, but did not block in on_upgrade");
      // Make further calls to `cno_consume` forward everything as data to this
      // stream.
      c->state = CNO_STATE_H1_UPGRADED;
    }
    if (cno_h1_write_head(c, s, m, final)) return CNO_ERROR_UP();
    if (c->client && c->last_stream[CNO_REMOTE] == 0)
      // Allow `cno_when_h1_head` to accept a response. XXX: this breaks h2c
      // upgrade!
      c->last_stream[CNO_REMOTE] = s->id;
  }

  if (final) {
    if (cno_stream_end_by_write(c, s)) return CNO_ERROR_UP();
  } else if (m->code == 101 || !cno_is_informational(m->code)) {
    s->w_state = CNO_STREAM_DATA;
  }
  return CNO_OK;
}

int cno_write_data(struct cno_connection_t *c, uint32_t sid, const char *data,
                   size_t size, int final) {
  if (c->state == CNO_STATE_CLOSED)
    return CNO_ERROR(DISCONNECT, "connection closed");
  struct cno_stream_t *CNO_STREAM_REF s = cno_stream_find(c, sid);
  if (!s) return CNO_ERROR(INVALID_STREAM, "this stream is closed");
  if (s->w_state == CNO_STREAM_HEADERS)
    return CNO_ERROR(ASSERTION, "did not send headers on this stream");
  if (s->w_state == CNO_STREAM_CLOSED)
    return CNO_ERROR(ASSERTION, "already finished writing on this stream");

  if (c->mode == CNO_HTTP2) {
    int64_t limit =
        s->window[CNO_REMOTE] + c->settings[CNO_REMOTE].initial_window_size;
    if (limit > c->window[CNO_REMOTE]) limit = c->window[CNO_REMOTE];
    if (limit < 0)
      // May happen if a SETTINGS lowers the initial stream window size too
      // much.
      limit = 0;
    if (size > (uint64_t)limit) size = limit, final = 0;
    size_t rtsize = size;
    if (!c->client && s->head_response)
      // Pretend to send the chunk, but discard it. Preferably, the application
      // should detect HEAD and avoid generating data at all, but eh.
      size = 0;
    if (size || final) {
      // Should be done before writing the frame to allow `on_writev` to yield
      // safely.
      c->window[CNO_REMOTE] -= size;
      s->window[CNO_REMOTE] -= size;
      struct cno_frame_t frame = {
          CNO_FRAME_DATA, final ? CNO_FLAG_END_STREAM : 0, sid, {data, size}};
      if (cno_frame_write_data(c, &frame)) {
        c->window[CNO_REMOTE] += size;
        s->window[CNO_REMOTE] += size;
        return CNO_ERROR_UP();
      }
    }
    size = rtsize;
  } else if (!c->client && s->head_response) {
    // Don't write (even the trailing zero-length chunk), only update the state.
  } else if (s->writing_chunked) {
    if (size) {
      struct cno_buffer_t tail = final
                                     ? CNO_BUFFER_CONST_STRING("\r\n0\r\n\r\n")
                                     : CNO_BUFFER_CONST_STRING("\r\n");
      if (CNO_WRITEV(c, cno_fmt_chunk_length((char[24]){0}, 24, size),
                     {data, size}, tail))
        return CNO_ERROR_UP();
    } else if (final) {
      if (CNO_WRITEV(c, CNO_BUFFER_CONST_STRING("0\r\n\r\n")))
        return CNO_ERROR_UP();
    }
  } else if (size && CNO_WRITEV(c, {data, size})) {
    return CNO_ERROR_UP();
  }

  if (final && cno_stream_end_by_write(c, s)) return CNO_ERROR_UP();
  return (int)size;
}

int cno_write_tail(struct cno_connection_t *c, uint32_t sid,
                   const struct cno_tail_t *t) {
  if (!t || !t->headers_len)
    // When I said "equivalent", I meant it: even the frame is DATA instead of
    // HEADERS.
    return cno_write_data(c, sid, NULL, 0, 1);

  if (c->state == CNO_STATE_CLOSED)
    return CNO_ERROR(DISCONNECT, "connection closed");
  if (cno_check_headers(t->headers, t->headers_len, 1)) return CNO_ERROR_UP();

  struct cno_stream_t *CNO_STREAM_REF s = cno_stream_find(c, sid);
  if (!s) return CNO_ERROR(INVALID_STREAM, "this stream is closed");
  if (s->w_state == CNO_STREAM_HEADERS)
    return CNO_ERROR(ASSERTION, "did not send headers on this stream");
  if (s->w_state == CNO_STREAM_CLOSED)
    return CNO_ERROR(ASSERTION, "already finished writing on this stream");

  if (c->mode == CNO_HTTP2) {
    struct cno_buffer_dyn_t enc = {0};
    if (cno_hpack_encode(&c->encoder, &enc, t->headers, t->headers_len) ||
        cno_frame_write_head(
            c, &(struct cno_frame_t){CNO_FRAME_HEADERS,
                                     CNO_FLAG_END_STREAM | CNO_FLAG_END_HEADERS,
                                     s->id, CNO_BUFFER_VIEW(enc)}))
      // Irrecoverable (compression state desync). FIXME: see `cno_write_push`.
      return cno_buffer_dyn_clear(&enc), CNO_ERROR_UP();
    cno_buffer_dyn_clear(&enc);
  } else if ((c->client || !s->head_response) && s->writing_chunked) {
    if (CNO_WRITEV(c, CNO_BUFFER_CONST_STRING("0\r\n"))) return CNO_ERROR_UP();
    for (const struct cno_header_t *it = t->headers, *end = it + t->headers_len;
         it != end; ++it)
      if (CNO_WRITEV(c, it->name, CNO_BUFFER_CONST_STRING(": "), it->value,
                     CNO_BUFFER_CONST_STRING("\r\n")))
        return CNO_ERROR_UP();
    if (CNO_WRITEV(c, CNO_BUFFER_CONST_STRING("\r\n"))) return CNO_ERROR_UP();
  }
  return cno_stream_end_by_write(c, s);
}

int cno_write_ping(struct cno_connection_t *c, const char data[8]) {
  if (c->mode != CNO_HTTP2)
    return CNO_ERROR(ASSERTION, "cannot ping HTTP/1.x endpoints");
  return cno_frame_write(
      c, &(struct cno_frame_t){CNO_FRAME_PING, 0, 0, {data, 8}});
}

int cno_write_frame(struct cno_connection_t *c, const struct cno_frame_t *f) {
  if (c->mode != CNO_HTTP2)
    return CNO_ERROR(ASSERTION,
                     "cannot send HTTP2 frames to HTTP/1.x endpoints");
  if (f->type < CNO_FRAME_UNKNOWN && f->type != CNO_FRAME_PRIORITY)
    // Interfering with this library is not a good idea. TODO: priority API.
    return CNO_ERROR(
        ASSERTION,
        "cannot use `cno_write_frame` to send standard-defined frames");
  if (f->payload.size > c->settings[CNO_REMOTE].max_frame_size)
    // XXX if `on_writev` yields before writing anything, the limit may change.
    return CNO_ERROR(ASSERTION, "frame too big");
  return cno_frame_write(c, f);
}

int cno_open_flow(struct cno_connection_t *c, uint32_t sid, uint32_t delta) {
  if (c->mode != CNO_HTTP2 || !delta) return CNO_OK;
  struct cno_stream_t *CNO_STREAM_REF s = cno_stream_find(c, sid);
  // Disregard changes in reset streams' window size.
  if (sid && (!s || s->r_state == CNO_STREAM_CLOSED)) return CNO_OK;
  if (cno_frame_write(c, &(struct cno_frame_t){CNO_FRAME_WINDOW_UPDATE, 0, sid,
                                               PACK(I32(delta))}))
    return CNO_ERROR_UP();
  if (sid)
    s->window[CNO_LOCAL] += delta;
  else
    c->window[CNO_LOCAL] += delta;
  return CNO_OK;
}
