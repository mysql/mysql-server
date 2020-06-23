/*
  Copyright (c) 2018, 2020, Oracle and/or its affiliates.

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
 * HTTP server plugin.
 */

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>  // shared_ptr
#include <mutex>
#include <stdexcept>
#include <thread>

#include <sys/types.h>  // timeval

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/listener.h>
#include <event2/util.h>

// Harness interface include files
#include "common.h"  // rename_thread()
#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/utility/string.h"

#include "http_auth.h"
#include "http_server_plugin.h"
#include "mysqlrouter/http_auth_realm_component.h"
#include "mysqlrouter/http_common.h"
#include "mysqlrouter/http_server_component.h"
#include "mysqlrouter/plugin_config.h"
#include "posix_re.h"
#include "socket_operations.h"
#include "static_files.h"
#include "tls_server_context.h"

IMPORT_LOG_FUNCTIONS()

static constexpr const char kSectionName[]{"http_server"};

std::promise<void> stopper;
std::future<void> stopped = stopper.get_future();

/**
 * request router
 *
 * send requests for a path of the URI to a handler callback
 *
 * if no handler is found, reply with 404 not found
 */
void HttpRequestRouter::append(const std::string &url_regex_str,
                               std::unique_ptr<BaseRequestHandler> cb) {
  std::lock_guard<std::mutex> lock(route_mtx_);
  request_handlers_.emplace_back(
      RouterData{url_regex_str, PosixRE{url_regex_str}, std::move(cb)});
}

void HttpRequestRouter::remove(const std::string &url_regex_str) {
  std::lock_guard<std::mutex> lock(route_mtx_);
  for (auto it = request_handlers_.begin(); it != request_handlers_.end();) {
    if (it->url_regex_str == url_regex_str) {
      it = request_handlers_.erase(it);
    } else {
      it++;
    }
  }
}

// if no routes are specified, return 404
void HttpRequestRouter::route_default(HttpRequest &req) {
  if (default_route_) {
    default_route_->handle_request(req);
  } else {
    if (!require_realm_.empty()) {
      if (auto realm =
              HttpAuthRealmComponent::get_instance().get(require_realm_)) {
        if (HttpAuth::require_auth(req, realm)) {
          // request is already handled, nothing to do
          return;
        }

        // access granted, fall through
      }
    }
    req.send_error(HttpStatusCode::NotFound);
  }
}

void HttpRequestRouter::set_default_route(
    std::unique_ptr<BaseRequestHandler> cb) {
  std::lock_guard<std::mutex> lock(route_mtx_);
  default_route_ = std::move(cb);
}

void HttpRequestRouter::clear_default_route() {
  std::lock_guard<std::mutex> lock(route_mtx_);
  default_route_ = nullptr;
}

void HttpRequestRouter::route(HttpRequest req) {
  std::lock_guard<std::mutex> lock(route_mtx_);

  auto uri = req.get_uri();

  // CONNECT can't be routed to the request handlers as it doesn't have a "path"
  // part.
  //
  // If the client Accepts "application/problem+json", send it a RFC7807 error
  // otherwise a classic text/html one.
  if (req.get_method() == HttpMethod::Connect) {
    const char *hdr_accept = req.get_input_headers().get("Accept");
    if (hdr_accept &&
        std::string(hdr_accept).find("application/problem+json") !=
            std::string::npos) {
      req.get_output_headers().add("Content-Type", "application/problem+json");
      auto buffers = req.get_output_buffer();
      std::string json_problem(R"({
  "title": "Method Not Allowed",
  "status": 405
})");
      buffers.add(json_problem.data(), json_problem.size());
      int status_code = HttpStatusCode::MethodNotAllowed;
      req.send_reply(status_code,
                     HttpStatusCode::get_default_status_text(status_code),
                     buffers);
    } else {
      req.send_error(HttpStatusCode::MethodNotAllowed);
    }
    return;
  }

  for (auto &request_handler : request_handlers_) {
    if (request_handler.url_regex.search(uri.get_path())) {
      request_handler.handler->handle_request(req);
      return;
    }
  }

  route_default(req);
}

void stop_eventloop(evutil_socket_t, short, void *cb_arg) {
  auto *ev_base = static_cast<event_base *>(cb_arg);

  if (stopped.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    event_base_loopexit(ev_base, nullptr);
  }
}

void HttpRequestThread::accept_socket() {
  // we could replace the callback after accept here, but sadly
  // we don't have access to it easily
  evhttp_accept_socket_with_handle(ev_http.get(), accept_fd_);
}

void HttpRequestThread::set_request_router(HttpRequestRouter &router) {
  evhttp_set_gencb(
      ev_http.get(),
      [](evhttp_request *req, void *user_data) {
        auto *rtr = static_cast<HttpRequestRouter *>(user_data);
        rtr->route(
            HttpRequest{std::unique_ptr<evhttp_request,
                                        std::function<void(evhttp_request *)>>(
                req, [](evhttp_request *) {})});
      },
      &router);
}

void HttpRequestThread::wait_and_dispatch() {
  struct timeval tv {
    0, 10 * 1000
  };
  event_add(ev_shutdown_timer.get(), &tv);
  event_base_dispatch(ev_base.get());
}

class HttpRequestMainThread : public HttpRequestThread {
 public:
  void bind(const std::string &address, uint16_t port) {
    int err;
    struct addrinfo hints, *ainfo;

    auto *sock_ops = mysql_harness::SocketOperations::instance();

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    err = getaddrinfo(address.c_str(), std::to_string(port).c_str(), &hints,
                      &ainfo);
    if (err != 0) {
      throw std::runtime_error(std::string("getaddrinfo() failed: ") +
                               gai_strerror(err));
    }

    std::shared_ptr<void> exit_guard(nullptr,
                                     [&](void *) { freeaddrinfo(ainfo); });

    const auto accept_res = sock_ops->socket(
        ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
    if (!accept_res) {
      throw std::system_error(accept_res.error(), "socket() failed");
    }

    accept_fd_ = accept_res.value();

    if (evutil_make_socket_nonblocking(accept_fd_) < 0) {
      const auto ec = net::impl::socket::last_error_code();

      sock_ops->close(accept_fd_);

      throw std::system_error(ec, "evutil_make_socket_nonblocking() failed");
    }

    if (evutil_make_socket_closeonexec(accept_fd_) < 0) {
      const auto ec = net::impl::socket::last_error_code();

      sock_ops->close(accept_fd_);

      throw std::system_error(ec, "evutil_make_socket_closeonexec() failed");
    }

    {
      int option_value = 1;
      const auto sockopt_res =
          sock_ops->setsockopt(accept_fd_, SOL_SOCKET, SO_REUSEADDR,
                               reinterpret_cast<const char *>(&option_value),
                               static_cast<socklen_t>(sizeof(int)));
      if (!sockopt_res) {
        sock_ops->close(accept_fd_);

        throw std::system_error(sockopt_res.error(),
                                "setsockopt(SO_REUSEADDR) failed");
      }
    }

    {
      int option_value = 1;
      const auto sockopt_res =
          sock_ops->setsockopt(accept_fd_, SOL_SOCKET, SO_KEEPALIVE,
                               reinterpret_cast<const char *>(&option_value),
                               static_cast<socklen_t>(sizeof(int)));
      if (!sockopt_res) {
        sock_ops->close(accept_fd_);

        throw std::system_error(sockopt_res.error(),
                                "setsockopt(SO_KEEPALIVE) failed");
      }
    }

    {
      const auto bind_res =
          sock_ops->bind(accept_fd_, ainfo->ai_addr, ainfo->ai_addrlen);
      if (!bind_res) {
        sock_ops->close(accept_fd_);

        throw std::system_error(
            bind_res.error(),
            "bind('0.0.0.0:" + std::to_string(port) + ") failed");
      }
    }

    {
      const auto listen_res = sock_ops->listen(accept_fd_, 128);
      if (!listen_res) {
        sock_ops->close(accept_fd_);

        throw std::system_error(listen_res.error(), "listen() failed");
      }
    }

    auto handle = evhttp_accept_socket_with_handle(ev_http.get(), accept_fd_);

    if (nullptr == handle) {
      const auto ec = net::impl::socket::last_error_code();

      sock_ops->close(accept_fd_);

      throw std::system_error(ec, "evhttp_accept_socket_with_handle() failed");
    }
  }
};

#ifdef EVENT__HAVE_OPENSSL
class HttpsRequestMainThread : public HttpRequestMainThread {
 public:
  HttpsRequestMainThread(SSL_CTX *ssl_ctx) {
    evhttp_set_bevcb(
        ev_http.get(),
        [](struct event_base *base, void *arg) {
          return bufferevent_openssl_socket_new(
              base, -1, SSL_new(static_cast<SSL_CTX *>(arg)),
              BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_CLOSE_ON_FREE);
        },
        ssl_ctx);
  }
};
#endif

class HttpRequestWorkerThread : public HttpRequestThread {
 public:
  explicit HttpRequestWorkerThread(harness_socket_t accept_fd) {
    accept_fd_ = accept_fd;
  }
};

#ifdef EVENT__HAVE_OPENSSL
class HttpsRequestWorkerThread : public HttpRequestWorkerThread {
 public:
  explicit HttpsRequestWorkerThread(harness_socket_t accept_fd,
                                    SSL_CTX *ssl_ctx)
      : HttpRequestWorkerThread(accept_fd) {
    evhttp_set_bevcb(
        ev_http.get(),
        [](struct event_base *base, void *arg) {
          return bufferevent_openssl_socket_new(
              base, -1, SSL_new(static_cast<SSL_CTX *>(arg)),
              BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_CLOSE_ON_FREE);
        },
        ssl_ctx);
  }
};
#endif

void HttpServer::join_all() {
  while (!sys_threads_.empty()) {
    auto &thr = sys_threads_.back();
    thr.join();
    sys_threads_.pop_back();
  }
}

void HttpServer::start(size_t max_threads) {
  {
    auto main_thread = HttpRequestMainThread();
    main_thread.bind(address_, port_);
    thread_contexts_.emplace_back(std::move(main_thread));
  }

  const harness_socket_t accept_fd = thread_contexts_[0].get_socket_fd();
  for (size_t ndx = 1; ndx < max_threads; ndx++) {
    thread_contexts_.emplace_back(HttpRequestWorkerThread(accept_fd));
  }

  for (size_t ndx = 0; ndx < max_threads; ndx++) {
    auto &thr = thread_contexts_[ndx];

    sys_threads_.emplace_back([&]() {
      mysql_harness::rename_thread("HttpSrv Worker");

      thr.set_request_router(request_router_);
      thr.accept_socket();
      thr.wait_and_dispatch();
    });
  }
}

#ifdef EVENT__HAVE_OPENSSL
class HttpsServer : public HttpServer {
 public:
  HttpsServer(TlsServerContext &&tls_ctx, const std::string &address,
              uint16_t port)
      : HttpServer(address.c_str(), port), ssl_ctx_{std::move(tls_ctx)} {}
  void start(size_t max_threads) override;

 private:
  TlsServerContext ssl_ctx_;
};

void HttpsServer::start(size_t max_threads) {
  {
    auto main_thread = HttpsRequestMainThread(ssl_ctx_.get());
    main_thread.bind(address_, port_);
    thread_contexts_.emplace_back(std::move(main_thread));
  }

  const harness_socket_t accept_fd = thread_contexts_[0].get_socket_fd();
  for (size_t ndx = 1; ndx < max_threads; ndx++) {
    thread_contexts_.emplace_back(
        HttpsRequestWorkerThread(accept_fd, ssl_ctx_.get()));
  }

  for (size_t ndx = 0; ndx < max_threads; ndx++) {
    auto &thr = thread_contexts_[ndx];

    sys_threads_.emplace_back([&]() {
      mysql_harness::rename_thread("HttpSrv Worker");

      thr.set_request_router(request_router_);
      thr.accept_socket();
      thr.wait_and_dispatch();
    });
  }
}
#endif

void HttpServer::add_route(const std::string &url_regex,
                           std::unique_ptr<BaseRequestHandler> cb) {
  log_debug("adding route for regex: %s", url_regex.c_str());
  if (url_regex.empty()) {
    request_router_.set_default_route(std::move(cb));
  } else {
    request_router_.append(url_regex, std::move(cb));
  }
}

void HttpServer::remove_route(const std::string &url_regex) {
  log_debug("removing route for regex: %s", url_regex.c_str());
  if (url_regex.empty()) {
    request_router_.clear_default_route();
  } else {
    request_router_.remove(url_regex);
  }
}

class HttpServerPluginConfig : public mysqlrouter::BasePluginConfig {
 public:
  std::string static_basedir;
  std::string srv_address;
  std::string require_realm;
  std::string ssl_cert;
  std::string ssl_key;
  std::string ssl_cipher;
  std::string ssl_dh_params;
  std::string ssl_curves;
  bool with_ssl;
  uint16_t srv_port;

  explicit HttpServerPluginConfig(const mysql_harness::ConfigSection *section)
      : mysqlrouter::BasePluginConfig(section),
        static_basedir(get_option_string(section, "static_folder")),
        srv_address(get_option_string(section, "bind_address")),
        require_realm(get_option_string(section, "require_realm")),
        ssl_cert(get_option_string(section, "ssl_cert")),
        ssl_key(get_option_string(section, "ssl_key")),
        ssl_cipher(get_option_string(section, "ssl_cipher")),
        ssl_dh_params(get_option_string(section, "ssl_dh_param")),
        ssl_curves(get_option_string(section, "ssl_curves")),
        with_ssl(get_uint_option<bool>(section, "ssl")),
        srv_port(get_uint_option<uint16_t>(section, "port")) {}

  std::string get_default_ciphers() const {
    return mysql_harness::join(TlsServerContext::default_ciphers(), ":");
  }

  std::string get_default(const std::string &option) const override {
    const std::map<std::string, std::string> defaults{
        {"bind_address", "0.0.0.0"},
        {"port", "8081"},
        {"ssl", "0"},
        {"ssl_cipher", get_default_ciphers()},
    };

    auto it = defaults.find(option);
    if (it == defaults.end()) {
      return std::string();
    }
    return it->second;
  }

  bool is_required(const std::string & /* option */) const override {
    return false;
  }
};
static std::map<std::string, std::shared_ptr<HttpServer>> http_servers;

class HttpServerFactory {
 public:
  static std::shared_ptr<HttpServer> create(
      const HttpServerPluginConfig &config) {
    if (config.with_ssl) {
      // init the TLS Server context according to our config-values
      TlsServerContext tls_ctx;
      tls_ctx.load_key_and_cert(config.ssl_cert, config.ssl_key);

      if (!config.ssl_curves.empty()) {
        if (tls_ctx.has_set_curves_list()) {
          tls_ctx.curves_list(config.ssl_curves);
        } else {
          throw std::invalid_argument(
              "setting ssl-curves is not supported by the ssl library, it "
              "should stay unset");
        }
      }

      tls_ctx.init_tmp_dh(config.ssl_dh_params);

      if (!config.ssl_cipher.empty()) tls_ctx.cipher_list(config.ssl_cipher);

#ifdef EVENT__HAVE_OPENSSL
      // tls-context is owned by the HttpsServer
      return std::make_shared<HttpsServer>(std::move(tls_ctx),
                                           config.srv_address, config.srv_port);
#else
      throw std::invalid_argument("SSL support disabled at compile-time");
#endif
    } else {
      return std::make_shared<HttpServer>(config.srv_address.c_str(),
                                          config.srv_port);
    }
  }
};

static void init(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::AppInfo *info = get_app_info(env);
  bool has_started = false;

  if (nullptr == info->config) {
    return;
  }

  // calls the openssl library initialize code
  TlsLibraryContext tls_lib_ctx;

  // assume there is only one section for us
  try {
    std::set<std::string> known_realms;
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name == "http_auth_realm") {
        known_realms.emplace(section->key);
      }
    }
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name != kSectionName) {
        continue;
      }

      if (has_started) {
        // ignore all the other sections for now
        continue;
      }

      has_started = true;

      HttpServerPluginConfig config{section};

      if (config.with_ssl &&
          (config.ssl_cert.empty() || config.ssl_key.empty())) {
        throw std::invalid_argument(
            "if ssl=1 is set, ssl_cert and ssl_key must be set too.");
      }

      if (!config.require_realm.empty() &&
          (known_realms.find(config.require_realm) == known_realms.end())) {
        throw std::invalid_argument(
            "unknown authentication realm for [http_server] '" + section->key +
            "': " + config.require_realm +
            ", known realm(s): " + mysql_harness::join(known_realms, ","));
      }

      log_info("listening on %s:%u", config.srv_address.c_str(),
               config.srv_port);

      http_servers.emplace(
          std::make_pair(section->name, HttpServerFactory::create(config)));

      auto srv = http_servers.at(section->name);

      // forward the global require-realm to the request-router
      srv->request_router().require_realm(config.require_realm);

      HttpServerComponent::get_instance().init(srv);

      if (!config.static_basedir.empty()) {
        srv->add_route("", std::make_unique<HttpStaticFolderHandler>(
                               config.static_basedir, config.require_realm));
      }
    }
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

static void start(mysql_harness::PluginFuncEnv *env) {
  // - version string
  // - hostname
  // - active-connections vs. max-connections
  // - listing port statistics
  //   - ip addresses
  // - backend status
  //   - throughput
  //   - response latency (min time first packet)
  //   - replication lag
  // - backend health
  // - configuration
  //   - ports
  //   - log-level
  //   - metadata server ip
  // - cluster health
  //   - time since last check
  //   - log group member changes
  // - important log messages
  // - mismatch between group-membership and metadata

  mysql_harness::rename_thread("HttpSrv Main");

  try {
    auto srv = http_servers.at(get_config_section(env)->name);

    // add routes

    srv->start(8);
    mysql_harness::on_service_ready(env);

    // wait until we got asked to shutdown.
    //
    // 0 == wait-forever
    wait_for_stop(env, 0);
    stopper.set_value();

    srv->join_all();
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

const static std::array<const char *, 1> required = {{
    "logger",
}};

extern "C" {
mysql_harness::Plugin HTTP_SERVER_EXPORT harness_plugin_http_server = {
    mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
    mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
    "HTTP_SERVER",                           // name
    VERSION_NUMBER(0, 0, 1),
    // requires
    required.size(), required.data(),
    // conflicts
    0, nullptr,
    init,     // init
    nullptr,  // deinit
    start,    // start
    nullptr,  // stop
    true,     // declares_readiness
};
}
