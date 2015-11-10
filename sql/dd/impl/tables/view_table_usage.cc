/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "dd/impl/tables/view_table_usage.h"

#include "my_base.h"                      // HA_WHOLE_KEY
#include "field.h"                        // Field

#include "dd/impl/object_key.h"           // dd::Object_key
#include "dd/impl/raw/object_keys.h"      // dd::Parent_id_range_key
#include "dd/impl/raw/raw_key.h"          // dd::Raw_key
#include "dd/impl/raw/raw_table.h"        // dd::Raw_table

#include <sstream>      // std::stringstream

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

// Primary key class for VIEW_TABLE_USAGE table.

class View_table_usage_pk : public Object_key
{
public:
  View_table_usage_pk(Object_id view_id,
                      const std::string &table_catalog,
                      const std::string &table_schema,
                      const std::string &table_name)
   :m_view_id(view_id),
    m_table_catalog(table_catalog),
    m_table_schema(table_schema),
    m_table_name(table_name)
  { }

public:
  virtual Raw_key *create_access_key(Raw_table *db_table) const;

  virtual std::string str() const;

private:
  Object_id m_view_id;

  std::string m_table_catalog;
  std::string m_table_schema;
  std::string m_table_name;
};

///////////////////////////////////////////////////////////////////////////

Object_key *View_table_usage::create_key_by_view_id(
  Object_id view_id)
{
  return new (std::nothrow) Parent_id_range_key(0, FIELD_VIEW_ID, view_id);
}

///////////////////////////////////////////////////////////////////////////

Object_key *View_table_usage::create_primary_key(
  Object_id view_id,
  const std::string &table_catalog,
  const std::string &table_schema,
  const std::string &table_name)
{
  return new (std::nothrow) View_table_usage_pk(view_id,
                                 table_catalog,
                                 table_schema,
                                 table_name);
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

Raw_key *View_table_usage_pk::create_access_key(Raw_table *db_table) const
{
  const int INDEX_NO= 0;

  TABLE *t= db_table->get_table();

  t->use_all_columns();

  t->field[View_table_usage::FIELD_VIEW_ID]->store(m_view_id, true);

  t->field[View_table_usage::FIELD_TABLE_CATALOG]->store(
    m_table_catalog.c_str(),
    m_table_catalog.length(),
    system_charset_info);

  t->field[View_table_usage::FIELD_TABLE_SCHEMA]->store(
    m_table_schema.c_str(),
    m_table_schema.length(),
    system_charset_info);

  t->field[View_table_usage::FIELD_TABLE_NAME]->store(
    m_table_name.c_str(),
    m_table_name.length(),
    system_charset_info);

  KEY *key_info= t->key_info + INDEX_NO;

  Raw_key *k= new (std::nothrow) Raw_key(INDEX_NO,
                          key_info->key_length,
                          HA_WHOLE_KEY);

  key_copy(k->key, t->record[0], key_info, k->key_len);

  return k;
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin inspected */
std::string View_table_usage_pk::str() const
{
  std::stringstream ss;
  ss << m_view_id << ":"
     << m_table_catalog << ":"
     << m_table_schema << ":"
     << m_table_name;
  return ss.str();
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

}
}
