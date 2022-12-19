/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_MODE_H_
#define ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_MODE_H_

#include <map>
#include <string>

enum BoostrapMode { k_all, k_bootstrap, k_configure_mrs };

class BootstrapMode {
 public:
  using Enum = BoostrapMode;
  using Map = std::map<std::string, Enum>;

  BootstrapMode() {}
  BootstrapMode(Enum e) : enum_value_{e}, valid_{true} {}

  void set(const std::string &v) { valid_ = convert(v, &enum_value_); }

  bool is_valid() const { return valid_; }

  Enum get() {
    if (!valid_)
      throw std::runtime_error("bootstrap mode has an invalid value");
    return enum_value_;
  }

  bool should_start_router() {
    if (!valid_) return false;
    return enum_value_ == k_all || enum_value_ == k_bootstrap;
  }

  bool should_configure_mrs() {
    if (!valid_) return false;
    return enum_value_ == k_all || enum_value_ == k_configure_mrs;
  }

 private:
  bool convert(const std::string &str_value, Enum *out_value) {
    const static Map map{
        {"bootstrap", k_bootstrap}, {"mrs", k_configure_mrs}, {"all", k_all}};

    auto it = map.find(str_value);
    if (map.end() == it) return false;

    *out_value = it->second;
    return true;
  }

  Enum enum_value_{k_all};
  bool valid_{false};
};

#endif  // ROUTER_SRC_BOOTSTRAP_SRC_BOOTSTRAP_MODE_H_
