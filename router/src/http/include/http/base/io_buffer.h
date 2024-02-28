/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_IO_BUFFER_H_
#define ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_IO_BUFFER_H_

#include <my_inttypes.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "mysqlrouter/http_common_export.h"

namespace http {
namespace base {

class HTTP_COMMON_EXPORT IOBuffer {
 public:
  using CIterator = std::string::const_iterator;
  IOBuffer() = default;
  IOBuffer(const char *data, size_t length) : content_{data, length} {}

  IOBuffer(std::string value) : content_{std::move(value)} {}

  virtual ~IOBuffer();

  virtual void clear() { content_.clear(); }
  virtual size_t length() const { return content_.length(); }
  virtual CIterator begin() const { return content_.begin(); }
  virtual CIterator end() const { return content_.end(); }

  virtual std::vector<uint8_t> pop_front(size_t size) {
    std::vector<uint8_t> result;

    if (size > content_.length()) size = content_.length();

    result.assign(content_.begin(), content_.begin() + size);

    content_.erase(0, size);

    return result;
  }

  virtual std::vector<uint8_t> copy(size_t size) const {
    std::vector<uint8_t> result;

    if (size > content_.length()) size = content_.length();

    result.assign(content_.begin(), content_.begin() + size);

    return result;
  }

  virtual void add(const char *data, size_t length) {
    content_.append(data, length);
  }

  virtual const std::string &get() const { return content_; }
  virtual std::string &get() { return content_; }

 private:
  std::string content_;
};

}  // namespace base
}  // namespace http

#endif  // ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_IO_BUFFER_H_
