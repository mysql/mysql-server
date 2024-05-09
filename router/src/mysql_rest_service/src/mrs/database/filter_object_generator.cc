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

#include "mrs/database/filter_object_generator.h"

#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "helper/container/map.h"
#include "helper/container/to_string.h"
#include "helper/json/rapid_json_interator.h"
#include "helper/json/text_to.h"
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

class tosGeom {
 private:
  static bool is_geo_json(Value *v) {
    // TODO: this function is not correct yet, it will reject valid geo jsons
    // like:
    // {"type":"Feature", "geometry": {"type": "Point", "coordinates": [1, 1]},
    // "properties": {}}
    // that are accepted by mysql server. If possible it would be best to use
    // some server function for this classification.
    using namespace std::string_literals;
    if (!v->IsObject()) return false;

    bool has_type{false}, has_coords{false}, has_geometries{false};
    auto v_as_object = v->GetObject();

    for (auto kv : helper::json::member_iterator(v_as_object)) {
      if (!has_type && "type"s == kv.first) {
        has_type = kv.second->IsString();
      } else if (!has_geometries && "geometries"s == kv.first) {
        // GEOMETRYCOLLECTION will have "geometries" array of geometry objects
        // instead of "coordintes" directly
        has_geometries = kv.second->IsArray();
      } else if (!has_coords && "coordinates"s == kv.first) {
        has_coords = kv.second->IsArray();
      }
    }

    return has_type && (has_coords || has_geometries);
  }

 public:
  bool acceptable(entry::DataField *dfield, Value *v) const {
    if (!dfield) return false;
    if (dfield->source->type != entry::ColumnType::GEOMETRY) return false;
    return v->IsString() || is_geo_json(v);
  }
  mysqlrouter::sqlstring to_sqlstring(entry::DataField *dfield,
                                      Value *v) const {
    if (v->IsString())
      return mysqlrouter::sqlstring("ST_GeomFromText(?, ?)")
             << v->GetString() << dfield->source->srid;

    return mysqlrouter::sqlstring("ST_GeomFromGeoJSON(?,1,?)")
           << helper::json::to_string(v) << dfield->source->srid;
  }
};

class tosString {
 public:
  bool acceptable(entry::DataField *, Value *v) const { return v->IsString(); }
  mysqlrouter::sqlstring to_sqlstring(entry::DataField *, Value *v) const {
    return mysqlrouter::sqlstring("?") << v->GetString();
  }
};

class tosNumber {
 public:
  bool acceptable(entry::DataField *, Value *v) const { return v->IsNumber(); }
  mysqlrouter::sqlstring to_sqlstring(entry::DataField *, Value *v) const {
    return mysqlrouter::sqlstring(helper::json::to_string(v).c_str());
  }
};

class tosBoolean {
 public:
  bool acceptable(entry::DataField *df, Value *) const {
    if (df && df->source->type == entry::ColumnType::BOOLEAN) {
      return true;
    }

    return false;
  }
  mysqlrouter::sqlstring to_sqlstring(entry::DataField *, Value *v) const {
    if (v->IsBool()) {
      if (v->GetBool()) return {"TRUE"};
      return {"FALSE"};
    }
    return mysqlrouter::sqlstring(helper::json::to_string(v).c_str());
  }
};

class tosDate {
 public:
  const char *k_date{"$date"};
  bool acceptable(entry::DataField *, Value *v) const {
    if (!v->IsObject()) return false;

    auto it = v->FindMember(k_date);
    if (it == v->MemberEnd()) return false;

    // TODO(lkotula): Parse string for date ! (Shouldn't be in review)
    return it->value.IsString();
  }

  mysqlrouter::sqlstring to_sqlstring(entry::DataField *, Value *v) const {
    auto o = v->GetObject();
    return mysqlrouter::sqlstring("?") << o[k_date].GetString();
  }
};

class Result {
 public:
  explicit Result(entry::DataField *dfield, Value *v)
      : dfield_{dfield}, v_{v} {}

  template <typename Z>
  Result &operator<<(const Z &t) {
    if (result.is_empty() && t.acceptable(dfield_, v_)) {
      result = t.to_sqlstring(dfield_, v_);
    }

    return *this;
  }

  mysqlrouter::sqlstring result;
  entry::DataField *dfield_;
  Value *v_;
};

template <typename... T>
mysqlrouter::sqlstring to_sqlstring(entry::DataField *dfield, Value *value) {
  Result r(dfield, value);
  (r << ... << T());

  if (r.result.is_empty()) throw RestError("Not supported type.");

  return r.result;
}

FilterObjectGenerator::FilterObjectGenerator(
    std::shared_ptr<database::entry::Object> object, bool joins_allowed,
    uint64_t wait_timeout, bool use_wait_in_where)
    : object_metadata_{object},
      joins_allowed_{joins_allowed},
      wait_timeout_{wait_timeout},
      use_wait_in_where_{use_wait_in_where} {}

void FilterObjectGenerator::reconfigure(uint64_t wait_timeout,
                                        bool use_wait_in_where) {
  wait_timeout_ = wait_timeout;
  use_wait_in_where_ = use_wait_in_where;
}

mysqlrouter::sqlstring FilterObjectGenerator::get_result() const {
  mysqlrouter::sqlstring tmp;
  tmp.append_preformatted(where_);
  if (has_asof() && use_wait_in_where_) {
    if (has_where()) tmp.append_preformatted(" AND ");

    mysqlrouter::sqlstring wait{" 0=WAIT_FOR_EXECUTED_GTID_SET(?,?) "};
    wait << asof_gtid_;
    wait << wait_timeout_;
    tmp.append_preformatted(wait);
  }

  tmp.append_preformatted(order_);
  return tmp;
}

void FilterObjectGenerator::reset(const Clear clear) {
  if (clear & Clear::kWhere) {
    log_debug("Resetting where");
    where_.reset("");
  }
  if (clear & Clear::kOrder) {
    log_debug("Resetting order");
    order_.reset("");
  }
  if (clear & Clear::kAsof) {
    log_debug("Resetting asof");
    asof_gtid_.reset("");
  }
}

void FilterObjectGenerator::parse(const Document &doc) {
  reset();

  if (doc.IsNull()) return;
  if (!doc.IsObject()) throw RestError("`FilterObject` must be a json object.");

  parse_orderby_asof_wmember(doc.GetObject());
}

void FilterObjectGenerator::parse(const std::string &filter_query) {
  parse(helper::json::text_to_document(filter_query));
}

void FilterObjectGenerator::parse_orderby_asof_wmember(Object object) {
  static std::string k_order{"$orderby"};
  static std::string k_asof{"$asof"};
  for (auto member : helper::json::member_iterator(object)) {
    if (k_asof == member.first) {
      parse_asof(member.second);
    } else if (k_order == member.first) {
      if (!member.second->IsObject())
        throw RestError("`orderby` must be and json object.");
      parse_order(member.second->GetObject());
    } else {
      if (!where_.is_empty()) where_.append_preformatted(" AND");
      //      else
      //        where_ = " WHERE";
      parse_wmember(member.first, member.second);
    }
  }
}

bool FilterObjectGenerator::parse_complex_object(const char *name,
                                                 Value *value) {
  //  log_debug("Parser complex_object ");
  if ("$or"s == name) {
    where_.append_preformatted("(");
    parse_complex_or(value);
    where_.append_preformatted(")");
  } else if ("$and"s == name) {
    where_.append_preformatted("(");
    parse_complex_and(value);
    where_.append_preformatted(")");
  } else if ("$match"s == name) {
    where_.append_preformatted("(");
    parse_match(value);
    where_.append_preformatted(")");
  } else {
    return false;
  }

  return true;
}

bool FilterObjectGenerator::parse_simple_object(Value *object) {
  if (!object->IsObject()) return false;
  if (object->MemberCount() != 1) return false;

  auto name = object->MemberBegin()->name.GetString();
  Value *value = &object->MemberBegin()->value;
  auto field_name = argument_.back().c_str();
  auto dfield = resolve_field(field_name);
  auto db_name = resolve_field_name(dfield, field_name, false);

  log_debug("dispatched type %i", static_cast<int>(value->GetType()));
  where_.append_preformatted(" ");
  if ("$eq"s == name) {
    log_debug("Parser simple_object $eq");
    where_.append_preformatted(db_name)
        .append_preformatted(" = ")
        .append_preformatted(
            to_sqlstring<tosGeom, tosString, tosBoolean, tosNumber, tosDate>(
                dfield.get(), value));
  } else if ("$ne"s == name) {
    log_debug("Parser simple_object $ne");
    where_.append_preformatted(db_name)
        .append_preformatted(" <> ")
        .append_preformatted(
            to_sqlstring<tosGeom, tosString, tosBoolean, tosNumber, tosDate>(
                dfield.get(), value));
  } else if ("$lt"s == name) {
    log_debug("Parser simple_object $lt");
    where_.append_preformatted(db_name)
        .append_preformatted(" < ")
        .append_preformatted(
            to_sqlstring<tosNumber, tosDate>(dfield.get(), value));
  } else if ("$lte"s == name) {
    log_debug("Parser simple_object $lte");
    where_.append_preformatted(db_name)
        .append_preformatted(" <= ")
        .append_preformatted(
            to_sqlstring<tosNumber, tosDate>(dfield.get(), value));
  } else if ("$gt"s == name) {
    log_debug("Parser simple_object $gt");
    where_.append_preformatted(db_name)
        .append_preformatted(" > ")
        .append_preformatted(
            to_sqlstring<tosNumber, tosDate>(dfield.get(), value));
  } else if ("$gte"s == name) {
    log_debug("Parser simple_object $gte");
    where_.append_preformatted(db_name)
        .append_preformatted(" >= ")
        .append_preformatted(
            to_sqlstring<tosNumber, tosDate>(dfield.get(), value));
  } else if ("$instr"s == name) {
    log_debug("Parser simple_object $instr");
    where_.append_preformatted("instr(")
        .append_preformatted(db_name)
        .append_preformatted(", ")
        .append_preformatted(to_sqlstring<tosString>(dfield.get(), value))
        .append_preformatted(")");
  } else if ("$ninstr"s == name) {
    log_debug("Parser simple_object $not instr");
    where_.append_preformatted("not instr(")
        .append_preformatted(db_name)
        .append_preformatted(", ")
        .append_preformatted(to_sqlstring<tosString>(dfield.get(), value))
        .append_preformatted(")");
  } else if ("$like"s == name) {
    log_debug("Parser simple_object $like");
    where_.append_preformatted(db_name)
        .append_preformatted(" like ")
        .append_preformatted(to_sqlstring<tosString>(dfield.get(), value));
  } else if ("$null"s == name) {
    log_debug("Parser simple_object $null");
    where_.append_preformatted(db_name).append_preformatted(" IS NULL");
    log_debug("Parser simple_object $notnull");
  } else if ("$notnull"s == name) {
    where_.append_preformatted(db_name).append_preformatted(" IS NOT NULL");
  } else if ("$between"s == name) {
    log_debug("Parser simple_object $between");
    if (!value->IsArray())
      throw RestError("Between operator, requires an array field.");
    if (value->Size() != 2)
      throw RestError("Between field, requires array with size of two.");
    // TODO(lkotula): Support of NULL values with different types of `tos-es`
    // (Shouldn't be in review)
    where_.append_preformatted(db_name)
        .append_preformatted(" BETWEEN ")
        .append_preformatted(to_sqlstring<tosString, tosNumber, tosDate>(
            dfield.get(), &(*value)[0]))
        .append_preformatted(" AND ")
        .append_preformatted(to_sqlstring<tosString, tosNumber, tosDate>(
            dfield.get(), &(*value)[1]));
  } else {
    return false;
  }

  return true;
}

void FilterObjectGenerator::parse_match(Value *value) {
  log_debug("parse_complex_match");
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
  where_.append_preformatted(v);
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
    if (!first) where_.append_preformatted(" AND");
    first = false;

    if (!el.IsObject())
      throw RestError("Complex expression, array element must be an object.");
    where_.append_preformatted("(");
    auto el_as_object = el.GetObject();
    for (auto member : helper::json::member_iterator(el_as_object)) {
      parse_wmember(member.first, member.second);
    }
    where_.append_preformatted(")");
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
    if (!first) where_.append_preformatted(" OR");
    first = false;

    if (!el.IsObject())
      throw RestError("Complex expression, array element must be an object.");
    where_.append_preformatted("(");
    auto el_as_object = el.GetObject();
    for (auto member : helper::json::member_iterator(el_as_object)) {
      parse_wmember(member.first, member.second);
    }
    where_.append_preformatted(")");
  }
}

mysqlrouter::sqlstring FilterObjectGenerator::get_asof() const {
  return asof_gtid_;
}

bool FilterObjectGenerator::has_where() const { return !where_.is_empty(); }

bool FilterObjectGenerator::has_order() const { return !order_.is_empty(); }

bool FilterObjectGenerator::has_asof() const { return !asof_gtid_.is_empty(); }

void FilterObjectGenerator::parse_wmember(const char *name, Value *value) {
  log_debug("Parser wmember");
  using namespace std::literals::string_literals;
  argument_.push_back(name);
  if (parse_complex_object(name, value)) return;
  if (parse_simple_object(value)) return;
  log_debug("direct field=value");

  auto dfield = resolve_field(name);
  mysqlrouter::sqlstring dbname = resolve_field_name(dfield, name, false);

  // TODO(lkotula): array of ComplectValues (Shouldn't be in review)
  where_.append_preformatted(
      mysqlrouter::sqlstring(" !=?")
      << dbname
      << to_sqlstring<tosGeom, tosString, tosBoolean, tosNumber, tosDate>(
             dfield.get(), value));
  argument_.pop_back();
}

void FilterObjectGenerator::parse_asof(Value *value) {
  log_debug("Parser asof");
  if (!value->IsString())
    throw RestError("Wrong value for `asof`, requires string with GTID.");
  asof_gtid_.reset("?");
  asof_gtid_ << value->GetString();
}

void FilterObjectGenerator::parse_order(Object object) {
  log_debug("Parser Order");
  const char *kWrongValueForOrder =
      "Wrong value for order, expected: [1,-1, ASC, DESC].";
  const char *kWrongTypeForOrder =
      "Wrong value type for order, expected INTEGER or STRING type "
      "with following values [1,-1, ASC, DESC].";
  bool first = order_.is_empty();

  if (0 == object.MemberCount())
    throw RestError("Wrong value for `orderby`, requires object with fields.");

  for (auto member : helper::json::member_iterator(object)) {
    order_.append_preformatted(first ? " ORDER BY " : ", ");
    first = false;
    bool asc = false;
    const auto &field_name = member.first;
    auto dfield = resolve_field(field_name);
    order_.append_preformatted(resolve_field_name(dfield, field_name, true));

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

    order_.append_preformatted(asc ? " ASC" : " DESC");
  }
}

std::shared_ptr<entry::DataField> FilterObjectGenerator::resolve_field(
    const char *name) {
  if (!object_metadata_) return nullptr;

  auto field = object_metadata_->get_field(name);
  return std::dynamic_pointer_cast<entry::DataField>(field);
}

mysqlrouter::sqlstring FilterObjectGenerator::resolve_field_name(
    std::shared_ptr<entry::DataField> &dfield, const char *name,
    bool for_sorting) const {
  if (!object_metadata_) return mysqlrouter::sqlstring("!") << name;

  if (dfield) {
    if (!dfield->allow_filtering && !for_sorting)
      throw RestError("Cannot filter on field "s + name);
    if (!dfield->allow_sorting && for_sorting)
      throw RestError("Cannot sort on field "s + name);

    if (joins_allowed_)
      return mysqlrouter::sqlstring("!.!")
             << dfield->source->table.lock()->table_alias
             << dfield->source->name;
    else
      return mysqlrouter::sqlstring("!") << dfield->source->name;
  }
  // TODO(alfredo) filter on nested fields
  if (!for_sorting)
    throw RestError("Cannot filter on field "s + name);
  else
    throw RestError("Cannot sort on field "s + name);
}

}  // namespace database
}  // namespace mrs
