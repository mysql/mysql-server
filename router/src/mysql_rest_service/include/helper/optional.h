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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_OPTIONAL_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_OPTIONAL_H_

#include <cassert>

namespace helper {

/**
 * Lightweight implementation of optional.
 *
 * This class was especially created for POD types.
 */
template <typename ValueType>
class Optional {
 public:
  Optional() = default;
  Optional(const ValueType value) : is_set_{true}, v_{value} {}
  Optional(const Optional &other) { *this = other; }

  ValueType &operator*() {
    assert(is_set_);
    return v_;
  }

  ValueType operator*() const {
    assert(is_set_);
    return v_;
  }

  ValueType *operator->() {
    assert(is_set_);
    return &v_;
  }

  const ValueType *operator->() const {
    assert(is_set_);
    return &v_;
  }

  Optional &operator=(const Optional &value) {
    v_ = value.v_;
    is_set_ = value.is_set_;

    return *this;
  }

  Optional &operator=(const ValueType value) {
    v_ = value;
    is_set_ = true;

    return *this;
  }

  void reset() { is_set_ = false; }

  bool has_value() const { return is_set_; }
  ValueType value() const {
    assert(is_set_);
    return v_;
  }

  operator bool() const { return is_set_; }

 private:
  bool is_set_{false};
  ValueType v_{};
};

template <class T>
bool operator==(const Optional<T> &lhs, const Optional<T> &rhs) {
  if (lhs.has_value() != rhs.has_value()) return false;

  return *lhs == *rhs;
}

template <class T>
bool operator==(const T &lhs, const Optional<T> &rhs) {
  if (!rhs.has_value()) return false;

  return lhs == *rhs;
}

template <class T>
bool operator==(const Optional<T> &lhs, const T rhs) {
  if (!lhs.has_value()) return false;

  return *lhs == rhs;
}

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_OPTIONAL_H_
