/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_URI_H_
#define ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_URI_H_

#include <string>

#include "mysqlrouter/uri.h"

#include "mysqlrouter/http_common_export.h"

namespace http {
namespace base {

class HTTP_COMMON_EXPORT Uri {
 public:
  Uri();
  Uri(const std::string &uri);  // NOLINT(runtime/explicit)
  Uri(Uri &&uri);
  Uri(const Uri &uri);
  virtual ~Uri();

  /**
   * convert URI to string.
   */
  virtual std::string join() const;
  virtual std::string join_path() const;

  virtual std::string get_scheme() const;
  virtual void set_scheme(const std::string &scheme);

  virtual std::string get_userinfo() const;
  virtual void set_userinfo(const std::string &userinfo);

  virtual std::string get_host() const;
  virtual void set_host(const std::string &host);

  // Return -1 and uint16_t range
  virtual int32_t get_port() const;
  virtual void set_port(int32_t port);
  /**
   * get path part of the URI.
   */
  virtual std::string get_path() const;
  virtual void set_path(const std::string &path);

  virtual std::string get_fragment() const;
  virtual void set_fragment(const std::string &fragment);

  virtual std::string get_query() const;
  virtual bool set_query(const std::string &query);

  /**
   * check if URI is valid.
   */
  operator bool() const;

  Uri &operator=(Uri &&other);
  Uri &operator=(const Uri &other);

 private:
  mysqlrouter::URI uri_impl_;
};

/**
 * canonicalize a URI path.
 *
 * input  | output
 * -------|-------
 * /      | /
 * /./    | /
 * //     | /
 * /../   | /
 * /a/../ | /
 * /../a/ | /a/
 * /../a  | /a
 */
HTTP_COMMON_EXPORT std::string http_uri_path_canonicalize(
    const std::string &uri_path);

}  // namespace base
}  // namespace http

#endif  // ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_URI_H_
