/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "helper/container/map.h"
#include "helper/container/to_string.h"
#include "helper/json/rapid_json_interator.h"
#include "helper/json/to_string.h"
#include "mrs/interface/rest_error.h"

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/utils_sqlstring.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

using namespace std::string_literals;
using Value = FilterObjectGenerator::Value;
using RestError = mrs::interface::RestError;

std::vector<std::string> get_array_of_string(Value *value) {
  if (value->IsString()) return {value->GetString()};

  if (!value->IsArray())
    throw RestError(
        "One of parameters must be a string or an array of strings");

  std::vector<std::string> result;
  auto array = value->GetArray();
  for (auto &v : helper::json::array_iterator(array)) {
    if (!v.IsString())
      throw RestError("All values in array must be of type string.");

    result.push_back(v.GetString());
  }

  return result;
}

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

  if (r.result.empty()) throw RestError("Not supported type.");

  return r.result;
}

FilterObjectGenerator::FilterObjectGenerator(
    std::shared_ptr<database::entry::Object> object, bool joins_allowed)
    : object_metadata_(object), joins_allowed_(joins_allowed) {}

std::string FilterObjectGenerator::get_result() const {
  return where_ + order_;
}

void FilterObjectGenerator::reset() {
  where_.clear();
  order_.clear();
  has_order_ = false;
  has_filter_ = false;
}

void FilterObjectGenerator::parse(const Document &doc) {
  if (!doc.IsObject()) throw RestError("`FilterObject` must be a json object.");

  reset();

  parse_orderby_asof_wmember(doc.GetObject());
}

void FilterObjectGenerator::parse_orderby_asof_wmember(Object object) {
  static std::string k_order{"$orderby"};
  static std::string k_asof{"$asof"};
  for (auto member : helper::json::member_iterator(object)) {
    if (k_asof == member.first)
      parse_asof(member.second);
    else if (k_order == member.first) {
      if (!member.second->IsObject())
        throw RestError("`orderby` must be and json object.");
      prase_order(member.second->GetObject());
    } else {
      if (!where_.empty()) where_ += " AND";
      //      else
      //        where_ = " WHERE";
      parse_wmember(member.first, member.second);
    }
  }
}

bool FilterObjectGenerator::parse_complex_object(const char *name,
                                                 Value *value) {
  log_debug("Parser complex_object ");
  if ("$or"s == name) {
    where_ += "(";
    parse_complex_or(value);
    where_ += ")";
  } else if ("$and"s == name) {
    where_ += "(";
    parse_complex_and(value);
    where_ += ")";
  } else if ("$match"s == name) {
    where_ += "(";
    parse_match(value);
    where_ += ")";
  } else
    return false;

  return true;
}

bool FilterObjectGenerator::parse_simple_object(Value *object) {
  log_debug("Parser simple_object");
  if (!object->IsObject()) return false;
  if (object->MemberCount() != 1) return false;

  auto name = object->MemberBegin()->name.GetString();
  Value *value = &object->MemberBegin()->value;
  auto &argument = argument_.back();

  log_debug("parse_simple_object %i", static_cast<int>(value->GetType()));
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
      throw RestError("Between operator, requires an array field.");
    if (value->Size() != 2)
      throw RestError("Between field, requires array with size of two.");
    // TODO(lkotula): Support of NULL values with different types of `tos-es`
    // (Shouldn't be in review)
    where_ += argument + " BETWEEN " +
              to_string<tosString, tosNumber, tosDate>(&(*value)[0]) + " AND " +
              to_string<tosString, tosNumber, tosDate>(&(*value)[1]);
  } else
    return false;

  return true;
}

void FilterObjectGenerator::parse_match(Value *value) {
  log_debug("Parser match");
  if (!value->IsObject())
    throw RestError("Match operator, requires JSON object as value.");
  auto param = value->FindMember("$params");
  auto against = value->FindMember("$against");

  if (param == value->MemberEnd() || !param->value.IsArray())
    throw RestError(
        "Match operator, requires JSON array under \"$params\" key.");

  if (against == value->MemberEnd() || !against->value.IsObject())
    throw RestError(
        "Match operator, requires JSON object under \"$against\" key.");

  auto fields = get_array_of_string(&param->value);

  auto against_expr = against->value.FindMember("$expr");
  auto against_mod = against->value.FindMember("$modifier");

  if (against_expr == against->value.MemberEnd() ||
      !against_expr->value.IsString()) {
    throw RestError("Match operator, requires string value in \"$expr\" key.");
  }

  mysqlrouter::sqlstring selected_modifier{""};

  if (against_mod != against->value.MemberEnd()) {
    if (!against_mod->value.IsString()) {
      throw RestError(
          "Match operator, optional value under \"modifier\" key must be a "
          "string.");
    }
    const static std::set<std::string> allowed_values{
        "IN NATURAL LANGUAGE MODE",
        "IN NATURAL LANGUAGE MODE WITH QUERY EXPANSION", "IN BOOLEAN MODE",
        "WITH QUERY EXPANSION"};

    if (!allowed_values.count(against_mod->value.GetString())) {
      using namespace std::string_literals;
      throw RestError(
          "Match operator, optional value under \"modifier\" key must be a "
          "string set to one of: ["s +
          helper::container::to_string(allowed_values) + "]");
    }
    selected_modifier = mysqlrouter::sqlstring{against_mod->value.GetString()};
  }

  mysqlrouter::sqlstring v{"MATCH (!) AGAINST(? ?) "};
  v << fields << against_expr->value.GetString() << selected_modifier;
  where_ += v.str();
}

void FilterObjectGenerator::parse_complex_and(Value *value) {
  log_debug("Parser complex_and");
  if (value->IsObject())
    throw RestError(
        "Simple operators are not supported for complex operations (just "
        "arrays).");

  if (!value->IsArray())
    throw RestError("Complex operations requires and array argument.");
  auto arr = value->GetArray();
  bool first = true;
  for (auto &el : helper::json::array_iterator(arr)) {
    if (!first) where_ += " AND";
    first = false;

    if (!el.IsObject())
      throw RestError("Complex expression, array element must be an object.");
    where_ += "(";
    auto el_as_object = el.GetObject();
    for (auto member : helper::json::member_iterator(el_as_object)) {
      parse_wmember(member.first, member.second);
    }
    where_ += ")";
  }
}
void FilterObjectGenerator::parse_complex_or(Value *value) {
  log_debug("Parser complex_or");
  if (value->IsObject())
    throw RestError(
        "Simple operators are not supported for complex operations (just "
        "arrays).");

  if (!value->IsArray())
    throw RestError("Complex operations requires and array argument.");
  auto arr = value->GetArray();
  bool first = true;
  for (auto &el : helper::json::array_iterator(arr)) {
    if (!first) where_ += " OR";
    first = false;

    if (!el.IsObject())
      throw RestError("Complex expression, array element must be an object.");
    where_ += "(";
    auto el_as_object = el.GetObject();
    for (auto member : helper::json::member_iterator(el_as_object)) {
      parse_wmember(member.first, member.second);
    }
    where_ += ")";
  }
}

bool FilterObjectGenerator::has_order() const { return has_order_; }

void FilterObjectGenerator::parse_wmember(const char *name, Value *value) {
  log_debug("Parser wmember");
  using namespace std::literals::string_literals;
  has_filter_ = true;
  argument_.push_back(name);
  if (parse_complex_object(name, value)) return;
  if (parse_simple_object(value)) return;
  log_debug("fallback");

  std::string dbname = resolve_field_name(name);

  // TODO(lkotula): array of ComplectValues (Shouldn't be in review)
  where_ +=
      " "s + dbname + "=" + to_string<tosString, tosNumber, tosDate>(value);
  argument_.pop_back();
}

void FilterObjectGenerator::parse_asof(Value * /*value*/) {
  log_debug("Parser asof");
  throw RestError("`asof` attribute not supported.");
}

void FilterObjectGenerator::prase_order(Object object) {
  log_debug("Parser Order");
  const char *kWrongValueForOrder =
      "Wrong value for order, expected: [1,-1, ASC, DESC].";
  const char *kWrongTypeForOrder =
      "Wrong value type for order, expected INTEGER or STRING type "
      "with following values [1,-1, ASC, DESC].";
  bool first = order_.empty();

  if (0 == object.MemberCount())
    throw RestError("Wrong falue for `orderby`, requires object with fields.");

  for (auto member : helper::json::member_iterator(object)) {
    order_ += first ? " ORDER BY " : ", ";
    first = false;
    bool asc = false;
    order_ += member.first;

    auto value = member.second;

    if (value->IsString()) {
      auto vstring = value->GetString();
      static std::map<std::string, bool> allowed_values{
          {"1", true}, {"-1", false}, {"ASC", true}, {"DESC", false}};
      if (!helper::container::get_value(allowed_values, vstring, &asc))
        throw RestError(kWrongValueForOrder);
    } else if (value->IsNumber()) {
      if (value->IsUint64()) {
        if (value->GetUint64() != 1) throw RestError(kWrongValueForOrder);
        asc = true;
      } else if (value->IsInt64()) {
        auto vint = value->GetInt64();
        if (vint == -1)
          asc = false;
        else if (vint == 1)
          asc = true;
        else
          throw RestError(kWrongValueForOrder);
      } else {
        throw RestError(kWrongTypeForOrder);
      }
    } else {
      throw RestError(kWrongTypeForOrder);
    }

    order_ += asc ? " ASC" : " DESC";
  }
  has_order_ = true;
}

std::string FilterObjectGenerator::resolve_field_name(const char *name) const {
  if (object_metadata_) {
    for (const auto &field : object_metadata_->fields) {
      if (field->name.compare(name) == 0) {
        if (joins_allowed_)
          return mysqlrouter::sqlstring("!.!")
                 << field->source->table_alias << field->db_name;
        else
          return mysqlrouter::sqlstring("!") << field->db_name;
      }
    }
    // TODO(alfredo) filter on nested fields
    throw std::runtime_error("Cannot filter on field "s + name);
  } else {
    return name;
  }
}

}  // namespace database

}  // namespace mrs
