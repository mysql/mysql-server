/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/include/plugin_variables/recovery_endpoints.h"
#include <include/mysql/group_replication_priv.h>
#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>
#include "my_dbug.h"
#include "my_sys.h"
#include "plugin/group_replication/include/plugin.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>

Recovery_endpoints::Recovery_endpoints()
    : m_mysqld_port(0), m_mysqld_admin_port(0), m_endpoints(), m_remote(true) {}

Recovery_endpoints::~Recovery_endpoints() = default;

std::vector<std::pair<std::string, uint>> Recovery_endpoints::get_endpoints() {
  DBUG_TRACE;

  return m_endpoints;
}

void Recovery_endpoints::set_port_settings(uint mysqld_port, uint admin_port) {
  DBUG_TRACE;

  m_remote = false;

  m_mysqld_port = mysqld_port;
  m_mysqld_admin_port = admin_port;
}

#ifndef _WIN32
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

int Recovery_endpoints::local_interfaces_ips(std::set<std::string> &local_ips) {
  DBUG_TRACE;

  struct ifaddrs *myaddrs, *ifa;
  void *in_addr = nullptr;
  char buf[INET6_ADDRSTRLEN];

  // collect all IPs used on local computer
  if (getifaddrs(&myaddrs) != 0) {
    return 1; /* purecov: inspected */
  }

  for (ifa = myaddrs; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr) continue;
    if (!(ifa->ifa_flags & IFF_UP)) continue;

    switch (ifa->ifa_addr->sa_family) {
      case AF_INET: {
        struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;
        in_addr = &s4->sin_addr;
        break;
      }

      case AF_INET6: {
        struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ifa->ifa_addr;
        in_addr = &s6->sin6_addr;
        break;
      }

      default:
        continue;
    }

    if (inet_ntop(ifa->ifa_addr->sa_family, in_addr, buf, INET6_ADDRSTRLEN)) {
      local_ips.insert(std::string(buf));
    } else {
      return 1; /* purecov: inspected */
    }
  }

  freeifaddrs(myaddrs);

  return 0;
}
#endif

#ifdef _WIN32

#include <IPTypes.h>
#include <iphlpapi.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define WORKING_BUFFER_SIZE 1024 * 1024

int Recovery_endpoints::local_interfaces_ips(std::set<std::string> &local_ips) {
  DBUG_TRACE;

  ULONG flags = GAA_FLAG_INCLUDE_PREFIX, family = AF_UNSPEC, out_buflen = 0;
  PIP_ADAPTER_ADDRESSES addresses;
  PIP_ADAPTER_ADDRESSES curr_addresses;

  out_buflen = WORKING_BUFFER_SIZE;

  addresses = (IP_ADAPTER_ADDRESSES *)malloc(out_buflen);
  if (addresses == nullptr) {
    return 1;
  }

  GetAdaptersAddresses(family, flags, nullptr, addresses, &out_buflen);

  curr_addresses = addresses;

  while (curr_addresses) {
    PIP_ADAPTER_UNICAST_ADDRESS_LH curr_unicast_address;

    curr_unicast_address = curr_addresses->FirstUnicastAddress;
    while (curr_unicast_address) {
      void *in_addr = nullptr;
      switch (curr_unicast_address->Address.lpSockaddr->sa_family) {
        case AF_INET: {
          sockaddr_in *s4 =
              (sockaddr_in *)(curr_unicast_address->Address.lpSockaddr);
          in_addr = &s4->sin_addr;
          break;
        }
        case AF_INET6: {
          sockaddr_in6 *s6 =
              (sockaddr_in6 *)(curr_unicast_address->Address.lpSockaddr);
          in_addr = &s6->sin6_addr;
          break;
        }
        default:
          continue;
      }
      char a[INET6_ADDRSTRLEN] = {};
      if (inet_ntop(curr_unicast_address->Address.lpSockaddr->sa_family,
                    in_addr, a, sizeof(a))) {
        local_ips.insert(std::string(a));
      } else {
        return 1;
      }

      curr_unicast_address = curr_unicast_address->Next;
    }

    curr_addresses = curr_addresses->Next;
  }

  return 0;
}

#endif

int Recovery_endpoints::hostname_check_and_log(std::string host,
                                               std::set<std::string> host_ips) {
  DBUG_TRACE;

  struct addrinfo *result, *res;
  bool hostname_local = false;
  int error;

  /* resolve the domain name into a list of addresses */
  error = getaddrinfo(host.c_str(), nullptr, nullptr, &result);
  if (error != 0) {
    return 1;
  }

  /* loop over all returned results and do inverse lookup */
  for (res = result; res != nullptr && !hostname_local; res = res->ai_next) {
    char hostname[NI_MAXHOST];
    error = getnameinfo(res->ai_addr, res->ai_addrlen, hostname, NI_MAXHOST,
                        nullptr, 0, 0);
    if (error != 0) {
      continue; /* purecov: inspected */
    }
    char addrstr[INET6_ADDRSTRLEN];
    void *ptr = nullptr;

    switch (res->ai_family) {
      case AF_INET:
        ptr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        break;
      case AF_INET6:
        ptr = &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
        break;
    }
    inet_ntop(res->ai_family, ptr, addrstr, INET6_ADDRSTRLEN);

    // check if addresses of hostname provided matches with hostname IPs
    // provided.
    if (*hostname != '\0' && host_ips.find(addrstr) != host_ips.end()) {
      hostname_local = true;
    }
  }

  freeaddrinfo(result);

  if (!m_remote && !hostname_local) {
    error = 1;
  }

  return error;
}

std::pair<Recovery_endpoints::enum_status, std::string>
Recovery_endpoints::check(const char *endpoints_arg) {
  DBUG_TRACE;

  std::string err_string("");
  Recovery_endpoints::enum_status check_res =
      Recovery_endpoints::enum_status::OK;
  const char *endpoints_ptr = endpoints_arg;

  // DEFAULT is valid value
  if (strcmp(endpoints_ptr, "DEFAULT") == 0) {
    return std::make_pair(Recovery_endpoints::enum_status::OK, err_string);
  }

  std::set<std::string> host_ips;

  if (local_interfaces_ips(host_ips)) {
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_RECOVERY_ENDPOINT_INTERFACES_IPS); /* purecov: inspected */
    return std::make_pair(Recovery_endpoints::enum_status::ERROR,
                          err_string); /* purecov: inspected */
  }

  if (strlen(endpoints_arg) == 0) {
    check_res = Recovery_endpoints::enum_status::BADFORMAT;
  }

  bool last = false;
  std::string endpoint;

  // if we have a list of endpoints that we have to check
  while (check_res == Recovery_endpoints::enum_status::OK &&
         strlen(endpoints_ptr) > 0) {
    const char *comma_pos = nullptr;

    // endopint terminate on ',' or is full string
    if ((comma_pos = strchr(endpoints_ptr, ',')) == nullptr) {
      comma_pos = endpoints_ptr + strlen(endpoints_ptr);
      last = true;
    }

    endpoint.assign(endpoints_ptr, comma_pos - endpoints_ptr);

    uint port;
    std::string host;
    std::string port_str;
    struct sockaddr_in sa;
    auto pos = endpoint.find_last_of(':');
    if (pos != std::string::npos) {
      host = endpoint.substr(0, pos);
      port_str = endpoint.substr(pos + 1);
      port = std::strtoul(port_str.c_str(), nullptr, 10);

      if (port_str.empty() ||
          !std::all_of(port_str.begin(), port_str.end(), ::isdigit)) {
        check_res = Recovery_endpoints::enum_status::BADFORMAT;
        break;
      } else {
        if (port < 1 || port > 65535) {
          check_res = Recovery_endpoints::enum_status::INVALID;
          break;
        }
      }

    } else {
      check_res = Recovery_endpoints::enum_status::BADFORMAT;
      break;
    }

    if (host.find('/') != std::string::npos) {
      // namespace aren't allowed
      check_res = Recovery_endpoints::enum_status::BADFORMAT;
    } else if (host.find('[') != std::string::npos &&
               host.find(']') != std::string::npos) {
      auto ipv6_begin = host.find('[');
      auto ipv6_end = host.find(']');
      host = host.substr(ipv6_begin + 1, ipv6_end - (ipv6_begin + 1));
      struct sockaddr_in sa;
      if (inet_pton(AF_INET6, host.c_str(), &sa) == 1) {
        if (!m_remote && host_ips.find(host.c_str()) == host_ips.end()) {
          check_res = Recovery_endpoints::enum_status::INVALID;
        }
      } else {
        check_res = Recovery_endpoints::enum_status::INVALID;
      }
    } else if (inet_pton(AF_INET, host.c_str(), &sa) ==
               1) {  // check host is an IPv4
      if (!m_remote && host_ips.find(host.c_str()) == host_ips.end()) {
        check_res = Recovery_endpoints::enum_status::INVALID;
      }
    } else {
      if (hostname_check_and_log(host, host_ips)) {
        check_res = Recovery_endpoints::enum_status::INVALID;
      }
    }

    if (!m_remote) {
      if (port != m_mysqld_port && port != m_mysqld_admin_port)
        check_res = Recovery_endpoints::enum_status::INVALID;
    }

    // remove character ',' from endpoints_ptr
    if (!last) comma_pos += 1;
    endpoints_ptr += comma_pos - endpoints_ptr;

    if (check_res == Recovery_endpoints::enum_status::OK)
      m_endpoints.push_back(std::pair<std::string, uint>{host, port});
  }

  switch (check_res) {
    case Recovery_endpoints::enum_status::BADFORMAT:
      err_string.assign(endpoints_arg);
      break;
    case Recovery_endpoints::enum_status::INVALID:
      err_string = endpoint;
      break;
    default:
      break;
  }

  if (check_res != Recovery_endpoints::enum_status::OK) m_endpoints.clear();

  return std::make_pair(check_res, err_string);
}

Advertised_recovery_endpoints::Advertised_recovery_endpoints() = default;

Advertised_recovery_endpoints::~Advertised_recovery_endpoints() = default;

bool Advertised_recovery_endpoints::check(const char *endpoints,
                                          enum_log_context where) {
  DBUG_TRACE;

  Recovery_endpoints::enum_status error =
      Recovery_endpoints::enum_status::ERROR;
  std::string err_string;
  char *hostname = nullptr;
  char *uuid = nullptr;
  uint port = 0U;
  uint server_version = 0U;
  uint admin_port = 0U;

  get_server_parameters(&hostname, &port, &uuid, &server_version, &admin_port);

  set_port_settings(port, admin_port);

  std::tie(error, err_string) = Recovery_endpoints::check(endpoints);

  if (error == Recovery_endpoints::enum_status::INVALID ||
      error == Recovery_endpoints::enum_status::BADFORMAT) {
    std::stringstream ss;
    switch (where) {
      case enum_log_context::ON_SET:
        if (error == Recovery_endpoints::enum_status::INVALID)
          ss << "Invalid value on recovery endpoint '" << err_string << "'.";
        if (error == Recovery_endpoints::enum_status::BADFORMAT)
          ss << "Please, provide a valid, comma separated, list of endpoints "
                "(IP:port).";
        mysql_error_service_emit_printf(
            mysql_runtime_error_service,
            ER_WRONG_VALUE_FOR_VAR_PLUS_ACTIONABLE_PART, 0,
            "group_replication_advertise_recovery_endpoints", endpoints,
            ss.str().c_str());
        break;
      case enum_log_context::ON_BOOT:
        if (error == Recovery_endpoints::enum_status::INVALID)
          LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_RECOVERY_ENDPOINT_INVALID,
                       err_string.c_str());
        if (error == Recovery_endpoints::enum_status::BADFORMAT)
          LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_RECOVERY_ENDPOINT_FORMAT,
                       err_string.c_str());
        break;
      case enum_log_context::ON_START:
        if (error == Recovery_endpoints::enum_status::INVALID) {
          mysql_error_service_emit_printf(
              mysql_runtime_error_service,
              ER_DA_GRP_RPL_RECOVERY_ENDPOINT_INVALID, 0, err_string.c_str());
        }
        if (error == Recovery_endpoints::enum_status::BADFORMAT) {
          mysql_error_service_emit_printf(
              mysql_runtime_error_service,
              ER_DA_GRP_RPL_RECOVERY_ENDPOINT_FORMAT, 0, err_string.c_str());
        }
    }
  }

  return error != Recovery_endpoints::enum_status::OK;
}

Donor_recovery_endpoints::Donor_recovery_endpoints() = default;

Donor_recovery_endpoints::~Donor_recovery_endpoints() = default;

std::vector<std::pair<std::string, uint>>
Donor_recovery_endpoints::get_endpoints(Group_member_info *donor) {
  DBUG_TRACE;

  std::string err_string;
  Recovery_endpoints::enum_status error =
      Recovery_endpoints::enum_status::ERROR;
  std::vector<std::pair<std::string, uint>> endpoints;

  if (strcmp(donor->get_recovery_endpoints().c_str(), "DEFAULT") == 0) {
    error = Recovery_endpoints::enum_status::OK;
    endpoints.push_back(
        std::pair<std::string, uint>{donor->get_hostname(), donor->get_port()});
  } else {
    std::tie(error, err_string) =
        check(donor->get_recovery_endpoints().c_str());
    if (error == Recovery_endpoints::enum_status::OK)
      endpoints = Recovery_endpoints::get_endpoints();
  }

#ifndef NDEBUG
  DBUG_EXECUTE_IF("gr_recovery_endpoints_invalid_donor", {
    error = Recovery_endpoints::enum_status::INVALID;
    endpoints.clear();
  });
#endif /* NDEBUG */

  if (error == Recovery_endpoints::enum_status::BADFORMAT ||
      error == Recovery_endpoints::enum_status::INVALID)
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_RECOVERY_ENDPOINT_INVALID_DONOR_ENDPOINT,
                 donor->get_recovery_endpoints().c_str());

  return endpoints;
}
