/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_VARIANT_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_VARIANT_H_

#include <chrono>
#include <string>

namespace helper {

class VariantPointer {
 public:
  using seconds = std::chrono::seconds;
  enum Type { kTypeNone, kTypeString, kTypeInteger, kTypeSeconds };

 public:
  VariantPointer(std::string *output) : type_{kTypeString}, ostring_{output} {}
  VariantPointer(int *output) : type_{kTypeInteger}, ointeger_{output} {}
  VariantPointer(std::chrono::seconds *output)
      : type_{kTypeInteger}, oseconds_{output} {}

  void operator=(const std::string &v) { set(v); }
  VariantPointer &operator*() { return *this; }

  void set(const std::string &v) {
    switch (type_) {
      case kTypeNone:
        break;
      case kTypeString:
        *ostring_ = v;
        break;
      case kTypeInteger:
        *ointeger_ = atoi(v.c_str());
        break;
      case kTypeSeconds:
        *oseconds_ = seconds(atoi(v.c_str()));
        break;
    }
  }

  template <typename Callback>
  void dispatch_value(Callback &cb) {
    switch (type_) {
      case kTypeNone:
        cb();
        break;
      case kTypeString:
        cb(*ostring_);
        break;
      case kTypeInteger:
        cb(*ointeger_);
        break;
      case kTypeSeconds:
        cb(*oseconds_);
        break;
    }
  }

 private:
  Type type_{kTypeNone};
  union {
    seconds *oseconds_;
    std::string *ostring_;
    int *ointeger_;
  };
};

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_VARIANT_H_
