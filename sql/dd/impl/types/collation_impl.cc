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

#include "dd/impl/types/collation_impl.h"

#include "dd/impl/raw/object_keys.h"     // Primary_id_key
#include "dd/impl/raw/raw_record.h"      // Raw_record
#include "dd/impl/tables/collations.h"   // Collations
#include "dd/impl/transaction_impl.h"    // Open_dictionary_tables_ctx
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"                // ER_*

using dd::tables::Collations;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Collation implementation.
///////////////////////////////////////////////////////////////////////////

const Dictionary_object_table &Collation::OBJECT_TABLE()
{
  return Collations::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Collation::TYPE()
{
  static Collation_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Collation_impl implementation.
///////////////////////////////////////////////////////////////////////////

bool Collation_impl::validate() const
{
  if (m_charset_id == INVALID_OBJECT_ID)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Collation_impl::OBJECT_TABLE().name().c_str(),
             "Charset ID is not set");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Collation_impl::restore_attributes(const Raw_record &r)
{
  restore_id(r, Collations::FIELD_ID);
  restore_name(r, Collations::FIELD_NAME);

  m_is_compiled= r.read_bool(Collations::FIELD_IS_COMPILED);
  m_sort_length= r.read_uint(Collations::FIELD_SORT_LENGTH);
  m_charset_id= r.read_ref_id(Collations::FIELD_CHARACTER_SET_ID);
  m_pad_attribute= r.read_str(Collations::FIELD_PAD_ATTRIBUTE);

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Collation_impl::store_attributes(Raw_record *r)
{
  return store_id(r, Collations::FIELD_ID) ||
         store_name(r, Collations::FIELD_NAME) ||
         r->store_ref_id(Collations::FIELD_CHARACTER_SET_ID, m_charset_id) ||
         r->store(Collations::FIELD_IS_COMPILED, m_is_compiled) ||
         r->store(Collations::FIELD_SORT_LENGTH, m_sort_length) ||
         r->store(Collations::FIELD_PAD_ATTRIBUTE, m_pad_attribute);
}

///////////////////////////////////////////////////////////////////////////

bool Collation::update_id_key(id_key_type *key, Object_id id)
{
  key->update(id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Collation::update_name_key(name_key_type *key, const String_type &name)
{ return Collations::update_object_key(key, name); }

///////////////////////////////////////////////////////////////////////////
// Collation_type implementation.
///////////////////////////////////////////////////////////////////////////

void Collation_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Collations>();
}

///////////////////////////////////////////////////////////////////////////

}
