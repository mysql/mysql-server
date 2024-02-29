/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include "plugin_config.h"

#include <algorithm>  // transform
#include <array>
#include <cinttypes>
#include <initializer_list>
#include <stdexcept>  // invalid_argument
#include <string>
#include <string_view>
#include <vector>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "context.h"
#include "dest_metadata_cache.h"  // get_server_role_from_uri
#include "hostname_validator.h"
#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/dynamic_config.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/section_config_exposer.h"
#include "mysql/harness/string_utils.h"  // trim
#include "mysql/harness/utility/string.h"
#include "mysql_router_thread.h"  // kDefaultStackSizeInKiloByte
#include "mysqlrouter/routing.h"  // Mode
#include "mysqlrouter/routing_component.h"
#include "mysqlrouter/ssl_mode.h"
#include "mysqlrouter/supported_routing_options.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"  // is_valid_socket_name
#include "tcp_address.h"

using namespace std::string_view_literals;
IMPORT_LOG_FUNCTIONS()

using StringOption = mysql_harness::StringOption;
using BoolOption = mysql_harness::BoolOption;
using DoubleOption = mysql_harness::DoubleOption;

template <class T>
using IntOption = mysql_harness::IntOption<T>;

class ProtocolOption {
 public:
  Protocol::Type operator()(const std::optional<std::string> &value,
                            const std::string & /* option_desc */) {
    if (!value) return Protocol::get_default();

    // all other cases are treated as "empty string"
    auto lc_value = value.value();

    std::transform(lc_value.begin(), lc_value.end(), lc_value.begin(),
                   ::tolower);

    return Protocol::get_by_name(lc_value);
  }
};

class AccessModeOption {
 public:
  routing::AccessMode operator()(const std::optional<std::string> &value,
                                 const std::string &option_desc) {
    if (!value.has_value() || value->empty()) {
      return routing::AccessMode::kUndefined;
    }

    std::string lc_value{value.value()};

    std::transform(lc_value.begin(), lc_value.end(), lc_value.begin(),
                   ::tolower);

    routing::AccessMode result = routing::get_access_mode(lc_value);
    if (result == routing::AccessMode::kUndefined) {
      const std::string valid = routing::get_access_mode_names();
      throw std::invalid_argument(option_desc + " is invalid; valid are " +
                                  valid + " (was '" + value.value() + "')");
    }

    return result;
  }
};

class RoutingStrategyOption {
 public:
  RoutingStrategyOption(bool is_metadata_cache)
      : is_metadata_cache_{is_metadata_cache} {}

  routing::RoutingStrategy operator()(const std::optional<std::string> &value,
                                      const std::string &option_desc) {
    if (!value) {
      throw std::invalid_argument(option_desc + " is required");
      return routing::RoutingStrategy::kUndefined;
    } else if (value->empty()) {
      throw std::invalid_argument(option_desc + " needs a value");
    }

    std::string lc_value{value.value()};
    std::transform(lc_value.begin(), lc_value.end(), lc_value.begin(),
                   ::tolower);

    auto result = routing::get_routing_strategy(lc_value);
    if (result == routing::RoutingStrategy::kUndefined ||
        ((result == routing::RoutingStrategy::kRoundRobinWithFallback) &&
         !is_metadata_cache_)) {
      const std::string valid =
          routing::get_routing_strategy_names(is_metadata_cache_);
      throw std::invalid_argument(option_desc + " is invalid; valid are " +
                                  valid + " (was '" + value.value() + "')");
    }
    return result;
  }

 private:
  bool is_metadata_cache_;
};

class DestinationsOption {
 public:
  DestinationsOption(bool &metadata_cache) : metadata_cache_{metadata_cache} {}

  std::string operator()(const std::string &value,
                         const std::string &option_desc) {
    try {
      // disable root-less paths like mailto:foo@example.org to stay
      // backward compatible with
      //
      //   localhost:1234,localhost:1235
      //
      // which parse into:
      //
      //   scheme: localhost
      //   path: 1234,localhost:1235
      auto uri = mysqlrouter::URI(value,  // raises URIError when URI is invalid
                                  false   // allow_path_rootless
      );
      if (uri.scheme == "metadata-cache") {
        metadata_cache_ = true;
      } else {
        throw std::invalid_argument(option_desc +
                                    " has an invalid URI scheme '" +
                                    uri.scheme + "' for URI " + value);
      }
      return value;
    } catch (const mysqlrouter::URIError &) {
      for (auto part : mysql_harness::split_string(value, ',')) {
        mysql_harness::trim(part);
        if (part.empty()) {
          throw std::invalid_argument(
              option_desc + ": empty address found in destination list (was '" +
              value + "')");
        }

        auto make_res = mysql_harness::make_tcp_address(part);

        if (!make_res) {
          throw std::invalid_argument(option_desc +
                                      ": address in destination list '" + part +
                                      "' is invalid");
        }

        auto address = make_res->address();

        if (!mysql_harness::is_valid_ip_address(address) &&
            !mysql_harness::is_valid_hostname(address)) {
          throw std::invalid_argument(option_desc +
                                      " has an invalid destination address '" +
                                      address + "'");
        }
      }
    }

    return value;
  }

 private:
  bool &metadata_cache_;
};

class NamedSocketOption {
 public:
  mysql_harness::Path operator()(const std::string &value,
                                 const std::string &option_desc) {
    std::string error;
    if (!mysqlrouter::is_valid_socket_name(value, error)) {
      throw std::invalid_argument(option_desc + ": " + error);
    }

    if (value.empty()) {
      return mysql_harness::Path();
    }
    return mysql_harness::Path(value);
  }
};

/**
 * empty or 1..65335
 */
class BindPortOption {
 public:
  uint16_t operator()(const std::string &value,
                      const std::string &option_desc) {
    if (value.empty()) return 0;

    return IntOption<uint16_t>{1}(value, option_desc);
  }
};

class TCPAddressOption {
 public:
  TCPAddressOption(bool require_port, int default_port)
      : require_port_{require_port}, default_port_{default_port} {}
  mysql_harness::TCPAddress operator()(const std::string &value,
                                       const std::string &option_desc) {
    if (value.empty()) return {};

    const auto make_res = mysql_harness::make_tcp_address(value);
    if (!make_res) {
      throw std::invalid_argument(option_desc + ": '" + value +
                                  "' is not a valid endpoint");
    }

    const auto address = make_res->address();
    uint16_t port = make_res->port();

    if (port <= 0) {
      if (default_port_ > 0) {
        port = static_cast<uint16_t>(default_port_);
      } else if (require_port_) {
        throw std::invalid_argument(option_desc + " requires a TCP port");
      }
    }

    if (!(mysql_harness::is_valid_hostname(address) ||
          mysql_harness::is_valid_ip_address(address))) {
      throw std::invalid_argument(option_desc + ": '" + address + "' in '" +
                                  value +
                                  "' is not a valid IP-address or hostname");
    }

    return {address, port};
  }

 private:
  const bool require_port_;
  const int default_port_;
};

class SslModeOption {
 public:
  SslModeOption(std::initializer_list<SslMode> allowed_ssl_modes)
      : allowed_ssl_modes_{allowed_ssl_modes} {}

  SslMode operator()(const std::string &value, const std::string &option_desc) {
    // convert name to upper-case to get case-insensitive comparison.
    auto uc_value = value;
    std::transform(value.begin(), value.end(), uc_value.begin(), ::toupper);

    // check if the mode is allowed
    const auto it =
        std::find_if(allowed_ssl_modes_.begin(), allowed_ssl_modes_.end(),
                     [uc_value](auto const &allowed_ssl_mode) {
                       return uc_value == ssl_mode_to_string(allowed_ssl_mode);
                     });
    if (it != allowed_ssl_modes_.end()) {
      return *it;
    }

    // build list of allowed modes, but don't mention the default case.
    std::string allowed_names;
    for (const auto &allowed_ssl_mode : allowed_ssl_modes_) {
      if (allowed_ssl_mode == SslMode::kDefault) continue;

      if (!allowed_names.empty()) {
        allowed_names.append(",");
      }

      allowed_names += ssl_mode_to_string(allowed_ssl_mode);
    }

    throw std::invalid_argument("invalid value '" + value + "' for " +
                                option_desc +
                                ". Allowed are: " + allowed_names + ".");
  }

 private:
  std::vector<SslMode> allowed_ssl_modes_;
};

/**
 * get the name for a SslVerify.
 *
 * @param verify a SslVerify value
 *
 * @returns name of a SslVerify
 * @retval nullptr if verify is unknown.
 */
static const char *ssl_verify_to_string(SslVerify verify) {
  switch (verify) {
    case SslVerify::kVerifyCa:
      return "VERIFY_CA";
    case SslVerify::kVerifyIdentity:
      return "VERIFY_IDENTITY";
    case SslVerify::kDisabled:
      return "DISABLED";
  }

  return nullptr;
}

class SslVerifyOption {
 public:
  SslVerifyOption(std::initializer_list<SslVerify> allowed_)
      : allowed_{allowed_} {}

  SslVerify operator()(const std::string &value,
                       const std::string &option_desc) {
    // convert name to upper-case to get case-insensitive comparison.
    auto uc_value = value;
    std::transform(value.begin(), value.end(), uc_value.begin(), ::toupper);

    // check if the mode is allowed
    const auto it = std::find_if(
        allowed_.begin(), allowed_.end(), [uc_value](auto const &allowed) {
          return uc_value == ssl_verify_to_string(allowed);
        });
    if (it != allowed_.end()) {
      return *it;
    }

    std::string allowed_names;
    for (const auto &allowed : allowed_) {
      if (!allowed_names.empty()) {
        allowed_names.append(",");
      }

      allowed_names += ssl_verify_to_string(allowed);
    }

    throw std::invalid_argument("invalid value '" + value + "' for " +
                                option_desc +
                                ". Allowed are: " + allowed_names + ".");
  }

 private:
  std::vector<SslVerify> allowed_;
};

class MaxConnectionsOption {
 public:
  uint16_t operator()(const std::string &value,
                      const std::string &option_desc) {
    const auto result = IntOption<uint16_t>{}(value, option_desc);

    auto &routing_component = MySQLRoutingComponent::get_instance();

    if (result != routing::kDefaultMaxConnections &&
        result > routing_component.max_total_connections()) {
      log_warning(
          "Value configured for max_connections > max_total_connections (%u "
          "> %" PRIu64 "). Will have no effect.",
          result, routing_component.max_total_connections());
    }

    return result;
  }
};

#define GET_OPTION_CHECKED(option, section, name, value)                  \
  static_assert(                                                          \
      mysql_harness::str_in_collection(routing_supported_options, name)); \
  option = get_option(section, name, value);

#define GET_OPTION_NO_DEFAULT_CHECKED(option, section, name, value)       \
  static_assert(                                                          \
      mysql_harness::str_in_collection(routing_supported_options, name)); \
  option = get_option_no_default(section, name, value);

/** @brief Constructor
 *
 * @param section from configuration file provided as ConfigSection
 */
RoutingPluginConfig::RoutingPluginConfig(
    const mysql_harness::ConfigSection *section)
    : BasePluginConfig{section}, metadata_cache_(false) {
  namespace options = routing::options;

  GET_OPTION_NO_DEFAULT_CHECKED(protocol, section, options::kProtocol,
                                ProtocolOption{});
  GET_OPTION_CHECKED(destinations, section, options::kDestinations,
                     DestinationsOption{metadata_cache_});
  GET_OPTION_CHECKED(bind_port, section, options::kBindPort, BindPortOption{});
  auto bind_address_op = TCPAddressOption{false, bind_port};
  GET_OPTION_CHECKED(bind_address, section, options::kBindAddress,
                     bind_address_op);
  GET_OPTION_CHECKED(named_socket, section, options::kSocket,
                     NamedSocketOption{});
  GET_OPTION_CHECKED(connect_timeout, section, options::kConnectTimeout,
                     IntOption<uint16_t>{1});
  auto routing_strategy_op = RoutingStrategyOption{metadata_cache_};
  GET_OPTION_NO_DEFAULT_CHECKED(routing_strategy, section,
                                options::kRoutingStrategy, routing_strategy_op);
  GET_OPTION_CHECKED(max_connections, section, options::kMaxConnections,
                     MaxConnectionsOption{});
  auto max_connect_errors_op = IntOption<uint32_t>{1, UINT32_MAX};
  GET_OPTION_CHECKED(max_connect_errors, section, options::kMaxConnectErrors,
                     max_connect_errors_op);
  auto client_connect_timeout_op = IntOption<uint32_t>{2, 31536000};
  GET_OPTION_CHECKED(client_connect_timeout, section,
                     options::kClientConnectTimeout, client_connect_timeout_op);
  auto net_buffer_length_op = IntOption<uint32_t>{1024, 1048576};
  GET_OPTION_CHECKED(net_buffer_length, section, options::kNetBufferLength,
                     net_buffer_length_op);
  auto thread_stack_size_op = IntOption<uint32_t>{1, 65535};
  GET_OPTION_CHECKED(thread_stack_size, section, options::kThreadStackSize,
                     thread_stack_size_op);
  auto source_ssl_mode_op =
      SslModeOption{SslMode::kDisabled, SslMode::kPreferred, SslMode::kRequired,
                    SslMode::kPassthrough, SslMode::kDefault};
  GET_OPTION_CHECKED(source_ssl_mode, section, options::kClientSslMode,
                     source_ssl_mode_op);
  GET_OPTION_CHECKED(source_ssl_cert, section, options::kClientSslCert,
                     StringOption{});
  GET_OPTION_CHECKED(source_ssl_key, section, options::kClientSslKey,
                     StringOption{});
  GET_OPTION_CHECKED(source_ssl_cipher, section, options::kClientSslCipher,
                     StringOption{});
  GET_OPTION_CHECKED(source_ssl_ca_file, section, options::kClientSslCa,
                     StringOption{});
  GET_OPTION_CHECKED(source_ssl_ca_dir, section, options::kClientSslCaPath,
                     StringOption{});
  GET_OPTION_CHECKED(source_ssl_crl_file, section, options::kClientSslCrl,
                     StringOption{});
  GET_OPTION_CHECKED(source_ssl_crl_dir, section, options::kClientSslCrlPath,
                     StringOption{});
  GET_OPTION_CHECKED(source_ssl_curves, section, options::kClientSslCurves,
                     StringOption{});
  GET_OPTION_CHECKED(source_ssl_dh_params, section, options::kClientSslDhParams,
                     StringOption{});
  auto dest_ssl_mode_op = SslModeOption{SslMode::kDisabled, SslMode::kPreferred,
                                        SslMode::kRequired, SslMode::kAsClient};
  GET_OPTION_CHECKED(dest_ssl_mode, section, options::kServerSslMode,
                     dest_ssl_mode_op);
  GET_OPTION_CHECKED(dest_ssl_cert, section, options::kServerSslCert,
                     StringOption{});
  GET_OPTION_CHECKED(dest_ssl_key, section, options::kServerSslKey,
                     StringOption{});
  auto dest_ssl_verify_op = SslVerifyOption{
      SslVerify::kDisabled, SslVerify::kVerifyCa, SslVerify::kVerifyIdentity};
  GET_OPTION_CHECKED(dest_ssl_verify, section, options::kServerSslVerify,
                     dest_ssl_verify_op);
  GET_OPTION_CHECKED(dest_ssl_cipher, section, options::kServerSslCipher,
                     StringOption{});
  GET_OPTION_CHECKED(dest_ssl_ca_file, section, options::kServerSslCa,
                     StringOption{});
  GET_OPTION_CHECKED(dest_ssl_ca_dir, section, options::kServerSslCaPath,
                     StringOption{});
  GET_OPTION_CHECKED(dest_ssl_crl_file, section, options::kServerSslCrl,
                     StringOption{});
  GET_OPTION_CHECKED(dest_ssl_crl_dir, section, options::kServerSslCrlPath,
                     StringOption{});
  GET_OPTION_CHECKED(dest_ssl_curves, section, options::kServerSslCurves,
                     StringOption{});
  auto ssl_session_cache_size_op = IntOption<uint32_t>{1, 0x7fffffff};
  auto ssl_session_cache_timeout_op = IntOption<uint32_t>{0, 84600};
  GET_OPTION_CHECKED(client_ssl_session_cache_mode, section,
                     options::kClientSslSessionCacheMode, BoolOption{});
  GET_OPTION_CHECKED(client_ssl_session_cache_size, section,
                     options::kClientSslSessionCacheSize,
                     ssl_session_cache_size_op);
  GET_OPTION_CHECKED(client_ssl_session_cache_timeout, section,
                     options::kClientSslSessionCacheTimeout,
                     ssl_session_cache_timeout_op);
  GET_OPTION_CHECKED(server_ssl_session_cache_mode, section,
                     options::kServerSslSessionCacheMode, BoolOption{});
  GET_OPTION_CHECKED(server_ssl_session_cache_size, section,
                     options::kServerSslSessionCacheSize,
                     ssl_session_cache_size_op);
  GET_OPTION_CHECKED(server_ssl_session_cache_timeout, section,
                     options::kServerSslSessionCacheTimeout,
                     ssl_session_cache_timeout_op);
  GET_OPTION_CHECKED(router_require_enforce, section,
                     options::kRouterRequireEnforce, BoolOption{});

  GET_OPTION_CHECKED(connection_sharing, section, options::kConnectionSharing,
                     BoolOption{});

  static_assert(mysql_harness::str_in_collection(
      routing_supported_options, options::kConnectionSharingDelay));
  connection_sharing_delay =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::duration<double, std::chrono::seconds::period>(
              get_option(section, options::kConnectionSharingDelay,
                         DoubleOption{0})));

  static_assert(mysql_harness::str_in_collection(
      routing_supported_options, options::kConnectRetryTimeout));
  connect_retry_timeout =
      get_option(section, options::kConnectRetryTimeout,
                 mysql_harness::MilliSecondsOption{0, 3600});

  GET_OPTION_CHECKED(access_mode, section, options::kAccessMode,
                     AccessModeOption{});
  GET_OPTION_CHECKED(wait_for_my_writes, section, options::kWaitForMyWrites,
                     BoolOption{});
  GET_OPTION_CHECKED(
      wait_for_my_writes_timeout, section, options::kWaitForMyWritesTimeout,
      mysql_harness::DurationOption<std::chrono::seconds>(0, 3600));

  if (access_mode == routing::AccessMode::kAuto) {
    if (!metadata_cache_) {
      throw std::invalid_argument(
          "'access_mode=auto' requires 'destinations=metadata-cache:...'");
    }
    auto uri =
        mysqlrouter::URI(destinations,  // raises URIError when URI is invalid
                         false          // allow_path_rootless
        );

    auto server_role = get_server_role_from_uri(uri.query);
    if (server_role !=
        DestMetadataCacheGroup::ServerRole::PrimaryAndSecondary) {
      throw std::invalid_argument(
          "'access_mode=auto' requires that "
          "the 'role' in 'destinations=metadata-cache:...?role=...' is "
          "'PRIMARY_AND_SECONDARY'");
    }

    if (protocol != Protocol::Type::kClassicProtocol) {
      throw std::invalid_argument(
          "'access_mode=auto' is only supported with 'protocol=classic'");
    }

    if (source_ssl_mode == SslMode::kPassthrough) {
      throw std::invalid_argument(
          "'access_mode=auto' is not supported with "
          "'client_ssl_mode=PASSTHROUGH'");
    }

    if (source_ssl_mode == SslMode::kPreferred &&
        dest_ssl_mode == SslMode::kAsClient) {
      throw std::invalid_argument(
          "'access_mode=auto' is not supported with "
          "'client_ssl_mode=PREFERRED' and 'server_ssl_mode=AS_CLIENT'");
    }

    if (!connection_sharing) {
      throw std::invalid_argument(
          "'access_mode=auto' requires 'connection_sharing=1'");
    }
  }

  using namespace std::string_literals;

  // either bind_address or socket needs to be set, or both
  if (!bind_address.port() && !named_socket.is_set()) {
    throw std::invalid_argument(
        "either bind_address or socket option needs to be supplied, or both");
  }

  // if client-ssl-mode isn't set, use either PASSTHROUGH or PREFERRED
  if (source_ssl_mode == SslMode::kDefault) {
    if (source_ssl_cert.empty() && source_ssl_key.empty()) {
      source_ssl_mode = SslMode::kPassthrough;
    } else {
      source_ssl_mode = SslMode::kPreferred;
    }
  }

  if (source_ssl_mode != SslMode::kDisabled &&
      source_ssl_mode != SslMode::kPassthrough) {
    if (source_ssl_cert.empty()) {
      throw std::invalid_argument(
          "client_ssl_cert must be set, if client_ssl_mode is '"s +
          ssl_mode_to_string(source_ssl_mode) + "'.");
    }
    if (source_ssl_key.empty()) {
      throw std::invalid_argument(
          "client_ssl_key must be set, if client_ssl_mode is '"s +
          ssl_mode_to_string(source_ssl_mode) + "'.");
    }
  }

  if (source_ssl_mode == SslMode::kPassthrough &&
      dest_ssl_mode != SslMode::kAsClient) {
    throw std::invalid_argument(
        "If client_ssl_mode is PASSTHROUGH, server_ssl_mode must be "
        "AS_CLIENT.");
  }

  if (dest_ssl_verify != SslVerify::kDisabled) {
    if (dest_ssl_ca_dir.empty() && dest_ssl_ca_file.empty()) {
      throw std::invalid_argument(
          "server_ssl_ca or server_ssl_capath must be set, if "
          "server_ssl_verify is '"s +
          ssl_verify_to_string(dest_ssl_verify) + "'.");
    }
  }

  if (source_ssl_mode == SslMode::kPassthrough) {
    if (!source_ssl_ca_file.empty()) {
      throw std::invalid_argument(
          "client_ssl_mode=PASSTHROUGH can not be combined with "
          "client_ssl_ca=" +
          source_ssl_ca_file);
    }
    if (!source_ssl_ca_dir.empty()) {
      throw std::invalid_argument(
          "client_ssl_mode=PASSTHROUGH can not be combined with "
          "client_ssl_capath=" +
          source_ssl_ca_dir);
    }
    if (!source_ssl_crl_file.empty()) {
      throw std::invalid_argument(
          "client_ssl_mode=PASSTHROUGH can not be combined with "
          "client_ssl_crl=" +
          source_ssl_crl_file);
    }
    if (!source_ssl_crl_dir.empty()) {
      throw std::invalid_argument(
          "client_ssl_mode=PASSTHROUGH can not be combined with "
          "client_ssl_crlpath=" +
          source_ssl_crl_dir);
    }
    if (!dest_ssl_key.empty()) {
      throw std::invalid_argument(
          "client_ssl_mode=PASSTHROUGH can not be combined with "
          "server_ssl_key=" +
          dest_ssl_key);
    }
    if (!dest_ssl_cert.empty()) {
      throw std::invalid_argument(
          "client_ssl_mode=PASSTHROUGH can not be combined with "
          "server_ssl_cert=" +
          dest_ssl_cert);
    }
    if (router_require_enforce != 0) {
      throw std::invalid_argument(
          "client_ssl_mode=PASSTHROUGH can not be combined with "
          "router_require_enforce=" +
          std::to_string(router_require_enforce));
    }
  } else if (source_ssl_mode == SslMode::kDisabled) {
    if (!source_ssl_ca_file.empty()) {
      throw std::invalid_argument(
          "client_ssl_mode=DISABLED can not be combined with "
          "client_ssl_ca=" +
          source_ssl_ca_file);
    }
    if (!source_ssl_ca_dir.empty()) {
      throw std::invalid_argument(
          "client_ssl_mode=DISABLED can not be combined with "
          "client_ssl_capath=" +
          source_ssl_ca_dir);
    }
    if (!source_ssl_crl_file.empty()) {
      throw std::invalid_argument(
          "client_ssl_mode=DISABLED can not be combined with "
          "client_ssl_crl=" +
          source_ssl_crl_file);
    }
    if (!source_ssl_crl_dir.empty()) {
      throw std::invalid_argument(
          "client_ssl_mode=DISABLED can not be combined with "
          "client_ssl_crlpath=" +
          source_ssl_crl_dir);
    }
  }

  if (dest_ssl_mode == SslMode::kDisabled ||
      (source_ssl_mode == SslMode::kDisabled &&
       dest_ssl_mode == SslMode::kAsClient)) {
    if (!dest_ssl_key.empty()) {
      throw std::invalid_argument(
          "server_ssl_mode=DISABLED can not be combined with "
          "server_ssl_key=" +
          dest_ssl_key);
    }
    if (!dest_ssl_cert.empty()) {
      throw std::invalid_argument(
          "server_ssl_mode=DISABLED can not be combined with "
          "server_ssl_cert=" +
          dest_ssl_cert);
    }
  }

  if (protocol == Protocol::Type::kXProtocol) {
    if (!source_ssl_ca_file.empty()) {
      throw std::invalid_argument(
          "protocol=x can not be combined with "
          "client_ssl_ca=" +
          source_ssl_ca_file);
    }
    if (!source_ssl_ca_dir.empty()) {
      throw std::invalid_argument(
          "protocol=x can not be combined with "
          "client_ssl_capath=" +
          source_ssl_ca_dir);
    }
    if (!source_ssl_crl_file.empty()) {
      throw std::invalid_argument(
          "protocol=x can not be combined with "
          "client_ssl_crl=" +
          source_ssl_crl_file);
    }
    if (!source_ssl_crl_dir.empty()) {
      throw std::invalid_argument(
          "protocol=x can not be combined with "
          "client_ssl_crlpath=" +
          source_ssl_crl_dir);
    }
    if (router_require_enforce != 0) {
      throw std::invalid_argument(
          "protocol=x can not be combined with "
          "router_require_enforce=" +
          std::to_string(router_require_enforce));
    }
  }
}

std::string RoutingPluginConfig::get_default(std::string_view option) const {
  namespace options = routing::options;

  static const std::map<std::string_view, std::string> defaults{
      {options::kBindAddress, std::string{routing::kDefaultBindAddress}},
      {options::kMaxConnections,
       std::to_string(routing::kDefaultMaxConnections)},
      {options::kConnectTimeout,
       std::to_string(routing::kDefaultDestinationConnectionTimeout.count())},
      {options::kMaxConnectErrors,
       std::to_string(routing::kDefaultMaxConnectErrors)},
      {options::kClientConnectTimeout,
       std::to_string(routing::kDefaultClientConnectTimeout.count())},
      {options::kNetBufferLength,
       std::to_string(routing::kDefaultNetBufferLength)},
      {options::kThreadStackSize,
       std::to_string(mysql_harness::kDefaultStackSizeInKiloBytes)},
      {options::kClientSslMode, std::string{routing::kDefaultClientSslMode}},
      {options::kServerSslMode, std::string{routing::kDefaultServerSslMode}},
      {options::kServerSslVerify,
       std::string{routing::kDefaultServerSslVerify}},
      {options::kConnectionSharing,
       routing::kDefaultConnectionSharing ? "1" : "0"},
      {options::kConnectionSharingDelay,
       std::to_string(routing::kDefaultConnectionSharingDelay.count() / 1000)},
      {options::kClientSslSessionCacheMode,
       routing::kDefaultSslSessionCacheMode ? "1" : "0"},
      {options::kClientSslSessionCacheSize,
       std::to_string(routing::kDefaultSslSessionCacheSize)},
      {options::kClientSslSessionCacheTimeout,
       std::to_string(routing::kDefaultSslSessionCacheTimeout.count())},
      {options::kServerSslSessionCacheMode,
       routing::kDefaultSslSessionCacheMode ? "1" : "0"},
      {options::kServerSslSessionCacheSize,
       std::to_string(routing::kDefaultSslSessionCacheSize)},
      {options::kServerSslSessionCacheTimeout,
       std::to_string(routing::kDefaultSslSessionCacheTimeout.count())},
      {options::kConnectRetryTimeout,
       std::to_string(routing::kDefaultConnectRetryTimeout.count())},
      {options::kWaitForMyWrites,
       std::to_string(routing::kDefaultWaitForMyWrites)},
      {options::kWaitForMyWritesTimeout,
       std::to_string(routing::kDefaultWaitForMyWritesTimeout.count())},
      {options::kRouterRequireEnforce, "0"},
  };

  const auto it = defaults.find(option);
  if (it == defaults.end()) return {};

  return it->second;
}

bool RoutingPluginConfig::is_required(std::string_view option) const {
  namespace options = routing::options;

  return option == options::kDestinations;
}

namespace {

class RoutingConfigExposer : public mysql_harness::SectionConfigExposer {
 public:
  RoutingConfigExposer(const bool initial,
                       const RoutingPluginConfig &plugin_config,
                       const mysql_harness::ConfigSection &default_section,
                       const std::string &endpoint_key)
      : mysql_harness::SectionConfigExposer(initial, default_section,
                                            {"endpoints", endpoint_key}),
        plugin_config_(plugin_config),
        endpoint_key_(endpoint_key) {}

  void expose() override {
    namespace options = routing::options;

    const auto section_type =
        routing::get_section_type_from_routing_name(endpoint_key_);

    expose_option(
        options::kProtocol, Protocol::to_string(plugin_config_.protocol),
        Protocol::to_string(routing::get_default_protocol(section_type)),
        false);

    expose_option(options::kDestinations, plugin_config_.destinations,
                  plugin_config_.destinations, false);

    expose_option(options::kBindPort, plugin_config_.bind_port,
                  routing::get_default_port(section_type), false);
    expose_option(options::kBindAddress, plugin_config_.bind_address.address(),
                  std::string(routing::kDefaultBindAddressBootstrap), true);
    expose_option("socket", plugin_config_.named_socket.str(),
                  std::string(routing::kDefaultNamedSocket), true);

    expose_option(options::kConnectTimeout, plugin_config_.connect_timeout,
                  routing::kDefaultDestinationConnectionTimeout.count(), true);
    expose_option(options::kClientConnectTimeout,
                  plugin_config_.client_connect_timeout,
                  routing::kDefaultClientConnectTimeout.count(), true);

    expose_option(options::kRoutingStrategy,
                  get_routing_strategy_name(plugin_config_.routing_strategy),
                  get_routing_strategy_name(
                      routing::get_default_routing_strategy(section_type)),
                  false);

    expose_option(options::kMaxConnections, plugin_config_.max_connections,
                  routing::kDefaultMaxConnections, true);
    expose_option(options::kMaxConnectErrors,
                  static_cast<int64_t>(plugin_config_.max_connect_errors),
                  static_cast<int64_t>(routing::kDefaultMaxConnectErrors),
                  true);
    expose_option(options::kNetBufferLength, plugin_config_.net_buffer_length,
                  routing::kDefaultNetBufferLength, true);
    expose_option(
        options::kThreadStackSize, plugin_config_.thread_stack_size,
        static_cast<int64_t>(mysql_harness::kDefaultStackSizeInKiloBytes),
        true);

    expose_option(options::kClientSslMode,
                  ssl_mode_to_string(plugin_config_.source_ssl_mode),
                  std::string{routing::kDefaultClientSslModeBootstrap}, true);
    expose_option(options::kClientSslCert, plugin_config_.source_ssl_cert,
                  std::monostate{}, true);
    expose_option(options::kClientSslKey, plugin_config_.source_ssl_key,
                  std::monostate{}, true);
    expose_option(options::kClientSslCipher, plugin_config_.source_ssl_cipher,
                  std::string{routing::kDefaultClientSslCipherBootstrap}, true);
    expose_option(options::kClientSslCurves, plugin_config_.source_ssl_curves,
                  std::string{routing::kDefaultClientSslCurvesBootstrap}, true);
    expose_option(
        options::kClientSslDhParams, plugin_config_.source_ssl_dh_params,
        std::string{routing::kDefaultClientSslDhParamsBootstrap}, true);

    expose_option(options::kServerSslMode,
                  ssl_mode_to_string(plugin_config_.dest_ssl_mode),
                  std::string{routing::kDefaultServerSslModeBootstrap}, true);
    expose_option(options::kServerSslVerify,
                  ssl_verify_to_string(plugin_config_.dest_ssl_verify),
                  std::string{routing::kDefaultServerSslVerify}, true);
    expose_option(options::kServerSslCipher, plugin_config_.dest_ssl_cipher,
                  std::string{routing::kDefaultServerSslCipherBootstrap}, true);
    expose_option(options::kServerSslCa, plugin_config_.dest_ssl_ca_file,
                  std::string{routing::kDefaultServerSslCaBootstrap}, true);
    expose_option(options::kServerSslCaPath, plugin_config_.dest_ssl_ca_dir,
                  std::string{routing::kDefaultServerSslCaPathBootstrap}, true);
    expose_option(options::kServerSslCrl, plugin_config_.dest_ssl_crl_file,
                  std::string{routing::kDefaultServerSslCrlFileBootstrap},
                  true);
    expose_option(options::kServerSslCrlPath, plugin_config_.dest_ssl_crl_dir,
                  std::string{routing::kDefaultServerSslCrlPathBootstrap},
                  true);
    expose_option(options::kServerSslCurves, plugin_config_.dest_ssl_curves,
                  std::string{routing::kDefaultServerSslCurvesBootstrap}, true);

    expose_option(options::kConnectionSharing,
                  static_cast<bool>(plugin_config_.connection_sharing),
                  routing::get_default_connection_sharing(section_type), true);
    expose_option(options::kConnectionSharingDelay,
                  std::chrono::duration_cast<std::chrono::duration<double>>(
                      plugin_config_.connection_sharing_delay)
                      .count(),
                  std::chrono::duration_cast<std::chrono::duration<double>>(
                      routing::kDefaultConnectionSharingDelay)
                      .count(),
                  true);
    expose_option(options::kRouterRequireEnforce,
                  plugin_config_.router_require_enforce,
                  get_default_router_require_enforce(section_type), false);

    // expose access_mode only if it is not empty
    const auto access_mode =
        routing::get_access_mode_name(plugin_config_.access_mode);
    const auto default_access_mode = routing::get_access_mode_name(
        routing::get_default_access_mode(section_type));
    expose_option(
        options::kAccessMode,
        access_mode.empty() ? OptionValue(std::monostate{}) : access_mode,
        default_access_mode.empty() ? OptionValue(std::monostate{})
                                    : default_access_mode,
        false);

    expose_option(routing::options::kWaitForMyWrites,
                  plugin_config_.wait_for_my_writes,
                  routing::kDefaultWaitForMyWrites, true);
    expose_option(routing::options::kWaitForMyWritesTimeout,
                  plugin_config_.wait_for_my_writes_timeout.count(),
                  routing::kDefaultWaitForMyWritesTimeout.count(), true);
  }

 private:
  const RoutingPluginConfig &plugin_config_;
  const std::string endpoint_key_;
};

}  // namespace

void RoutingPluginConfig::expose_configuration(
    const std::string &key, const mysql_harness::ConfigSection &default_section,
    const bool initial) const {
  RoutingConfigExposer(initial, *this, default_section, key).expose();
}
