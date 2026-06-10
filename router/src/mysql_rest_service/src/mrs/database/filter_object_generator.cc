/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

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
#include "helper/json/rapid_json_iterator.h"
#include "helper/json/text_to.h"
#include "helper/json/to_string.h"
#include "helper/mysql_column_types.h"
#include "mrs/interface/rest_error.h"

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/utils_sqlstring.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

using namespace std::string_literals;
using Value = FilterObjectGenerator::Value;
using RestError = mrs::interface::RestError;

static bool is_date_type(const enum_field_types ft) {
  switch (ft) {
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
      return true;

    default:
      return false;
  }
}

static std::vector<std::string> get_array_of_string(Value *value) {
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

class tosVec {
 private:
  static bool is_vec_json(Value *v) {
    using namespace std::string_literals;
    if (!v->IsArray()) return false;
    auto v_as_array = v->GetArray();

    for (auto &el : helper::json::array_iterator(v_as_array)) {
      if (!el.IsNumber()) return false;
    }

    return v_as_array.Size() > 0;
  }

 public:
  bool acceptable(entry::Column *dfield, Value *v) const {
    if (!dfield) return false;
    if (dfield->type != entry::ColumnType::VECTOR) return false;
    return v->IsString() || is_vec_json(v);
  }
  mysqlrouter::sqlstring to_sqlstring(entry::Column *, Value *v) const {
    if (v->IsString())
      return mysqlrouter::sqlstring("STRING_TO_VECTOR(?)") << v->GetString();

    return mysqlrouter::sqlstring("STRING_TO_VECTOR(?)")
           << helper::json::to_string(v);
  }
};

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
  bool acceptable(entry::Column *dfield, Value *v) const {
    if (!dfield) return false;
    if (dfield->type != entry::ColumnType::GEOMETRY) return false;
    return v->IsString() || is_geo_json(v);
  }
  mysqlrouter::sqlstring to_sqlstring(entry::Column *dfield, Value *v) const {
    if (v->IsString())
      return mysqlrouter::sqlstring("ST_GeomFromText(?, ?)")
             << v->GetString() << dfield->srid;

    return mysqlrouter::sqlstring("ST_GeomFromGeoJSON(?,1,?)")
           << helper::json::to_string(v) << dfield->srid;
  }
};

class tosString {
 public:
  bool acceptable(entry::Column *, Value *v) const { return v->IsString(); }
  mysqlrouter::sqlstring to_sqlstring(entry::Column *col, Value *v) const {
    if (col && col->type == entry::ColumnType::BINARY)
      return mysqlrouter::sqlstring("FROM_BASE64(?)") << v->GetString();
    else
      return mysqlrouter::sqlstring("?") << v->GetString();
  }
};

class tosNumber {
 public:
  bool acceptable(entry::Column *, Value *v) const { return v->IsNumber(); }
  mysqlrouter::sqlstring to_sqlstring(entry::Column *, Value *v) const {
    return mysqlrouter::sqlstring(helper::json::to_string(v).c_str());
  }
};

class tosBoolean {
 public:
  bool acceptable(entry::Column *df, Value *) const {
    if (df && df->type == entry::ColumnType::BOOLEAN) {
      return true;
    }

    return false;
  }

  mysqlrouter::sqlstring to_sqlstring(entry::Column *, Value *v) const {
    if (v->IsBool()) {
      if (v->GetBool()) return {"TRUE"};
      return {"FALSE"};
    }
    return mysqlrouter::sqlstring(helper::json::to_string(v).c_str());
  }
};

class tosDateAsString {
 public:
  bool acceptable(entry::Column *df, Value *v) const {
    if (df &&
        is_date_type(helper::from_mysql_txt_column_type(df->datatype.c_str())
                         .type_mysql)) {
      return v->IsString();
    }

    return false;
  }

  mysqlrouter::sqlstring to_sqlstring(entry::Column *, Value *v) const {
    return mysqlrouter::sqlstring(helper::json::to_string(v).c_str());
  }
};

class tosDate {
 public:
  const char *k_date{"$date"};
  bool acceptable(entry::Column *, Value *v) const {
    if (!v->IsObject()) return false;

    auto it = v->FindMember(k_date);
    if (it == v->MemberEnd()) return false;

    // TODO(lkotula): Parse string for date ! (Shouldn't be in review)
    return it->value.IsString();
  }

  mysqlrouter::sqlstring to_sqlstring(entry::Column *, Value *v) const {
    auto o = v->GetObject();
    return mysqlrouter::sqlstring("?") << o[k_date].GetString();
  }
};

class tosNull {
 public:
  bool acceptable(entry::Column *, Value *v) const { return v->IsNull(); }

  mysqlrouter::sqlstring to_sqlstring(entry::Column *, Value *) const {
    return {"NULL"};
  }
};

class Result {
 public:
  explicit Result(entry::Column *dfield, Value *v) : dfield_{dfield}, v_{v} {}

  template <typename Z>
  Result &operator<<(const Z &t) {
    if (result.is_empty() && t.acceptable(dfield_, v_)) {
      result = t.to_sqlstring(dfield_, v_);
    }

    return *this;
  }

  mysqlrouter::sqlstring result;
  entry::Column *dfield_;
  Value *v_;
};

template <typename... T>
mysqlrouter::sqlstring to_sqlstring(entry::Column *dfield, Value *value) {
  Result r(dfield, value);
  (r << ... << T());

  if (r.result.is_empty())
    throw RestError("Not supported type used in `FilterObject`.");

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
  if (has_where()) tmp.append_preformatted(where_);

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
    log_debug2("Resetting where");
    where_.reset("");
  }
  if (clear & Clear::kOrder) {
    log_debug2("Resetting order");
    order_.reset("");
  }
  if (clear & Clear::kAsof) {
    log_debug2("Resetting asof");
    asof_gtid_.reset("");
  }
}

void FilterObjectGenerator::parse(const Document &doc) {
  reset();

  if (doc.HasParseError())
    throw RestError("Value used for `FilterObject` is not JSON.");
  if (doc.IsNull()) return;
  if (!doc.IsObject()) throw RestError("`FilterObject` must be a json object.");

  parse_orderby_asof_wmember(doc.GetObject());
}

void FilterObjectGenerator::parse(const std::string &filter_query) {
  log_debug2("FilterObjectGenerator::parse(filter_query=%s)",
             filter_query.c_str());
  if (filter_query.empty()) return;

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
      if (!where_.is_empty()) where_.append_preformatted(" AND ");
      const bool result = parse_wmember(member.first, member.second);
      if (!result) {
        throw RestError("Invalid `FilterObject`");
      }
    }
  }
}

/*
 * complexValue
 *  1) simpleOperatorObject
 *  2) complexOperatorObject
 *  3) columnObject
 */
std::optional<mysqlrouter::sqlstring>
FilterObjectGenerator::parse_complex_value(const std::string_view &column_name,
                                           Value *value) {
  log_debug2("parse_complex_value %s", column_name.data());
  if (!value->IsObject()) return {};
  if (value->MemberCount() != 1) return {};

  auto name = value->MemberBegin()->name.GetString();
  Value *child = &value->MemberBegin()->value;

  // 1) simpleOperatorObject
  auto result = parse_simple_operator_object(column_name, value);
  if (result) return result;

  // 2) complexOperatorObject
  result = parse_complex_operator_object(column_name, child, name);
  if (result) return result;

  // 3) columnObject
  return parse_column_object(name, child);
}

/*
 * complexOperatorProperty
 *  1) complexKey : [complexValues]
 *  2) complexKey : simpleOperatorObject
 */
std::optional<mysqlrouter::sqlstring>
FilterObjectGenerator::parse_complex_operator_object(
    const std::string_view &column_name, Value *value,
    const std::string_view &complex_key) {
  log_debug2("parse_complex_operator_object, column=%s, operator=%s",
             column_name.data(), complex_key.data());
  if ("$or"s == complex_key || "$and"s == complex_key) {
    // 1) complexKey : [complexValues]
    auto result = parse_complex_values(column_name, value, complex_key);
    if (result) return result;

    // 2) complexKey : simpleOperatorObject
    return parse_simple_operator_object(column_name, value);
  } else if ("$match"s == complex_key) {
    // this is our extension to the grammar
    return parse_match(value);
  }

  return {};
}

std::optional<mysqlrouter::sqlstring>
FilterObjectGenerator::parse_simple_operator_object(
    const std::string_view &column_name, Value *object) {
  log_debug2("parse_simple_operator_object %s", column_name.data());
  if (column_name.empty()) return {};
  if (!object->IsObject() || (object->MemberCount() != 1)) return {};

  auto [table, dfield] = resolve_field(column_name);
  auto db_name = resolve_field_name(table, dfield, column_name, false);

  mysqlrouter::sqlstring result;
  auto name = object->MemberBegin()->name.GetString();
  Value *value = &object->MemberBegin()->value;
  log_debug2("dispatched type %i", static_cast<int>(value->GetType()));

  if ("$eq"s == name) {
    log_debug2("parse_simple_operator_object $eq");
    result.append_preformatted(db_name)
        .append_preformatted(" = ")
        .append_preformatted(
            to_sqlstring<tosVec, tosGeom, tosString, tosBoolean, tosNumber,
                         tosDate>(dfield.get(), value));
  } else if ("$ne"s == name) {
    log_debug2("parse_simple_operator_object $ne");
    result.append_preformatted(db_name)
        .append_preformatted(" <> ")
        .append_preformatted(
            to_sqlstring<tosVec, tosGeom, tosString, tosBoolean, tosNumber,
                         tosDate>(dfield.get(), value));
  } else if ("$lt"s == name) {
    log_debug2("parse_simple_operator_object $lt");
    result.append_preformatted(db_name)
        .append_preformatted(" < ")
        .append_preformatted(to_sqlstring<tosNumber, tosDate, tosDateAsString>(
            dfield.get(), value));
  } else if ("$lte"s == name) {
    log_debug2("parse_simple_operator_object $lte");
    result.append_preformatted(db_name)
        .append_preformatted(" <= ")
        .append_preformatted(to_sqlstring<tosNumber, tosDate, tosDateAsString>(
            dfield.get(), value));
  } else if ("$gt"s == name) {
    log_debug2("parse_simple_operator_object $gt");
    result.append_preformatted(db_name)
        .append_preformatted(" > ")
        .append_preformatted(to_sqlstring<tosNumber, tosDate, tosDateAsString>(
            dfield.get(), value));
  } else if ("$gte"s == name) {
    log_debug2("parse_simple_operator_object $gte");
    result.append_preformatted(db_name)
        .append_preformatted(" >= ")
        .append_preformatted(to_sqlstring<tosNumber, tosDate, tosDateAsString>(
            dfield.get(), value));
  } else if ("$instr"s == name) {
    log_debug2("parse_simple_operator_object $instr");
    result.append_preformatted("instr(")
        .append_preformatted(db_name)
        .append_preformatted(", ")
        .append_preformatted(to_sqlstring<tosString>(dfield.get(), value))
        .append_preformatted(")");
  } else if ("$ninstr"s == name) {
    log_debug2("parse_simple_operator_object $not instr");
    result.append_preformatted("not instr(")
        .append_preformatted(db_name)
        .append_preformatted(", ")
        .append_preformatted(to_sqlstring<tosString>(dfield.get(), value))
        .append_preformatted(")");
  } else if ("$like"s == name) {
    log_debug2("parse_simple_operator_object $like");
    result.append_preformatted(db_name)
        .append_preformatted(" like ")
        .append_preformatted(to_sqlstring<tosString>(dfield.get(), value));
  } else if ("$null"s == name) {
    log_debug2("parse_simple_operator_object $null");
    if (!value->IsNull()) {
      throw RestError(
          "Operator '$null' in Filter object accepts only null value.");
    }
    result.append_preformatted(db_name).append_preformatted(" IS NULL");
  } else if ("$notnull"s == name) {
    log_debug2("parse_simple_operator_object $notnull");
    if (!value->IsNull()) {
      throw RestError(
          "Operator '$notnull' in Filter object accepts only null value.");
    }

    result.append_preformatted(db_name).append_preformatted(" IS NOT NULL");
  } else if ("$between"s == name) {
    log_debug2("parse_simple_operator_object $between");
    if (!value->IsArray())
      throw RestError("Between operator, requires an array field.");
    if (value->Size() != 2)
      throw RestError("Between field, requires array with size of two.");
    result.append_preformatted(db_name)
        .append_preformatted(" BETWEEN ")
        .append_preformatted(
            to_sqlstring<tosString, tosNumber, tosDate, tosNull>(dfield.get(),
                                                                 &(*value)[0]))
        .append_preformatted(" AND ")
        .append_preformatted(
            to_sqlstring<tosString, tosNumber, tosDate, tosNull>(dfield.get(),
                                                                 &(*value)[1]));
  } else {
    return {};
  }

  return result;
}

std::optional<mysqlrouter::sqlstring> FilterObjectGenerator::parse_match(
    Value *value) {
  log_debug2("parse_complex_match");
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

  return v;
}

/*
 * columnProperty
 *   columnName : string
 *   columnName : number
 *   columnName : date
 *   columnName : <other types>
 */
std::optional<mysqlrouter::sqlstring> FilterObjectGenerator::parse_direct_value(
    const std::string_view &column_name, Value *value) {
  log_debug2("parse_direct_value %s", column_name.data());

  auto [table, dfield] = resolve_field(column_name);
  mysqlrouter::sqlstring dbname =
      resolve_field_name(table, dfield, column_name, false);

  mysqlrouter::sqlstring result;

  try {
    result.append_preformatted(
        mysqlrouter::sqlstring("!=?")
        << dbname
        << to_sqlstring<tosVec, tosGeom, tosString, tosBoolean, tosNumber,
                        tosDate>(dfield.get(), value));
  } catch (const RestError &) {
    // If it is an object we try the other matchers so don't throw, just leave.
    // According to the grammar we could just leave at the beginning of this
    // function as an object should not be a fit for direct value, but we
    // additionally support a GEO datatype which can be represented as an object
    // in the filter JSON.
    // Same is true for an array, we could also skip it at the beginning but
    // here we use it to support Vec datatype matching.
    if (value->IsObject() || value->IsArray()) {
      return {};
    }
    throw;
  }

  return result;
}

/*
 * complexValues
 *   complexValue , complexValues
 */
std::optional<mysqlrouter::sqlstring>
FilterObjectGenerator::parse_complex_values(
    const std::string_view &column_name, Value *value,
    const std::string_view &complex_key) {
  log_debug2("parse_complex_values %s", column_name.data());
  assert(complex_key == "$and" || complex_key == "$or");

  const std::string sql_operator = complex_key == "$and" ? "AND" : "OR";
  if (!value->IsArray()) {
    return {};
  }

  const auto arr = value->GetArray();
  if (arr.Size() == 0) {
    throw RestError("parse_complex_values: array can't be empty");
  }

  mysqlrouter::sqlstring output;
  bool first = true;
  for (auto &el : helper::json::array_iterator(arr)) {
    auto result = parse_complex_value(column_name, &el);
    if (!result) {
      throw RestError("parse_complex_values: failed to parse complex_value");
    }

    if (!first) {
      output.append_preformatted(" ");
      output.append_preformatted(sql_operator.c_str());
      output.append_preformatted(" ");
    } else {
      first = false;
    }

    output.append_preformatted("(");
    output.append_preformatted(*result);
    output.append_preformatted(")");
  }

  return output;
}

namespace {
/**
 * columnName
 *  "\p{Alpha}[[\p{Alpha}]]([[\p{Alnum}]#$_])*$"
 */
bool is_valid_column_name(const std::string_view &str) {
  if (str.empty()) return false;
  // Check if the first character is alphabetic
  if (!std::isalpha(str[0])) return false;

  // Check remaining characters: should be alphanumeric, '#', '$', or '_'
  for (size_t i = 1; i < str.size(); ++i) {
    const auto &ch = str[i];
    if (!std::isalnum(ch) && ch != '#' && ch != '$' && ch != '_') {
      return false;
    }
  }

  return true;
}
}  // namespace

/*
 * columnProperty
 *   1) columnName : string OR number OR date OR geometry OR vector ...
 *   2) columnName : simpleOperatorObject
 *   3) columnName : complexOperatorObject
 *   4) columnName : [complexValues]
 */
std::optional<mysqlrouter::sqlstring>
FilterObjectGenerator::parse_column_object(const std::string_view &column_name,
                                           Value *value) {
  log_debug2("parse_column_object %s", column_name.data());
  if (!is_valid_column_name(column_name)) return {};

  // 1) columnName : simple type
  auto result = parse_direct_value(column_name, value);
  if (result) return result;

  // 2) columnName : simpleOperatorObject
  result = parse_simple_operator_object(column_name, value);
  if (result) return result;

  // 3) columnName : complexOperatorObject
  if ((value->IsObject()) && (value->MemberCount() == 1)) {
    auto name = value->MemberBegin()->name.GetString();
    Value *child = &value->MemberBegin()->value;
    result = parse_complex_operator_object(column_name, child, name);
    if (result) return result;
  }

  // 4) columnName : [complexValues]
  return parse_complex_values(column_name, value, "$and");
}

mysqlrouter::sqlstring FilterObjectGenerator::get_asof() const {
  return asof_gtid_;
}

bool FilterObjectGenerator::has_where(bool filter_only) const {
  if (!filter_only && has_asof() && use_wait_in_where_) {
    return true;
  }
  return !where_.is_empty();
}

bool FilterObjectGenerator::has_order() const { return !order_.is_empty(); }

bool FilterObjectGenerator::has_asof() const { return !asof_gtid_.is_empty(); }

/*
 * wpair
 *   1) columnProperty
 *   2) complexOperatorProperty
 */
bool FilterObjectGenerator::parse_wmember(const std::string_view &name,
                                          Value *value) {
  log_debug2("parse_wmember %s", name.data());
  // 1) columnProperty
  auto result = parse_column_object(name, value);

  // 2) complexOperatorProperty
  if (!result) result = parse_complex_operator_object("", value, name);

  if (result) {
    where_.append_preformatted("(")
        .append_preformatted(*result)
        .append_preformatted(")");
    return true;
  }

  log_debug2("parse_wmember: no match!");
  return false;
}

void FilterObjectGenerator::parse_asof(Value *value) {
  log_debug2("Parser asof");
  if (!value->IsString())
    throw RestError("Wrong value for `asof`, requires string with GTID.");
  asof_gtid_.reset("?");
  asof_gtid_ << value->GetString();
}

void FilterObjectGenerator::parse_order(Object object) {
  log_debug2("Parser Order");
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
    auto [table, dfield] = resolve_field(field_name);
    order_.append_preformatted(
        resolve_field_name(table, dfield, field_name, true));

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

std::pair<std::shared_ptr<entry::Table>, std::shared_ptr<entry::Column>>
FilterObjectGenerator::resolve_field(const std::string_view &name) {
  if (!object_metadata_) return {nullptr, nullptr};

  auto field = object_metadata_->get_field(name);
  return {object_metadata_, std::dynamic_pointer_cast<entry::Column>(field)};
}

mysqlrouter::sqlstring FilterObjectGenerator::resolve_field_name(
    const std::shared_ptr<entry::Table> &table,
    const std::shared_ptr<entry::Column> &dfield, const std::string_view &name,
    bool for_sorting) const {
  if (!object_metadata_) return mysqlrouter::sqlstring("!") << name.data();

  if (dfield) {
    if (!dfield->allow_filtering && !for_sorting && !dfield->is_primary)
      throw RestError("Cannot filter on field "s + name.data());
    if (!dfield->allow_sorting && for_sorting && !dfield->is_primary)
      throw RestError("Cannot sort on field "s + name.data());

    if (joins_allowed_)
      return mysqlrouter::sqlstring("!.!")
             << table->table_alias << dfield->column_name;
    else
      return mysqlrouter::sqlstring("!") << dfield->column_name;
  }
  // TODO(alfredo) filter on nested fields
  if (!for_sorting)
    throw RestError("Cannot filter on field "s + name.data());
  else
    throw RestError("Cannot sort on field "s + name.data());
}

}  // namespace database
}  // namespace mrs
