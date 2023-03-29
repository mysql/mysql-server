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

#include <utility>
#include "helper/json/text_to.h"
#include "helper/mysql_row.h"
#include "mysql/harness/logging/logging.h"

#include <iostream>
#include "mysqld_error.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

void QueryEntryObject::query_entries(MySQLSession *session,
                                     const std::string &schema,
                                     const std::string &schema_object) {
  object.schema = schema;
  object.schema_object = schema_object;

  query_ =
      "SELECT object_field.id,"
      " object_field.parent_reference_id,"
      " object_field.name,"
      " object_field.db_name,"
      " object_field.enabled,"
      " object_field.allow_filtering,"
      " object_reference.id,"
      " object_reference.reduce_to_value_of_field_id,"
      " object_reference.reference_mapping->>'$.referencedSchema',"
      " object_reference.reference_mapping->>'$.referencedSchemaObject',"
      " object_reference.reference_mapping->'$.columnMapping',"
      " object_reference.reference_mapping->'$.toMany',"
      " object_reference.unnest,"
      " object_reference.crud_operations"
      " FROM mysql_rest_service_metadata.object_field"
      " LEFT JOIN mysql_rest_service_metadata.object_reference"
      "  ON object_field.represents_reference_id = object_reference.id"
      " WHERE object_field.object_id = ("
      "  SELECT object.id FROM mysql_rest_service_metadata.db_object"
      "  JOIN mysql_rest_service_metadata.db_schema"
      "   ON db_schema_id=db_schema.id"
      "  JOIN mysql_rest_service_metadata.object"
      "   ON object.db_object_id=db_object.id"
      "  WHERE db_schema.name=? AND db_object.name=? LIMIT 1)";
  query_ << schema << schema_object;

  execute(session);

  for (const auto &field : m_unparented_fields) {
    m_fields_by_reference_id[field->parent_reference_id->to_string()]
        ->reference->fields.push_back(field);
  }
}

void QueryEntryObject::on_row(const Row &r) {
  class ColumnMappingConverter {
   public:
    void operator()(ObjectField::ColumnMapping *out, const char *value) const {
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

  auto field = std::make_shared<ObjectField>();

  helper::MySQLRow row(r);

  row.unserialize_with_converter(&field->id, entry::UniversalId::from_raw);
  row.unserialize_with_converter(&field->parent_reference_id,
                                 entry::UniversalId::from_raw_optional);
  row.unserialize(&field->name);
  row.unserialize(&field->db_name);
  row.unserialize(&field->enabled);
  row.unserialize(&field->allow_filtering);

  std::optional<entry::UniversalId> reference_id;
  row.unserialize_with_converter(&reference_id,
                                 entry::UniversalId::from_raw_optional);
  if (reference_id.has_value()) {
    ObjectField::Reference reference;

    reference.id = *reference_id;
    row.unserialize_with_converter(&reference.reduce_to_field_id,
                                   entry::UniversalId::from_raw_optional);
    row.unserialize(&reference.schema_name);
    row.unserialize(&reference.object_name);
    row.unserialize_with_converter(&reference.column_mapping,
                                   ColumnMappingConverter{});
    row.unserialize(&reference.to_many);
    row.unserialize(&reference.unnest);
    row.unserialize(&reference.crud_operations);

    reference.table_alias = "t" + std::to_string(m_alias_count++);

    m_fields_by_reference_id[reference.id.to_string()] = field;

    field->reference = std::move(reference);
  }

  if (field->parent_reference_id) {
    m_unparented_fields.emplace_back(field);
  } else {
    object.fields.emplace_back(field);
  }
}

}  // namespace database
}  // namespace mrs
