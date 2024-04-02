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

#ifndef ROUTER_TESTS_MRS_CLIENT_JSON_JSON_CONTAINER_H_
#define ROUTER_TESTS_MRS_CLIENT_JSON_JSON_CONTAINER_H_

#include <map>

#include <my_rapidjson_size_t.h>
#include <rapidjson/document.h>

namespace json {

class JsonContainer {
 public:
  using Document = rapidjson::Document;
  using Value = rapidjson::Value;
  using StringRefType = rapidjson::Value::StringRefType;

  JsonContainer(Document *doc) : doc_{doc} {}

  void cursor_move(const std::string &path, const std::string &prop_name,
                   const bool is_array, const bool is_leaf) {
    Node n =
        is_leaf ? Node::k_leaf : (is_array ? Node::k_array : Node::k_object);

    move(path, prop_name, n);
  }

  void cursor_reset() {
    cursor_ = nullptr;
    first_ = true;
  }

  void cursor_set_value(Value &value) { *cursor_ = value; }

 private:
  enum class Node { k_leaf, k_object, k_array };
  rapidjson::Type empty_object(Node n) {
    switch (n) {
      case Node::k_array:
        return rapidjson::Type::kArrayType;
      case Node::k_object:
        return rapidjson::Type::kObjectType;
      case Node::k_leaf:
        return rapidjson::Type::kNullType;
    }
    return rapidjson::Type::kNullType;
  }

  void move(const std::string &path, const std::string &prop_name,
            const Node n) {
    StringRefType key{prop_name.c_str(), prop_name.length()};

    if (!cursor_) {
      if (doc_->GetType() == rapidjson::Type::kNullType) {
        if (Node::k_object == n)
          doc_->SetObject();
        else
          doc_->SetArray();
      }
      cursor_ = doc_;
    } else if (cursor_->IsObject()) {
      if (!cursor_->HasMember(key)) {
        Value v(empty_object(n));
        Value name{prop_name.c_str(), prop_name.length(), doc_->GetAllocator()};
        cursor_->AddMember(name, v, doc_->GetAllocator());
      }
      cursor_ = &cursor_->FindMember(key.s)->value;
    } else if (cursor_->IsArray()) {
      uint64_t idx;
      auto it = key_to_idx_.find(path);
      if (key_to_idx_.end() != it) {
        idx = it->second;
      } else {
        Value v(empty_object(n));
        cursor_->PushBack(v, doc_->GetAllocator());
        idx = key_to_idx_[path] = cursor_->Size() - 1;
      }
      cursor_ = &(*cursor_)[idx];
    }
  }

  std::map<std::string, uint64_t> key_to_idx_;
  bool first_{true};
  Document *doc_;
  Value *cursor_;
};

}  // namespace json

#endif  // ROUTER_TESTS_MRS_CLIENT_JSON_JSON_CONTAINER_H_
