/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

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

#include <unicode/uclean.h>  // u_cleanup

#include "my_thread.h"  // my_thread_self_setname
#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/dynamic_config.h"
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

#include "http/http_server_context.h"
#include "http_auth.h"
#include "mysql/harness/section_config_exposer.h"
#include "mysql/harness/tls_server_context.h"
#include "mysqlrouter/component/http_auth_realm_component.h"
#include "mysqlrouter/component/http_server_component.h"
#include "mysqlrouter/http_constants.h"
#include "mysqlrouter/http_server_export.h"
#include "mysqlrouter/io_component.h"
#include "mysqlrouter/supported_http_options.h"
#include "static_files.h"

IMPORT_LOG_FUNCTIONS()

static constexpr const char kHttpServerSectionName[]{"http_server"};
static constexpr const char kDefaultBindAddress[]{"0.0.0.0"};
static constexpr const uint16_t kDefaultPort{8081};
static constexpr const unsigned kDefaultSsl{0};

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

  std::string get_default(std::string_view option) const override {
    const std::map<std::string_view, std::string> defaults{
        {"bind_address", kDefaultBindAddress},
        {"port", std::to_string(kDefaultPort)},
        {"ssl", std::to_string(kDefaultSsl)},
        {"ssl_cipher", get_default_ciphers()},
    };

    const auto it = defaults.find(option);
    if (it == defaults.end()) {
      return std::string();
    }
    return it->second;
  }

  bool is_required(std::string_view /* option */) const override {
    return false;
  }
};

static std::map<std::string, std::shared_ptr<http::HttpServerContext>>
    http_servers;

// work-guards to keep the io-context alive
//
// - one per routing instance
// - it MUST be taken before the io-context 'starts'
// - it MUST be released after routing is finished using it (before routing
// 'start' ends)
static std::mutex io_context_work_guard_mtx;
static std::list<IoComponent::Workguard> io_context_work_guards;

class HttpServerFactory {
 public:
  static std::shared_ptr<http::HttpServerContext> create(
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

      auto &io = IoComponent::get_instance();
      return std::make_shared<http::HttpServerContext>(
          &io.io_context(), &io.io_threads(), std::move(tls_ctx),
          config.srv_address.c_str(), config.srv_port);
    }

    auto &io = IoComponent::get_instance();

    return std::make_shared<http::HttpServerContext>(
        &io.io_context(), &io.io_threads(), config.srv_address.c_str(),
        config.srv_port);
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
      if (section->name != kHttpServerSectionName) {
        continue;
      }

      io_context_work_guards.emplace_back(IoComponent::get_instance());

      if (has_started) {
        // ignore all the other sections for now
        continue;
      }

      has_started = true;

      const HttpServerPluginConfig config{section};

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

      // There is possibility to put multiple http_servers to the map,
      // anyway in the current version the "init" function limits it to just
      // one.
      http_servers.emplace(section->name, HttpServerFactory::create(config));

      log_info("listening on %s:%u", config.srv_address.c_str(),
               config.srv_port);

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
    io_context_work_guards.clear();
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
    io_context_work_guards.clear();
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
    io_context_work_guards.clear();
  }
}

static void deinit(mysql_harness::PluginFuncEnv *) {
  http_servers.clear();

  io_context_work_guards.clear();

  u_cleanup();
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

  my_thread_self_setname("HttpSrv Main");

  try {
    auto srv = http_servers.at(get_config_section(env)->name);

    srv->start();
    mysql_harness::on_service_ready(env);

    // wait until we got asked to shutdown.
    //
    // 0 == wait-forever
    wait_for_stop(env, 0);

    srv->stop();
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }

  {
    std::lock_guard<std::mutex> lk(io_context_work_guard_mtx);

    io_context_work_guards.erase(io_context_work_guards.begin());
  }
}

static constexpr std::array required{
    "logger",
    "router_openssl",
    // as long as this plugin links against http_auth_backend_lib which links
    // against metadata_cache there is a need to cleanup protobuf
    "router_protobuf",
    "io",
};

namespace {

class HttpServerConfigExposer : public mysql_harness::SectionConfigExposer {
 public:
  using DC = mysql_harness::DynamicConfig;
  HttpServerConfigExposer(const bool initial,
                          const HttpServerPluginConfig &plugin_config,
                          const mysql_harness::ConfigSection &default_section)
      : mysql_harness::SectionConfigExposer(
            initial, default_section,
            DC::SectionId{kHttpServerSectionName, ""}),
        plugin_config_(plugin_config) {}

  void expose() override {
    expose_option("static_folder", plugin_config_.static_basedir, "");
    expose_option("bind_address", plugin_config_.srv_address,
                  kDefaultBindAddress);
    expose_option("require_realm", plugin_config_.require_realm, "");
    expose_option("ssl_cert", plugin_config_.ssl_cert, "");
    expose_option("ssl_key", plugin_config_.ssl_key, "");
    expose_option("ssl_cipher", plugin_config_.ssl_cipher,
                  plugin_config_.get_default_ciphers());
    expose_option("ssl_dh_params", plugin_config_.ssl_dh_params, "");
    expose_option("ssl_curves", plugin_config_.ssl_curves, "");
    expose_option("ssl", plugin_config_.with_ssl,
                  (kHttpPluginDefaultSslBootstrap == 1));
    expose_option("port", plugin_config_.srv_port,
                  kHttpPluginDefaultPortBootstrap);
  }

 private:
  const HttpServerPluginConfig &plugin_config_;
};

}  // namespace

static void expose_configuration(mysql_harness::PluginFuncEnv *env,
                                 const char * /*key*/, bool initial) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (!info->config) return;

  for (const mysql_harness::ConfigSection *section : info->config->sections()) {
    if (section->name == kHttpServerSectionName) {
      HttpServerPluginConfig config{section};
      HttpServerConfigExposer(initial, config,
                              info->config->get_default_section())
          .expose();
    }
  }
}

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
    expose_configuration,
};
}
