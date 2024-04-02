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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_RAPID_JSON_INTERATOR_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_RAPID_JSON_INTERATOR_H_

#include <utility>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>

namespace helper {

namespace json {

template <typename Object = rapidjson::Document::ConstObject,
          typename Holder = Object *>
class IterableObject {
 public:
  using Value = rapidjson::Document::ValueType;
  using MemberIterator = typename Object::ConstMemberIterator;
  using Pair = std::pair<const char *, const Value *>;

  class It {
   public:
    It(MemberIterator it) : it_{it} {}

    It &operator++() {
      ++it_;
      return *this;
    }
    auto operator*() { return Pair(it_->name.GetString(), &it_->value); }
    bool operator!=(const It &other) { return it_ != other.it_; }

   private:
    MemberIterator it_;
  };

 public:
  IterableObject(Holder object) : obj_{object} {}

  It begin() { return It{obj_->MemberBegin()}; }
  It end() { return It{obj_->MemberEnd()}; }

  Holder obj_;
};

template <typename Array = rapidjson::Document::ConstArray,
          typename Holder = Array *>
class IterableArray {
 public:
  using Value = rapidjson::Document::ValueType;
  using Iterator = typename Array::ValueIterator;

 public:
  IterableArray(Holder a) : arr_{a} {}

  Iterator begin() { return arr_->Begin(); }
  Iterator end() { return arr_->End(); }

  Holder arr_;
};

template <typename Obj>
auto member_iterator(Obj &o) {
  return IterableObject<Obj>(&o);
}

template <typename Array>
auto array_iterator(Array &o) {
  return IterableArray<Array>(&o);
}

}  // namespace json

}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_RAPID_JSON_INTERATOR_H_
