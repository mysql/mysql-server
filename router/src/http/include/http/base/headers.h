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

#ifndef ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_HEADERS_H_
#define ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_HEADERS_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mysqlrouter/http_common_export.h"

namespace http {
namespace base {

/**
 * headers of a HTTP response/request.
 */
class HTTP_COMMON_EXPORT Headers {
 public:
  Headers();
  Headers(Headers &&h);
  virtual ~Headers();

  // The default implementation was done at std::map,
  // still it doesn't preserve the fields order.
  using Map = std::vector<std::pair<std::string, std::string>>;
  using Iterator = Map::iterator;
  using CIterator = Map::const_iterator;

  virtual void add(const std::string_view &key, std::string &&value);

  virtual const std::string *find(const std::string_view &) const;
  virtual const char *find_cstr(const char *) const;

  virtual Iterator begin();
  virtual Iterator end();

  virtual CIterator begin() const;
  virtual CIterator end() const;

  virtual uint32_t size() const;

  virtual void clear();

 private:
  void remove(const std::string_view &key);
  mutable Map map_;
};

HTTP_COMMON_EXPORT bool compare_case_insensitive(const std::string &l,
                                                 const std::string_view &r);

}  // namespace base
}  // namespace http

#endif  // ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_HEADERS_H_
