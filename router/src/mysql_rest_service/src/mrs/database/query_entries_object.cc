/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "mrs/database/query_entries_object.h"

#include <string>
#include <utility>
#include "helper/json/text_to.h"
#include "helper/mysql_row.h"
#include "mysql/harness/logging/logging.h"

#include "mysqld_error.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

void QueryEntryObject::query_entries(MySQLSession *session,
                                     const std::string &schema,
                                     const std::string &schema_object) {
  entry::UniversalId object_id;
  {
    mysqlrouter::sqlstring q{
        "SELECT object.id FROM mysql_rest_service_metadata.db_object"
        "  JOIN mysql_rest_service_metadata.db_schema"
        "   ON db_schema_id=db_schema.id"
        "  JOIN mysql_rest_service_metadata.object"
        "   ON object.db_object_id=db_object.id"
        "  WHERE db_schema.name=? AND db_object.name=?"};
    q << schema << schema_object;

    auto res = query_one(session, q.str());
    entry::UniversalId::from_raw(&object_id, (*res)[0]);
  }

  auto base_table = std::make_shared<entry::BaseTable>();
  base_table->schema = schema;
  base_table->table = schema_object;
  base_table->table_alias = "t";
  // XXX base_table->crud_operations
  m_tables[entry::UniversalId{}] = base_table;

  object = std::make_shared<Object>();
  object->base_tables.push_back(base_table);
  m_objects[entry::UniversalId{}] = object;

  m_loading_references = true;
  query_ =
      "SELECT"
      " object_reference.id,"
      " object_reference.reduce_to_value_of_field_id,"
      " object_reference.reference_mapping->>'$.referenced_schema',"
      " object_reference.reference_mapping->>'$.referenced_table',"
      " object_reference.reference_mapping->'$.column_mapping',"
      " object_reference.reference_mapping->'$.to_many',"
      " object_reference.unnest,"
      " object_reference.crud_operations"
      " FROM mysql_rest_service_metadata.object_field"
      " JOIN mysql_rest_service_metadata.object_reference"
      "  ON object_field.represents_reference_id = object_reference.id"
      " WHERE object_field.object_id = ?";
  query_ << object_id;

  execute(session);

  m_loading_references = false;
  query_ =
      "SELECT object_field.id,"
      " object_field.parent_reference_id,"
      " object_field.represents_reference_id,"
      " object_field.name,"
      " object_field.position,"
      " object_field.db_column->>'$.name',"
      " object_field.db_column->>'$.datatype',"
      " object_field.db_column->>'$.auto_inc',"
      " object_field.db_column->>'$.not_null',"
      " object_field.db_column->>'$.is_primary',"
      " object_field.db_column->>'$.is_unique',"
      " object_field.db_column->>'$.is_generated',"
      " object_field.enabled,"
      " object_field.allow_filtering,"
      " object_field.no_check"
      " FROM mysql_rest_service_metadata.object_field"
      " WHERE object_field.object_id = ?"
      " ORDER BY object_field.represents_reference_id";
  query_ << object_id;

  execute(session);

  assert(m_pending_reduce_to_field.empty());
}

void QueryEntryObject::on_row(const Row &r) {
  if (m_loading_references)
    on_reference_row(r);
  else
    on_field_row(r);
}

void QueryEntryObject::on_reference_row(const Row &r) {
  class ColumnMappingConverter {
   public:
    void operator()(entry::JoinedTable::ColumnMapping *out,
                    const char *value) const {
      if (nullptr == value) {
        *out = {};
        return;
      }

      rapidjson::Document doc = helper::json::text_to_document(value);
      if (!doc.IsObject()) {
        throw std::runtime_error("bad metadata");
      }

      out->clear();
      for (auto m = doc.MemberBegin(); m != doc.MemberEnd(); ++m) {
        if (!m->value.IsString()) {
          throw std::runtime_error("bad metadata");
        }
        out->emplace_back(m->name.GetString(), m->value.GetString());
      }
    }
  };

  auto reference = std::make_shared<entry::JoinedTable>();

  entry::UniversalId reference_id;
  std::optional<entry::UniversalId> reduce_to_field_id;

  helper::MySQLRow row(r);
  row.unserialize_with_converter(&reference_id, entry::UniversalId::from_raw);
  row.unserialize_with_converter(&reduce_to_field_id,
                                 entry::UniversalId::from_raw_optional);
  row.unserialize(&reference->schema);
  row.unserialize(&reference->table);
  row.unserialize_with_converter(&reference->column_mapping,
                                 ColumnMappingConverter{});
  row.unserialize(&reference->to_many);
  row.unserialize(&reference->unnest);
  row.unserialize(&reference->crud_operations);

  // if (reference->unnest && reference->to_many)
  //   throw std::runtime_error("Invalid object definition for reference to " +
  //                            reference->schema + "." + reference->table + "
  //                            (" + reference_id.to_string() +
  //                            "): cannot unnest a 1:n reference");

  reference->table_alias = "t" + std::to_string(++m_alias_count);
  m_tables[reference_id] = reference;

  if (reduce_to_field_id)
    m_pending_reduce_to_field[*reduce_to_field_id].push_back(reference);

  auto object = std::make_shared<Object>();
  object->name = reference->table_key();
  object->base_tables.push_back(reference);
  m_objects[reference_id] = object;
}

// regular
// unnest 1:1
// unnest 1:n
// unnest n:m

void QueryEntryObject::on_field_row(const Row &r) {
  auto field = std::make_shared<entry::ObjectField>();

  helper::MySQLRow row(r);

  entry::UniversalId field_id;
  entry::UniversalId parent_reference_id;
  std::optional<entry::UniversalId> represents_reference_id;

  row.unserialize_with_converter(&field_id, entry::UniversalId::from_raw);
  row.unserialize_with_converter(&parent_reference_id,
                                 entry::UniversalId::from_raw_zero_on_null);
  row.unserialize_with_converter(&represents_reference_id,
                                 entry::UniversalId::from_raw_optional);
  row.unserialize(&field->name);
  row.unserialize(&field->position);
  row.unserialize(&field->db_name);
  row.unserialize(&field->db_datatype);
  row.unserialize(&field->db_auto_inc);
  row.unserialize(&field->db_not_null);
  row.unserialize(&field->db_is_primary);
  row.unserialize(&field->db_is_unique);
  row.unserialize(&field->db_is_generated);
  row.unserialize(&field->enabled);
  row.unserialize(&field->allow_filtering);
  row.unserialize(&field->no_check);

  if (auto refs = m_pending_reduce_to_field.find(field_id);
      refs != m_pending_reduce_to_field.end()) {
    for (auto ref : refs->second) ref->reduce_to_field = field;
    m_pending_reduce_to_field.erase(refs);
  }

  std::shared_ptr<Object> parent_object = m_objects[parent_reference_id];
  auto table = m_tables[parent_reference_id];

  if (represents_reference_id) {
    std::shared_ptr<entry::FieldSource> reference =
        m_tables.at(*represents_reference_id);

    bool unnest = false;
    if (auto join = std::dynamic_pointer_cast<entry::JoinedTable>(reference);
        join) {
      unnest = join->unnest;
    }

    // if the represented reference is unnested, the field itself is just a
    // placeholder and isn't included in the output object
    if (unnest) {
      // fields in the unnested object must be added to this object

      for (auto f : m_objects[*represents_reference_id]->fields) {
        parent_object->fields.push_back(f);
      }
      m_objects[*represents_reference_id] = parent_object;
    } else {
      field->nested_object = m_objects[*represents_reference_id];
      field->nested_object->parent = parent_object;

      parent_object->fields.push_back(field);
    }
  } else {
    field->source = table;
    parent_object->fields.push_back(field);
  }
}

}  // namespace database
}  // namespace mrs
