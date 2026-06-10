/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "destinations_option_parser.h"

#include <vector>

#include "hostname_validator.h"  // is_valid_hostname
#include "mysql/harness/destination.h"
#include "mysql/harness/string_utils.h"    // trim
#include "mysql/harness/utility/string.h"  // join

stdx::expected<
    std::variant<mysqlrouter::URI, std::vector<mysql_harness::Destination>>,
    std::string>
DestinationsOptionParser::parse(const std::string &value) {
  // if it starts with metadata-cache:// ... only allow one URL.

  if (value.starts_with("metadata-cache://")) {
    auto uri = mysqlrouter::URI(value);

    if (uri.scheme == "metadata-cache") {
      return uri;
    }

    return stdx::unexpected("invalid URI scheme '" + uri.scheme + "' for URI " +
                            value);
  }

  const bool is_win32 =
#ifdef _WIN32
      true;
#else
      false;
#endif

  // ... otherwise allow a mix of:
  //
  // - hostname[:port]
  // - ipv4[:port]
  // - ipv6[:port]
  // - local://...
  //
  // ... all seperated by comma.

  std::vector<mysql_harness::Destination> dests;
  for (auto part : mysql_harness::split_string(value, ',')) {
    mysql_harness::trim(part);
    if (part.empty()) {
      return stdx::unexpected("empty address found in destination list (was '" +
                              value + "')");
    }

    try {
      auto uri = mysqlrouter::URI(part, false);

      if (uri.scheme != "local" || is_win32) {
        // the server doesn't support unix-domain sockets on windows.
        return stdx::unexpected("invalid URI scheme '" + uri.scheme +
                                "' for URI " + value);
      }

      if (!uri.host.empty()) {
        // "local" URIs require an empty 'authority' field
        return stdx::unexpected(
            "local:-URI with a non-empty "
            "//hostname/ part: '" +
            uri.host + "' in " + value +
            ". Ensure that local: is followed by either 1 or 3 slashes.");
      }

      if (uri.path.empty()) {
        // "local" URIs require an non-empty 'path' field
        return stdx::unexpected("local:-URI with an empty /path part in " +
                                part + ".");
      }

      if (!uri.query.empty()) {
        // "local" URIs require an empty 'query' field
        return stdx::unexpected("local:-URI with a non-empty ?query part in " +
                                part + ". Ensure the URI contains no '?'.");
      }

      if (!uri.username.empty()) {
        // "local" URIs require an empty 'username' field
        return stdx::unexpected(
            "local:-URI with a non-empty username@ part in " + part +
            ". Ensure the URI contains no '@'.");
      }

      if (!uri.password.empty()) {
        // "local" URIs require an empty 'password' field
        return stdx::unexpected(
            "local:-URI with a non-empty :password@ part in " + part +
            ". Ensure the URI contains no '@'.");
      }

      // success.
      dests.emplace_back(mysql_harness::LocalDestination(
          "/" + mysql_harness::join(uri.path, "/")));
    } catch (const mysqlrouter::URIError &) {
      // either (host|ipv4|[ipv6])[:port]

      auto make_res = mysql_harness::make_tcp_destination(part);

      if (!make_res) {
        return stdx::unexpected("invalid destination address '" + part + "'");
      }

      const auto &hostname = make_res->hostname();

      if (!mysql_harness::is_valid_ip_address(hostname) &&
          !mysql_harness::is_valid_hostname(hostname)) {
        return stdx::unexpected("invalid destination address '" + hostname +
                                "'");
      }

      dests.emplace_back(*make_res);
    }
  }

  return dests;
}
