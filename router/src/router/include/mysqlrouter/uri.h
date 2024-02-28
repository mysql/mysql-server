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

#ifndef URI_ROUTING_INCLUDED
#define URI_ROUTING_INCLUDED

#include "mysqlrouter/router_export.h"

#include <cstdint>
#include <exception>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace mysqlrouter {

using URIAuthority = std::tuple<std::string, uint16_t, std::string,
                                std::string>;  // host, port, username, password
using URIPath = std::vector<std::string>;
using URIQuery = std::map<std::string, std::string>;

/** @class URIError
 * @brief Exception when URI was not valid
 *
 */
class URIError : public std::runtime_error {
 public:
  URIError(const char *msg, const std::string &uri, size_t position);
  explicit URIError(const std::string &what_arg)
      : std::runtime_error(what_arg) {}
};

/** @class URI
 * @brief Parse and create URIs according to RFC3986
 *
 * This class will parse and make the elements of the URI
 * available as members.
 *
 * Links:
 * * (RFC 3986)[https://tools.ietf.org/html/rfc3986)
 *
 */
class ROUTER_LIB_EXPORT URI {
 public:
  /** @brief Delimiter used in the Query part */
  static const char query_delimiter = '&';

  /** @brief Default constructor
   *
   * Rootless URIs like "mailto:user@example.com" may be forbidden to make sure
   * that simple "host:addr" doesn't get parsed as (scheme='host', path='addr')
   *
   * @param uri URI string to decode
   * @param allow_path_rootless if parsing rootless URIs is allowed
   * @param allow_schemeless define if schema is mandatory
   * @param path_keep_last_slash parsing the URL keeps last slash
   * @param query_single_parameter_when_cant_parse This parameter allows to
   * handles query parameter that follows the RFC but is not accepted by default
   * implementation of URL
   */
  URI(const std::string &uri, bool allow_path_rootless = true,
      bool allow_schemeless = false, bool path_keep_last_slash = false,
      bool query_single_parameter_when_cant_parse = false)
      : scheme(),
        host(),
        port(0),
        username(),
        password(),
        path(),
        query(),
        fragment(),
        uri_(uri),
        allow_path_rootless_(allow_path_rootless),
        allow_schemeless_{allow_schemeless},
        path_keep_last_slash_{path_keep_last_slash},
        query_single_parameter_when_cant_parse_{
            query_single_parameter_when_cant_parse} {
    if (!uri.empty()) {
      init_from_uri(uri);
    }
  }

  bool operator==(const URI &u2) const;
  bool operator!=(const URI &u2) const;

  /** return string representation of the URI */
  std::string str() const;

  /** @brief overload */
  URI() : URI("") {}

  /** @brief Sets URI using the given URI string
   *
   * @param uri URI as string
   */
  void set_uri(const std::string &uri) { init_from_uri(uri); }

  /** @brief Path part of the URI as string */
  std::string get_path_as_string(bool needs_first_slash = true) const;

  /** @brief Set the path part of the URI as string
   *
   * @param p path string to decode and store in URI object
   */
  void set_path_from_string(const std::string &p);

  /** @brief Get the URIs path part */
  std::string get_query_as_string() const;

  /** @brief Set the URI query part by reparsing query string
   *
   * @param q query string to decode and store in URI object
   */
  void set_query_from_string(const std::string &q);

  /** @brief Scheme of the URI */
  std::string scheme;
  /** @brief Host part found in the Authority */
  std::string host;
  /** @brief Port found in the Authority */
  uint16_t port;  // 0 means use default (no dynamically allocation needed here)
  /** @brief Username part found in the Authority */
  std::string username;
  /** @brief Password part found in the Authority */
  std::string password;
  /** @brief Path part of the URI */
  URIPath path;
  /** @brief Query part of the URI */
  URIQuery query;
  /** @brief Fragment part of the URI */
  std::string fragment;

 private:
  friend std::ostream &operator<<(std::ostream &strm, const URI &uri);
  /** @brief Sets information using the given URI
   *
   * Takes a and parsers out all URI elements.
   *
   * Throws URIError on errors.
   *
   * @param uri URI to use
   */
  void init_from_uri(const std::string &uri);

  /** @brief Copy of the original given URI */
  std::string uri_;

  /** @brief all URIs like mail:foo@example.org which don't have a authority */
  bool allow_path_rootless_;

  /** @brief all URIs like foo@example.org which don't have a scheme */
  bool allow_schemeless_;
  bool path_keep_last_slash_;
  bool query_single_parameter_when_cant_parse_;
  bool query_is_signle_parameter_{false};
};

std::ostream &operator<<(std::ostream &strm, const URI &uri);

class ROUTER_LIB_EXPORT URIParser {
 public:
  static std::string decode(const std::string &uri, bool decode_plus);
  static URI parse(const std::string &uri, bool allow_path_rootless = true,
                   bool allow_schemeless = false,
                   bool path_keep_last_slash = false,
                   bool query_single_parameter_when_cant_parse = false);
  static URI parse_shorthand_uri(const std::string &uri,
                                 bool allow_path_rootless = true,
                                 const std::string &default_scheme = "mysql");
};

}  // namespace mysqlrouter

#endif  // URI_ROUTING_INCLUDED
