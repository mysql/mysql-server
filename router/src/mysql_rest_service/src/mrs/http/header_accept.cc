/*
 Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "mrs/http/header_accept.h"

#include "helper/string/trim.h"

#include "mysql/harness/string_utils.h"

namespace mrs {
namespace http {

static bool parse_mime_type(std::string mime_type, MimeClass *out_class,
                            MimeClass *out_subclass,
                            const bool is_accept = true) {
  auto b = mime_type.find(";");
  if (std::string::npos != b) mime_type = mime_type.substr(0, b);

  helper::trim(&mime_type);

  auto p = mime_type.find("/");
  if (std::string::npos == p) {
    out_class->emplace(mime_type);

    if (is_accept)
      out_subclass->reset();
    else
      out_subclass->emplace("");
    return false;
  }

  auto p1 = mime_type.substr(0, p);
  auto p2 = mime_type.substr(p + 1);

  if (is_accept && (p1.empty() || p1 == "*"))
    out_class->reset();
  else
    out_class->emplace(p1);

  if (is_accept && (p2.empty() || p2 == "*"))
    out_subclass->reset();
  else
    out_subclass->emplace(p2);

  return true;
}
Accepts::Accepts(const std::string &mime_type) {
  parse_mime_type(mime_type, &mime_class_, &mime_subclass_, true);
}

bool Accepts::is_acceptable(const std::string &other_mime_type) {
  MimeClass other_mime_class, other_mime_subclass;

  if (!parse_mime_type(other_mime_type, &other_mime_class,
                       &other_mime_subclass))
    return false;

  if (mime_class_.has_value()) {
    if (mime_class_.value() != other_mime_class.value()) return false;
  }

  if (mime_subclass_.has_value()) {
    if (mime_subclass_.value() != other_mime_subclass.value()) return false;
  }

  return true;
}

HeaderAccept::HeaderAccept() {}

HeaderAccept::HeaderAccept(const char *header_accept) {
  if (header_accept) {
    auto accepts = mysql_harness::split_string(header_accept, ',', false);

    for (auto &a : accepts) {
      accepts_.emplace_back(a);
    }
  }
}

bool HeaderAccept::is_acceptable(const helper::MediaType &mime_type) {
  return is_acceptable(helper::get_mime_name(mime_type));
}

bool HeaderAccept::is_acceptable(const std::string &mime_type) {
  if (accepts_.empty()) return true;

  for (auto &a : accepts_) {
    if (a.is_acceptable(mime_type)) return true;
  }

  return false;
}

std::optional<helper::MediaType> HeaderAccept::is_acceptable(
    const std::vector<helper::MediaType> &mime_types) {
  for (const auto &m : mime_types) {
    if (is_acceptable(m)) return m;
  }

  return {};
}

}  // namespace http
}  // namespace mrs
