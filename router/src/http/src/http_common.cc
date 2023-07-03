/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * API Facade around libevent's http interface
 */
#include "mysqlrouter/http_common.h"

#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <utility>

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/keyvalq_struct.h>
#include <event2/listener.h>
#include <event2/thread.h>
#include <event2/util.h>

#include "http_request_impl.h"
#include "mysql/harness/utility/string.h"
#include "mysqlrouter/http_request.h"

static_assert(EV_TIMEOUT == EventFlags::Timeout,
              "libevent constant must match the wrappers constant");
static_assert(EV_READ == EventFlags::Read,
              "libevent constant must match the wrappers constant");
static_assert(EV_WRITE == EventFlags::Write,
              "libevent constant must match the wrappers constant");
static_assert(EV_SIGNAL == EventFlags::Signal,
              "libevent constant must match the wrappers constant");

static_assert(BUFFEREVENT_SSL_OPEN ==
                  static_cast<int>(EventBuffer::SslState::Open),
              "libevent constant must match the wrappers constant");
static_assert(BUFFEREVENT_SSL_CONNECTING ==
                  static_cast<int>(EventBuffer::SslState::Connecting),
              "libevent constant must match the wrappers constant");
static_assert(BUFFEREVENT_SSL_ACCEPTING ==
                  static_cast<int>(EventBuffer::SslState::Accepting),
              "libevent constant must match the wrappers constant");

static_assert(EventBufferOptionsFlags::CloseOnFree == BEV_OPT_CLOSE_ON_FREE,
              "libevent constant must match the wrappers constant");
static_assert(EventBufferOptionsFlags::ThreadSafe == BEV_OPT_THREADSAFE,
              "libevent constant must match the wrappers constant");
static_assert(EventBufferOptionsFlags::DeferCallbacks ==
                  BEV_OPT_DEFER_CALLBACKS,
              "libevent constant must match the wrappers constant");
static_assert(EventBufferOptionsFlags::UnlockCallbacks ==
                  BEV_OPT_UNLOCK_CALLBACKS,
              "libevent constant must match the wrappers constant");

static_assert(EVHTTP_REQ_GET == HttpMethod::Get,
              "libevent constant must match the wrappers constant");
static_assert(EVHTTP_REQ_POST == HttpMethod::Post,
              "libevent constant must match the wrappers constant");
static_assert(EVHTTP_REQ_HEAD == HttpMethod::Head,
              "libevent constant must match the wrappers constant");
static_assert(EVHTTP_REQ_PUT == HttpMethod::Put,
              "libevent constant must match the wrappers constant");
static_assert(EVHTTP_REQ_DELETE == HttpMethod::Delete,
              "libevent constant must match the wrappers constant");
static_assert(EVHTTP_REQ_OPTIONS == HttpMethod::Options,
              "libevent constant must match the wrappers constant");
static_assert(EVHTTP_REQ_TRACE == HttpMethod::Trace,
              "libevent constant must match the wrappers constant");
static_assert(EVHTTP_REQ_CONNECT == HttpMethod::Connect,
              "libevent constant must match the wrappers constant");
static_assert(EVHTTP_REQ_PATCH == HttpMethod::Patch,
              "libevent constant must match the wrappers constant");

template <typename Unique>
auto impl_get_base(Unique &unique) {
  return unique.get()->base.get();
}

template <typename Impl, typename InnerType, typename Function_type>
std::unique_ptr<Impl> impl_new(InnerType *inner, Function_type function) {
  return std::unique_ptr<Impl>(new Impl{{inner, function}});
}

static evkeyval *get_node(HttpHeaders::Iterator::IteratorHandle handle) {
  return reinterpret_cast<evkeyval *>(handle);
}

// wrap generic libevent functions

bool Event::initialize_threads() {
#ifdef EVTHREAD_USE_WINDOWS_THREADS_IMPLEMENTED
  return 0 == evthread_use_windows_threads();
#elif EVTHREAD_USE_PTHREADS_IMPLEMENTED
  return 0 == evthread_use_pthreads();
#else
  return false;
#endif  // EVTHREAD_USE_PTHREADS_IMPLEMENTED
}

void Event::shutdown() { libevent_global_shutdown(); }

void Event::set_log_callback(const CallbackLog cb) {
  cbLog_ = cb;
  event_set_log_callback([](int severity, const char *message) {
    const static std::map<int, Log> map{{EVENT_LOG_DEBUG, Log::Debug},
                                        {EVENT_LOG_ERR, Log::Error},
                                        {EVENT_LOG_WARN, Log::Warning},
                                        {EVENT_LOG_MSG, Log::Message}};

    if (cbLog_) {
      cbLog_(map.at(severity), message);
    }
  });
}

void Event::enable_debug_logging(const DebugLogLevel which) {
  event_enable_debug_logging(static_cast<uint32_t>(which));
}

bool Event::has_ssl() {
  // Do not bring `libevent` headers outside this file, thus macro/definitions
  // must be check at runtime by calling such methods as these.
#ifdef EVENT__HAVE_OPENSSL
  return true;
#else
  return false;
#endif
}

// wrap event_base
struct EventBase::impl {
  std::unique_ptr<event_base, decltype(&event_base_free)> base;
};

Event::CallbackLog Event::cbLog_ = nullptr;

EventBase::EventBase()
    : pImpl_{new impl{{event_base_new(), &event_base_free}}} {}

EventBase::EventBase(EventBase &&event) : pImpl_{std::move(event.pImpl_)} {}

EventBase::EventBase(std::unique_ptr<impl> &&pImpl)
    : pImpl_(std::move(pImpl)) {}

// Needed because of pimpl
// Destructor must be placed in the compilation unit
EventBase::~EventBase() = default;

bool EventBase::once(const SocketHandle fd, const EventFlags::Bitset events,
                     CallbackEvent cb, void *arg, const struct timeval *tv) {
  return 0 == event_base_once(impl_get_base(pImpl_),
                              static_cast<evutil_socket_t>(fd),
                              events.to_ullong(), cb, arg, tv);
}

bool EventBase::loop_exit(const struct timeval *tv) {
  return 0 == event_base_loopexit(impl_get_base(pImpl_), tv);
}

int EventBase::dispatch() { return event_base_dispatch(impl_get_base(pImpl_)); }

// wrap for bufferevent

struct EventBuffer::impl {
  std::unique_ptr<bufferevent, decltype(&bufferevent_free)> base;
};

EventBuffer::EventBuffer(EventBase *base, const SocketHandle socket,
                         TlsContext *tls_context, const SslState state,
                         const EventBufferOptionsFlags::Bitset &options) {
  // We receive a SSL_context from which we create a new SSL_connection,
  // Thus the socket state can't be set to "already open".

#ifdef EVENT__HAVE_OPENSSL
  pImpl_ = impl_new<EventBuffer::impl>(
      bufferevent_openssl_socket_new(
          impl_get_base(base->pImpl_), static_cast<evutil_socket_t>(socket),
          SSL_new(tls_context->get()),
          static_cast<bufferevent_ssl_state>(state), options.to_ullong()),
      &bufferevent_free);
#endif  // EVENT__HAVE_OPENSSL
}

EventBuffer::EventBuffer(EventBuffer &&other)
    : pImpl_(std::move(other.pImpl_)) {}

// Needed because of pimpl
// Destructor must be placed in the compilation unit
EventBuffer::~EventBuffer() {}

// wrap for evhttp

struct EventHttp::impl {
  class EvHttpDeleter {
   public:
    void operator()(evhttp *http) { evhttp_free(http); }
  };

  impl(evhttp *http, event_base *ev_base)
      : base{std::move(http)}, ev_base_{ev_base} {}

  std::unique_ptr<evhttp, EvHttpDeleter> base;

  event_base *ev_base_;

  CallbackBuffer bufferCallback_ = nullptr;
  void *bufferArgument_ = nullptr;
  CallbackRequest requestCallback_ = nullptr;
  void *requestArgument_ = nullptr;
};

EventHttp::EventHttp(EventBase *base)
    : pImpl_{std::make_unique<impl>(evhttp_new(impl_get_base(base->pImpl_)),
                                    impl_get_base(base->pImpl_))} {}

EventHttp::EventHttp(EventHttp &&http) : pImpl_(std::move(http.pImpl_)) {}

// Needed because of pimpl
// Destructor must be placed in the compilation unit
EventHttp::~EventHttp() = default;

void EventHttp::set_allowed_http_methods(const HttpMethod::Bitset methods) {
  evhttp_set_allowed_methods(impl_get_base(pImpl_), methods.to_ullong());
}

EventHttpBoundSocket EventHttp::accept_socket_with_handle(
    const SocketHandle fd) {
  auto http = impl_get_base(pImpl_);

  // same as evhttp_accept_socket_with_handle(), except it doesn't take
  // ownership via LEV_OPT_CLOSE_ON_FREE
  const int flags = LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_EXEC;

  struct evconnlistener *listener =
      evconnlistener_new(pImpl_->ev_base_, nullptr, nullptr, flags, 0, fd);
  if (listener == nullptr) return nullptr;

  struct evhttp_bound_socket *bound = evhttp_bind_listener(http, listener);
  if (bound == nullptr) {
    evconnlistener_free(listener);
    return nullptr;
  }

  return bound;
}

void EventHttp::set_gencb(CallbackRequest cb, void *cbarg) {
  pImpl_->requestCallback_ = cb;
  pImpl_->requestArgument_ = cbarg;
  evhttp_set_gencb(
      impl_get_base(pImpl_),
      [](evhttp_request *req, void *arg) {
        HttpRequest request(
            impl_new<HttpRequest::impl>(req, [](evhttp_request *) {}));
        auto current = reinterpret_cast<EventHttp::impl *>(arg);
        current->requestCallback_(&request, current->requestArgument_);
      },
      pImpl_.get());
}

void EventHttp::set_bevcb(CallbackBuffer cb, void *cbarg) {
  pImpl_->bufferCallback_ = cb;
  pImpl_->bufferArgument_ = cbarg;
  evhttp_set_bevcb(
      impl_get_base(pImpl_),
      [](event_base *base, void *arg) {
        auto current = reinterpret_cast<EventHttp::impl *>(arg);

        auto impl = impl_new<EventBase::impl>(base, [](event_base *) {});

        EventBase event(std::move(impl));
        auto result =
            current->bufferCallback_(&event, current->bufferArgument_);
        return result.pImpl_->base.release();
      },
      pImpl_.get());
}

// wrap evhttp_uri

struct HttpUri::impl {
  std::unique_ptr<evhttp_uri, decltype(&evhttp_uri_free)> uri;
};

HttpUri::HttpUri() : pImpl_(new impl{{evhttp_uri_new(), &evhttp_uri_free}}) {}

HttpUri::HttpUri(std::unique_ptr<impl> &&uri) { pImpl_ = std::move(uri); }

HttpUri::HttpUri(HttpUri &&) = default;
HttpUri::~HttpUri() = default;

HttpUri::operator bool() const { return pImpl_->uri.operator bool(); }

std::string HttpUri::decode(const std::string &uri_str,
                            const bool decode_plus) {
  size_t out_size = 0;
  std::unique_ptr<char, decltype(&free)> decoded{
      evhttp_uridecode(uri_str.c_str(), decode_plus ? 1 : 0, &out_size), &free};
  return std::string(decoded.get(), out_size);
}

HttpUri HttpUri::parse(const std::string &uri_str) {
  // wrap a owned pointer
  auto evhttp_uri = evhttp_uri_parse(uri_str.c_str());
  auto impl = impl_new<HttpUri::impl>(evhttp_uri, &evhttp_uri_free);
  return HttpUri(std::move(impl));
}

std::string HttpUri::get_scheme() const {
  const char *const u = evhttp_uri_get_scheme(pImpl_->uri.get());

  return u != nullptr ? u : "";
}

void HttpUri::set_scheme(const std::string &scheme) {
  evhttp_uri_set_scheme(pImpl_->uri.get(), scheme.c_str());
}

std::string HttpUri::get_userinfo() const {
  const char *const u = evhttp_uri_get_userinfo(pImpl_->uri.get());

  return u != nullptr ? u : "";
}
void HttpUri::set_userinfo(const std::string &userinfo) {
  evhttp_uri_set_userinfo(pImpl_->uri.get(), userinfo.c_str());
}

std::string HttpUri::get_host() const {
  const char *const u = evhttp_uri_get_host(pImpl_->uri.get());

  return u != nullptr ? u : "";
}

void HttpUri::set_host(const std::string &host) {
  evhttp_uri_set_host(pImpl_->uri.get(), host.c_str());
}

uint16_t HttpUri::get_port() const {
  return evhttp_uri_get_port(pImpl_->uri.get());
}

void HttpUri::set_port(uint16_t port) const {
  evhttp_uri_set_port(pImpl_->uri.get(), port);
}

std::string HttpUri::get_path() const {
  const char *const u = evhttp_uri_get_path(pImpl_->uri.get());

  return u != nullptr ? u : "";
}

void HttpUri::set_path(const std::string &path) {
  if (0 != evhttp_uri_set_path(pImpl_->uri.get(), path.c_str())) {
    throw std::invalid_argument("URL path isn't valid: " + path);
  }
}

std::string HttpUri::get_fragment() const {
  const char *const u = evhttp_uri_get_fragment(pImpl_->uri.get());

  return u != nullptr ? u : "";
}
void HttpUri::set_fragment(const std::string &fragment) {
  evhttp_uri_set_fragment(pImpl_->uri.get(), fragment.c_str());
}

std::string HttpUri::get_query() const {
  const char *const u = evhttp_uri_get_query(pImpl_->uri.get());

  return u != nullptr ? u : "";
}
void HttpUri::set_query(const std::string &query) {
  evhttp_uri_set_query(pImpl_->uri.get(), query.c_str());
}

std::string HttpUri::join() const {
  char buf[16 * 1024];
  if (evhttp_uri_join(pImpl_->uri.get(), buf, sizeof(buf))) {
    return buf;
  }

  throw std::invalid_argument("join failed");
}

std::string http_uri_path_canonicalize(const std::string &uri_path) {
  if (uri_path.empty()) return "/";

  std::deque<std::string> sections;

  std::istringstream ss(uri_path);
  for (std::string section; std::getline(ss, section, '/');) {
    if (section == "..") {
      // remove last item on the stack
      if (!sections.empty()) {
        sections.pop_back();
      }
    } else if (section != "." && !section.empty()) {
      sections.emplace_back(section);
    }
  }

  bool has_trailing_slash = uri_path.back() == '/';
  if (has_trailing_slash) sections.emplace_back("");

  auto out = "/" + mysql_harness::join(sections, "/");

  return out;
}

// wrap evbuffer

struct HttpBuffer::impl {
  std::unique_ptr<evbuffer, std::function<void(evbuffer *)>> buffer;
};

// non-owning pointer
HttpBuffer::HttpBuffer(std::unique_ptr<impl> &&buffer) {
  pImpl_ = std::move(buffer);
}

void HttpBuffer::add(const char *data, size_t data_size) {
  evbuffer_add(pImpl_->buffer.get(), data, data_size);
}

void HttpBuffer::add_file(int file_fd, off_t offset, off_t size) {
  evbuffer_add_file(pImpl_->buffer.get(), file_fd, offset, size);
}

size_t HttpBuffer::length() const {
  return evbuffer_get_length(pImpl_->buffer.get());
}

std::vector<uint8_t> HttpBuffer::pop_front(size_t len) {
  std::vector<uint8_t> data;
  data.resize(len);

  int bytes_read;
  if (-1 == (bytes_read = evbuffer_remove(pImpl_->buffer.get(), data.data(),
                                          data.size()))) {
    throw std::runtime_error("couldn't pop bytes from front of buffer");
  }

  data.resize(bytes_read);
  data.shrink_to_fit();

  return data;
}

HttpBuffer::HttpBuffer(HttpBuffer &&) = default;
HttpBuffer::~HttpBuffer() = default;

// wrap evkeyvalq

struct HttpHeaders::impl {
  std::unique_ptr<evkeyvalq, std::function<void(evkeyvalq *)>> hdrs;
};

HttpHeaders::HttpHeaders(std::unique_ptr<impl> &&impl)
    : pImpl_(std::move(impl)) {}

int HttpHeaders::add(const char *key, const char *value) {
  return evhttp_add_header(pImpl_->hdrs.get(), key, value);
}

const char *HttpHeaders::get(const char *key) const {
  return evhttp_find_header(pImpl_->hdrs.get(), key);
}

std::pair<std::string, std::string> HttpHeaders::Iterator::operator*() {
  auto node = get_node(node_);
  return {node->key, node->value};
}

HttpHeaders::Iterator &HttpHeaders::Iterator::operator++() {
  auto node = get_node(node_);
  node_ = node->next.tqe_next;

  return *this;
}

bool HttpHeaders::Iterator::operator!=(const Iterator &it) const {
  return node_ != it.node_;
}

HttpHeaders::Iterator HttpHeaders::begin() { return pImpl_->hdrs->tqh_first; }

HttpHeaders::Iterator HttpHeaders::end() { return *(pImpl_->hdrs->tqh_last); }

HttpHeaders::HttpHeaders(HttpHeaders &&) = default;
HttpHeaders::~HttpHeaders() = default;

// wrap evhttp_request

struct RequestHandlerCtx {
  HttpRequest *req;
  HttpRequest::RequestHandler cb;
  void *cb_data;
};

void HttpRequest::sync_callback(HttpRequest *req, void *) {
  // if connection was successful, keep the request-object alive past this
  // request-handler lifetime
  evhttp_request *ev_req = req->pImpl_->req.get();
  if (ev_req) {
#if LIBEVENT_VERSION_NUMBER >= 0x02010600
    evhttp_request_own(ev_req);
#else
    // swap the evhttp_request, as evhttp_request_own() is broken
    // before libevent 2.1.6.
    //
    // see: https://github.com/libevent/libevent/issues/68
    //
    // libevent will free the event, let's make sure it only sees
    // an empty evhttp_request
    auto *copied_req = evhttp_request_new(nullptr, nullptr);

    std::swap(copied_req->evcon, ev_req->evcon);
    std::swap(copied_req->flags, ev_req->flags);

    std::swap(copied_req->input_headers, ev_req->input_headers);
    std::swap(copied_req->output_headers, ev_req->output_headers);
    std::swap(copied_req->remote_host, ev_req->remote_host);
    std::swap(copied_req->remote_port, ev_req->remote_port);
    std::swap(copied_req->host_cache, ev_req->host_cache);
    std::swap(copied_req->kind, ev_req->kind);
    std::swap(copied_req->type, ev_req->type);
    std::swap(copied_req->headers_size, ev_req->headers_size);
    std::swap(copied_req->body_size, ev_req->body_size);
    std::swap(copied_req->uri, ev_req->uri);
    std::swap(copied_req->uri_elems, ev_req->uri_elems);
    std::swap(copied_req->major, ev_req->major);
    std::swap(copied_req->minor, ev_req->minor);
    std::swap(copied_req->response_code, ev_req->response_code);
    std::swap(copied_req->response_code_line, ev_req->response_code_line);
    std::swap(copied_req->input_buffer, ev_req->input_buffer);
    std::swap(copied_req->ntoread, ev_req->ntoread);
    copied_req->chunked = ev_req->chunked;
    copied_req->userdone = ev_req->userdone;
    std::swap(copied_req->output_buffer, ev_req->output_buffer);

    // release the old one, and let the event-loop free it
    req->pImpl_->req.release();

    // but take ownership of the new one
    req->pImpl_->req.reset(copied_req);
    req->pImpl_->own();
#endif
  }
}

HttpRequest::HttpRequest(HttpRequest::RequestHandler cb, void *cb_arg) {
  auto *ev_req = evhttp_request_new(
      [](evhttp_request *req, void *ev_cb_arg) {
        auto *ctx = static_cast<RequestHandlerCtx *>(ev_cb_arg);

        if ((req == nullptr) && (errno != 0)) {
          // request failed. Try to capture the last errno and hope
          // it is related to the failure
          ctx->req->socket_error_code({errno, std::system_category()});
        }

        ctx->req->pImpl_->req.release();   // the old request object may already
                                           // be free()ed in case of error
        ctx->req->pImpl_->req.reset(req);  // just update with what we have
        ctx->cb(ctx->req, ctx->cb_data);

        delete ctx;
      },
      new RequestHandlerCtx{this, cb, cb_arg});

#if LIBEVENT_VERSION_NUMBER >= 0x02010000
  evhttp_request_set_error_cb(
      ev_req, [](evhttp_request_error err_code, void *ev_cb_arg) {
        auto *ctx = static_cast<RequestHandlerCtx *>(ev_cb_arg);

        ctx->req->error_code(err_code);
      });
#endif

  pImpl_ = impl_new<HttpRequest::impl>(ev_req, evhttp_request_free);
}

HttpRequest::HttpRequest(std::unique_ptr<impl> &&impl)
    : pImpl_{std::move(impl)} {}

HttpRequest::HttpRequest(HttpRequest &&rhs) : pImpl_{std::move(rhs.pImpl_)} {}

HttpRequest::~HttpRequest() = default;

void HttpRequest::socket_error_code(std::error_code error_code) {
  pImpl_->socket_error_code_ = error_code;
}

std::error_code HttpRequest::socket_error_code() const {
  return pImpl_->socket_error_code_;
}

void HttpRequest::send_error(int status_code, std::string status_text) {
  evhttp_send_error(pImpl_->req.get(), status_code, status_text.c_str());
}

void HttpRequest::send_reply(int status_code, std::string status_text,
                             HttpBuffer &chunk) {
  evhttp_send_reply(pImpl_->req.get(), status_code, status_text.c_str(),
                    chunk.pImpl_->buffer.get());
}

void HttpRequest::send_reply(int status_code, std::string status_text) {
  evhttp_send_reply(pImpl_->req.get(), status_code, status_text.c_str(),
                    nullptr);
}

HttpRequest::operator bool() const { return pImpl_->req.operator bool(); }

void HttpRequest::error_code(int err_code) { pImpl_->error_code = err_code; }

int HttpRequest::error_code() { return pImpl_->error_code; }

std::string HttpRequest::error_msg() {
  switch (pImpl_->error_code) {
#if LIBEVENT_VERSION_NUMBER >= 0x02010000
    case EVREQ_HTTP_TIMEOUT:
      return "timeout";
    case EVREQ_HTTP_EOF:
      return "eof";
    case EVREQ_HTTP_INVALID_HEADER:
      return "invalid-header";
    case EVREQ_HTTP_BUFFER_ERROR:
      return "buffer-error";
    case EVREQ_HTTP_REQUEST_CANCEL:
      return "request-cancel";
    case EVREQ_HTTP_DATA_TOO_LONG:
      return "data-too-long";
#endif
    default:
      return "unknown";
  }
}

HttpUri HttpRequest::get_uri() const {
  // return a wrapper around a borrowed evhttp_uri
  //
  // it is owned by the HttpRequest, not by the HttpUri itself

  auto uri_impl = impl_new<HttpUri::impl>(
      const_cast<struct evhttp_uri *>(
          evhttp_request_get_evhttp_uri(pImpl_->req.get())),
      [](struct evhttp_uri *) {});

  return HttpUri{std::move(uri_impl)};
}

HttpHeaders HttpRequest::get_output_headers() {
  auto *ev_req = pImpl_->req.get();

  if (nullptr == ev_req) {
    throw std::logic_error("request is null");
  }
  // wrap a non-owned pointer
  return impl_new<HttpHeaders::impl>(evhttp_request_get_output_headers(ev_req),
                                     [](evkeyvalq *) {});
}

HttpHeaders HttpRequest::get_input_headers() const {
  auto *ev_req = pImpl_->req.get();

  if (nullptr == ev_req) {
    throw std::logic_error("request is null");
  }
  // wrap a non-owned pointer
  return impl_new<HttpHeaders::impl>(evhttp_request_get_input_headers(ev_req),
                                     [](evkeyvalq *) {});
}

HttpBuffer HttpRequest::get_output_buffer() {
  // wrap a non-owned pointer
  auto *ev_req = pImpl_->req.get();

  if (nullptr == ev_req) {
    throw std::logic_error("request is null");
  }

  auto result = impl_new<HttpBuffer::impl>(
      evhttp_request_get_output_buffer(ev_req), [](evbuffer *) {});
  return HttpBuffer(std::move(result));
}

unsigned HttpRequest::get_response_code() const {
  auto *ev_req = pImpl_->req.get();

  if (nullptr == ev_req) {
    throw std::logic_error("request is null");
  }

  return evhttp_request_get_response_code(ev_req);
}

std::string HttpRequest::get_response_code_line() const {
  auto *ev_req = pImpl_->req.get();
  if (nullptr == ev_req) {
    throw std::logic_error("request is null");
  }

#if LIBEVENT_VERSION_NUMBER >= 0x02010000
  const char *code_line = evhttp_request_get_response_code_line(ev_req);
  return code_line != nullptr ? code_line : "";
#else
  return HttpStatusCode::get_default_status_text(
      evhttp_request_get_response_code(ev_req));
#endif
}

HttpBuffer HttpRequest::get_input_buffer() const {
  auto *ev_req = pImpl_->req.get();

  if (nullptr == ev_req) {
    throw std::logic_error("request is null");
  }

  // wrap a non-owned pointer
  return impl_new<HttpBuffer::impl>(evhttp_request_get_input_buffer(ev_req),
                                    [](evbuffer *) {});
}

HttpMethod::type HttpRequest::get_method() const {
  return evhttp_request_get_command(pImpl_->req.get());
}

bool HttpRequest::is_modified_since(time_t last_modified) {
  auto req_hdrs = get_input_headers();

  auto *if_mod_since = req_hdrs.get("If-Modified-Since");
  if (if_mod_since != nullptr) {
    try {
      time_t if_mod_since_ts = time_from_rfc5322_fixdate(if_mod_since);

      if (!(last_modified > if_mod_since_ts)) {
        return false;
      }
    } catch (const std::exception &) {
      return false;
    }
  }
  return true;
}

bool HttpRequest::add_last_modified(time_t last_modified) {
  auto out_hdrs = get_output_headers();
  char date_buf[50];

  if (sizeof(date_buf) -
          time_to_rfc5322_fixdate(last_modified, date_buf, sizeof(date_buf)) >
      0) {
    out_hdrs.add("Last-Modified", date_buf);

    return true;
  } else {
    return false;
  }
}
