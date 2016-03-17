/* Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/raw/object_keys.h"

#include "my_base.h"                   // HA_WHOLE_KEY
#include "field.h"                     // Field
#include "key.h"                       // KEY
#include "table.h"                     // TABLE

#include "dd/impl/raw/raw_key.h"       // dd::Raw_key
#include "dd/impl/raw/raw_table.h"     // dd::Raw_table

#include <sstream>

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Primary_id_key
///////////////////////////////////////////////////////////////////////////

Raw_key *Primary_id_key::create_access_key(Raw_table *db_table) const
{
  // Positional index of PK-Index on object-id field.
  // It is 0 for any DD-table (PK-Index is the 1st index on a DD-table).
  const int ID_INDEX_NO= 0;

  // Positional index of PK-object-id-column.
  // It is 0 for any DD-table (object-id is the 1st column on a DD-table).
  const int ID_COLUMN_NO= 0;

  TABLE *t= db_table->get_table();

  t->use_all_columns();

  t->field[ID_COLUMN_NO]->store(m_object_id, true);

  KEY *key_info= t->key_info + ID_INDEX_NO;

  Raw_key *k= new (std::nothrow) Raw_key(ID_INDEX_NO,
                                key_info->key_length,
                                HA_WHOLE_KEY);

  key_copy(k->key, t->record[0], key_info, k->key_len);

  return k;
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin inspected */
std::string Primary_id_key::str() const
{
  std::stringstream ss;
  ss << m_object_id;
  return ss.str();
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////
// Parent_id_range_key
///////////////////////////////////////////////////////////////////////////

Raw_key *Parent_id_range_key::create_access_key(Raw_table *db_table) const
{
  TABLE *t= db_table->get_table();

  t->use_all_columns();

  t->field[m_id_column_no]->store(m_object_id, true);

  KEY *key_info= t->key_info + m_id_index_no;

  Raw_key *k= new (std::nothrow) Raw_key(m_id_index_no,
                                key_info->key_length,
                                1 /* Use 1st column */);

  key_copy(k->key, t->record[0], key_info, k->key_len);

  return k;
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin inspected */
std::string Parent_id_range_key::str() const
{
  // XXX: not needed
  std::stringstream ss;
  ss << m_id_column_no << ":" << m_object_id;
  return ss.str();
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////
// Global_name_key
///////////////////////////////////////////////////////////////////////////

Raw_key *Global_name_key::create_access_key(Raw_table *db_table) const
{
  TABLE *t= db_table->get_table();

  t->use_all_columns();

  t->field[m_name_column_no]->store(m_object_name.c_str(),
                                    m_object_name.length(),
                                    &my_charset_bin);

  KEY *key_info= t->key_info + 1 /* index_no */;

  Raw_key *k= new (std::nothrow) Raw_key(1 /* index_no */,
                                key_info->key_length,
                                HA_WHOLE_KEY);

  key_copy(k->key, t->record[0], key_info, k->key_len);

  return k;
}

///////////////////////////////////////////////////////////////////////////
// Item_name_key
///////////////////////////////////////////////////////////////////////////

Raw_key *Item_name_key::create_access_key(Raw_table *db_table) const
{
  TABLE *t= db_table->get_table();

  t->use_all_columns();

  t->field[m_container_id_column_no]->store(m_container_id, true);

  t->field[m_name_column_no]->store(m_object_name.c_str(),
                                    m_object_name.length(),
                                    &my_charset_bin);

  KEY *key_info= t->key_info + 1 /* index_no */;

  Raw_key *k= new (std::nothrow) Raw_key(1 /* index_no */,
                                key_info->key_length,
                                HA_WHOLE_KEY);

  key_copy(k->key, t->record[0], key_info, k->key_len);

  return k;
}

///////////////////////////////////////////////////////////////////////////

std::string Item_name_key::str() const
{
  std::stringstream ss;
  ss << m_container_id << ":" << m_object_name;
  return ss.str();
}

///////////////////////////////////////////////////////////////////////////
// Se_private_id_key
///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Raw_key *Se_private_id_key::create_access_key(Raw_table *db_table) const
{
  key_part_map keypart_map= 1;
  TABLE *t= db_table->get_table();

  t->use_all_columns();

  DBUG_ASSERT(m_engine);
  t->field[m_engine_column_no]->store(m_engine->c_str(), m_engine->length(),
                                     &my_charset_bin);
  t->field[m_engine_column_no]->set_notnull();

  t->field[m_private_id_column_no]->store(m_private_id, true);
  t->field[m_private_id_column_no]->set_notnull();

  if (m_private_id != INVALID_OBJECT_ID)
    keypart_map= HA_WHOLE_KEY;

  KEY *key_info= t->key_info + m_index_no;

  Raw_key *k= new (std::nothrow) Raw_key(m_index_no,
                          key_info->key_length,
                          keypart_map);

  key_copy(k->key, t->record[0], key_info, k->key_len);

  return k;
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

std::string Se_private_id_key::str() const
{
  std::stringstream ss;
  ss << *m_engine << ":" << m_private_id;
  return ss.str();
}

///////////////////////////////////////////////////////////////////////////
// Composite_pk
///////////////////////////////////////////////////////////////////////////

Raw_key *Composite_pk::create_access_key(Raw_table *db_table) const
{
  TABLE *t= db_table->get_table();

  t->use_all_columns();

  t->field[m_first_column_no]->store(m_first_id, true);
  t->field[m_second_column_no]->store(m_second_id, true);

  KEY *key_info= t->key_info + m_index_no;

  Raw_key *k= new (std::nothrow) Raw_key(m_index_no,
                          key_info->key_length,
                          HA_WHOLE_KEY);

  key_copy(k->key, t->record[0], key_info, k->key_len);

  return k;
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin inspected */
std::string Composite_pk::str() const
{
  std::stringstream ss;
  ss << m_first_id << ":" << m_second_id;
  return ss.str();
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////
// Routine_name_key
///////////////////////////////////////////////////////////////////////////

Raw_key *Routine_name_key::create_access_key(Raw_table *db_table) const
{
  TABLE *t= db_table->get_table();

  t->use_all_columns();

  t->field[m_container_id_column_no]->store(m_container_id, true);

  t->field[m_type_column_no]->store(m_type, true);

  t->field[m_name_column_no]->store(m_object_name.c_str(),
                                    m_object_name.length(),
                                    &my_charset_bin);

  KEY *key_info= t->key_info + 1 /* index_no */;

  Raw_key *k= new (std::nothrow) Raw_key(1 /* index_no */,
                                         key_info->key_length,
                                         HA_WHOLE_KEY);

  key_copy(k->key, t->record[0], key_info, k->key_len);

  return k;
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin inspected */
std::string Routine_name_key::str() const
{
  std::stringstream ss;
  ss << m_container_id << ":" << m_type << ":" << m_object_name;
  return ss.str();
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

bool Routine_name_key::operator <(const Routine_name_key &rhs) const
{
  if (m_container_id != rhs.m_container_id)
    return m_container_id < rhs.m_container_id;
  if (m_type != rhs.m_type)
    return m_type < rhs.m_type;
  // Case insensitive comparison
  return my_strcasecmp(system_charset_info,
                       m_object_name.c_str(), rhs.m_object_name.c_str()) < 0;
}

///////////////////////////////////////////////////////////////////////////
}
