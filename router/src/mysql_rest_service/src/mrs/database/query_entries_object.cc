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

#include "mrs/database/query_entry_object.h"

#include <algorithm>
#include <string>
#include <utility>

#include "helper/json/text_to.h"
#include "helper/mysql_row.h"
#include "mrs/interface/rest_error.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"

#include "mysqld_error.h"

IMPORT_LOG_FUNCTIONS()

#if defined(_WIN32)
#define mrs_strcasecmp(a, b) _stricmp(a, b)
#else
#define mrs_strcasecmp(a, b) strcasecmp(a, b)
#endif  // defined(_WIN32)

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

namespace mrs {
namespace database {

using RestError = mrs::interface::RestError;

namespace {

void from_optional_user_ownership_field_id(
    std::optional<entry::OwnerUserField> *out, const char *value) {
  if (!value) {
    out->reset();
    return;
  }

  out->emplace();
  entry::UniversalId::from_raw(&(*out)->uid, value);
}

using KindType = entry::KindType;
using ModeType = entry::ModeType;

void convert_kind(KindType *out, const char *value) {
  const static std::map<std::string, KindType> converter{
      {"PARAMETERS", KindType::PARAMETERS}, {"RESULT", KindType::RESULT}};

  if (!value) value = "";
  auto result = mysql_harness::make_upper(value);
  try {
    *out = converter.at(result);
  } catch (const std::exception &e) {
    log_debug("'KindTypeConverter 'do not handle value: %s", result.c_str());
    throw;
  }
}

class ColumnMappingConverter {
 public:
  explicit ColumnMappingConverter(std::shared_ptr<entry::JoinedTable> ref_table)
      : ref_table_(ref_table) {}

  void operator()(entry::JoinedTable::ColumnMapping *out,
                  const char *value) const {
    if (nullptr == value) {
      *out = {};
      return;
    }

    rapidjson::Document doc = helper::json::text_to_document(value);
    if (!doc.IsArray()) {
      throw RestError("Column 'metadata', must be an array");
    }

    out->clear();
    for (const auto &col : doc.GetArray()) {
      if (!col.IsObject())
        throw RestError("Column 'metadata', element must be an object.");
      if (!col.HasMember("base") || !col["base"].IsString())
        throw RestError(
            "Column 'metadata', element must contain 'base' field with "
            "string value.");
      if (!col.HasMember("ref") || !col["ref"].IsString())
        throw RestError(
            "Column 'metadata', element must contain 'ref' field with string "
            "value.");

      auto lplaceholder = std::make_shared<entry::Column>();
      lplaceholder->name = col["base"].GetString();
      auto rplaceholder = std::make_shared<entry::Column>();
      rplaceholder->name = col["ref"].GetString();
      rplaceholder->table = ref_table_;
      out->emplace_back(lplaceholder, rplaceholder);
    }
  }

 private:
  std::shared_ptr<entry::JoinedTable> ref_table_;
};

}  // namespace

namespace v2 {

QueryEntryObject::UniversalId QueryEntryObject::query_object(
    MySQLSession *session, const UniversalId &db_object_id, Object *obj) {
  entry::UniversalId object_id;

  mysqlrouter::sqlstring q{
      "SELECT object.id, object.kind,"
      " CAST(db_object.crud_operations AS UNSIGNED)"
      "  FROM mysql_rest_service_metadata.object"
      "  JOIN mysql_rest_service_metadata.db_object"
      "    ON object.db_object_id = db_object.id"
      "  WHERE object.db_object_id=?"};
  q << db_object_id;

  auto res = query_one(session, q.str());

  if (nullptr == res.get()) return {};

  auto base_table = obj->base_tables.back();
  entry::UniversalId::from_raw(&object_id, (*res)[0]);
  base_table->crud_operations = std::stoi((*res)[2]);

  convert_kind(&obj->kind, (*res)[1]);

  return object_id;
}

void QueryEntryObject::query_entries(MySQLSession *session,
                                     const std::string &schema_name,
                                     const std::string &object_name,
                                     const UniversalId &db_object_id) {
  // Cleanup
  m_alias_count = 0;
  m_tables.clear();
  m_objects.clear();
  object.reset();

  // Build the query and resulting objects.
  auto base_table = std::make_shared<entry::BaseTable>();
  base_table->schema = schema_name;
  base_table->table = object_name;
  base_table->table_alias = "t";

  object = std::make_shared<Object>();
  object->base_tables.push_back(base_table);
  m_objects[entry::UniversalId{}] = object;

  entry::UniversalId object_id;

  m_tables[entry::UniversalId{}] = base_table;

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
      " ORDER BY object_field.represents_reference_id";
  query_ << object_id;

  execute(session);

  // post-processing... re-order object fields, resolve placeholders
  for (const auto &o : m_objects) {
    auto object = o.second;
    std::sort(
        object->fields.begin(), object->fields.end(),
        [](const auto &a, const auto &b) { return a->position > b->position; });
  }
  for (const auto &t : m_tables) {
    auto table = t.second;
    auto join = std::dynamic_pointer_cast<entry::JoinedTable>(table);
    if (join && join->enabled) {
      entry::JoinedTable::ColumnMapping fixed_mapping;
      for (auto &c : join->column_mapping) {
        auto ltable = c.first->table.lock();
        auto rtable = c.second->table.lock();

        if (!ltable || !rtable) {
          log_error("Invalid metadata for JOIN for %s", table->table.c_str());
          continue;
        }

        auto lcolumn = ltable->get_column(c.first->name);
        auto rcolumn = rtable->get_column(c.second->name);

        if (!lcolumn)
          throw std::runtime_error("Invalid column " + ltable->table + "." +
                                   c.first->name + " in column_mapping");

        if (!rcolumn)
          throw std::runtime_error("Invalid column " + rtable->table + "." +
                                   c.second->name + " in column_mapping");

        if (join->to_many)
          rcolumn->is_foreign = true;
        else
          lcolumn->is_foreign = true;

        fixed_mapping.emplace_back(lcolumn, rcolumn);
      }

      join->column_mapping = fixed_mapping;
    }
  }

  // TODO(alfredo) - do some sanity checks
  // - are there PKs defined
  // - are there more than 1 row-owner-id or generated columns defined
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
      // TODO reduce_to_value_of_field_id will be removed
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
  if (m_loading_references)
    on_reference_row(r);
  else
    on_field_row(r);
}

void QueryEntryObject::on_reference_row(const ResultRow &r) {
  auto reference = std::make_shared<entry::JoinedTable>();

  entry::UniversalId reference_id;

  helper::MySQLRow row(r, metadata_, no_od_metadata_);
  row.unserialize_with_converter(&reference_id, entry::UniversalId::from_raw);
  row.unserialize(&reference->schema);
  row.unserialize(&reference->table);
  row.unserialize(&reference->to_many);
  row.unserialize_with_converter(&reference->column_mapping,
                                 ColumnMappingConverter{reference});
  row.unserialize(&reference->unnest);
  row.unserialize(&reference->crud_operations);

  reference->table_alias = "t" + std::to_string(++m_alias_count);
  m_tables[reference_id] = reference;

  auto object = std::make_shared<Object>();
  object->name = reference->table_key();
  object->base_tables.push_back(reference);
  m_objects[reference_id] = object;
}

void QueryEntryObject::on_field_row(const ResultRow &r) {
  class IdGenerationTypeConverter {
   public:
    void operator()(entry::IdGenerationType *out, const char *value) const {
      if (nullptr == value) {
        *out = entry::IdGenerationType::NONE;
        return;
      }

      if (mrs_strcasecmp(value, "auto_inc") == 0) {
        *out = entry::IdGenerationType::AUTO_INCREMENT;
      } else if (mrs_strcasecmp(value, "rev_uuid") == 0) {
        *out = entry::IdGenerationType::REVERSE_UUID;
      } else if (mrs_strcasecmp(value, "null") == 0) {
        *out = entry::IdGenerationType::NONE;
      } else {
        throw std::runtime_error("bad metadata");
      }
    }
  };

  helper::MySQLRow row(r, metadata_, no_od_metadata_);

  entry::UniversalId field_id;
  entry::UniversalId parent_reference_id;
  std::optional<entry::UniversalId> represents_reference_id;

  row.unserialize_with_converter(&field_id, entry::UniversalId::from_raw);
  row.unserialize_with_converter(&parent_reference_id,
                                 entry::UniversalId::from_raw_zero_on_null);
  row.unserialize_with_converter(&represents_reference_id,
                                 entry::UniversalId::from_raw_optional);

  auto parent_object_it = m_objects.find(parent_reference_id);
  if (parent_object_it == m_objects.end()) {
    log_debug("No parent_object found, referenced by parent_reference_id:%s",
              to_string(parent_reference_id).c_str());
    return;
  }
  auto parent_object = parent_object_it->second;

  auto table = m_tables[parent_reference_id];
  if (!table) {
    log_debug("No table found, referenced by parent_reference_id:%s",
              to_string(parent_reference_id).c_str());
    return;
  }

  if (represents_reference_id) {
    auto ofield = std::make_shared<entry::ReferenceField>();
    ofield->id = field_id;

    log_debug("Reference");
    CONVERT(&ofield->name);
    row.unserialize(&ofield->position);
    CONVERT(&ofield->enabled);
    row.skip(9);
    row.unserialize(&ofield->allow_filtering);
    row.unserialize(&ofield->allow_sorting);
    row.unserialize(&ofield->no_check);
    row.unserialize(&ofield->no_update);

    auto reference = m_tables.at(*represents_reference_id);

    bool unnest = false;
    bool is_array = false;
    if (auto join = std::dynamic_pointer_cast<entry::JoinedTable>(reference);
        join) {
      if (!ofield->enabled) {
        join->enabled = false;
      }

      unnest = join->unnest;

      if (join->to_many) {
        is_array = true;
      }

      for (const auto &c : join->column_mapping) {
        c.first->table = table;
      }
    }

    // if the represented reference is unnested, the field itself is just a
    // placeholder and isn't included in the output object
    if (unnest) {
      // fields in the unnested object must be added to this object
      auto &obj = m_objects[*represents_reference_id];
      if (!obj) {
        log_debug("Object with 'represents_reference_id', not found.");
        return;
      }

      // if we're unnesting a 1:n (array of objects), we need to keep the field
      // as a reference, so it gets translated as a subquery with an aggregation
      if (is_array) {
        ofield->nested_object = m_objects[*represents_reference_id];

        parent_object->fields.push_back(ofield);
      } else {
        for (auto f : obj->fields) {
          parent_object->fields.push_back(f);
        }
        for (auto &t : obj->base_tables) {
          parent_object->base_tables.push_back(t);
        }
        m_objects[*represents_reference_id] = parent_object;
      }
    } else {
      ofield->nested_object = m_objects[*represents_reference_id];

      parent_object->fields.push_back(ofield);
    }
  } else {
    std::shared_ptr<entry::DataField> dfield;

    if (parent_object->kind == KindType::PARAMETERS)
      dfield = std::make_shared<entry::ParameterField>();
    else
      dfield = std::make_shared<entry::DataField>();

    dfield->id = field_id;
    CONVERT(&dfield->name);
    row.unserialize(&dfield->position);
    CONVERT(&dfield->enabled);

    auto column = std::make_shared<entry::Column>();
    CONVERT(&column->name);
    row.unserialize(&column->datatype);
    // disabled fields can come in as NULL
    if (dfield->enabled || !column->datatype.empty())
      column->type = column_datatype_to_type(column->datatype);
    row.unserialize_with_converter(&column->id_generation,
                                   IdGenerationTypeConverter());
    row.unserialize(&column->not_null);
    row.unserialize(&column->is_primary);
    row.unserialize(&column->is_unique);
    CONVERT(&column->is_generated);
    bool parameter_in{false}, parameter_out{false};
    row.unserialize(&parameter_in);
    row.unserialize(&parameter_out);

    if (parent_object->kind == KindType::PARAMETERS) {
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
    CONVERT_WITH_DEFAULT(&column->srid, static_cast<uint32_t>(0));
    row.unserialize(&dfield->allow_filtering);
    row.unserialize(&dfield->no_check);

    dfield->source = column;
    column->table = table;
    log_debug("Creating dfield name=%s, table=%p", dfield->name.c_str(),
              table.get());

    table->columns.push_back(column);
    parent_object->fields.push_back(dfield);
  }
}

}  // namespace v2

namespace v3 {

void QueryEntryObject::query_entries(MySQLSession *session,
                                     const std::string &schema_name,
                                     const std::string &object_name,
                                     const UniversalId &db_object_id) {
  v2::QueryEntryObject::query_entries(session, schema_name, object_name,
                                      db_object_id);

  for (auto &[_, v] : m_objects) {
    if (!v->user_ownership_field.has_value()) continue;

    v->user_ownership_field->field = v->get_field(v->user_ownership_field->uid);
  }
}

QueryEntryObject::UniversalId QueryEntryObject::query_object(
    MySQLSession *session, const UniversalId &db_object_id, Object *obj) {
  entry::UniversalId object_id;

  mysqlrouter::sqlstring q{
      "SELECT object.id, object.kind,"
      " CAST(db_object.crud_operations AS UNSIGNED),"
      " row_ownership_field_id"
      "  FROM mysql_rest_service_metadata.object"
      "  JOIN mysql_rest_service_metadata.db_object"
      "    ON object.db_object_id = db_object.id"
      "  WHERE object.db_object_id=?"};
  q << db_object_id;

  auto res = query_one(session, q.str());

  if (nullptr == res.get()) return {};

  auto base_table = obj->base_tables.back();

  helper::MySQLRow row(*res, nullptr, res->size());
  row.unserialize_with_converter(&object_id, entry::UniversalId::from_raw);
  row.unserialize_with_converter(&obj->kind, convert_kind);
  row.unserialize(&base_table->crud_operations);
  row.unserialize_with_converter(&obj->user_ownership_field,
                                 from_optional_user_ownership_field_id);

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
      // TODO reduce_to_value_of_field_id will be removed
      " object_reference.unnest OR "
      "   object_reference.reduce_to_value_of_field_id IS NOT NULL,"
      " CAST(object_reference.crud_operations AS UNSIGNED),"
      " object_reference.row_ownership_field_id"
      " FROM mysql_rest_service_metadata.object_field"
      " JOIN mysql_rest_service_metadata.object_reference"
      "  ON object_field.represents_reference_id = object_reference.id"
      " WHERE object_field.object_id = ?";
  query_ << object_id;
}

void QueryEntryObject::on_reference_row(const ResultRow &r) {
  auto reference = std::make_shared<entry::JoinedTable>();
  auto object = std::make_shared<Object>();

  entry::UniversalId reference_id;

  helper::MySQLRow row(r, metadata_, no_od_metadata_);
  row.unserialize_with_converter(&reference_id, entry::UniversalId::from_raw);
  row.unserialize(&reference->schema);
  row.unserialize(&reference->table);
  row.unserialize(&reference->to_many);
  row.unserialize_with_converter(&reference->column_mapping,
                                 ColumnMappingConverter{reference});
  row.unserialize(&reference->unnest);
  row.unserialize(&reference->crud_operations);
  row.unserialize_with_converter(&object->user_ownership_field,
                                 from_optional_user_ownership_field_id);

  reference->table_alias = "t" + std::to_string(++m_alias_count);
  m_tables[reference_id] = reference;

  object->name = reference->table_key();
  object->base_tables.push_back(reference);
  m_objects[reference_id] = object;
}

}  // namespace v3

static std::map<std::string, entry::ColumnType> k_datatype_map{
    {"TINYINT", entry::ColumnType::INTEGER},
    {"SMALLINT", entry::ColumnType::INTEGER},
    {"MEDIUMINT", entry::ColumnType::INTEGER},
    {"INT", entry::ColumnType::INTEGER},
    {"BIGINT", entry::ColumnType::INTEGER},
    {"FLOAT", entry::ColumnType::DOUBLE},
    {"REAL", entry::ColumnType::DOUBLE},
    {"DOUBLE", entry::ColumnType::DOUBLE},
    {"DECIMAL", entry::ColumnType::DOUBLE},
    {"CHAR", entry::ColumnType::STRING},
    {"NCHAR", entry::ColumnType::STRING},
    {"VARCHAR", entry::ColumnType::STRING},
    {"NVARCHAR", entry::ColumnType::STRING},
    {"BINARY", entry::ColumnType::BINARY},
    {"VARBINARY", entry::ColumnType::BINARY},
    {"TINYTEXT", entry::ColumnType::STRING},
    {"TEXT", entry::ColumnType::STRING},
    {"MEDIUMTEXT", entry::ColumnType::STRING},
    {"LONGTEXT", entry::ColumnType::STRING},
    {"TINYBLOB", entry::ColumnType::BINARY},
    {"BLOB", entry::ColumnType::BINARY},
    {"MEDIUMBLOB", entry::ColumnType::BINARY},
    {"LONGBLOB", entry::ColumnType::BINARY},
    {"JSON", entry::ColumnType::JSON},
    {"DATETIME", entry::ColumnType::STRING},
    {"DATE", entry::ColumnType::STRING},
    {"TIME", entry::ColumnType::STRING},
    {"YEAR", entry::ColumnType::INTEGER},
    {"TIMESTAMP", entry::ColumnType::STRING},
    {"GEOMETRY", entry::ColumnType::GEOMETRY},
    {"POINT", entry::ColumnType::GEOMETRY},
    {"LINESTRING", entry::ColumnType::GEOMETRY},
    {"POLYGON", entry::ColumnType::GEOMETRY},
    {"GEOMCOLLECTION", entry::ColumnType::GEOMETRY},
    {"GEOMETRYCOLLECTION", entry::ColumnType::GEOMETRY},
    {"MULTIPOINT", entry::ColumnType::GEOMETRY},
    {"MULTILINESTRING", entry::ColumnType::GEOMETRY},
    {"MULTIPOLYGON", entry::ColumnType::GEOMETRY},
    {"BIT", entry::ColumnType::BINARY},
    {"BOOLEAN", entry::ColumnType::BOOLEAN},
    {"ENUM", entry::ColumnType::STRING},
    {"SET", entry::ColumnType::STRING}};

entry::ColumnType column_datatype_to_type(const std::string &datatype) {
  auto spc = datatype.find(' ');
  auto p = datatype.find('(');

  p = std::min(spc, p);
  if (p != std::string::npos) {
    if (auto it = k_datatype_map.find(
            mysql_harness::make_upper(datatype.substr(0, p)));
        it != k_datatype_map.end()) {
      if (it->second == entry::ColumnType::BINARY) {
        if (mysql_harness::make_upper(datatype) == "BIT(1)")
          return entry::ColumnType::BOOLEAN;
      }
      return it->second;
    }
  } else {
    if (auto it = k_datatype_map.find(mysql_harness::make_upper(datatype));
        it != k_datatype_map.end())
      return it->second;
  }
  throw std::runtime_error("Unknown datatype " + datatype);
}

}  // namespace database
}  // namespace mrs
