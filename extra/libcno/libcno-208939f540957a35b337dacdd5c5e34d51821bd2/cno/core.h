#pragma once

#include "common.h"
#include "config.h"
#include "hpack.h"

#ifdef __cplusplus
extern "C" {
#endif

enum CNO_PEER_KIND {
  CNO_REMOTE,
  CNO_LOCAL,
};

enum CNO_CONNECTION_KIND {
  CNO_SERVER,
  CNO_CLIENT,
};

enum CNO_HTTP_VERSION {
  CNO_HTTP1,
  CNO_HTTP2,
};

enum CNO_FRAME_TYPE {
  CNO_FRAME_DATA = 0x0,
  CNO_FRAME_HEADERS = 0x1,
  CNO_FRAME_PRIORITY = 0x2,
  CNO_FRAME_RST_STREAM = 0x3,
  CNO_FRAME_SETTINGS = 0x4,
  CNO_FRAME_PUSH_PROMISE = 0x5,
  CNO_FRAME_PING = 0x6,
  CNO_FRAME_GOAWAY = 0x7,
  CNO_FRAME_WINDOW_UPDATE = 0x8,
  CNO_FRAME_CONTINUATION = 0x9,
  CNO_FRAME_UNKNOWN = 0xa,
};

enum CNO_RST_STREAM_CODE {
  CNO_RST_NO_ERROR = 0x0,
  CNO_RST_PROTOCOL_ERROR = 0x1,
  CNO_RST_INTERNAL_ERROR = 0x2,
  CNO_RST_FLOW_CONTROL_ERROR = 0x3,
  CNO_RST_SETTINGS_TIMEOUT = 0x4,
  CNO_RST_STREAM_CLOSED = 0x5,
  CNO_RST_FRAME_SIZE_ERROR = 0x6,
  CNO_RST_REFUSED_STREAM = 0x7,
  CNO_RST_CANCEL = 0x8,
  CNO_RST_COMPRESSION_ERROR = 0x9,
  CNO_RST_CONNECT_ERROR = 0xa,
  CNO_RST_ENHANCE_YOUR_CALM = 0xb,
  CNO_RST_INADEQUATE_SECURITY = 0xc,
  CNO_RST_HTTP_1_1_REQUIRED = 0xd,
};

enum CNO_FRAME_FLAGS {
  CNO_FLAG_ACK = 0x1,
  CNO_FLAG_END_STREAM = 0x1,
  CNO_FLAG_END_HEADERS = 0x4,
  CNO_FLAG_PADDED = 0x8,
  CNO_FLAG_PRIORITY = 0x20,
};

enum CNO_CONNECTION_SETTINGS {
  CNO_SETTINGS_HEADER_TABLE_SIZE = 0x1,
  CNO_SETTINGS_ENABLE_PUSH = 0x2,
  CNO_SETTINGS_MAX_CONCURRENT_STREAMS = 0x3,
  CNO_SETTINGS_INITIAL_WINDOW_SIZE = 0x4,
  CNO_SETTINGS_MAX_FRAME_SIZE = 0x5,
  CNO_SETTINGS_MAX_HEADER_LIST_SIZE = 0x6,
  CNO_SETTINGS_UNKNOWN_1 = 0x7,
  CNO_SETTINGS_ENABLE_CONNECT_PROTOCOL = 0x8,
  CNO_SETTINGS_UNDEFINED = 0x9,
};

struct cno_frame_t {
  uint8_t /* enum CNO_FRAME_TYPE  */ type;
  uint8_t /* enum CNO_FRAME_FLAGS */ flags;
  uint32_t stream;
  struct cno_buffer_t payload;
};

struct cno_message_t {
  int code;
  struct cno_buffer_t method;  // reason string in HTTP/1.x mode responses
  struct cno_buffer_t path;
  struct cno_header_t *headers;
  size_t headers_len;
};

struct cno_tail_t {
  struct cno_header_t *headers;
  size_t headers_len;
};

struct cno_stream_t;

struct cno_settings_t {
  union {
    // TODO implement this in a way not dependent on alignment
    // TODO extensions
    struct {
      uint32_t header_table_size;
      uint32_t enable_push;
      uint32_t max_concurrent_streams;
      uint32_t initial_window_size;
      uint32_t max_frame_size;
      uint32_t max_header_list_size;
      uint32_t unknown;
      uint32_t enable_connect_protocol;
    };
    uint32_t array[8];
  };
};

struct cno_vtable_t {
  // There is something to send to the other side. Transport level is outside
  // the scope of this library. NOTE: calls to `on_writev` must be atomic w.r.t.
  // each other so that frames don't mix.
  int (*on_writev)(void *, const struct cno_buffer_t *, size_t count);
  // The transport should be closed and all further writes discarded.
  int (*on_close)(void *);
  // A new stream has been created due to sending/receiving a request or sending
  // a push promise. In the latter two cases, `on_message_head` will be called
  // shortly afterwards.
  int (*on_stream_start)(void *, uint32_t id);
  // Either a response has been sent/received fully, or the stream has been
  // reset.
  int (*on_stream_end)(void *, uint32_t id, uint32_t code, enum CNO_PEER_KIND);
  // The other side has signaled that it is willing to accept more data.
  // There is a global limit (shared between all streams) and one for each
  // stream; `cno_write_data` will send as much data as the lowest of the two
  // allows. When the global limit is updated, this function is called with
  // stream id = 0.
  int (*on_flow_increase)(void *, uint32_t id);
  // A request/response has been received (depending on whether this is a server
  // connection or not). Each stream carries exactly one request/response pair.
  int (*on_message_head)(void *, uint32_t id, const struct cno_message_t *);
  // Client only: server is intending to push a response to a request that
  // it anticipates in advance.
  int (*on_message_push)(void *, uint32_t id, const struct cno_message_t *,
                         uint32_t parent);
  // A chunk of the payload has arrived.
  int (*on_message_data)(void *, uint32_t id, const char *, size_t);
  // All chunks of the payload (and possibly trailers) have arrived.
  int (*on_message_tail)(void *, uint32_t id,
                         const struct cno_tail_t * /* NULL if none */);
  // An HTTP 2 frame has been received.
  int (*on_frame)(void *, const struct cno_frame_t *);
  // An HTTP 2 frame will be sent with `on_writev` soon.
  int (*on_frame_send)(void *, const struct cno_frame_t *);
  // An acknowledgment of one of the previously sent pings has arrived.
  int (*on_pong)(void *, const char[8]);
  // New connection-wide settings have been chosen by the peer.
  int (*on_settings)(void *);
  // HTTP 1 server only: the previous request (see `on_message_head`) has
  // requested an upgrade to a different protocol. If `cno_write_head` is called
  // with code 101 before this function returns, the connection will be proxied
  // as payload for the last created stream (read -> on_message_data,
  // cno_write_data -> write). Otherwise, the upgrade is ignored.
  int (*on_upgrade)(void *, uint32_t id);
};

struct cno_connection_t {
  // public:
  const struct cno_vtable_t *cb_code;
  // Passed as the first argument to all callbacks.
  void *cb_data;
  // Disable automatic sending of stream WINDOW_UPDATEs after receiving DATA;
  // application must call `cno_open_flow` after processing a chunk from
  // `on_message_data`.
  uint8_t manual_flow_control : 1;
  // Disable special handling of the "Upgrade: h2c" header in HTTP/1.x mode.
  // NOTE: this is set by default because:
  //   1. when using tls, you *have* to set this to be compliant;
  //   2. most browsers/servers only support h2 over tls anyway;
  //   3. weird things may happen if you reset stream 1 without consuming the
  //   whole payload.
  uint8_t disallow_h2_upgrade : 1;
  // Disable special handling of the HTTP2 preface in HTTP/1.x mode.
  uint8_t disallow_h2_prior_knowledge : 1;
  // Whether `cno_init` was called with `CNO_CLIENT`.
  uint8_t /* read-only */ client : 1;
  // Whether `cno_begin` was called with `CNO_HTTP2` or an upgrade has beed
  // performed.
  uint8_t /* read-only */ mode : 1;

  // private:
  uint8_t state;
  uint8_t recently_reset_next;
  uint32_t recently_reset[CNO_STREAM_RESET_HISTORY];
  uint32_t last_stream[2];  // dereferencable with CNO_REMOTE/CNO_LOCAL
  uint32_t stream_count[2];
  uint32_t goaway[2];  // sent by {them, us}
  int64_t window[2];   // advertised by {them, us}
  struct cno_settings_t settings[2];
  struct cno_buffer_dyn_t buffer;
  struct cno_hpack_t decoder;
  struct cno_hpack_t encoder;
  struct cno_stream_t *streams[CNO_STREAM_BUCKETS];
};

// Initialize a freshly constructed connection object. (Set up the callbacks
// after this.)
void cno_init(struct cno_connection_t *, enum CNO_CONNECTION_KIND);

// Free all resources associated with a connection. *Does not emit events*: if
// you store additional per-stream data, discard it.
void cno_fini(struct cno_connection_t *);

// Begin the message exchange using the negotiated protocol.
int cno_begin(struct cno_connection_t *, enum CNO_HTTP_VERSION);

// Handle some new data from the transport level. In h1 mode, this function may
// return CNO_ERRNO_WOULD_BLOCK; this means you need to handle some pipelined
// requests before accepting more of them (or raise the concurrent stream
// limit). In that case, the provided data is buffered and must not be passed
// again. Pass an empty chunk to have another go at parsing the buffered data if
// you've nothing to add.
int cno_consume(struct cno_connection_t *, const char *, size_t);

// Handle an EOF from a half-closed transport. Half-closed connections are not
// supported because they behave really inconveniently (e.g. you can't detect
// RST unless you write).
int cno_eof(struct cno_connection_t *);

// Gracefully shut down the connection. (After calling this, wait for remaining
// streams to end, then close the transport.)
int cno_shutdown(struct cno_connection_t *);

// Set a new configuration for HTTP 2. (The current one can be read as
// `c->settings[CNO_LOCAL]`.)
int cno_configure(struct cno_connection_t *, const struct cno_settings_t *);

// Obtain a new stream id to send a request with.
uint32_t cno_next_stream(const struct cno_connection_t *);

// Client: send a new request. Server: respond to a previously received one. If
// `final` is 0 (only allowed for requests and non-1xx responses), one or more
// calls to `cno_write_data` must follow. For a 1xx response, call this function
// again with a proper code later. 101 responses must only be sent while
// `cno_consume` is blocked in `on_message_head` or `on_upgrade` of an HTTP 1
// connection; see `on_upgrade`. This function may return CNO_ERRNO_WOULD_BLOCK
// if you've created too many streams already (client) or there are pipelined h1
// requests that need to be handled before this one (server).
int cno_write_head(struct cno_connection_t *, uint32_t stream,
                   const struct cno_message_t *, int final);

// Server: initiate a request in anticipation of the client doing it anyway.
// This should only be done for safe requests without payload. The function will
// silently do nothing if the client uses HTTP 1 or has specified that it does
// not want push messages.
int cno_write_push(struct cno_connection_t *, uint32_t stream,
                   const struct cno_message_t *);

// Attach more data to the previously sent message. If `final` is 0, more calls
// must follow. Returns -1 on error, else the number of sent bytes, which may be
// less than requested due to HTTP 2 flow control. If that's the case, wait for
// an `on_flow_increase` on the same stream (or on stream 0) before retrying.
int cno_write_data(struct cno_connection_t *, uint32_t stream, const char *,
                   size_t, int final);

// Write the trailers and close the stream. In h1 mode, only works if
// `content-length` was not specified due to protocol limitations; otherwise,
// the trailers are silently dropped. (The standard says they're optional!) For
// `NULL` or an empty list of trailers, equivalent to `cno_write_data(conn,
// stream, NULL, 0, 1)`.
int cno_write_tail(struct cno_connection_t *, uint32_t stream,
                   const struct cno_tail_t *);

// Reject a stream. Has no effect in HTTP 1 mode (in which case you should
// simply close the transport) or if the stream has already finished because a
// response or another reset has been received/sent.
int cno_write_reset(struct cno_connection_t *, uint32_t stream,
                    enum CNO_RST_STREAM_CODE);

// Send a ping with some data. See also `on_pong`. Only works in HTTP 2 mode.
int cno_write_ping(struct cno_connection_t *, const char[8]);

// Send a raw HTTP 2 frame. Only works in HTTP 2 mode, and only for extension
// frames.
int cno_write_frame(struct cno_connection_t *, const struct cno_frame_t *);

// Increase the flow window by the specified amount, allowing the peer to send
// more data.
//
// NOTE: if manual flow control is disabled, the window size is kept constant by
// increasing
//       it before emitting `on_message_data`. This function can still be used
//       to make the window bigger than the default.
int cno_open_flow(struct cno_connection_t *, uint32_t stream, uint32_t delta);

#ifdef __cplusplus
}  // extern "C"
#endif
