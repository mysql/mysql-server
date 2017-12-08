/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dd/impl/types/charset_impl.h"

#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "sql/dd/impl/raw/object_keys.h"
#include "sql/dd/impl/raw/raw_record.h"    // Raw_record
#include "sql/dd/impl/tables/character_sets.h" // Character_sets
#include "sql/dd/impl/transaction_impl.h"  // Open_dictionary_tables_ctx

using dd::tables::Character_sets;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Charset implementation.
///////////////////////////////////////////////////////////////////////////

const Entity_object_table &Charset::OBJECT_TABLE()
{
  return Character_sets::instance();
}

const Object_type &Charset::TYPE()
{
  static Charset_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Charset_impl implementation.
///////////////////////////////////////////////////////////////////////////

bool Charset_impl::validate() const
{
  if (m_default_collation_id == INVALID_OBJECT_ID)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Charset_impl::OBJECT_TABLE().name().c_str(),
             "Collation ID is not set");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Charset_impl::restore_attributes(const Raw_record &r)
{
  restore_id(r, Character_sets::FIELD_ID);
  restore_name(r, Character_sets::FIELD_NAME);

  m_mb_max_length= r.read_uint(Character_sets::FIELD_MB_MAX_LENGTH);
  m_comment=       r.read_str(Character_sets::FIELD_COMMENT);

  m_default_collation_id=
    r.read_ref_id(
      Character_sets::FIELD_DEFAULT_COLLATION_ID);

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Charset_impl::store_attributes(Raw_record *r)
{
  return store_id(r, Character_sets::FIELD_ID) ||
         store_name(r, Character_sets::FIELD_NAME) ||
         r->store_ref_id(Character_sets::FIELD_DEFAULT_COLLATION_ID,
                         m_default_collation_id) ||
         r->store(Character_sets::FIELD_COMMENT, m_comment) ||
         r->store(Character_sets::FIELD_MB_MAX_LENGTH, m_mb_max_length);
}

///////////////////////////////////////////////////////////////////////////

bool Charset::update_id_key(id_key_type *key, Object_id id)
{
  key->update(id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Charset::update_name_key(name_key_type *key, const String_type &name)
{ return Character_sets::update_object_key(key, name); }

///////////////////////////////////////////////////////////////////////////
// Charset_type implementation.
///////////////////////////////////////////////////////////////////////////

void Charset_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Character_sets>();
}

///////////////////////////////////////////////////////////////////////////

}
