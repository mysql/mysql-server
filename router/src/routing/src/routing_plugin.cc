/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include "mysqlrouter/routing_plugin_export.h"

#include <atomic>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "dim.h"
#include "hostname_validator.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/loader_config.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/tls_server_context.h"
#include "mysql/harness/utility/string.h"  // join, string_format
#include "mysql_routing.h"
#include "mysqlrouter/connection_pool.h"
#include "mysqlrouter/connection_pool_component.h"
#include "mysqlrouter/destination.h"
#include "mysqlrouter/io_component.h"
#include "mysqlrouter/routing_component.h"
#include "mysqlrouter/supported_routing_options.h"
#include "plugin_config.h"
#include "scope_guard.h"
#include "ssl_mode.h"

using mysql_harness::AppInfo;
using mysql_harness::ConfigSection;
using mysqlrouter::URI;
using mysqlrouter::URIError;
IMPORT_LOG_FUNCTIONS()

const mysql_harness::AppInfo *g_app_info;
static const std::string kSectionName = "routing";

static void validate_socket_info(const std::string &err_prefix,
                                 const mysql_harness::ConfigSection *section,
                                 const RoutingPluginConfig &config) {
  auto is_valid_port = [](int port) -> bool {
    return 0 < port && port < 65536;
  };

  bool have_named_sock = section->has("socket");
  bool have_bind_port = section->has("bind_port");
  bool have_bind_addr = section->has("bind_address");
  bool have_bind_addr_port = have_bind_addr && config.bind_address.port() != 0;

  // NOTE: Several test scenarios below are also covered by
  // RoutingPluginConfig() constructor's simple check.
  //       However, RoutingPluginConfig() doesn't check everything, and
  //       sometimes produces misleading error messages (when bind_address has
  //       no port number, and bind_port is not provided, is one such example;
  //       passing empty socket is another). Below tests should probably replace
  //       the simple check in that constructor, and get called from both
  //       places.

  // validate bind_port
  if (have_bind_port && !is_valid_port(config.bind_port)) {
    throw std::invalid_argument(err_prefix + "invalid bind_port '" +
                                std::to_string(config.bind_port) + "'");
  }

  // validate bind_address : IP
  if (have_bind_addr &&
      !mysql_harness::is_valid_domainname(config.bind_address.address())) {
    throw std::invalid_argument(err_prefix +
                                "invalid IP or name in bind_address '" +
                                config.bind_address.str() + "'");
  }

  // validate bind_address : TCP port
  if (have_bind_addr_port && !is_valid_port(config.bind_address.port())) {
    throw std::invalid_argument(err_prefix + "invalid bind_address '" +
                                config.bind_address.str() + "'");
  }

  // validate socket
  if (have_named_sock && !config.named_socket.is_set()) {
    throw std::invalid_argument(err_prefix + "invalid socket '" +
                                config.named_socket.str() + "'");
  }

  // check if we have enough information to open some listening socket (a Unix
  // socket/Windows named pipe or a TCP socket)
  if (!(have_named_sock || have_bind_port || have_bind_addr_port)) {
    if (have_bind_addr) {
      throw std::invalid_argument(err_prefix +
                                  "no socket, no bind_port, and TCP port in "
                                  "bind_address is not provided");
    } else {
      throw std::invalid_argument(
          err_prefix + "one of bind_port, bind_address, or socket is required");
    }
  }
}

// a non-static gateway function, meant to be called only by unit tests (to
// allow calling validate_socket_info(), which is static)
void validate_socket_info_test_proxy(
    const std::string &err_prefix, const mysql_harness::ConfigSection *section,
    const RoutingPluginConfig &config) {
  validate_socket_info(err_prefix, section, config);
}

// work-guards to keep the io-context alive
//
// - one per routing instance
// - it MUST be taken before the io-context 'starts'
// - it MUST be released after routing is finished using it (before routing
// 'start' ends)
std::mutex io_context_work_guard_mtx;
std::list<IoComponent::Workguard> io_context_work_guards;

static void init(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  try {
    if (info->config != nullptr) {
      MySQLRoutingComponent::get_instance().init(*info->config);
      bool have_metadata_cache = false;
      bool need_metadata_cache = false;
      std::vector<mysql_harness::TCPAddress> bind_addresses;
      for (const mysql_harness::ConfigSection *section :
           info->config->sections()) {
        if (section->name == kSectionName) {
          io_context_work_guards.emplace_back(IoComponent::get_instance());

          const auto err_prefix = mysql_harness::utility::string_format(
              "in [%s%s%s]: ", section->name.c_str(),
              section->key.empty() ? "" : ":", section->key.c_str());
          // Check the configuration
          RoutingPluginConfig config(section);  // throws std::invalid_argument
          validate_socket_info(err_prefix, section,
                               config);  // throws std::invalid_argument

          // ensure that TCP port is unique
          if (config.bind_address.port()) {
            const auto &config_addr = config.bind_address;

            // Check uniqueness of bind_address and port, using IP address
            auto found_addr =
                std::find(bind_addresses.begin(), bind_addresses.end(),
                          config.bind_address);
            if (found_addr != bind_addresses.end()) {
              throw std::invalid_argument(
                  err_prefix + "duplicate IP or name found in bind_address '" +
                  config.bind_address.str() + "'");
            }
            // Check ADDR_ANY binding on same port
            else if (config_addr.address() == "0.0.0.0" ||
                     config_addr.address() == "::") {
              found_addr = std::find_if(
                  bind_addresses.begin(), bind_addresses.end(),
                  [&config](const mysql_harness::TCPAddress &addr) {
                    return config.bind_address.port() == addr.port();
                  });
              if (found_addr != bind_addresses.end()) {
                throw std::invalid_argument(
                    err_prefix +
                    "duplicate IP or name found in bind_address '" +
                    config.bind_address.str() + "'");
              }
            }
            bind_addresses.push_back(config.bind_address);
          }

          // We check if we need special plugins based on URI
          try {
            auto uri = URI(config.destinations, false);
            if (uri.scheme == "metadata-cache") {
              need_metadata_cache = true;
            }
          } catch (URIError &) {
            // No URI, no extra plugin needed
          }
        } else if (section->name == "metadata_cache") {
          have_metadata_cache = true;
        }
      }

      if (need_metadata_cache && !have_metadata_cache) {
        throw std::invalid_argument(
            "Routing needs Metadata Cache, but no none "
            "was found in configuration.");
      }
    }
    g_app_info = info;
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

static void ensure_readable_directory(const std::string &opt_name,
                                      const std::string &opt_value) {
  const auto p = mysql_harness::Path(opt_value);

  // if it is set, check it exists.
  if (!p.exists()) {
    throw std::runtime_error(opt_name + "=" + opt_value + " does not exist");
  }
  if (!p.is_directory()) {
    throw std::runtime_error(opt_name + "=" + opt_value +
                             " is not a directory");
  }
  if (!p.is_readable()) {
    throw std::runtime_error(opt_name + "=" + opt_value + " is not readable");
  }
}

static std::string get_default_ciphers() {
  return mysql_harness::join(TlsServerContext::default_ciphers(), ":");
}

static void start(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::ConfigSection *section = get_config_section(env);

  std::string name;
  if (!section->key.empty()) {
    name = section->name + ":" + section->key;
  } else {
    name = section->name;
  }

  try {
    RoutingPluginConfig config(section);

    // connect_timeout is in seconds, we want milli's
    //
    std::chrono::milliseconds destination_connect_timeout(
        config.connect_timeout * 1000);
    std::chrono::milliseconds client_connect_timeout(
        config.client_connect_timeout * 1000);

    // client side TlsContext.
    TlsServerContext source_tls_ctx;

    if (config.source_ssl_mode != SslMode::kDisabled &&
        config.source_ssl_mode != SslMode::kPassthrough) {
      if (config.source_ssl_cert.empty()) {
        throw std::invalid_argument(
            "client_ssl_cert must be set, if client_ssl_mode is enabled.");
      }
      if (config.source_ssl_key.empty()) {
        throw std::invalid_argument(
            "client_ssl_key must be set, if client_ssl_mode is enabled.");
      }

      const auto res = source_tls_ctx.load_key_and_cert(config.source_ssl_key,
                                                        config.source_ssl_cert);
      if (!res) {
        throw std::system_error(
            res.error(), "loading client_ssl_cert '" + config.source_ssl_key +
                             "' and client_ssl_key '" + config.source_ssl_cert +
                             "' failed");
      }

      if (!config.source_ssl_curves.empty()) {
        if (source_tls_ctx.has_set_curves_list()) {
          const auto res = source_tls_ctx.curves_list(config.source_ssl_curves);
          if (!res) {
            throw std::system_error(res.error(), "setting client_ssl_curves=" +
                                                     config.source_ssl_curves +
                                                     " failed");
          }

        } else {
          throw std::invalid_argument(
              "setting client_ssl_curves is not supported by the ssl library, "
              "it should stay unset");
        }
      }

      {
        const auto res =
            source_tls_ctx.init_tmp_dh(config.source_ssl_dh_params);
        if (!res) {
          throw std::system_error(res.error(),
                                  "setting client_ssl_dh_params failed");
        }
      }

      {
        const std::string ssl_cipher = config.source_ssl_cipher.empty()
                                           ? get_default_ciphers()
                                           : config.source_ssl_cipher;

        const auto res = source_tls_ctx.cipher_list(ssl_cipher);
        if (!res) {
          throw std::system_error(res.error(), "setting client_ssl_cipher=" +
                                                   ssl_cipher + " failed");
        }
      }
    }

    DestinationTlsContext dest_tls_ctx;
    if (config.dest_ssl_mode != SslMode::kDisabled) {
      // validate the config-values one time
      TlsServerContext tls_server_ctx;

      {
        const std::string dest_ssl_cipher = config.dest_ssl_cipher.empty()
                                                ? get_default_ciphers()
                                                : config.dest_ssl_cipher;
        const auto res = tls_server_ctx.cipher_list(dest_ssl_cipher);
        if (!res) {
          throw std::system_error(res.error(), "setting server_ssl_cipher=" +
                                                   dest_ssl_cipher + " failed");
        }
        dest_tls_ctx.ciphers(dest_ssl_cipher);
      }
      if (!config.dest_ssl_curves.empty()) {
        const auto res = tls_server_ctx.curves_list(config.dest_ssl_curves);
        if (!res) {
          if (res.error() == std::errc::function_not_supported) {
            throw std::runtime_error(
                "setting server_ssl_curves=" + config.dest_ssl_curves +
                " is not supported by the ssl library, it should stay unset");
          } else {
            throw std::system_error(res.error(), "setting server_ssl_curves=" +
                                                     config.dest_ssl_curves +
                                                     " failed");
          }
        }
        dest_tls_ctx.curves(config.dest_ssl_curves);
      }
      if (!config.dest_ssl_ca_file.empty() || !config.dest_ssl_ca_dir.empty()) {
        if (!config.dest_ssl_ca_dir.empty()) {
          // throws on error
          ensure_readable_directory("server_ssl_capath",
                                    config.dest_ssl_ca_dir);
        }
        const auto res = tls_server_ctx.ssl_ca(config.dest_ssl_ca_file,
                                               config.dest_ssl_ca_dir);
        if (!res) {
          throw std::system_error(
              res.error(), "setting server_ssl_ca=" + config.dest_ssl_ca_file +
                               " and server_ssl_capath=" +
                               config.dest_ssl_ca_dir + " failed");
        }
        dest_tls_ctx.ca_file(config.dest_ssl_ca_file);
        dest_tls_ctx.ca_path(config.dest_ssl_ca_dir);
      }

      if (!config.dest_ssl_crl_file.empty() ||
          !config.dest_ssl_crl_dir.empty()) {
        if (!config.dest_ssl_crl_dir.empty()) {
          // throws on error
          ensure_readable_directory("server_ssl_crlpath",
                                    config.dest_ssl_crl_dir);
        }
        const auto res = tls_server_ctx.crl(config.dest_ssl_crl_file,
                                            config.dest_ssl_crl_dir);
        if (!res) {
          throw std::system_error(
              res.error(),
              "setting server_ssl_crl=" + config.dest_ssl_crl_file +
                  " and server_ssl_crlpath=" + config.dest_ssl_crl_dir +
                  " failed");
        }

        dest_tls_ctx.crl_file(config.dest_ssl_crl_file);
        dest_tls_ctx.crl_path(config.dest_ssl_crl_dir);
      }

      dest_tls_ctx.verify(config.dest_ssl_verify);
    }

    if (config.connection_sharing == 1) {
      if (config.source_ssl_mode == SslMode::kPassthrough) {
        log_warning(
            "[%s].connection_sharing=1 has been ignored, as "
            "client_ssl_mode=PASSTHROUGH.",
            name.c_str());
      } else if (config.source_ssl_mode == SslMode::kPreferred &&
                 config.dest_ssl_mode == SslMode::kAsClient) {
        log_warning(
            "[%s].connection_sharing=1 has been ignored, as "
            "client_ssl_mode=PREFERRED and server_ssl_mode=AS_CLIENT.",
            name.c_str());
      }

      auto &pool_component = ConnectionPoolComponent::get_instance();
      auto default_pool_name = pool_component.default_pool_name();
      auto default_pool = pool_component.get(default_pool_name);
      if (!default_pool) {
        log_warning(
            "[%s].connection_sharing=1 has been ignored, as "
            "there is no [connection_pool]",
            name.c_str());
      } else if (default_pool->max_pooled_connections() == 0) {
        log_warning(
            "[%s].connection_sharing=1 has been ignored, as "
            "[connection_pool].max_idle_server_connections=0",
            name.c_str());
      }

      if (config.protocol == Protocol::Type::kXProtocol) {
        log_warning("[%s].connection_sharing=1 has been ignored, as protocol=x",
                    name.c_str());
      }
    }

    net::io_context &io_ctx = IoComponent::get_instance().io_context();
    auto r = std::make_shared<MySQLRouting>(
        io_ctx, config.routing_strategy, config.bind_address.port(),
        config.protocol, config.mode, config.bind_address.address(),
        config.named_socket, name, config.max_connections,
        destination_connect_timeout, config.max_connect_errors,
        client_connect_timeout, config.net_buffer_length,
        config.source_ssl_mode,
        config.source_ssl_mode != SslMode::kDisabled ? &source_tls_ctx
                                                     : nullptr,
        config.dest_ssl_mode,
        config.dest_ssl_mode != SslMode::kDisabled ? &dest_tls_ctx : nullptr,
        config.connection_sharing, config.connection_sharing_delay);

    try {
      // don't allow rootless URIs as we did already in the
      // get_option_destinations()
      r->set_destinations_from_uri(URI(config.destinations, false));
    } catch (const URIError &) {
      r->set_destinations_from_csv(config.destinations);
    }
    MySQLRoutingComponent::get_instance().register_route(section->key, r);

    Scope_guard guard{[section_key = section->key]() {
      MySQLRoutingComponent::get_instance().erase(section_key);
    }};

    r->run(env);
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::runtime_error &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s: %s", name.c_str(),
              exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }

  {
    // as the r->run() shuts down all in parallel, synchronize the access to
    std::lock_guard<std::mutex> lk(io_context_work_guard_mtx);

    io_context_work_guards.erase(io_context_work_guards.begin());
  }
}

static void deinit(mysql_harness::PluginFuncEnv * /* env */) {
  MySQLRoutingComponent::get_instance().deinit();
  // release all that may still be taken
  io_context_work_guards.clear();
}

static const std::array<const char *, 6> required = {{
    "logger",
    "router_protobuf",
    "router_openssl",
    "io",
    "connection_pool",
    "destination_status",
}};

mysql_harness::Plugin ROUTING_PLUGIN_EXPORT harness_plugin_routing = {
    mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
    mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
    "Routing MySQL connections between MySQL clients/connectors and "
    "servers",  // name
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
    routing_supported_options.size(),
    routing_supported_options.data(),
};
