/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>

#include <sys/types.h>  // timeval

#include "my_thread.h"  // my_thread_self_setname
#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/impl/socket_error.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/net_ts/socket.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/plugin_config.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/utility/string.h"
#include "scope_guard.h"

#include "http_auth.h"
#include "http_server_plugin.h"
#include "mysql/harness/tls_server_context.h"
#include "mysqlrouter/http_auth_realm_component.h"
#include "mysqlrouter/http_common.h"
#include "mysqlrouter/http_server_component.h"
#include "mysqlrouter/supported_http_options.h"
#include "static_files.h"

IMPORT_LOG_FUNCTIONS()

static constexpr const char kSectionName[]{"http_server"};

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
  request_handlers_.emplace_back(RouterData{
      url_regex_str, std::regex{url_regex_str, std::regex_constants::extended},
      std::move(cb)});
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
    if (std::regex_search(uri.get_path(), request_handler.url_regex)) {
      request_handler.handler->handle_request(req);
      return;
    }
  }

  route_default(req);
}

void HttpRequestThread::accept_socket() {
  // we could replace the callback after accept here, but sadly
  // we don't have access to it easily
  event_http_.accept_socket_with_handle(accept_fd_);
}

void HttpRequestThread::set_request_router(HttpRequestRouter &router) {
  event_http_.set_gencb(
      [](HttpRequest *req, void *user_data) {
        auto *rtr = static_cast<HttpRequestRouter *>(user_data);
        rtr->route(std::move(*req));
      },
      &router);
}

void HttpRequestThread::wait_and_dispatch() {
  event_base_.once(-1, {EventFlags::Timeout},
                   HttpRequestThread::on_event_loop_ready, this, nullptr);
  event_base_.dispatch();

  // In case when something fails in `ev_base`, while first dispatch, then
  // there is a possibility that `initialized` was not set by the
  // `on_event_loop_read`. Next code line is going to ensure that no
  // thread is going to wait for this worker to become ready.
  initialization_finished();
}

void HttpRequestThread::break_dispatching() {
  // `loopexit` function is thread safe, and can be called
  // from different thread than the one is handling events
  // in wait_and_dispatch.
  //
  // There is one additional requirement that needs to be
  // fulfilled, `libevents` locks must be initialized with
  // `evthread_use_pthreads` or similar function.
  event_base_.loop_exit(nullptr);
}

void HttpRequestThread::wait_until_ready() {
  initialized_.wait([](auto value) -> bool { return value == true; });
}

void HttpRequestThread::on_event_loop_ready(native_handle_type, short,
                                            void *ctxt) {
  auto current = reinterpret_cast<HttpRequestThread *>(ctxt);

  current->initialization_finished();
}

bool HttpRequestThread::is_initalized() const {
  bool result;

  initialized_.serialize_with_cv(
      [&result](auto &initialized, auto &) { result = initialized; });

  return result;
}

void HttpRequestThread::initialization_finished() {
  initialized_.serialize_with_cv([](auto &initialized, auto &cv) {
    initialized = true;
    cv.notify_one();
  });
}

class HttpRequestMainThread : public HttpRequestThread {
 public:
  void bind(net::ip::tcp::acceptor &listen_sock, const std::string &address,
            uint16_t port) {
    auto bind_res = bind_acceptor(listen_sock, address, port);
    if (!bind_res) throw std::system_error(bind_res.error());

    accept_fd_ = listen_sock.native_handle();

    auto handle = event_http_.accept_socket_with_handle(accept_fd_);
    if (!handle.is_valid()) {
      const auto ec = net::impl::socket::last_error_code();

      throw std::system_error(ec, "evhttp_accept_socket_with_handle() failed");
    }
  }

 private:
  static stdx::expected<void, std::error_code> bind_acceptor(
      net::ip::tcp::acceptor &sock, const std::string &address, uint16_t port) {
    net::ip::tcp::resolver resolver(sock.get_executor().context());
    auto resolve_res = resolver.resolve(address, std::to_string(port));
    if (!resolve_res) return stdx::make_unexpected(resolve_res.error());

    for (auto const &resolved : resolve_res.value()) {
      sock.close();

      auto open_res = sock.open(resolved.endpoint().protocol());
      if (!open_res) return open_res.get_unexpected();

      sock.native_non_blocking(true);
      auto setop_res = sock.set_option(net::socket_base::reuse_address(true));
      if (!setop_res) return setop_res.get_unexpected();

      setop_res = sock.set_option(net::socket_base::keep_alive(true));
      if (!setop_res) return setop_res.get_unexpected();

      auto bind_res = sock.bind(resolved.endpoint());
      if (!bind_res) return bind_res.get_unexpected();

      auto listen_res = sock.listen(128);
      if (!listen_res) return listen_res.get_unexpected();

      return listen_res;
    }

    // no address
    return stdx::make_unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }
};

class HttpsRequestMainThread : public HttpRequestMainThread {
 public:
  HttpsRequestMainThread(TlsServerContext *tls_ctx) {
    event_http_.set_bevcb(
        [](EventBase *base, void *arg) {
          return EventBuffer(base, -1, static_cast<TlsServerContext *>(arg),
                             EventBuffer::SslState::Accepting,
                             {EventBufferOptionsFlags::CloseOnFree});
        },
        tls_ctx);
  }
};

class HttpRequestWorkerThread : public HttpRequestThread {
 public:
  explicit HttpRequestWorkerThread(native_handle_type accept_fd) {
    accept_fd_ = accept_fd;
  }
};

class HttpsRequestWorkerThread : public HttpRequestWorkerThread {
 public:
  explicit HttpsRequestWorkerThread(native_handle_type accept_fd,
                                    TlsServerContext *tls_ctx)
      : HttpRequestWorkerThread(accept_fd) {
    event_http_.set_bevcb(
        [](EventBase *base, void *arg) {
          return EventBuffer(base, -1, static_cast<TlsServerContext *>(arg),
                             EventBuffer::SslState::Accepting,
                             {EventBufferOptionsFlags::CloseOnFree});
        },
        tls_ctx);
  }
};

void HttpServer::join_all() {
  while (!sys_threads_.empty()) {
    auto &thr = sys_threads_.back();
    thr.join();
    sys_threads_.pop_back();
  }

  thread_contexts_.clear();
}

void HttpServer::start(size_t max_threads) {
  {
    auto main_thread = HttpRequestMainThread();
    main_thread.bind(listen_sock_, address_, port_);

    thread_contexts_.emplace_back(std::move(main_thread));
  }

  const net::impl::socket::native_handle_type accept_fd =
      thread_contexts_[0].get_socket_fd();
  for (size_t ndx = 1; ndx < max_threads; ndx++) {
    thread_contexts_.emplace_back(HttpRequestWorkerThread(accept_fd));
  }

  for (size_t ndx = 0; ndx < max_threads; ndx++) {
    auto &thr = thread_contexts_[ndx];

    sys_threads_.emplace_back([&]() {
      my_thread_self_setname("HttpSrv Worker");

      thr.set_request_router(request_router_);
      thr.accept_socket();
      thr.wait_and_dispatch();
    });
  }

  for (auto &thr : thread_contexts_) {
    thr.wait_until_ready();
  }
}

void HttpServer::stop() {
  for (auto &worker : thread_contexts_) {
    worker.break_dispatching();
  }
}

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
    auto main_thread = HttpsRequestMainThread(&ssl_ctx_);
    main_thread.bind(listen_sock_, address_, port_);
    thread_contexts_.emplace_back(std::move(main_thread));
  }

  const net::impl::socket::native_handle_type accept_fd =
      thread_contexts_[0].get_socket_fd();
  for (size_t ndx = 1; ndx < max_threads; ndx++) {
    thread_contexts_.emplace_back(
        HttpsRequestWorkerThread(accept_fd, &ssl_ctx_));
  }

  for (size_t ndx = 0; ndx < max_threads; ndx++) {
    auto &thr = thread_contexts_[ndx];

    sys_threads_.emplace_back([&]() {
      my_thread_self_setname("HttpSrv Worker");

      thr.set_request_router(request_router_);
      thr.accept_socket();
      thr.wait_and_dispatch();
    });
  }
}

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

using mysql_harness::IntOption;
using mysql_harness::StringOption;

#define GET_OPTION_CHECKED(option, section, name, value)                      \
  static_assert(                                                              \
      mysql_harness::str_in_collection(http_server_supported_options, name)); \
  option = get_option(section, name, value);

class HttpServerPluginConfig : public mysql_harness::BasePluginConfig {
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
      : mysql_harness::BasePluginConfig(section) {
    GET_OPTION_CHECKED(static_basedir, section, "static_folder",
                       StringOption{});
    GET_OPTION_CHECKED(srv_address, section, "bind_address", StringOption{});
    GET_OPTION_CHECKED(require_realm, section, "require_realm", StringOption{});
    GET_OPTION_CHECKED(ssl_cert, section, "ssl_cert", StringOption{});
    GET_OPTION_CHECKED(ssl_key, section, "ssl_key", StringOption{});
    GET_OPTION_CHECKED(ssl_cipher, section, "ssl_cipher", StringOption{});
    GET_OPTION_CHECKED(ssl_dh_params, section, "ssl_dh_param", StringOption{});
    GET_OPTION_CHECKED(ssl_curves, section, "ssl_curves", StringOption{});
    GET_OPTION_CHECKED(with_ssl, section, "ssl", IntOption<bool>{});
    GET_OPTION_CHECKED(srv_port, section, "port", IntOption<uint16_t>{});
  }

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

      {
        const auto res =
            tls_ctx.load_key_and_cert(config.ssl_key, config.ssl_cert);
        if (!res) {
          throw std::system_error(
              res.error(), "using SSL private key file '" + config.ssl_key +
                               "' or SSL certificate file '" + config.ssl_cert +
                               "' failed");
        }
      }

      if (!config.ssl_curves.empty()) {
        if (tls_ctx.has_set_curves_list()) {
          const auto res = tls_ctx.curves_list(config.ssl_curves);
          if (!res) {
            throw std::system_error(res.error(), "using ssl-curves failed");
          }
        } else {
          throw std::invalid_argument(
              "setting ssl-curves is not supported by the ssl library, it "
              "should stay unset");
        }
      }

      {
        const auto res = tls_ctx.init_tmp_dh(config.ssl_dh_params);
        if (!res) {
          throw std::system_error(res.error(), "setting ssl_dh_params failed");
        }
      }

      if (!config.ssl_cipher.empty()) {
        const auto res = tls_ctx.cipher_list(config.ssl_cipher);
        if (!res) {
          throw std::system_error(res.error(), "using ssl-cipher list failed");
        }
      }

      if (Event::has_ssl()) {
        // tls-context is owned by the HttpsServer
        return std::make_shared<HttpsServer>(
            std::move(tls_ctx), config.srv_address, config.srv_port);
      } else {
        throw std::invalid_argument("SSL support disabled at compile-time");
      }
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

      Event::initialize_threads();
      Scope_guard initialization_finished([]() { Event::shutdown(); });

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

      http_servers.emplace(section->name, HttpServerFactory::create(config));

      auto srv = http_servers.at(section->name);

      // forward the global require-realm to the request-router
      srv->request_router().require_realm(config.require_realm);

      HttpServerComponent::get_instance().init(srv);

      if (!config.static_basedir.empty()) {
        srv->add_route("", std::make_unique<HttpStaticFolderHandler>(
                               config.static_basedir, config.require_realm));
      }

      initialization_finished.commit();
    }
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

static void deinit(mysql_harness::PluginFuncEnv *) { Event::shutdown(); }

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

  my_thread_self_setname("HttpSrv Main");

  try {
    auto srv = http_servers.at(get_config_section(env)->name);

    srv->start(8);
    mysql_harness::on_service_ready(env);

    // wait until we got asked to shutdown.
    //
    // 0 == wait-forever
    wait_for_stop(env, 0);
    srv->stop();

    srv->join_all();
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

static const std::array<const char *, 3> required = {{
    "logger",
    "router_openssl",
    // as long as this plugin links against http_auth_backend_lib which links
    // against metadata_cache there is a need to cleanup protobuf
    "router_protobuf",
}};

extern "C" {
mysql_harness::Plugin HTTP_SERVER_EXPORT harness_plugin_http_server = {
    mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
    mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
    "HTTP_SERVER",                           // name
    VERSION_NUMBER(0, 0, 1),
    // requires
    required.size(),
    required.data(),
    // conflicts
    0,
    nullptr,
    init,     // init
    deinit,   // deinit
    start,    // start
    nullptr,  // stop
    true,     // declares_readiness
    http_server_supported_options.size(),
    http_server_supported_options.data(),
};
}
