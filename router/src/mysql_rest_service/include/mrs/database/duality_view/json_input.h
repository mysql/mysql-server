/*
 * Copyright (c) 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_DUALITY_VIEW_JSON_INPUT_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_DUALITY_VIEW_JSON_INPUT_H_

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include "helper/json/rapid_json_to_text.h"  // XXX delme

namespace mrs {
namespace database {
namespace dv {

class JSONInput {
 public:
  JSONInput(const JSONInput &) = default;
  JSONInput &operator=(const JSONInput &) = default;

  virtual ~JSONInput() = default;

  virtual bool has_new() const { return false; }
  virtual bool has_old() const { return false; }

 protected:
  JSONInput() {}
};

class JSONInputObject : public JSONInput {
 public:
  struct MemberReference {
    MemberReference() {}

    std::string_view new_name() const { return (*new_)->name.GetString(); }
    std::string_view old_name() const { return (*old_)->name.GetString(); }

    const rapidjson::Value &new_value() const { return (*new_)->value; }
    const rapidjson::Value &old_value() const { return (*old_)->value; }

    bool has_new() const {
      return new_.has_value() &&
             *new_ != rapidjson::Value::ConstMemberIterator();
    }

    bool has_old() const {
      return old_.has_value() &&
             *old_ != rapidjson::Value::ConstMemberIterator();
    }

   private:
    friend class JSONInputObject;

    explicit MemberReference(rapidjson::Value::ConstMemberIterator anew)
        : new_(std::move(anew)) {}

    MemberReference(rapidjson::Value::ConstMemberIterator anew,
                    rapidjson::Value::ConstMemberIterator aold)
        : new_(std::move(anew)), old_(std::move(aold)) {}

    std::optional<rapidjson::Value::ConstMemberIterator> new_;
    std::optional<rapidjson::Value::ConstMemberIterator> old_;
  };

  JSONInputObject() {}

  explicit JSONInputObject(const rapidjson::Value &value)
      : new_value_(&value) {}

  JSONInputObject(std::nullptr_t, const rapidjson::Value &old_value)
      : old_value_(&old_value) {}

  JSONInputObject(const rapidjson::Value &value,
                  const rapidjson::Value &old_value)
      : new_value_(&value), old_value_(&old_value) {}

  template <typename S>
  MemberReference find(const S &name) const {
    if (!has_new()) return {};

    auto new_m = new_value_->FindMember(name);
    if (old_value_) {
      auto old_m = old_value_->FindMember(name);

      return MemberReference(new_m != new_value_->MemberEnd()
                                 ? new_m
                                 : rapidjson::Value::ConstMemberIterator(),
                             old_m != old_value_->MemberEnd()
                                 ? old_m
                                 : rapidjson::Value::ConstMemberIterator());
    } else {
      return MemberReference(new_m != new_value_->MemberEnd()
                                 ? new_m
                                 : rapidjson::Value::ConstMemberIterator());
    }
  }

  bool has_new() const override { return new_value_ != nullptr; }
  bool has_old() const override { return old_value_ != nullptr; }

  const rapidjson::Value &new_value() const { return *new_value_; }

  const rapidjson::Value &old_value() const {
    assert(has_old());

    return *old_value_;
  }

  rapidjson::Value::ConstObject new_object() const {
    return new_value_->GetObject();
  }

  rapidjson::Value::ConstObject old_object() const {
    assert(has_old());

    return old_value_->GetObject();
  }

  bool new_empty() const { return new_value_->GetObject().MemberCount() == 0; }

 private:
  const rapidjson::Value *new_value_ = nullptr;
  const rapidjson::Value *old_value_ = nullptr;
};

class JSONInputArray : public JSONInput {
 public:
  struct ValueReference {
    const rapidjson::Value &new_value() const { return *new_; }
    const rapidjson::Value &old_value() const { return *old_; }

    bool has_new() const { return new_ != nullptr; }
    bool has_old() const { return old_ != nullptr; }

   private:
    friend class JSONInputArray;

    explicit ValueReference(const rapidjson::Value &anew) : new_(&anew) {}

    ValueReference(const rapidjson::Value &anew, const rapidjson::Value &aold)
        : new_(&anew), old_(&aold) {}

    const rapidjson::Value *new_ = nullptr;
    const rapidjson::Value *old_ = nullptr;
  };

  JSONInputArray() {}
  JSONInputArray(const JSONInputArray &) = default;

  explicit JSONInputArray(const rapidjson::Value &value) : new_value_(&value) {}

  JSONInputArray(const rapidjson::Value &value,
                 const rapidjson::Value &old_value)
      : new_value_(&value), old_value_(&old_value) {}

  JSONInputArray(std::nullptr_t, const rapidjson::Value &old_value)
      : new_value_(nullptr), old_value_(&old_value) {}

  size_t size() const {
    if (!new_value_) return 0;
    return new_value_->Size();
  }

  ValueReference get(size_t i) const {
    assert(has_new());

    if (!has_old()) return ValueReference((*new_value_)[i]);
    if (old_value_->Size() > 0 && old_sorted_.empty())
      throw std::logic_error("sort_old() must be called first");

    if (i >= new_value_->Size()) throw std::range_error("invalid array index");

    int old_i = old_sorted_.at(i);

    if (old_i < 0)
      return ValueReference((*new_value_)[i]);
    else
      return ValueReference((*new_value_)[i], (*old_value_)[old_i]);
  }

  bool has_new() const override { return new_value_ != nullptr; }
  bool has_old() const override { return old_value_ != nullptr; }

  rapidjson::Value::ConstArray new_array() const {
    return new_value_->GetArray();
  }

  bool new_empty() const { return !new_value_ || new_value_->Empty(); }

  template <typename KeyType>
  void sort_old(const std::function<KeyType(const rapidjson::Value &)> &get_key,
                std::vector<KeyType> &out_missing_from_old) {
    if (!new_value_ || !old_value_) return;
    std::vector<std::tuple<KeyType, int>> old_array;

    auto index_in_old_array = [&](const rapidjson::Value &value) -> int {
      auto key = get_key(value);

      if (!key.empty()) {
        for (auto iter = old_array.begin(); iter != old_array.end(); ++iter) {
          if (std::get<0>(*iter) == key) {
            int i = std::get<1>(*iter);
            old_array.erase(iter);
            return i;
          }
        }
      }
      return -1;
    };

    int index = 0;
    for (const auto &ov : old_value_->GetArray()) {
      old_array.emplace_back(get_key(ov), index++);
    }
    for (const auto &obj : new_value_->GetArray()) {
      old_sorted_.push_back(index_in_old_array(obj));
    }

    for (const auto &elem : old_array) {
      out_missing_from_old.push_back(std::get<0>(elem));
    }
  }

 private:
  const rapidjson::Value *new_value_ = nullptr;
  const rapidjson::Value *old_value_ = nullptr;

  std::vector<int> old_sorted_;
};

JSONInputObject make_input_object(const JSONInputArray::ValueReference &ref,
                                  const std::string &table,
                                  const std::string &field = "");

JSONInputArray make_input_array(const JSONInputObject::MemberReference &ref,
                                const std::string &table,
                                const std::string &field = "");

JSONInputObject make_input_object(const JSONInputObject::MemberReference &ref,
                                  const std::string &table,
                                  const std::string &field = "");

}  // namespace dv
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_DUALITY_VIEW_JSON_INPUT_H_
