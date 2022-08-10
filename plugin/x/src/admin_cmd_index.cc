/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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
#include <cstring>
#include <memory>

#include "plugin/x/src/helper/generate_hash.h"
#include "plugin/x/src/index_array_field.h"
#include "plugin/x/src/index_field.h"
#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/session.h"
#include "plugin/x/src/sql_data_result.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_log.h"

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
  Sql_data_result result(&m_session->data_context());
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
    result.skip().get(&create_stmt);
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
             ? Index_type_id::k_unsupported
             : static_cast<Index_type_id>(i - INDEX_TYPE.begin());
}

std::string Admin_command_index::get_default_field_type(
    const Index_type_id id, const bool is_array) const {
  switch (id) {
    case Index_type_id::k_index:
      return is_array ? "CHAR(64)" : "TEXT(64)";
    case Index_type_id::k_spatial:
      return "GEOJSON";
    case Index_type_id::k_fulltext:
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
 * - type: string, optional - name of index's type
 *   {"INDEX"|"SPATIAL"|"FULLTEXT"}
 * - with_parser: string, optional - name of parser for fulltext index
 * - fields|constraint: object, list - detailed information for the generated
 *   column
 *   - field|member: string - path to document member for which the index
 *     will be created
 *   - required: bool, optional - whether the generated column will be created
 *     as NOT NULL
 *   - type: string, optional - data type of the indexed values
 *   - options: int, optional - parameter for generation spatial column
 *   - srid: int, optional - parameter for generation spatial column
 *   - array: bool, optional - indexed field is an array of scalars
 *
 * VARCHAR and CHAR are not indexable because:
 * - varchar column needs to be created with a length, which would limit
 *   documents to have that field smaller than that
 * - if we use left() to truncate the value of the column, then the index won't
 *   be usable unless queries also specify left(), which is not desired.
 */

ngs::Error_code Admin_command_index::create(Command_arguments *args) {
  std::string schema;
  std::string collection;
  std::string index_name;
  std::string index_type{"INDEX"};
  std::string parser;
  bool is_unique = false;
  std::vector<Command_arguments *> constraints;

  ngs::Error_code error =
      args->string_arg({"schema"}, &schema, Argument_appearance::k_obligatory)
          .string_arg({"collection"}, &collection,
                      Argument_appearance::k_obligatory)
          .string_arg({"name"}, &index_name, Argument_appearance::k_obligatory)
          .bool_arg({"unique"}, &is_unique, Argument_appearance::k_obligatory)
          .string_arg({"type"}, &index_type, Argument_appearance::k_optional)
          .string_arg({"with_parser"}, &parser, Argument_appearance::k_optional)
          .object_list({"fields", "constraint"}, &constraints,
                       Argument_appearance::k_obligatory)
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
  if (type_id == Index_type_id::k_unsupported)
    return ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                      "Argument value '%s' for index type is invalid",
                      index_type.c_str());

  if (is_unique) {
    if (type_id == Index_type_id::k_spatial)
      return ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                        "Unique spatial index is not supported");
    if (type_id == Index_type_id::k_fulltext)
      return ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                        "Unique fulltext index is not supported");
  }
  if (!parser.empty() && type_id != Index_type_id::k_fulltext)
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

  using Fields = std::vector<std::unique_ptr<const Index_field_interface>>;
  Fields fields;
  for (auto c : constraints) {
    fields.emplace_back(create_field(virtual_supported, type_id, c, &error));
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
    error = f->add_column_if_necessary(&m_session->data_context(), schema,
                                       collection, &qb);
    if (error) return error;
  }

  qb.put(" ADD ");
  if (is_unique) qb.put("UNIQUE ");
  if (type_id == Index_type_id::k_spatial) qb.put("SPATIAL ");
  if (type_id == Index_type_id::k_fulltext) qb.put("FULLTEXT ");
  qb.put("INDEX ")
      .quote_identifier(index_name)
      .put(" (")
      .put_list(fields.begin(), fields.end(),
                std::mem_fn(&Index_field_interface::add_field))
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

#define INDEX_NAME_REGEX "^\\\\$ix_[[:alnum:]_]+[[:xdigit:]]+$"
#define INDEX_NAME_REGEX_NO_BACKSLASH_ESCAPES \
  "^\\$ix_[[:alnum:]_]+[[:xdigit:]]+$"

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
      .put(" AND column_name RLIKE '");

  if (m_session->data_context().is_sql_mode_set("NO_BACKSLASH_ESCAPES"))
    qb.put(INDEX_NAME_REGEX_NO_BACKSLASH_ESCAPES);
  else
    qb.put(INDEX_NAME_REGEX);

  qb.put("') GROUP BY column_name HAVING count = 1");

  Sql_data_result result(&m_session->data_context());
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
ngs::Error_code Admin_command_index::drop(Command_arguments *args) {
  Query_string_builder qb;
  std::string schema;
  std::string collection;
  std::string name;

  ngs::Error_code error =
      args->string_arg({"schema"}, &schema, Argument_appearance::k_obligatory)
          .string_arg({"collection"}, &collection,
                      Argument_appearance::k_obligatory)
          .string_arg({"name"}, &name, Argument_appearance::k_obligatory)
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

const Admin_command_index::Index_field_interface *
Admin_command_index::create_field(const bool is_virtual_allowed,
                                  const Index_type_id &index_type,
                                  Command_arguments *constraint,
                                  ngs::Error_code *error) const {
  Index_field_info info;
  bool is_array{false};
  *error =
      constraint
          ->docpath_arg({"field", "member"}, &info.m_path,
                        Argument_appearance::k_obligatory)
          .string_arg({"type"}, &info.m_type, Argument_appearance::k_optional)
          .bool_arg({"required"}, &info.m_is_required,
                    Argument_appearance::k_optional)
          .uint_arg({"options"}, &info.m_options,
                    Argument_appearance::k_optional)
          .uint_arg({"srid"}, &info.m_srid, Argument_appearance::k_optional)
          .bool_arg({"array"}, &is_array, Argument_appearance::k_optional)
          .error();
  if (*error) return nullptr;

  if (info.m_type.empty())
    info.m_type = get_default_field_type(index_type, is_array);

  if (is_array) return Index_array_field::create(info, error);
  return Index_field::create(is_virtual_allowed, info, error);
}

}  // namespace xpl
