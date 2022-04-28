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

#include "plugin_config.h"

#include <algorithm>  // transform
#include <array>
#include <cinttypes>
#include <initializer_list>
#include <stdexcept>  // invalid_argument
#include <string>
#include <string_view>
#include <vector>

#include "context.h"
#include "hostname_validator.h"
#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"  // trim
#include "mysql_router_thread.h"         // kDefaultStackSizeInKiloByte
#include "mysqlrouter/routing.h"         // AccessMode
#include "mysqlrouter/routing_component.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"  // is_valid_socket_name
#include "ssl_mode.h"
#include "tcp_address.h"

using namespace std::string_view_literals;
IMPORT_LOG_FUNCTIONS()

const std::array<const char *, 29> routing_supported_options{
    "protocol",
    "destinations",
    "bind_port",
    "bind_address",
    "socket",
    "connect_timeout",
    "mode",
    "routing_strategy",
    "max_connect_errors",
    "max_connections",
    "client_connect_timeout",
    "net_buffer_length",
    "thread_stack_size",
    "client_ssl_mode",
    "client_ssl_cert",
    "client_ssl_key",
    "client_ssl_cipher",
    "client_ssl_curves",
    "client_ssl_dh_params",
    "server_ssl_mode",
    "server_ssl_verify",
    "disabled",
    "server_ssl_cipher",
    "server_ssl_ca",
    "server_ssl_capath",
    "server_ssl_crl",
    "server_ssl_crlpath",
    "server_ssl_curves",
    "unreachable_destination_refresh_interval"};

using StringOption = mysql_harness::StringOption;

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

class ModeOption {
 public:
  routing::AccessMode operator()(const std::optional<std::string> &value,
                                 const std::string &option_desc) {
    if (!value) return routing::AccessMode::kUndefined;

    if (value->empty()) {
      throw std::invalid_argument(option_desc + " needs a value");
    }

    std::string lc_value{value.value()};

    std::transform(lc_value.begin(), lc_value.end(), lc_value.begin(),
                   ::tolower);

    // if the mode is given it still needs to be valid
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
  RoutingStrategyOption(routing::AccessMode mode, bool is_metadata_cache)
      : mode_{mode}, is_metadata_cache_{is_metadata_cache} {}

  routing::RoutingStrategy operator()(const std::optional<std::string> &value,
                                      const std::string &option_desc) {
    if (!value) {
      // routing_strategy option is not given
      // this is fine as long as mode is set which means that we deal with an
      // old configuration which we still want to support

      if (mode_ == routing::AccessMode::kUndefined) {
        throw std::invalid_argument(option_desc + " is required");
      }

      /** @brief `mode` option read from configuration section */
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
  routing::AccessMode mode_;
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
    // convert name to upper-case to get case-insenstive comparision.
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
    // convert name to upper-case to get case-insenstive comparision.
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

/** @brief Constructor
 *
 * @param section from configuration file provided as ConfigSection
 */
RoutingPluginConfig::RoutingPluginConfig(
    const mysql_harness::ConfigSection *section)
    : BasePluginConfig{section},
      metadata_cache_(false),
      protocol(get_option_no_default(section, "protocol", ProtocolOption{})),
      destinations(get_option(section, "destinations",
                              DestinationsOption{metadata_cache_})),
      bind_port(get_option(section, "bind_port", BindPortOption{})),
      bind_address(get_option(section, "bind_address",
                              TCPAddressOption{false, bind_port})),
      named_socket(get_option(section, "socket", NamedSocketOption{})),
      connect_timeout(
          get_option(section, "connect_timeout", IntOption<uint16_t>{1})),
      mode(get_option_no_default(section, "mode", ModeOption{})),
      routing_strategy(
          get_option_no_default(section, "routing_strategy",
                                RoutingStrategyOption{mode, metadata_cache_})),
      max_connections(
          get_option(section, "max_connections", MaxConnectionsOption{})),
      max_connect_errors(get_option(section, "max_connect_errors",
                                    IntOption<uint32_t>{1, UINT32_MAX})),
      client_connect_timeout(get_option(section, "client_connect_timeout",
                                        IntOption<uint32_t>{2, 31536000})),
      net_buffer_length(get_option(section, "net_buffer_length",
                                   IntOption<uint32_t>{1024, 1048576})),
      thread_stack_size(get_option(section, "thread_stack_size",
                                   IntOption<uint32_t>{1, 65535})),
      source_ssl_mode{
          get_option(section, "client_ssl_mode",
                     SslModeOption{SslMode::kDisabled, SslMode::kPreferred,
                                   SslMode::kRequired, SslMode::kPassthrough,
                                   SslMode::kDefault})},
      source_ssl_cert{get_option(section, "client_ssl_cert", StringOption{})},
      source_ssl_key{get_option(section, "client_ssl_key", StringOption{})},
      source_ssl_cipher{
          get_option(section, "client_ssl_cipher", StringOption{})},
      source_ssl_curves{
          get_option(section, "client_ssl_curves", StringOption{})},
      source_ssl_dh_params{
          get_option(section, "client_ssl_dh_params", StringOption{})},
      dest_ssl_mode{
          get_option(section, "server_ssl_mode",
                     SslModeOption{SslMode::kDisabled, SslMode::kPreferred,
                                   SslMode::kRequired, SslMode::kAsClient})},
      dest_ssl_verify{
          get_option(section, "server_ssl_verify",
                     SslVerifyOption{SslVerify::kDisabled, SslVerify::kVerifyCa,
                                     SslVerify::kVerifyIdentity})},
      dest_ssl_cipher{get_option(section, "server_ssl_cipher", StringOption{})},
      dest_ssl_ca_file{get_option(section, "server_ssl_ca", StringOption{})},
      dest_ssl_ca_dir{get_option(section, "server_ssl_capath", StringOption{})},
      dest_ssl_crl_file{get_option(section, "server_ssl_crl", StringOption{})},
      dest_ssl_crl_dir{
          get_option(section, "server_ssl_crlpath", StringOption{})},
      dest_ssl_curves{get_option(section, "server_ssl_curves", StringOption{})},
      unreachable_destination_refresh_interval{
          get_option(section, "unreachable_destination_refresh_interval",
                     IntOption<uint32_t>{1, 65535})} {
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
}

std::string RoutingPluginConfig::get_default(const std::string &option) const {
  static const std::map<std::string, std::string> defaults{
      {"bind_address", std::string{routing::kDefaultBindAddress}},
      {"max_connections", std::to_string(routing::kDefaultMaxConnections)},
      {"connect_timeout",
       std::to_string(routing::kDefaultDestinationConnectionTimeout.count())},
      {"max_connect_errors", std::to_string(routing::kDefaultMaxConnectErrors)},
      {"client_connect_timeout",
       std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                          routing::kDefaultClientConnectTimeout)
                          .count())},
      {"net_buffer_length", std::to_string(routing::kDefaultNetBufferLength)},
      {"thread_stack_size",
       std::to_string(mysql_harness::kDefaultStackSizeInKiloBytes)},
      {"unreachable_destination_refresh_interval",
       std::to_string(
           routing::kDefaultUnreachableDestinationRefreshInterval.count())},
      {"client_ssl_mode", ""},
      {"server_ssl_mode", "as_client"},
      {"server_ssl_verify", "disabled"},

  };

  const auto it = defaults.find(option);
  if (it == defaults.end()) return {};

  return it->second;
}

bool RoutingPluginConfig::is_required(const std::string &option) const {
  return (option == "destinations"sv);
}
