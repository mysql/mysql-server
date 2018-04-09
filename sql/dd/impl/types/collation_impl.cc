/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/dd/impl/types/collation_impl.h"

#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"  // ER_*
#include "sql/dd/impl/raw/object_keys.h"
#include "sql/dd/impl/raw/raw_record.h"     // Raw_record
#include "sql/dd/impl/tables/collations.h"  // Collations
#include "sql/dd/impl/transaction_impl.h"   // Open_dictionary_tables_ctx

using dd::tables::Collations;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Collation_impl implementation.
///////////////////////////////////////////////////////////////////////////

bool Collation_impl::validate() const {
  if (m_charset_id == INVALID_OBJECT_ID) {
    my_error(ER_INVALID_DD_OBJECT, MYF(0), DD_table::instance().name().c_str(),
             "Charset ID is not set");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Collation_impl::restore_attributes(const Raw_record &r) {
  restore_id(r, Collations::FIELD_ID);
  restore_name(r, Collations::FIELD_NAME);

  m_is_compiled = r.read_bool(Collations::FIELD_IS_COMPILED);
  m_sort_length = r.read_uint(Collations::FIELD_SORT_LENGTH);
  m_charset_id = r.read_ref_id(Collations::FIELD_CHARACTER_SET_ID);
  m_pad_attribute = r.read_str(Collations::FIELD_PAD_ATTRIBUTE);

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Collation_impl::store_attributes(Raw_record *r) {
  return store_id(r, Collations::FIELD_ID) ||
         store_name(r, Collations::FIELD_NAME) ||
         r->store_ref_id(Collations::FIELD_CHARACTER_SET_ID, m_charset_id) ||
         r->store(Collations::FIELD_IS_COMPILED, m_is_compiled) ||
         r->store(Collations::FIELD_SORT_LENGTH, m_sort_length) ||
         r->store(Collations::FIELD_PAD_ATTRIBUTE, m_pad_attribute);
}

///////////////////////////////////////////////////////////////////////////

bool Collation::update_id_key(Id_key *key, Object_id id) {
  key->update(id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Collation::update_name_key(Name_key *key, const String_type &name) {
  return Collations::update_object_key(key, name);
}

///////////////////////////////////////////////////////////////////////////

const Object_table &Collation_impl::object_table() const {
  return DD_table::instance();
}

///////////////////////////////////////////////////////////////////////////

void Collation_impl::register_tables(Open_dictionary_tables_ctx *otx) {
  otx->add_table<Collations>();
}

///////////////////////////////////////////////////////////////////////////

}  // namespace dd
