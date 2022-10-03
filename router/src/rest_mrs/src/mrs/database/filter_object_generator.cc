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

#include "mrs/database/filter_object_generator.h"

#include <map>
#include <stdexcept>
#include <string>

#include "helper/container/map.h"
#include "helper/json/rapid_json_interator.h"
#include "helper/json/to_string.h"

namespace mrs {
namespace database {

using namespace std::string_literals;
using Value = FilterObjectGenerator::Value;

class tosString {
 public:
  bool acceptable(Value *v) const { return v->IsString(); }
  std::string to_string(Value *v) const {
    // TODO(lkotula): Do a proper escaping for SQL (Shouldn't be in review)
    return "'"s + v->GetString() + "'";
  }
};

class tosNumber {
 public:
  bool acceptable(Value *v) const { return v->IsNumber(); }
  std::string to_string(Value *v) const { return helper::json::to_string(v); }
};

class tosDate {
 public:
  const char *k_date{"$date"};
  bool acceptable(Value *v) const {
    if (!v->IsObject()) return false;

    auto it = v->FindMember(k_date);
    if (it == v->MemberEnd()) return false;

    // TODO(lkotula): Parse string for date ! (Shouldn't be in review)
    return it->value.IsString();
  }

  std::string to_string(Value *v) const {
    auto o = v->GetObject();
    // TODO(lkotula): Do a proper escaping for SQL (Shouldn't be in review)
    return "'"s + o[k_date].GetString() + "'";
  }
};

class Result {
 public:
  Result(Value *v) : v_{v} {}

  template <typename Z>
  Result &operator<<(const Z &t) {
    if (result.empty() && t.acceptable(v_)) {
      result = t.to_string(v_);
    }

    return *this;
  }

  std::string result;
  Value *v_;
};

template <typename... T>
std::string to_string(Value *value) {
  Result r(value);
  (r << ... << T());

  if (r.result.empty()) throw std::runtime_error("Not supported type.");

  return r.result;
}

std::string FilterObjectGenerator::get_result() const {
  return where_ + order_;
}

void FilterObjectGenerator::parse(const Document &doc) {
  if (!doc.IsObject())
    throw std::runtime_error("`FilterObject` must be a json object.");

  parse_orderby_asof_wmember(doc.GetObject());
}

void FilterObjectGenerator::parse_orderby_asof_wmember(Object object) {
  static std::string k_order{"$orderby"};
  static std::string k_asof{"$asof"};
  for (auto member : helper::json::Iterable(object)) {
    if (k_asof == member.first)
      parse_asof(member.second);
    else if (k_order == member.first) {
      if (!member.second->IsObject())
        throw std::runtime_error("`orderby` must be and json object.");
      prase_order(member.second->GetObject());
    } else {
      parse_wmember(member.first, member.second);
    }
  }
}

bool FilterObjectGenerator::parse_complex_object(Value *object) {
  if (!object->IsObject()) return false;
  if (object->MemberCount() != 1) return false;

  auto name = object->MemberBegin()->name.GetString();
  Value *value = &object->MemberBegin()->value;

  if ("$or"s == name)
    parse_complex_or(value);
  else if ("$and"s == name) {
    parse_complex_and(value);
  } else
    return false;

  return true;
}

bool FilterObjectGenerator::parse_simple_object(Value *object) {
  if (!object->IsObject()) return false;
  if (object->MemberCount() != 1) return false;

  auto name = object->MemberBegin()->name.GetString();
  Value *value = &object->MemberBegin()->value;
  auto &argument = argument_.back();

  where_ += " ";
  if ("$eq"s == name) {
    where_ +=
        argument + " = " + to_string<tosString, tosNumber, tosDate>(value);
  } else if ("$ne"s == name) {
    where_ +=
        argument + " <> " + to_string<tosString, tosNumber, tosDate>(value);
  } else if ("$lt"s == name) {
    where_ += argument + " < " + to_string<tosNumber, tosDate>(value);
  } else if ("$lte"s == name) {
    where_ += argument + " <= " + to_string<tosNumber, tosDate>(value);
  } else if ("$gt"s == name) {
    where_ += argument + " > " + to_string<tosNumber, tosDate>(value);
  } else if ("$gte"s == name) {
    where_ += argument + " >= " + to_string<tosNumber, tosDate>(value);
  } else if ("$instr"s == name) {
    where_ += "instr(" + argument + ", " + to_string<tosString>(value) + ")";
  } else if ("$ninstr"s == name) {
    where_ +=
        "not instr(" + argument + ", " + to_string<tosString>(value) + ")";
  } else if ("$like"s == name) {
    where_ += argument + " like " + to_string<tosString>(value);
  } else if ("$null"s == name) {
    where_ += argument + " IS NULL";
  } else if ("$notnull"s == name) {
    where_ += argument + " IS NOT NULL";
  } else if ("$between"s == name) {
    if (!value->IsArray())
      throw std::runtime_error("Between operator, requires an array field.");
    if (value->Size() != 2)
      throw std::runtime_error(
          "Between field, requires array with size of two.");
    // TODO(lkotula): Support of NULL values with different types of `tos-es`
    // (Shouldn't be in review)
    where_ += " BETWEEN(" + argument + ", " +
              to_string<tosString, tosNumber, tosDate>(&(*value)[0]) + ", " +
              to_string<tosString, tosNumber, tosDate>(&(*value)[1]) + ") ";
  } else
    return false;

  return true;
}

void FilterObjectGenerator::parse_complex_and(Value *value) {
  if (value->IsObject())
    throw std::runtime_error(
        "Simple operators are not supported for complex operations (just "
        "arrays).");

  if (!value->IsArray())
    throw std::runtime_error("Complex operations requires and array argument.");
}
void FilterObjectGenerator::parse_complex_or(Value * /*value*/) {}

void FilterObjectGenerator::parse_wmember(const char *name, Value *value) {
  using namespace std::literals::string_literals;
  if (!where_.empty()) where_ += " AND";
  argument_.push_back(name);
  if (parse_complex_object(value)) return;
  if (parse_simple_object(value)) return;
  // TODO(lkotula): array of ComplectValues (Shouldn't be in review)
  where_ += " "s + name + "=" + to_string<tosString, tosNumber, tosDate>(value);
  argument_.pop_back();
}

void FilterObjectGenerator::parse_asof(Value * /*value*/) {
  throw std::runtime_error("`asof` attribute not supported.");
}

void FilterObjectGenerator::prase_order(Object object) {
  const char *kWrongValueForOrder =
      "Wrong value for order, expected: [1,-1, ASC, DESC].";
  const char *kWrongTypeForOrder =
      "Wrong value type for order, expected INTEGER or STRING type "
      "with following values [1,-1, ASC, DESC].";
  bool first = order_.empty();
  if (first) {
    order_ += " ORDER BY";
  }

  for (auto member : helper::json::Iterable(object)) {
    order_ += first ? " ORDER BY" : ", ";
    first = false;
    bool asc = false;
    order_ += member.first;

    auto value = member.second;

    if (value->IsString()) {
      auto vstring = value->GetString();
      static std::map<std::string, bool> allowed_values{
          {"1", true}, {"-1", false}, {"ASC", true}, {"DESC", false}};
      if (!helper::container::get_value(allowed_values, vstring, &asc))
        throw std::runtime_error(kWrongValueForOrder);
    } else if (value->IsNumber()) {
      if (value->IsUint64()) {
        if (value->GetUint64() != 1)
          throw std::runtime_error(kWrongValueForOrder);
        asc = true;
      } else if (value->IsInt64()) {
        auto vint = value->GetInt64();
        if (vint == -1)
          asc = false;
        else if (vint == 1)
          asc = true;
        else
          throw std::runtime_error(kWrongValueForOrder);
      } else
        throw std::runtime_error(kWrongTypeForOrder);
    } else
      throw std::runtime_error(kWrongTypeForOrder);

    order_ += asc ? " ASC" : " DESC";
  }
}

}  // namespace database

}  // namespace mrs
