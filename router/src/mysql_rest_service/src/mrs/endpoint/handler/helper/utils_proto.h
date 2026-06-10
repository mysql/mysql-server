/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_HELPER_UTILS_PROTO_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_HELPER_UTILS_PROTO_H_

#include <set>
#include <string>

#include "http/base/uri.h"
#include "mrs/endpoint/handler/helper/protocol.h"
#include "mrs/endpoint/handler/helper/utilities.h"
#include "mrs/interface/endpoint_configuration.h"

namespace mrs {
namespace endpoint {
namespace handler {

inline UsedProtocol get_properly_configured_used_protocol(
    const std::set<std::string> &protocols,
    const mrs::interface::EndpointConfiguration *configuration) {
  // New mrs-schema has only one protocol assigned to the service
  // and it should work as "enforce".
  if (1 == protocols.size())
    return protocols.count(k_https) ? k_usedProtocolHttps : k_usedProtocolHttp;

  if (0 == protocols.count(configuration->does_server_support_https() ? k_https
                                                                      : k_http))
    return k_usedProtocolNone;

  if (configuration->does_server_support_https()) return k_usedProtocolHttps;

  return k_usedProtocolHttp;
}

inline Protocol get_properly_configured_protocol(
    const std::set<std::string> &protocols,
    const mrs::interface::EndpointConfiguration *configuration) {
  const auto used =
      get_properly_configured_used_protocol(protocols, configuration);

  switch (used) {
    case k_usedProtocolHttp:
      return k_protocolHttp;

    case k_usedProtocolHttps:
      return k_protocolHttps;

    case k_usedProtocolNone:
      break;
  }

  // For now just return what we support
  // in future lets check if there was X-Forward-proto set.
  return configuration->does_server_support_https() ? k_protocolHttps
                                                    : k_protocolHttp;
}

inline void add_protocol_to_host(UsedProtocol protocol,
                                 ::http::base::Uri *uri) {
  switch (protocol) {
    case k_usedProtocolHttp:
      uri->set_scheme("http");
      return;
    case k_usedProtocolHttps:
      uri->set_scheme("https");
      return;
    default:
      return;
  }
}

template <typename EndpointPtr>
Protocol get_protocol(EndpointPtr endpoint) {
  auto ep = lock(endpoint);
  auto endpoint_protocols = get_endpoint_protocol(ep);
  auto configuration = ep->get_configuration();
  return handler::get_properly_configured_protocol(endpoint_protocols,
                                                   configuration.get());
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_HELPER_UTILS_PROTO_H_ \
        */
