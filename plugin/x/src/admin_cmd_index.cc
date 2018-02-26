/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/admin_cmd_index.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <utility>

#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/sql_data_result.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_log.h"
#include "plugin/x/src/xpl_session.h"
#include "sha1.h"

namespace xpl {

bool Admin_command_index::is_table_support_virtual_columns(
    const std::string &schema, const std::string &name,
    ngs::Error_code *error) const {
  Query_string_builder qb;
  qb.put("SHOW CREATE TABLE ")
      .quote_identifier(schema)
      .dot()
      .quote_identifier(name);

  std::string create_stmt;
  Sql_data_result result(m_session->data_context());
  try {
    result.query(qb.get());
    if (result.size() != 1) {
      log_error(
          ER_XPLUGIN_FAILED_TO_GET_CREATION_STMT,
          std::string(schema.empty() ? name : schema + "." + name).c_str(),
          static_cast<unsigned long>(result.size()));  // NOLINT(runtime/int)
      *error = ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
      return false;
    }
    result.skip().get(create_stmt);
  } catch (const ngs::Error_code &e) {
    log_debug(
        "Unable to get creation stmt for collection '%s';"
        " exception message: '%s'",
        std::string(schema.empty() ? name : schema + "." + name).c_str(),
        e.message.c_str());
    *error = e;
    return false;
  }

  static const char *const engine = "ENGINE=";
  std::string::size_type pos = create_stmt.find(engine);
  if (pos == std::string::npos) {
    log_error(ER_XPLUGIN_FAILED_TO_GET_ENGINE_INFO,
              std::string(schema.empty() ? name : schema + "." + name).c_str(),
              create_stmt.c_str());
    *error = ngs::Error(ER_INTERNAL_ERROR, "Error executing statement");
    return false;
  }

  // currently only InnoDB supports VIRTUAL GENERATED columns
  static const char *const innodb = "InnoDB";
  return create_stmt.substr(pos + strlen(engine), strlen(innodb)) == innodb;
}

Admin_command_index::Index_type_id Admin_command_index::get_type_id(
    const std::string &type_name) const {
  static const std::array<const char *const, 3> INDEX_TYPE{
      {"INDEX", "SPATIAL", "FULLTEXT"}};
  std::string name{type_name};
  std::transform(name.begin(), name.end(), name.begin(), ::toupper);
  auto i = std::find_if(INDEX_TYPE.begin(), INDEX_TYPE.end(),
                        [&name](const char *const arg) {
                          return std::strcmp(name.c_str(), arg) == 0;
                        });
  return i == INDEX_TYPE.end()
             ? Index_type_id::UNSUPPORTED
             : static_cast<Index_type_id>(i - INDEX_TYPE.begin());
}

std::string Admin_command_index::get_default_field_type(
    const Index_type_id id) const {
  switch (id) {
    case Index_type_id::INDEX:
      return "TEXT(64)";
    case Index_type_id::SPATIAL:
      return "GEOJSON";
    case Index_type_id::FULLTEXT:
      return "FULLTEXT";
    default:
      break;
  }
  return "TEXT(64)";
}

/* Stmt: create_collection_index
 * Required arguments:
 * - name: string - name of index
 * - collection: string - name of indexed collection
 * - schema: string - name of collection's schema
 * - unique: bool - whether the index should be a unique index
 * - type: string, optional - name of index's type  {"INDEX"|"SPATIAL"}
 * - fields|constraint: object, list - detailed information for the generated
 *   column
 *   - field|member: string - path to document member for which the index
 *     will be created
 *   - required: bool - whether the generated column will be created as NOT NULL
 *   - type: string - data type of the generated column
 *   - options: int, optional - parameter for generation spatial column
 *   - srid: int, optional - parameter for generation spatial column
 *
 * VARCHAR and CHAR are not indexable because:
 * - varchar column needs to be created with a length, which would limit
 *   documents to have that field smaller than that
 * - if we use left() to truncate the value of the column, then the index won't
 *   be usable unless queries also specify left(), which is not desired.
 */

ngs::Error_code Admin_command_index::create(const std::string &name_space,
                                            Command_arguments *args) {
  std::string schema;
  std::string collection;
  std::string index_name;
  std::string index_type{"INDEX"};
  std::string parser;
  bool is_unique = false;
  std::vector<Command_arguments *> constraints;

  ngs::Error_code error;
  if (name_space == Admin_command_handler::MYSQLX_NAMESPACE)
    error = args->string_arg({"schema"}, &schema)
                .string_arg({"collection"}, &collection)
                .string_arg({"name"}, &index_name)
                .bool_arg({"unique"}, &is_unique)
                .string_arg({"type"}, &index_type, true)
                .string_arg({"with_parser"}, &parser, true)
                .object_list({"fields", "constraint"}, &constraints)
                .error();
  else
    error = args->string_arg({"schema"}, &schema)
                .string_arg({"collection"}, &collection)
                .string_arg({"name"}, &index_name)
                .bool_arg({"unique"}, &is_unique)
                .object_list({"constraint"}, &constraints)
                .error();

  if (error) return error;

  if (schema.empty())
    return ngs::Error(ER_X_BAD_SCHEMA, "Invalid schema '%s'", schema.c_str());
  if (collection.empty())
    return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name '%s'",
                      collection.c_str());
  if (index_name.empty())
    return ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                      "Argument value '%s' for index name is invalid",
                      index_name.c_str());

  Index_type_id type_id = get_type_id(index_type);
  if (type_id == Index_type_id::UNSUPPORTED)
    return ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                      "Argument value '%s' for index type is invalid",
                      index_type.c_str());

  if (is_unique) {
    if (type_id == Index_type_id::SPATIAL)
      return ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                        "Unique spatial index is not supported");
    if (type_id == Index_type_id::FULLTEXT)
      return ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                        "Unique fulltext index is not supported");
  }
  if (!parser.empty() && type_id != Index_type_id::FULLTEXT)
    return ngs::Error(
        ER_X_CMD_ARGUMENT_VALUE,
        "'with_parser' argument is supported for fulltext index only");

  // check if the table's engine supports index on the virtual column
  const bool virtual_supported =
      is_table_support_virtual_columns(schema, collection, &error);
  if (error) {
    if (error.error == ER_INTERNAL_ERROR)
      return error;
    else
      // if it is not internal then the reason is bad schema or table name
      return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name: %s.%s",
                        schema.c_str(), collection.c_str());
  }

  using Fields = std::vector<std::unique_ptr<const Index_field>>;
  Fields fields;
  for (auto c : constraints) {
    fields.emplace_back(Index_field::create(name_space, virtual_supported,
                                            get_default_field_type(type_id), c,
                                            &error));
    if (error) return error;
  }
  error = args->end();
  if (error) return error;

  Query_string_builder qb;
  qb.put("ALTER TABLE ")
      .quote_identifier(schema)
      .dot()
      .quote_identifier(collection);

  for (const auto &f : fields) {
    const bool is_field_exists = f->is_column_exists(
        &m_session->data_context(), schema, collection, &error);
    if (error) return error;
    if (!is_field_exists) {
      f->add_column(&qb);
      qb.put(",");
    }
  }

  qb.put(" ADD ");
  if (is_unique) qb.put("UNIQUE ");
  if (type_id == Index_type_id::SPATIAL) qb.put("SPATIAL ");
  if (type_id == Index_type_id::FULLTEXT) qb.put("FULLTEXT ");
  qb.put("INDEX ")
      .quote_identifier(index_name)
      .put(" (")
      .put_list(fields.begin(), fields.end(),
                std::mem_fn(&Index_field::add_field))
      .put(")");

  if (!parser.empty()) qb.put(" WITH PARSER ").put(parser);

  log_debug("CreateCollectionIndex: %s", qb.get().c_str());
  Empty_resultset rset;
  error = m_session->data_context().execute(qb.get().data(), qb.get().length(),
                                            &rset);
  if (error) {
    switch (error.error) {
      case ER_BAD_NULL_ERROR: {
        // if we're creating a NOT NULL generated index/column and get a NULL
        // error, it's because one of the existing documents had a
        // NULL / unset value
        bool is_required{false};
        for (auto &f : fields) is_required = is_required || f->is_required();
        return is_required
                   ? ngs::Error(
                         ER_X_DOC_REQUIRED_FIELD_MISSING,
                         "Collection contains document missing required field")
                   : error;
      }
      case ER_INVALID_USE_OF_NULL:
        return ngs::Error(
            ER_X_DOC_REQUIRED_FIELD_MISSING,
            "Collection contains document missing required field");
      case ER_SPATIAL_CANT_HAVE_NULL:
        return ngs::Error(ER_X_DOC_REQUIRED_FIELD_MISSING,
                          "GEOJSON index requires 'constraint.required: TRUE");
    }
    return error;
  }
  m_session->proto().send_exec_ok();
  return ngs::Success();
}

ngs::Error_code Admin_command_index::get_index_generated_column_names(
    const std::string &schema, const std::string &collection,
    const std::string &index_name,
    std::vector<std::string> *column_names) const {
  Query_string_builder qb;
  qb.put(
        "SELECT column_name, COUNT(index_name) AS count"
        " FROM information_schema.statistics WHERE table_name=")
      .quote_string(collection)
      .put(" AND table_schema=")
      .quote_string(schema)
      .put(
          " AND column_name IN ("
          "SELECT BINARY column_name FROM information_schema.statistics"
          " WHERE table_name=")
      .quote_string(collection)
      .put(" AND table_schema=")
      .quote_string(schema)
      .put(" AND index_name=")
      .quote_string(index_name)
      .put(
          " AND column_name RLIKE '^\\\\$ix_[[:alnum:]_]+[[:xdigit:]]+$')"
          " GROUP BY column_name HAVING count = 1");

  Sql_data_result result(m_session->data_context());
  try {
    result.query(qb.get());
    if (result.size() == 0) return ngs::Success();
    column_names->reserve(result.size());
    do {
      column_names->push_back(result.get<std::string>());
    } while (result.next_row());
  } catch (const ngs::Error_code &e) {
    return e;
  }

  return ngs::Success();
}

/* Stmt: drop_collection_index
 * Required arguments:
 * - name: string - name of dropped index
 * - collection: string - name of collection with dropped index
 * - schema: string - name of collection's schema
 */
ngs::Error_code Admin_command_index::drop(const std::string & /*name_space*/,
                                          Command_arguments *args) {
  Query_string_builder qb;
  std::string schema;
  std::string collection;
  std::string name;

  ngs::Error_code error = args->string_arg({"schema"}, &schema)
                              .string_arg({"collection"}, &collection)
                              .string_arg({"name"}, &name)
                              .end();
  if (error) return error;

  if (schema.empty()) return ngs::Error_code(ER_X_BAD_SCHEMA, "Invalid schema");
  if (collection.empty())
    return ngs::Error_code(ER_X_BAD_TABLE, "Invalid collection name");
  if (name.empty())
    return ngs::Error_code(ER_X_MISSING_ARGUMENT, "Invalid index name");

  std::vector<std::string> column_names;
  error =
      get_index_generated_column_names(schema, collection, name, &column_names);
  if (error) return error;

  // drop the index
  qb.put("ALTER TABLE ")
      .quote_identifier(schema)
      .dot()
      .quote_identifier(collection)
      .put(" DROP INDEX ")
      .quote_identifier(name);

  for (const std::string &c : column_names)
    qb.put(", DROP COLUMN ").quote_identifier(c);

  const ngs::PFS_string &tmp(qb.get());
  log_debug("DropCollectionIndex: %s", tmp.c_str());
  Empty_resultset rset;
  error = m_session->data_context().execute(tmp.data(), tmp.length(), &rset);

  if (error) {
    switch (error.error) {
      case ER_BAD_DB_ERROR:
      case ER_NO_SUCH_TABLE:
        return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name: %s.%s",
                          schema.c_str(), collection.c_str());
      default:
        return error;
    }
  }

  m_session->proto().send_exec_ok();
  return ngs::Success();
}

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
  std::string hash;
  hash.resize(2 * SHA1_HASH_SIZE + 2);
  // just an arbitrary hash
  ::make_scrambled_password(&hash[0], path.size() > 2
                                          ? path.substr(2).c_str()  // skip '$.'
                                          : path.c_str());  // hash for '$'
  hash.resize(2 * SHA1_HASH_SIZE + 1);                      // strip the \0
  return hash.substr(1);                                    // skip the 1st char
}

bool parse_type(const std::string &source, std::string *name,
                int32_t *precision, int32_t *scale, bool *is_unsigned) {
  if (source.empty()) return false;

  std::string::const_iterator c = source.begin();
  for (; c != source.end() && isalpha(*c); ++c) name->push_back(toupper(*c));
  if (c != source.end()) {
    int consumed;
    if (sscanf(source.c_str() + (c - source.begin()), "(%i,%i)%n", precision,
               scale, &consumed) == 2)
      c += consumed;
    else if (sscanf(source.c_str() + (c - source.begin()), "(%i)%n", precision,
                    &consumed) == 1)
      c += consumed;
    // skip potential spaces
    while (c != source.end() && isspace(*c)) c++;

    std::string ident;
    for (; c != source.end() && isalpha(*c); ++c) ident.push_back(toupper(*c));

    *is_unsigned = false;
    if (ident == "UNSIGNED") {
      *is_unsigned = true;
    } else {
      if (!ident.empty()) return false;
    }

    if (c != source.end()) return false;
  }
  return true;
}

}  // namespace

bool Admin_command_index::Index_field::is_column_exists(
    ngs::Sql_session_interface *sql_session, const std::string &schema_name,
    const std::string &table_name, ngs::Error_code *error) const {
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

void Admin_command_index::Index_field::add_column(
    Query_string_builder *qb) const {
  qb->put(" ADD COLUMN ").quote_identifier(m_name).put(" ");
  add_type(qb);
  qb->put(" GENERATED ALWAYS AS (");
  add_path(qb);
  qb->put(") ");
  add_options(qb);
}

void Admin_command_index::Index_field::add_field(
    Query_string_builder *qb) const {
  qb->quote_identifier(m_name);
  add_length(qb);
}

void Admin_command_index::Index_field::add_options(
    Query_string_builder *qb) const {
  qb->put(m_is_virtual_allowed ? "VIRTUAL" : "STORED");
  if (m_is_required) qb->put(" NOT NULL");
}

Admin_command_index::Index_field::Field_type_id
Admin_command_index::Index_field::get_type_id(const std::string &type_name) {
  static const std::array<const char *const, 21> VALID_TYPES{
      {"TINYINT", "SMALLINT",  "MEDIUMINT", "INT",     "INTEGER", "BIGINT",
       "REAL",    "FLOAT",     "DOUBLE",    "DECIMAL", "NUMERIC", "DATE",
       "TIME",    "TIMESTAMP", "DATETIME",  "YEAR",    "BIT",     "BLOB",
       "TEXT",    "GEOJSON",   "FULLTEXT"}};
  auto i = std::find_if(VALID_TYPES.begin(), VALID_TYPES.end(),
                        [&type_name](const char *const arg) {
                          return std::strcmp(type_name.c_str(), arg) == 0;
                        });
  return i == VALID_TYPES.end()
             ? Field_type_id::UNSUPPORTED
             : static_cast<Field_type_id>(i - VALID_TYPES.begin());
}
///////////////////////////////////

class Index_numeric_field : public Admin_command_index::Index_field {
 public:
  Index_numeric_field(const char *const prefix, const std::string &type_name,
                      const int32_t precision, const int32_t scale,
                      const bool is_unsigned, const std::string &path,
                      const bool is_required, const bool is_virtual_allowed)
      : Index_field(
            path, is_required,
            get_prefix(prefix, precision, scale, is_unsigned, is_required) +
                docpath_hash(path),
            is_virtual_allowed),
        m_type_name(type_name),
        m_precision(precision),
        m_scale(scale),
        m_is_unsigned(is_unsigned) {}

 protected:
  void add_type(Query_string_builder *qb) const override {
    qb->put(m_type_name);
    if (m_precision > 0) {
      qb->put("(").put(m_precision);
      if (m_scale > 0) qb->put(", ").put(m_scale);
      qb->put(")");
    }
    if (m_is_unsigned) qb->put(" UNSIGNED");
  }

  void add_path(Query_string_builder *qb) const override {
    qb->put("JSON_EXTRACT(doc, ").quote_string(m_path).put(")");
  }

  const std::string m_type_name;
  const int32_t m_precision, m_scale;
  const bool m_is_unsigned;
};

class Index_string_field : public Admin_command_index::Index_field {
 public:
  Index_string_field(const char *const prefix, const std::string &type_name,
                     const int32_t precision, const std::string &path,
                     const bool is_required, const bool is_virtual_allowed)
      : Index_field(path, is_required,
                    get_prefix(prefix, precision, -1, false, is_required) +
                        docpath_hash(path),
                    is_virtual_allowed),
        m_type_name(type_name),
        m_precision(precision) {}

 protected:
  void add_type(Query_string_builder *qb) const override {
    qb->put(m_type_name);
    if (m_precision > 0) qb->put("(").put(m_precision).put(")");
  }

  void add_path(Query_string_builder *qb) const override {
    qb->put("JSON_UNQUOTE(JSON_EXTRACT(doc, ").quote_string(m_path).put("))");
  }

  const std::string m_type_name;
  const int32_t m_precision;
};

class Index_binary_field : public Index_string_field {
 public:
  Index_binary_field(const char *const prefix, const std::string &type_name,
                     const int32_t precision, const std::string &path,
                     const bool is_required, const bool is_virtual_allowed)
      : Index_string_field(prefix, type_name, precision, path, is_required,
                           is_virtual_allowed) {}

 protected:
  void add_type(Query_string_builder *qb) const override {
    qb->put(m_type_name);
    if (m_type_name != "TEXT" && m_precision > 0)
      qb->put("(").put(m_precision).put(")");
  }

  void add_length(Query_string_builder *qb) const override {
    if (m_precision > 0) qb->put("(").put(m_precision).put(")");
  }
};

class Index_geojson_field : public Admin_command_index::Index_field {
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

class Index_fulltext_field : public Admin_command_index::Index_field {
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
inline bool set_invalid_type_error(const bool flag, const std::string &type,
                                   ngs::Error_code *error) {
  if (!flag) return false;
  *error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                      "Invalid or unsupported type specification '%s'",
                      type.c_str());
  return true;
}

inline bool set_unsupported_argument_error(const bool flag,
                                           const std::string &path,
                                           ngs::Error_code *error) {
  if (!flag) return false;
  *error =
      ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                 "Unsupported argumet specification for '%s'", path.c_str());
  return true;
}

constexpr const uint64_t MAX_UINT64 = std::numeric_limits<uint64_t>::max();

}  // namespace

const Admin_command_index::Index_field *
Admin_command_index::Index_field::create(const std::string &name_space,
                                         const bool is_virtual_allowed,
                                         const std::string &default_type,
                                         Command_arguments *constraint,
                                         ngs::Error_code *error) {
  std::string path, type{default_type};
  uint64_t options{MAX_UINT64}, srid{MAX_UINT64};
  bool is_required{false};
  if (name_space == Admin_command_handler::MYSQLX_NAMESPACE)
    *error = constraint->docpath_arg({"field", "member"}, &path)
                 .string_arg({"type"}, &type, true)
                 .bool_arg({"required"}, &is_required)
                 .uint_arg({"options"}, &options, true)
                 .uint_arg({"srid"}, &srid, true)
                 .error();
  else
    *error = constraint->docpath_arg({"member"}, &path)
                 .string_arg({"type"}, &type, true)
                 .bool_arg({"required"}, &is_required)
                 .error();

  if (*error) return nullptr;
  if (path.empty()) {
    *error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                        "Argument value for document member is invalid");
    return nullptr;
  }

  std::string type_name;
  bool is_unsigned{false};
  int32_t precision{-1}, scale{-1};
  if (!parse_type(type, &type_name, &precision, &scale, &is_unsigned)) {
    *error = ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                        "Invalid or unsupported type specification '%s'",
                        type.c_str());
    return nullptr;
  }

  switch (get_type_id(type_name)) {
    case Field_type_id::TINYINT:
      if (set_invalid_type_error(scale > 0, type, error)) break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_numeric_field("it", type_name, precision, scale,
                                     is_unsigned, path, is_required,
                                     is_virtual_allowed);
    case Field_type_id::SMALLINT:
      if (set_invalid_type_error(scale > 0, type, error)) break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_numeric_field("is", type_name, precision, scale,
                                     is_unsigned, path, is_required,
                                     is_virtual_allowed);
    case Field_type_id::MEDIUMINT:
      if (set_invalid_type_error(scale > 0, type, error)) break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_numeric_field("im", type_name, precision, scale,
                                     is_unsigned, path, is_required,
                                     is_virtual_allowed);
    case Field_type_id::INT:
    case Field_type_id::INTEGER:
      if (set_invalid_type_error(scale > 0, type, error)) break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_numeric_field("i", type_name, precision, scale,
                                     is_unsigned, path, is_required,
                                     is_virtual_allowed);
    case Field_type_id::BIGINT:
      if (set_invalid_type_error(scale > 0, type, error)) break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_numeric_field("ib", type_name, precision, scale,
                                     is_unsigned, path, is_required,
                                     is_virtual_allowed);
    case Field_type_id::REAL:
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_numeric_field("fr", type_name, precision, scale,
                                     is_unsigned, path, is_required,
                                     is_virtual_allowed);
    case Field_type_id::FLOAT:
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_numeric_field("f", type_name, precision, scale,
                                     is_unsigned, path, is_required,
                                     is_virtual_allowed);
    case Field_type_id::DOUBLE:
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_numeric_field("fd", type_name, precision, scale,
                                     is_unsigned, path, is_required,
                                     is_virtual_allowed);
    case Field_type_id::DECIMAL:
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_numeric_field("xd", type_name, precision, scale,
                                     is_unsigned, path, is_required,
                                     is_virtual_allowed);
    case Field_type_id::NUMERIC:
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_numeric_field("xn", type_name, precision, scale,
                                     is_unsigned, path, is_required,
                                     is_virtual_allowed);
    case Field_type_id::DATE:
      if (set_invalid_type_error(precision > 0 || scale > 0 || is_unsigned,
                                 type, error))
        break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_string_field("d", type_name, precision, path,
                                    is_required, is_virtual_allowed);
    case Field_type_id::TIME:
      if (set_invalid_type_error(scale > 0 || is_unsigned, type, error)) break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_string_field("dt", type_name, precision, path,
                                    is_required, is_virtual_allowed);
    case Field_type_id::TIMESTAMP:
      if (set_invalid_type_error(scale > 0 || is_unsigned, type, error)) break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_string_field("ds", type_name, precision, path,
                                    is_required, is_virtual_allowed);
    case Field_type_id::DATETIME:
      if (set_invalid_type_error(scale > 0 || is_unsigned, type, error)) break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_string_field("dd", type_name, precision, path,
                                    is_required, is_virtual_allowed);
    case Field_type_id::YEAR:
      if (set_invalid_type_error(scale > 0 || is_unsigned, type, error)) break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_string_field("dy", type_name, precision, path,
                                    is_required, is_virtual_allowed);
    case Field_type_id::BIT:
      if (set_invalid_type_error(scale > 0 || is_unsigned, type, error)) break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_string_field("t", type_name, precision, path,
                                    is_required, is_virtual_allowed);
    case Field_type_id::BLOB:
      if (set_invalid_type_error(scale > 0 || is_unsigned, type, error)) break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_binary_field("bt", type_name, precision, path,
                                    is_required, is_virtual_allowed);
    case Field_type_id::TEXT:
      if (set_invalid_type_error(scale > 0 || is_unsigned, type, error)) break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_binary_field("t", type_name, precision, path,
                                    is_required, is_virtual_allowed);
    case Field_type_id::GEOJSON:
      if (set_invalid_type_error(precision > 0 || scale > 0 || is_unsigned,
                                 type, error))
        break;
      return new Index_geojson_field(options != MAX_UINT64 ? options : 1,
                                     srid != MAX_UINT64 ? srid : 4326, path,
                                     is_required);
    case Field_type_id::FULLTEXT:
      if (set_invalid_type_error(precision > 0 || scale > 0 || is_unsigned,
                                 type, error))
        break;
      if (set_unsupported_argument_error(
              options != MAX_UINT64 || srid != MAX_UINT64, path, error))
        break;
      return new Index_fulltext_field(path, is_required);

    case Field_type_id::UNSUPPORTED:
      set_invalid_type_error(true, type, error);
      break;
  }
  return nullptr;
}

}  // namespace xpl
