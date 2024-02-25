/*
 * Copyright (c) 2018, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/index_field.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "plugin/x/src/helper/generate_hash.h"
#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_regex.h"
#include "plugin/x/src/xpl_resultset.h"

namespace xpl {

namespace {

std::string get_prefix(const char *const prefix, const int32_t precision,
                       const int32_t scale, const bool is_unsigned,
                       const bool is_required) {
  // type
  std::string result{"$ix_"};
  result += prefix;
  if (precision > 0) result += std::to_string(precision);
  if (scale > 0) result += "_" + std::to_string(scale);
  result += "_";

  // additional traits (unsigned, required, ...)
  std::string traits;
  if (is_unsigned) traits += "u";
  if (is_required) traits += "r";

  return traits.empty() ? result : result + traits + "_";
}

std::string docpath_hash(const std::string &path) {
  return generate_hash(path.size() > 2 ? path.substr(2)  // skip '$.'
                                       : path);          // hash for '$'
}

void extract_type_details(const std::string &type_name, int32_t *precision,
                          int32_t *scale, bool *is_unsigned) {
  static const Regex re(
      "\\w+(?:\\(([0-9]+)(?: *, *([0-9]+))?\\))?( +UNSIGNED)?.*");
  Regex::Group_list groups;
  if (!re.match_groups(type_name.c_str(), &groups, false) || groups.size() < 4)
    return;
  *precision = groups[1].empty() ? -1 : std::stoi(groups[1]);
  *scale = groups[2].empty() ? -1 : std::stoi(groups[2]);
  *is_unsigned = !groups[3].empty();
}

std::string get_virtual_column_name(const char *prefix,
                                    const std::string &type_name,
                                    const std::string &path,
                                    const bool is_required) {
  bool is_unsigned{false};
  int32_t precision{-1}, scale{-1};
  extract_type_details(type_name, &precision, &scale, &is_unsigned);
  return get_prefix(prefix, precision, scale, is_unsigned, is_required) +
         docpath_hash(path);
}

}  // namespace

ngs::Error_code Index_field::add_column_if_necessary(
    iface::Sql_session *sql_session, const std::string &schema,
    const std::string &collection, Query_string_builder *qb) const {
  ngs::Error_code error;
  const bool is_field_exists =
      is_column_exists(sql_session, schema, collection, &error);
  if (error) return error;
  if (!is_field_exists) {
    add_column(qb);
    qb->put(",");
  }
  return ngs::Success();
}

bool Index_field::is_column_exists(iface::Sql_session *sql_session,
                                   const std::string &schema_name,
                                   const std::string &table_name,
                                   ngs::Error_code *error) const {
  Query_string_builder qb;
  qb.put("SHOW COLUMNS FROM ")
      .quote_identifier(schema_name)
      .dot()
      .quote_identifier(table_name)
      .put(" WHERE Field = ")
      .quote_string(m_name);

  Collect_resultset resultset;
  *error = sql_session->execute(qb.get().data(), qb.get().length(), &resultset);
  return resultset.get_row_list().size() > 0;
}

void Index_field::add_column(Query_string_builder *qb) const {
  qb->put(" ADD COLUMN ").quote_identifier(m_name).put(" ");
  add_type(qb);
  qb->put(" GENERATED ALWAYS AS (");
  add_path(qb);
  qb->put(") ");
  add_options(qb);
}

void Index_field::add_field(Query_string_builder *qb) const {
  qb->quote_identifier(m_name);
  add_length(qb);
}

void Index_field::add_options(Query_string_builder *qb) const {
  qb->put(m_is_virtual_allowed ? "VIRTUAL" : "STORED");
  if (m_is_required) qb->put(" NOT NULL");
}

Index_field::Type_id Index_field::get_type_id(const std::string &type_name) {
  static const std::array<const char *const, 22> VALID_TYPES{
      {"TINYINT", "SMALLINT",  "MEDIUMINT", "INT",     "INTEGER", "BIGINT",
       "REAL",    "FLOAT",     "DOUBLE",    "DECIMAL", "NUMERIC", "DATE",
       "TIME",    "TIMESTAMP", "DATETIME",  "YEAR",    "BIT",     "BLOB",
       "TEXT",    "GEOJSON",   "FULLTEXT",  "CHAR"}};
  std::string name(type_name);
  std::transform(name.begin(), name.end(), name.begin(), ::toupper);
  auto i = std::find_if(VALID_TYPES.begin(), VALID_TYPES.end(),
                        [&name](const char *const arg) {
                          return std::strcmp(name.c_str(), arg) == 0;
                        });
  return i == VALID_TYPES.end()
             ? Type_id::k_unsupported
             : static_cast<Type_id>(std::distance(VALID_TYPES.begin(), i));
}

///////////////////////////////////

class Index_numeric_field : public Index_field {
 public:
  Index_numeric_field(const char *const prefix, const std::string &type_name,
                      const std::string &path, const bool is_required,
                      const bool is_virtual_allowed)
      : Index_field(
            path, is_required,
            get_virtual_column_name(prefix, type_name, path, is_required),
            is_virtual_allowed),
        m_type_name(type_name) {}

 protected:
  void add_type(Query_string_builder *qb) const override {
    qb->put(m_type_name);
  }

  void add_path(Query_string_builder *qb) const override {
    qb->put("JSON_EXTRACT(doc, ").quote_string(m_path).put(")");
  }

  const std::string m_type_name;
};

class Index_string_field : public Index_field {
 public:
  Index_string_field(const char *const prefix, const std::string &type_name,
                     const std::string &path, const bool is_required,
                     const bool is_virtual_allowed)
      : Index_field(
            path, is_required,
            get_virtual_column_name(prefix, type_name, path, is_required),
            is_virtual_allowed),
        m_type_name(type_name) {}

 protected:
  void add_type(Query_string_builder *qb) const override {
    qb->put(m_type_name);
  }

  void add_path(Query_string_builder *qb) const override {
    qb->put("JSON_UNQUOTE(JSON_EXTRACT(doc, ").quote_string(m_path).put("))");
  }

  const std::string m_type_name;
};

class Index_binary_field : public Index_string_field {
 public:
  Index_binary_field(const char *const prefix, const std::string &type_name,
                     const std::string &length, const std::string &path,
                     const bool is_required, const bool is_virtual_allowed)
      : Index_string_field(prefix, type_name, path, is_required,
                           is_virtual_allowed),
        m_length(length) {}

 protected:
  void add_length(Query_string_builder *qb) const override {
    qb->put(m_length);
  }
  const std::string m_length;
};

class Index_text_field : public Index_binary_field {
 public:
  Index_text_field(const char *const prefix, const std::string &type_name,
                   const std::string &length, const std::string &path,
                   const bool is_required, const bool is_virtual_allowed)
      : Index_binary_field(prefix, type_name, length, path, is_required,
                           is_virtual_allowed) {}

 protected:
  void add_type(Query_string_builder *qb) const override {
    std::string type_name(m_type_name);
    auto pos = type_name.find(m_length);
    type_name.erase(pos, m_length.size());
    qb->put(type_name);
  }
};

class Index_geojson_field : public Index_field {
 public:
  Index_geojson_field(const int64_t options, const int64_t srid,
                      const std::string &path, const bool is_required)
      : Index_field(
            path, is_required,
            get_prefix("gj", -1, -1, false, is_required) + docpath_hash(path),
            false),
        m_options(options),
        m_srid(srid) {}

 protected:
  void add_type(Query_string_builder *qb) const override {
    qb->put("GEOMETRY");
  }

  void add_path(Query_string_builder *qb) const override {
    qb->put("ST_GEOMFROMGEOJSON(JSON_EXTRACT(doc, ")
        .quote_string(m_path)
        .put("),")
        .put(m_options)
        .put(",")
        .put(m_srid)
        .put(")");
  }

  void add_options(Query_string_builder *qb) const override {
    Index_field::add_options(qb);
    qb->put(" SRID ").put(m_srid);
  }

  const int64_t m_options, m_srid;
};

class Index_fulltext_field : public Index_field {
 public:
  Index_fulltext_field(const std::string &path, const bool is_required)
      : Index_field(
            path, is_required,
            get_prefix("ft", -1, -1, false, is_required) + docpath_hash(path),
            false) {}

 protected:
  void add_type(Query_string_builder *qb) const override { qb->put("TEXT"); }

  void add_path(Query_string_builder *qb) const override {
    qb->put("JSON_UNQUOTE(JSON_EXTRACT(doc, ").quote_string(m_path).put("))");
  }
};

/////////////////////////////////////////////

namespace {

inline bool is_valid(const uint64_t arg) {
  return arg != std::numeric_limits<uint64_t>::max();
}

}  // namespace

const Index_field *Index_field::create(
    const bool is_virtual_allowed,
    const Admin_command_index::Index_field_info &info, ngs::Error_code *error) {
  if (info.m_path.empty()) {
    *error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                        "Argument value for document member is invalid");
    return nullptr;
  }

  static const Regex re{
      "(BIT)(?:\\([0-9]+\\))?|"
      "(TINYINT|SMALLINT|MEDIUMINT|INT|INTEGER|BIGINT)"
      "(?:\\([0-9]+\\))?(?: +UNSIGNED)?|"
      "(DECIMAL|FLOAT|DOUBLE|REAL|NUMERIC)"
      "(?:\\([0-9]+(?: *, *[0-9]+)?\\))?(?: +UNSIGNED)?|"
      "(DATE)|(TIME|TIMESTAMP|DATETIME)(?:\\([0-6]\\))?|(YEAR)(?:\\(4\\))?|"
      "(BLOB)(?:(\\([0-9]+\\)))?|"
      "(CHAR|TEXT)(?:(\\([0-9]+\\)))?"
      "(?: +(?:CHARACTER SET|CHARSET) +\\w+)?(?: +COLLATE +\\w+)?|"
      "(GEOJSON|FULLTEXT)",
  };

  Regex::Group_list re_groups;
  if (!re.match_groups(info.m_type.c_str(), &re_groups)) {
    *error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                        "Invalid or unsupported type specification '%s'",
                        info.m_type.c_str());
    return nullptr;
  }

  const std::string &type_name = re_groups[1];
  const std::string &length =
      re_groups.size() > 2 ? re_groups[2] : std::string();

  auto type_id = get_type_id(type_name);

  if (type_id != Type_id::k_geojson &&
      (is_valid(info.m_options) || is_valid(info.m_srid))) {
    *error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                        "Unsupported argument specification for '%s'",
                        info.m_path.c_str());
    return nullptr;
  }

  switch (type_id) {
    case Type_id::k_tinyint:
      return new Index_numeric_field("it", info.m_type, info.m_path,
                                     info.m_is_required, is_virtual_allowed);

    case Type_id::k_smallint:
      return new Index_numeric_field("is", info.m_type, info.m_path,
                                     info.m_is_required, is_virtual_allowed);

    case Type_id::k_mediumint:
      return new Index_numeric_field("im", info.m_type, info.m_path,
                                     info.m_is_required, is_virtual_allowed);

    case Type_id::k_int:
    case Type_id::k_integer:
      return new Index_numeric_field("i", info.m_type, info.m_path,
                                     info.m_is_required, is_virtual_allowed);

    case Type_id::k_bigint:
      return new Index_numeric_field("ib", info.m_type, info.m_path,
                                     info.m_is_required, is_virtual_allowed);

    case Type_id::k_real:
      return new Index_numeric_field("fr", info.m_type, info.m_path,
                                     info.m_is_required, is_virtual_allowed);

    case Type_id::k_float:
      return new Index_numeric_field("f", info.m_type, info.m_path,
                                     info.m_is_required, is_virtual_allowed);

    case Type_id::k_double:
      return new Index_numeric_field("fd", info.m_type, info.m_path,
                                     info.m_is_required, is_virtual_allowed);

    case Type_id::k_decimal:
      return new Index_numeric_field("xd", info.m_type, info.m_path,
                                     info.m_is_required, is_virtual_allowed);

    case Type_id::k_numeric:
      return new Index_numeric_field("xn", info.m_type, info.m_path,
                                     info.m_is_required, is_virtual_allowed);

    case Type_id::k_date:
      return new Index_string_field("d", info.m_type, info.m_path,
                                    info.m_is_required, is_virtual_allowed);

    case Type_id::k_time:
      return new Index_string_field("dt", info.m_type, info.m_path,
                                    info.m_is_required, is_virtual_allowed);

    case Type_id::k_timestamp:
      return new Index_string_field("ds", info.m_type, info.m_path,
                                    info.m_is_required, is_virtual_allowed);

    case Type_id::k_datetime:
      return new Index_string_field("dd", info.m_type, info.m_path,
                                    info.m_is_required, is_virtual_allowed);

    case Type_id::k_year:
      return new Index_string_field("dy", info.m_type, info.m_path,
                                    info.m_is_required, is_virtual_allowed);

    case Type_id::k_bit:
      return new Index_string_field("t", info.m_type, info.m_path,
                                    info.m_is_required, is_virtual_allowed);

    case Type_id::k_blob:
      return new Index_binary_field("bt", info.m_type, length, info.m_path,
                                    info.m_is_required, is_virtual_allowed);

    case Type_id::k_text:
      return new Index_text_field("t", info.m_type, length, info.m_path,
                                  info.m_is_required, is_virtual_allowed);

    case Type_id::k_geojson:
      return new Index_geojson_field(
          is_valid(info.m_options) ? info.m_options : 1,
          is_valid(info.m_srid) ? info.m_srid : 4326, info.m_path,
          info.m_is_required);

    case Type_id::k_fulltext:
      return new Index_fulltext_field(info.m_path, info.m_is_required);

    case Type_id::k_char:
      return new Index_string_field("c", info.m_type, info.m_path,
                                    info.m_is_required, is_virtual_allowed);

    case Type_id::k_unsupported:
      *error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                          "Invalid or unsupported type specification '%s'",
                          info.m_type.c_str());
      break;
  }
  return nullptr;
}

}  // namespace xpl
