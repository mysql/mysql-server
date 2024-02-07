/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/admin_cmd_collection_handler.h"

#include <algorithm>
#include <vector>

#include "plugin/x/protocol/encoders/encoding_xrow.h"
#include "plugin/x/src/admin_cmd_arguments.h"
#include "plugin/x/src/get_detailed_validation_error.h"
#include "plugin/x/src/helper/generate_hash.h"
#include "plugin/x/src/helper/get_system_variable.h"
#include "plugin/x/src/helper/sql_commands.h"
#include "plugin/x/src/helper/string_case.h"
#include "plugin/x/src/meta_schema_validator.h"
#include "plugin/x/src/mysql_function_names.h"
#include "plugin/x/src/ngs/protocol/column_info_builder.h"
#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/sql_data_result.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

Admin_command_collection_handler::Admin_command_collection_handler(
    iface::Session *session, const char *const mysqlx_namespace)
    : m_session(session), k_mysqlx_namespace(mysqlx_namespace) {}

namespace {

iface::Admin_command_arguments::Any create_default_schema_validation() {
  iface::Admin_command_arguments::Any any;
  any.set_type(::Mysqlx::Datatypes::Any::OBJECT);
  auto type_fld = any.mutable_obj()->add_fld();
  type_fld->set_key("type");
  auto details_any = type_fld->mutable_value();
  details_any->set_type(::Mysqlx::Datatypes::Any::SCALAR);
  auto scalar = details_any->mutable_scalar();
  scalar->set_type(::Mysqlx::Datatypes::Scalar::V_STRING);
  scalar->mutable_v_string()->set_value("object");
  return any;
}

iface::Admin_command_arguments::Object create_default_validation_obj() {
  iface::Admin_command_arguments::Object obj;
  auto schema_fld = obj.add_fld();
  schema_fld->set_key("schema");
  *schema_fld->mutable_value() = create_default_schema_validation();
  auto level_fld = obj.add_fld();
  level_fld->set_key("level");
  auto level_value = level_fld->mutable_value();
  level_value->set_type(::Mysqlx::Datatypes::Any::SCALAR);
  auto scalar = level_value->mutable_scalar();
  scalar->set_type(::Mysqlx::Datatypes::Scalar::V_STRING);
  scalar->mutable_v_string()->set_value("OFF");
  return obj;
}

}  // namespace

ngs::Error_code Admin_command_collection_handler::create_collection_impl(
    iface::Sql_session *da, const std::string &schema, const std::string &name,
    const Command_arguments::Object &validation) const {
  Command_arguments::Any validation_schema;
  bool is_enforced = true;
  ngs::Error_code error =
      get_validation_info(validation, &validation_schema, &is_enforced);
  if (error) return error;

  std::string schema_string;
  error = check_schema(validation_schema, &schema_string);
  if (error) return error;

  auto constraint_name = generate_constraint_name(name);

  Query_string_builder qb;
  qb.put("CREATE TABLE ");
  if (!schema.empty()) qb.quote_identifier(schema).dot();
  qb.quote_identifier(name)
      .put(
          " (doc JSON,"
          "_id VARBINARY(32) GENERATED ALWAYS AS "
          "(JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))) STORED PRIMARY KEY,"
          " _json_schema JSON GENERATED ALWAYS AS (")
      .quote_json_string(schema_string)
      .put("), CONSTRAINT `")
      .put(constraint_name)
      .put("` CHECK (JSON_SCHEMA_VALID(_json_schema, doc)) ")
      .put(is_enforced ? "ENFORCED" : "NOT ENFORCED")
      .put(") CHARSET utf8mb4 ENGINE=InnoDB");

  const ngs::PFS_string &tmp(qb.get());
  log_debug("CreateCollection: %s", tmp.c_str());
  Empty_resultset rset;
  return da->execute_sql(tmp.c_str(), tmp.length(), &rset);
}

ngs::Error_code Admin_command_collection_handler::create_collection(
    Command_arguments *args) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_create_collection);

  std::string schema;
  std::string collection;
  Admin_command_arguments_object::Object options;
  ngs::Error_code error =
      args->string_arg({"schema"}, &schema, Argument_appearance::k_obligatory)
          .string_arg({"name"}, &collection, Argument_appearance::k_obligatory)
          .object_arg({"options"}, &options, Argument_appearance::k_optional)
          .end();
  if (error) return error;

  auto options_arg = Admin_command_arguments_object(options);
  auto validation = create_default_validation_obj();
  bool reuse_existing = false;
  error = options_arg
              .object_arg({"validation"}, &validation,
                          Argument_appearance::k_optional)
              .bool_arg({"reuse_existing"}, &reuse_existing,
                        Argument_appearance::k_optional)
              .end();
  if (error) return error;

  if (validation.fld_size() == 0) validation = create_default_validation_obj();

  if (schema.empty()) return ngs::Error(ER_X_BAD_SCHEMA, "Invalid schema");
  if (collection.empty())
    return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name");

  error = create_collection_impl(&m_session->data_context(), schema, collection,
                                 validation);
  if (error) {
    if (!reuse_existing) return error;
    if (error.error != ER_TABLE_EXISTS_ERROR) return error;
    if (!is_collection(schema, collection))
      return ngs::Error(
          ER_X_INVALID_COLLECTION, "Table '%s' exists but is not a collection",
          (schema.empty() ? collection : schema + '.' + collection).c_str());
  }
  m_session->proto().send_exec_ok();
  return ngs::Success();
}

ngs::Error_code Admin_command_collection_handler::drop_collection(
    Command_arguments *args) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_drop_collection);

  Query_string_builder qb;
  std::string schema;
  std::string collection;

  ngs::Error_code error =
      args->string_arg({"schema"}, &schema, Argument_appearance::k_obligatory)
          .string_arg({"name"}, &collection, Argument_appearance::k_obligatory)
          .end();
  if (error) return error;

  if (schema.empty()) return ngs::Error(ER_X_BAD_SCHEMA, "Invalid schema");
  if (collection.empty())
    return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name");

  qb.put("DROP TABLE ")
      .quote_identifier(schema)
      .dot()
      .quote_identifier(collection);

  const ngs::PFS_string &tmp(qb.get());
  log_debug("DropCollection: %s", tmp.c_str());
  Empty_resultset rset;
  error =
      m_session->data_context().execute_sql(tmp.data(), tmp.length(), &rset);
  if (error) return error;
  m_session->proto().send_exec_ok();

  return ngs::Success();
}

ngs::Error_code Admin_command_collection_handler::ensure_collection(
    Command_arguments *args) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_ensure_collection);

  std::string schema;
  std::string collection;
  ::Mysqlx::Datatypes::Object options;
  auto validation = create_default_validation_obj();
  ngs::Error_code error =
      args->string_arg({"schema"}, &schema, Argument_appearance::k_optional)
          .string_arg({"name"}, &collection, Argument_appearance::k_obligatory)
          .object_arg({"options"}, &options, Argument_appearance::k_optional)
          .end();
  if (error) return error;

  auto options_arg = Admin_command_arguments_object(options);
  error = options_arg
              .object_arg({"validation"}, &validation,
                          Argument_appearance::k_optional)
              .end();
  if (error) return error;

  if (collection.empty())
    return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name");

  error = create_collection_impl(&m_session->data_context(), schema, collection,
                                 validation);
  if (error) {
    if (error.error != ER_TABLE_EXISTS_ERROR) return error;
    if (!is_collection(schema, collection))
      return ngs::Error(
          ER_X_INVALID_COLLECTION, "Table '%s' exists but is not a collection",
          (schema.empty() ? collection : schema + '.' + collection).c_str());

    // Collection exists but we should replace its schema
    error = modify_collection_validation(schema, collection, validation);
    if (error) return error;
  }
  m_session->proto().send_exec_ok();
  return ngs::Success();
}

ngs::Error_code Admin_command_collection_handler::modify_collection_validation(
    const std::string &schema, const std::string &collection,
    const Command_arguments::Object &validation) {
  Command_arguments::Any validation_schema;
  bool is_enforced = true;
  if (validation.fld_size() == 0)
    return ngs::Error(
        ER_X_CMD_ARGUMENT_OBJECT_EMPTY,
        "Arguments value used under \"validation\" must be an object with at"
        " least one field");

  ngs::Error_code error =
      get_validation_info(validation, &validation_schema, &is_enforced);
  if (error) return error;

  std::string schema_string;
  if (validation_schema.has_type()) {
    error = check_schema(validation_schema, &schema_string);
    if (error) return error;
  }

  const auto &constraint_name = generate_constraint_name(collection);
  Query_string_builder qb;
  qb.put("ALTER TABLE ");
  if (!schema.empty()) qb.quote_identifier(schema).dot();
  qb.quote_identifier(collection);
  if (validation.fld_size() == 2)
    qb.put(" MODIFY COLUMN _json_schema JSON GENERATED ALWAYS AS (")
        .quote_json_string(schema_string)
        .put(") VIRTUAL, ALTER CHECK ")
        .quote_identifier(constraint_name)
        .put(is_enforced ? " ENFORCED" : " NOT ENFORCED");
  else if (validation.fld(0).key() == "schema")
    qb.put(" MODIFY COLUMN _json_schema JSON GENERATED ALWAYS AS (")
        .quote_json_string(schema_string)
        .put(") VIRTUAL");
  else if (validation.fld(0).key() == "level")
    qb.put(" ALTER CHECK ")
        .quote_identifier(constraint_name)
        .put(is_enforced ? " ENFORCED" : " NOT ENFORCED");

  const ngs::PFS_string &tmp(qb.get());
  log_debug("ModifyCollectionOptions: %s", tmp.c_str());
  Empty_resultset rset;
  error = m_session->data_context().execute(tmp.c_str(), tmp.length(), &rset);
  if (error.error == ER_CHECK_CONSTRAINT_VIOLATED) {
    return get_detailed_validation_error(m_session->data_context());
  } else {
    // Check if modification was not performed on an old type of collection
    // (without validation), if so then modify the collection and add a
    // validation
    if (error.error == ER_CHECK_CONSTRAINT_NOT_FOUND ||
        error.error == ER_BAD_FIELD_ERROR) {
      std::string new_schema;
      if (std::find_if(validation.fld().begin(), validation.fld().end(),
                       [](const auto &e) { return e.key() == "schema"; }) ==
          validation.fld().end()) {
        new_schema = "{\"type\":\"object\"}";
      } else {
        new_schema = schema_string;
      }

      Query_string_builder qb;
      qb.put("ALTER TABLE ")
          .quote_identifier(schema)
          .dot()
          .quote_identifier(collection)
          .put(" ADD COLUMN _json_schema JSON GENERATED ALWAYS AS (")
          .quote_json_string(new_schema)
          .put(") VIRTUAL, ADD CONSTRAINT ")
          .quote_identifier(constraint_name)
          .put(" CHECK (JSON_SCHEMA_VALID(_json_schema, doc)) ")
          .put(is_enforced ? "ENFORCED" : "NOT ENFORCED");

      const ngs::PFS_string &tmp(qb.get());
      log_debug("ModifyCollectionOptions: %s", tmp.c_str());
      Empty_resultset rset;
      error =
          m_session->data_context().execute(tmp.c_str(), tmp.length(), &rset);
      if (error.error == ER_CHECK_CONSTRAINT_VIOLATED)
        return get_detailed_validation_error(m_session->data_context());
    } else if (error) {
      return error;
    }
  }
  return ngs::Success();
}

ngs::Error_code Admin_command_collection_handler::modify_collection_options(
    Command_arguments *args) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_modify_collection_options);

  std::string schema;
  std::string collection;
  ::Mysqlx::Datatypes::Object options;
  ::Mysqlx::Datatypes::Object validation;
  ngs::Error_code error =
      args->string_arg({"schema"}, &schema, Argument_appearance::k_obligatory)
          .string_arg({"name"}, &collection, Argument_appearance::k_obligatory)
          .object_arg({"options"}, &options, Argument_appearance::k_obligatory)
          .end();
  if (error) return error;

  auto options_arg = Admin_command_arguments_object(options);
  error = options_arg
              .object_arg({"validation"}, &validation,
                          Argument_appearance::k_optional)
              .end();
  if (error) return error;

  if (schema.empty()) return ngs::Error(ER_X_BAD_SCHEMA, "Invalid schema");
  if (collection.empty())
    return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name");

  error = modify_collection_validation(schema, collection, validation);
  if (error) return error;

  m_session->proto().send_exec_ok();
  return ngs::Success();
}

ngs::Error_code Admin_command_collection_handler::get_collection_options(
    Command_arguments *args) {
  m_session->update_status(
      &ngs::Common_status_variables::m_stmt_get_collection_options);

  std::string schema;
  std::string collection;
  std::vector<std::string> options;
  ngs::Error_code error =
      args->string_arg({"schema"}, &schema, Argument_appearance::k_obligatory)
          .string_arg({"name"}, &collection, Argument_appearance::k_obligatory)
          .string_list({"options"}, &options, Argument_appearance::k_obligatory)
          .end();

  if (error) return error;
  if (schema.empty()) return ngs::Error(ER_X_BAD_SCHEMA, "Invalid schema");
  if (collection.empty())
    return ngs::Error(ER_X_BAD_TABLE, "Invalid collection name");

  for (const auto &option : options) {
    if (!m_collection_option_handler.contains_handler(option))
      return ngs::Error(ER_X_COLLECTION_OPTION_DOESNT_EXISTS,
                        "Requested collection option '%s' doesn't exist.",
                        option.c_str());
  }

  error = check_if_collection_exists_and_is_accessible(schema, collection);
  if (error) return error;

  for (const auto &option : options) {
    error = m_collection_option_handler.dispatch(option, schema, collection);
    if (error) return error;
  }

  return ngs::Success();
}

Admin_command_collection_handler::Collection_option_handler::
    Collection_option_handler(
        Admin_command_collection_handler *cmd_collection_handler,
        iface::Session *session)
    : m_cmd_collection_handler(cmd_collection_handler),
      m_session(session),
      m_dispatcher{{"validation",
                    &Admin_command_collection_handler::
                        Collection_option_handler::get_validation_option}} {}

ngs::Error_code
Admin_command_collection_handler::Collection_option_handler::dispatch(
    const std::string &option, const std::string &schema,
    const std::string &collection) {
  return (this->*m_dispatcher.at(option))(schema, collection);
}

ngs::Error_code Admin_command_collection_handler::Collection_option_handler::
    get_validation_option(const std::string &schema,
                          const std::string &collection) {
  Sql_data_result sql_result(&m_session->data_context());
  Query_string_builder schema_qb, level_qb;
  schema_qb
      .put(
          "SELECT GENERATION_EXPRESSION FROM information_schema.COLUMNS "
          "WHERE TABLE_SCHEMA=")
      .quote_string(schema)
      .put(" AND TABLE_NAME=")
      .quote_string(collection)
      .put(" AND COLUMN_NAME='_json_schema';");

  level_qb
      .put(
          "SELECT IF(COUNT(*),\"strict\",\"off\") FROM "
          "information_schema.TABLE_CONSTRAINTS WHERE TABLE_SCHEMA=")
      .quote_string(schema)
      .put(" AND TABLE_NAME=")
      .quote_string(collection)
      .put(" AND CONSTRAINT_NAME = ")
      .quote_string(
          m_cmd_collection_handler->generate_constraint_name(collection))
      .put(" AND ENFORCED='YES';");

  std::string validation_schema_raw, validation_level;
  try {
    sql_result.query(schema_qb.get());
    if (sql_result.size() != 0) sql_result.get(&validation_schema_raw);
    sql_result.query(level_qb.get());
    if (sql_result.size() != 0) sql_result.get(&validation_level);
  } catch (const ngs::Error_code &e) {
    return e;
  }

  auto validation_json =
      create_validation_json(validation_schema_raw, validation_level);
  send_validation_option_json(validation_json);
  return ngs::Success();
}

std::string Admin_command_collection_handler::Collection_option_handler::
    create_validation_json(std::string validation_schema_raw,
                           std::string validation_level) {
  // schema and level can be empty for old collection types (without
  // validation)
  if (validation_schema_raw.empty()) {
    validation_schema_raw = "{\"type\": \"object\"}";
  } else {
    validation_schema_raw.erase(0, validation_schema_raw.find_first_of('{'));
    validation_schema_raw.erase(validation_schema_raw.find_last_of('}') + 1);
  }
  if (validation_level.empty()) validation_level = "off";

  return "{ \"validation\": { \"level\": \"" + validation_level +
         "\", \"schema\": " + validation_schema_raw + " } }";
}

void Admin_command_collection_handler::Collection_option_handler::
    send_validation_option_json(const std::string &validation_json) {
  auto &proto = m_session->proto();

  ngs::Column_info_builder column{Mysqlx::Resultset::ColumnMetaData::BYTES,
                                  "Result"};
  proto.send_column_metadata(column.get());

  proto.start_row();
  proto.row_builder()->field_string(validation_json.c_str(),
                                    validation_json.length());
  proto.send_row();

  proto.send_result_fetch_done();
  proto.send_exec_ok();
}

bool Admin_command_collection_handler::is_collection(const std::string &schema,
                                                     const std::string &name) {
  Query_string_builder qb;
  qb.put("SELECT COUNT(*) AS cnt,").put(k_count_doc).put(" AS doc,");

  if (m_session->data_context().is_sql_mode_set("NO_BACKSLASH_ESCAPES"))
    qb.put(k_count_id_no_backslash_escapes)
        .put(" AS id,")
        .put(k_count_gen_no_backslash_escapes);
  else
    qb.put(k_count_id).put(" AS id,").put(k_count_gen);

  qb.put(" AS gen, ")
      .put(k_count_schema)
      .put(" AS validation_schema ")
      .put("FROM information_schema.columns WHERE table_name = ")
      .quote_string(name)
      .put(" AND table_schema = ");
  if (schema.empty())
    qb.put("schema()");
  else
    qb.quote_string(schema);

  Sql_data_result result(&m_session->data_context());
  try {
    result.query(qb.get());
    if (result.size() != 1) {
      log_debug(
          "Unable to recognize '%s' as a collection; query result size: "
          "%" PRIu64,
          std::string(schema.empty() ? name : schema + "." + name).c_str(),
          static_cast<uint64_t>(result.size()));
      return false;
    }
    int64_t cnt = 0, doc = 0, id = 0, gen = 0, schema = 0;
    result.get(&cnt, &doc, &id, &gen, &schema);
    return doc == 1 && id == 1 && (cnt == gen + doc + id + schema);
  } catch (const ngs::Error_code &DEBUG_VAR(e)) {
    log_debug(
        "Unable to recognize '%s' as a collection; exception message: '%s'",
        std::string(schema.empty() ? name : schema + "." + name).c_str(),
        e.message.c_str());
    return false;
  }
}

ngs::Error_code Admin_command_collection_handler::get_validation_info(
    const Command_arguments::Object &validation,
    Command_arguments::Any *validation_schema, bool *enforce) const {
  static const char *k_level_strict = "STRICT";
  static const char *k_level_off = "OFF";

  std::string validation_level = k_level_strict;
  *validation_schema = create_default_schema_validation();

  auto validation_arg = Admin_command_arguments_object(validation);
  ngs::Error_code error = validation_arg
                              .any_arg({"schema"}, validation_schema,
                                       Argument_appearance::k_optional)
                              .string_arg({"level"}, &validation_level,
                                          Argument_appearance::k_optional)
                              .end();
  if (error) return error;

  validation_level = to_upper(validation_level);

  if (!validation_level.empty() && validation_level != k_level_off &&
      validation_level != k_level_strict)
    return ngs::Error(ER_X_CMD_ARGUMENT_VALUE,
                      "Invalid \"validation.level\" argument."
                      " Allowed values are 'OFF' and 'STRICT'");
  *enforce = validation_level != k_level_off;

  return ngs::Success();
}

std::string Admin_command_collection_handler::generate_constraint_name(
    const std::string &collection_name) const {
  const bool is_table_names_case_sensitive =
      get_system_variable<int64_t>(&m_session->data_context(),
                                   "lower_case_table_names") == 0l;

  const auto name = is_table_names_case_sensitive ? collection_name
                                                  : to_lower(collection_name);
  return std::string("$val_strict_") + generate_hash(name);
}

ngs::Error_code
Admin_command_collection_handler::check_if_collection_exists_and_is_accessible(
    const std::string &schema, const std::string &collection) {
  Query_string_builder check_collection_qb;
  check_collection_qb.put("SELECT 1 FROM ")
      .quote_identifier(schema)
      .dot()
      .quote_identifier(collection)
      .put(" LIMIT 1");
  Sql_data_result sql_result(&m_session->data_context());
  try {
    sql_result.query(check_collection_qb.get());
  } catch (const ngs::Error_code &e) {
    return e;
  }
  return ngs::Success();
}

ngs::Error_code Admin_command_collection_handler::check_schema(
    const Command_arguments::Any &validation_schema,
    std::string *schema_string) const {
  Meta_schema_validator v;
  return v.validate(validation_schema, schema_string);
}

}  // namespace xpl
