/*
  Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#include "mrs/database/query_entry_object.h"

#include <algorithm>
#include <string>
#include <utility>

#include "helper/json/text_to.h"
#include "helper/mysql_row.h"
#include "mrs/database/converters/column_datatype_converter.h"
#include "mrs/database/converters/column_mapping_converter.h"
#include "mrs/database/converters/id_generation_type_converter.h"
#include "mrs/database/converters/kind_converter.h"
#include "mrs/database/entry/object.h"
#include "mrs/interface/rest_error.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"

#include "mysqld_error.h"

IMPORT_LOG_FUNCTIONS()

#if 0
namespace {
template <typename T>
const char *to_str(const char *value, T *) {
  return value;
}

const char *to_str(const char *value, bool *) {
  if (value == nullptr) return "null";
  return value[0] ? "true" : "false";
}
}  // namespace

#define CONVERT_WITH_DEFAULT(OUT, DEF)                                    \
  {                                                                       \
    log_debug("Deserialize to %s = %s with default " #DEF ", is_null:%s", \
              #OUT, to_str(row.row_[row.field_index_], OUT),              \
              (row.row_[row.field_index_] == nullptr) ? "yes" : "no");    \
    row.unserialize(OUT, DEF);                                            \
  }

#define CONVERT(OUT)                                                   \
  {                                                                    \
    log_debug("Deserialize to %s = %s, is_null:%s", #OUT,              \
              to_str(row.row_[row.field_index_], OUT),                 \
              (row.row_[row.field_index_] == nullptr) ? "yes" : "no"); \
    row.unserialize(OUT);                                              \
  }
#endif

namespace mrs {
namespace database {

using RestError = mrs::interface::RestError;
using ForeignKeyReference = entry::ForeignKeyReference;
using Table = entry::Table;
using KindType = entry::KindType;
using ModeType = entry::ModeType;

namespace {

void from_user_ownership_field_id(entry::OwnerUserField *out,
                                  const char *value) {
  if (!value) {
    *out = {};
    return;
  }

  entry::UniversalId::from_raw(&out->uid, value);
}

}  // namespace

namespace v2 {

QueryEntryObject::UniversalId QueryEntryObject::query_object(
    MySQLSession *session, const UniversalId &db_object_id, Object *obj) {
  log_debug("Loading Object_v2::query_object");
  entry::UniversalId object_id;

  mysqlrouter::sqlstring q{
      "SELECT object.id, object.kind,"
      " CAST(db_object.crud_operations AS UNSIGNED),"
      " (SELECT objf.id FROM mysql_rest_service_metadata.object_field objf"
      "   WHERE objf.object_id = object.id AND objf.parent_reference_id IS NULL"
      "    AND db_object.row_user_ownership_column = objf.db_column->>'$.name')"
      "  FROM mysql_rest_service_metadata.object"
      "  JOIN mysql_rest_service_metadata.db_object"
      "    ON object.db_object_id = db_object.id"
      "  WHERE object.db_object_id=? ORDER by kind DESC"};
  q << db_object_id;
  auto res = query_one(session, q.str());

  if (nullptr == res.get()) return {};

  entry::UniversalId::from_raw(&object_id, (*res)[0]);
  obj->crud_operations = std::stoi((*res)[2]);

  obj->user_ownership_field.reset();
  if ((*res)[3]) {
    mrs::database::entry::OwnerUserField value;
    from_user_ownership_field_id(&value, (*res)[3]);
    obj->user_ownership_field = value;
  }

  KindTypeConverter()(&obj->kind, (*res)[1]);

  return object_id;
}

void QueryEntryObject::query_entries(MySQLSession *session,
                                     const std::string &schema_name,
                                     const std::string &object_name,
                                     const UniversalId &db_object_id) {
  log_debug("Loading Object_v2::query_entries");
  // Cleanup
  m_alias_count = 0;
  m_references.clear();

  // Build the query and resulting objects.
  object = std::make_shared<entry::Object>();
  object->schema = schema_name;
  object->table = object_name;
  object->table_alias = "t";

  entry::UniversalId object_id;

  object_id = query_object(session, db_object_id, object.get());

  m_loading_references = true;
  set_query_object_reference(object_id);

  execute(session);

  m_loading_references = false;
  query_ =
      "SELECT object_field.id,"
      " object_field.parent_reference_id,"
      " object_field.represents_reference_id,"
      " object_field.name,"
      " object_field.position,"
      " object_field.enabled,"
      " object_field.db_column->>'$.name',"
      " object_field.db_column->>'$.datatype',"
      " object_field.db_column->>'$.id_generation',"
      " object_field.db_column->>'$.not_null',"
      " object_field.db_column->>'$.is_primary',"
      " object_field.db_column->>'$.is_unique',"
      " object_field.db_column->>'$.is_generated',"
      " object_field.db_column->>'$.in',"
      " object_field.db_column->>'$.out',"
      " JSON_VALUE(object_field.db_column, '$.srid'),"
      " object_field.allow_filtering,"
      " object_field.allow_sorting,"
      " object_field.no_check,"
      " object_field.no_update"
      " FROM mysql_rest_service_metadata.object_field"
      " WHERE object_field.object_id = ?"
      " ORDER BY object_field.represents_reference_id, object_field.position";
  query_ << object_id;

  execute(session);

  // resolve row ownership column
  if (object->user_ownership_field.has_value()) {
    object->user_ownership_field->field =
        object->get_column(object->user_ownership_field->uid);
    if (object->user_ownership_field->field)
      object->user_ownership_field->field->is_row_owner = true;
  }

  for (auto &[_, r] : m_references) {
    auto v = r->ref_table;
    if (!v->user_ownership_field.has_value()) continue;

    v->user_ownership_field->field =
        v->get_column(v->user_ownership_field->uid);
    if (object->user_ownership_field->field)
      v->user_ownership_field->field->is_row_owner = true;
  }
}

void QueryEntryObject::set_query_object_reference(
    const entry::UniversalId &object_id) {
  query_ =
      "SELECT"
      " object_reference.id,"
      " object_reference.reference_mapping->>'$.referenced_schema',"
      " object_reference.reference_mapping->>'$.referenced_table',"
      " object_reference.reference_mapping->'$.to_many',"
      " object_reference.reference_mapping->'$.column_mapping',"
      " object_reference.unnest OR "
      "   object_reference.reduce_to_value_of_field_id IS NOT NULL,"
      " CAST(object_reference.crud_operations AS UNSIGNED)"
      " FROM mysql_rest_service_metadata.object_field"
      " JOIN mysql_rest_service_metadata.object_reference"
      "  ON object_field.represents_reference_id = object_reference.id"
      " WHERE object_field.object_id = ?";
  query_ << object_id;
}

void QueryEntryObject::on_row(const ResultRow &r) {
  try {
    if (m_loading_references)
      on_reference_row(r);
    else
      on_field_row(r);
  } catch (const std::exception &e) {
    UniversalId id;

    if (r.size() > 0 && r[0]) {
      id = UniversalId::from_cstr(r[0], r.get_data_size(0));
    }

    log_error("%s with id:%s, will be disabled because of following error: %s",
              (m_loading_references ? "Reference" : "Field"),
              id.to_string().c_str(), e.what());
  }
}

void QueryEntryObject::on_reference_row(const ResultRow &r) {
  log_debug("Loading Object_v2::on_reference_row");
  auto reference = std::make_shared<ForeignKeyReference>();
  reference->ref_table = std::make_shared<Table>();

  entry::UniversalId reference_id;

  helper::MySQLRow row(r, metadata_, num_of_metadata_);
  row.unserialize_with_converter(&reference_id, entry::UniversalId::from_raw);
  row.unserialize(&reference->ref_table->schema);
  row.unserialize(&reference->ref_table->table);
  row.unserialize(&reference->to_many);
  row.unserialize_with_converter(&reference->column_mapping,
                                 ColumnMappingConverter{});
  row.unserialize(&reference->unnest);
  row.unserialize(&reference->ref_table->crud_operations);

  reference->ref_table->table_alias = "t" + std::to_string(++m_alias_count);

  m_references[reference_id] = reference;
}

void QueryEntryObject::on_field_row(const ResultRow &r) {
  log_debug("Loading Object_v2::on_field_row");
  helper::MySQLRow row(r, metadata_, num_of_metadata_,
                       helper::MySQLRow::kEndCallRequired);
  entry::UniversalId field_id;

  entry::UniversalId parent_reference_id;
  std::optional<entry::UniversalId> represents_reference_id;

  row.unserialize_with_converter(&field_id, entry::UniversalId::from_raw);
  row.unserialize_with_converter(&parent_reference_id,
                                 entry::UniversalId::from_raw_zero_on_null);
  row.unserialize_with_converter(&represents_reference_id,
                                 entry::UniversalId::from_raw);

  std::shared_ptr<ForeignKeyReference> parent_ref;
  std::shared_ptr<Table> table;
  if (parent_reference_id != entry::UniversalId()) {
    auto ref_it = m_references.find(parent_reference_id);
    if (ref_it == m_references.end()) {
      using namespace std::string_literals;
      throw std::runtime_error(
          "No parent_object found, referenced by parent_reference_id:"s +
          to_string(parent_reference_id));
    }

    parent_ref = ref_it->second;
    table = parent_ref->ref_table;
    assert(table);
  } else {
    table = object;
  }

  if (represents_reference_id) {
    log_debug("Reference");

    if (auto it = m_references.find(*represents_reference_id);
        it == m_references.end()) {
      using namespace std::string_literals;
      throw std::runtime_error("Reference "s + to_string(parent_reference_id) +
                               " not found");
    } else {
      auto ofield = it->second;
      ofield->id = field_id;

      row.unserialize(&ofield->name);
      row.unserialize(&ofield->position);
      row.unserialize(&ofield->enabled);
      row.skip(10);
      row.unserialize(&ofield->allow_filtering);
      row.unserialize(&ofield->allow_sorting);
      row.skip(2);  // no no_check and no_update in a ref

      log_debug("Using rfield name=%s", ofield->name.c_str());

      table->fields.emplace_back(ofield);
    }
  } else {
    std::shared_ptr<entry::Column> dfield;

    if (object->kind == KindType::PARAMETERS)
      dfield = std::make_shared<entry::ParameterField>();
    else
      dfield = std::make_shared<entry::Column>();

    dfield->id = field_id;
    row.unserialize(&dfield->name);
    row.unserialize(&dfield->position);
    row.unserialize(&dfield->enabled);

    row.unserialize(&dfield->column_name);
    row.unserialize(&dfield->datatype);
    // disabled fields can come in as NULL
    if (dfield->enabled || !dfield->datatype.empty()) {
      ColumnDatatypeConverter()(&dfield->type, dfield->datatype);
    }
    row.unserialize_with_converter(&dfield->id_generation,
                                   IdGenerationTypeConverter());
    row.unserialize(&dfield->not_null);
    row.unserialize(&dfield->is_primary);
    row.unserialize(&dfield->is_unique);
    row.unserialize(&dfield->is_generated);
    bool parameter_in{false}, parameter_out{false};
    row.unserialize(&parameter_in);
    row.unserialize(&parameter_out);

    if (object->kind == KindType::PARAMETERS) {
      auto parameter_field =
          dynamic_cast<entry::ParameterField *>(dfield.get());
      parameter_field->mode = ModeType::kNONE;
      if (parameter_in && parameter_out)
        parameter_field->mode = ModeType::kIN_OUT;
      else if (parameter_in)
        parameter_field->mode = ModeType::kIN;
      else if (parameter_out)
        parameter_field->mode = ModeType::kOUT;
    }
    row.unserialize(&dfield->srid, static_cast<uint32_t>(0));
    row.unserialize(&dfield->allow_filtering);
    row.unserialize(&dfield->allow_sorting);

    bool no_check, no_update;
    row.unserialize(&no_check);
    row.unserialize(&no_update);

    if (no_check) dfield->with_check = false;
    if (no_update) dfield->with_update = false;

    log_debug("Creating dfield name=%s, table=%s", dfield->name.c_str(),
              table.get()->table.c_str());

    table->fields.push_back(dfield);
  }

  // Enable the assertion in destruction.
  //
  // Before this point the code may throw an error, we would like
  // to skip assertion in that case.
  row.end();
}

}  // namespace v2

namespace v3 {

QueryEntryObject::UniversalId QueryEntryObject::query_object(
    MySQLSession *session, const UniversalId &db_object_id, Object *obj) {
  log_debug("Loading Object_v3::query_object");
  entry::UniversalId object_id;

  mysqlrouter::sqlstring q{
      "SELECT object.id, object.kind,"
      " row_ownership_field_id,"
      " COALESCE(object.options->>'$.dataMappingViewInsert',"
      " object.options->>'$.duality_view_insert') = 'true',"
      " COALESCE(object.options->>'$.dataMappingViewUpdate',"
      " object.options->>'$.duality_view_update') = 'true',"
      " COALESCE(object.options->>'$.dataMappingViewDelete',"
      " object.options->>'$.duality_view_delete') = 'true',"
      " COALESCE(object.options->>'$.dataMappingViewNoCheck',"
      " object.options->>'$.duality_view_no_check') = 'true'"
      "  FROM mysql_rest_service_metadata.object"
      "  JOIN mysql_rest_service_metadata.db_object"
      "    ON object.db_object_id = db_object.id"
      "  WHERE object.db_object_id=? ORDER by kind DESC"};
  q << db_object_id;

  auto res = query_one(session, q.str());

  if (nullptr == res.get()) return {};

  helper::MySQLRow row(*res, nullptr, res->size());
  row.unserialize_with_converter(&object_id, entry::UniversalId::from_raw);
  row.unserialize_with_converter(&obj->kind, KindTypeConverter());
  row.unserialize_with_converter(&obj->user_ownership_field,
                                 from_user_ownership_field_id);
  bool with_insert = false, with_update = false, with_delete = false,
       with_no_check = false;
  row.unserialize(&with_insert);
  row.unserialize(&with_update);
  row.unserialize(&with_delete);
  row.unserialize(&with_no_check);
  obj->crud_operations = 0;
  if (with_insert)
    obj->crud_operations |= entry::Operation::Values::valueCreate;
  if (with_update)
    obj->crud_operations |= entry::Operation::Values::valueUpdate;
  if (with_delete)
    obj->crud_operations |= entry::Operation::Values::valueDelete;
  obj->with_check_ = !with_no_check;

  return object_id;
}

void QueryEntryObject::set_query_object_reference(
    const entry::UniversalId &object_id) {
  query_ =
      "SELECT"
      " object_reference.id,"
      " object_reference.reference_mapping->>'$.referenced_schema',"
      " object_reference.reference_mapping->>'$.referenced_table',"
      " object_reference.reference_mapping->'$.to_many',"
      " object_reference.reference_mapping->'$.column_mapping',"
      " object_reference.unnest OR "
      "   object_reference.reduce_to_value_of_field_id IS NOT NULL,"
      " object_reference.row_ownership_field_id,"
      " COALESCE(object_reference.options->>'$.dataMappingViewInsert',"
      " object_reference.options->>'$.duality_view_insert') = 'true',"
      " COALESCE(object_reference.options->>'$.dataMappingViewUpdate',"
      " object_reference.options->>'$.duality_view_update') = 'true',"
      " COALESCE(object_reference.options->>'$.dataMappingViewDelete',"
      " object_reference.options->>'$.duality_view_delete') = 'true',"
      " COALESCE(object_reference.options->>'$.dataMappingViewNoCheck',"
      " object_reference.options->>'$.duality_view_no_check') = 'true'"
      " FROM mysql_rest_service_metadata.object_field"
      " JOIN mysql_rest_service_metadata.object_reference"
      "  ON object_field.represents_reference_id = object_reference.id"
      " WHERE object_field.object_id = ?";
  query_ << object_id;
}

void QueryEntryObject::on_reference_row(const ResultRow &r) {
  log_debug("Loading Object_v3::on_reference_row");
  auto reference = std::make_shared<ForeignKeyReference>();
  reference->ref_table = std::make_shared<Table>();

  entry::UniversalId reference_id;

  helper::MySQLRow row(r, metadata_, num_of_metadata_);
  row.unserialize_with_converter(&reference_id, entry::UniversalId::from_raw);
  row.unserialize(&reference->ref_table->schema);
  row.unserialize(&reference->ref_table->table);
  row.unserialize(&reference->to_many);
  row.unserialize_with_converter(&reference->column_mapping,
                                 ColumnMappingConverter{});
  row.unserialize(&reference->unnest);

  // This values must be stored in ownership_field in each reference,
  // and used while building the SQL (needs separate fix).
  row.skip(1);
  // row.unserialize_with_converter(&object->user_ownership_field,
  //                                from_user_ownership_field_id);
  bool with_insert = false, with_update = false, with_delete = false,
       with_no_check = false;
  row.unserialize(&with_insert);
  row.unserialize(&with_update);
  row.unserialize(&with_delete);
  row.unserialize(&with_no_check);
  reference->ref_table->crud_operations = 0;
  if (with_insert)
    reference->ref_table->crud_operations |=
        entry::Operation::Values::valueCreate;
  if (with_update)
    reference->ref_table->crud_operations |=
        entry::Operation::Values::valueUpdate;
  if (with_delete)
    reference->ref_table->crud_operations |=
        entry::Operation::Values::valueDelete;
  reference->ref_table->with_check_ = !with_no_check;

  reference->ref_table->table_alias = "t" + std::to_string(++m_alias_count);
  m_references[reference_id] = reference;
}

}  // namespace v3

}  // namespace database
}  // namespace mrs
