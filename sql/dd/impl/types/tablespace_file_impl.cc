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

#include "dd/impl/types/tablespace_file_impl.h"

#include "mysqld_error.h"                    // ER_*

#include "dd/impl/collection_impl.h"         // Collection
#include "dd/impl/properties_impl.h"         // Properties_impl
#include "dd/impl/transaction_impl.h"        // Open_dictionary_tables_ctx
#include "dd/impl/raw/raw_record.h"          // Raw_record
#include "dd/impl/tables/tablespace_files.h" // Tablespace_files
#include "dd/impl/types/tablespace_impl.h"   // Tablespace_impl

#include <sstream>

using dd::tables::Tablespace_files;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Tablespace_file implementation.
///////////////////////////////////////////////////////////////////////////

const Object_table &Tablespace_file::OBJECT_TABLE()
{
  return Tablespace_files::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Tablespace_file::TYPE()
{
  static Tablespace_file_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Tablespace_file_impl implementation.
///////////////////////////////////////////////////////////////////////////

Tablespace_file_impl::Tablespace_file_impl()
 :m_ordinal_position(0),
  m_se_private_data(new Properties_impl())
{ } /* purecov: tested */

///////////////////////////////////////////////////////////////////////////

Tablespace &Tablespace_file_impl::tablespace()
{
  return *m_tablespace;
}

///////////////////////////////////////////////////////////////////////////

void Tablespace_file_impl::drop()
{
  m_tablespace->file_collection()->remove(this);
}

///////////////////////////////////////////////////////////////////////////

bool Tablespace_file_impl::set_se_private_data_raw(
  const std::string &se_private_data_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(se_private_data_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_se_private_data.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Tablespace_file_impl::validate() const
{
  if (!m_tablespace)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Tablespace_file_impl::OBJECT_TABLE().name().c_str(),
             "No tablespace associated with this file.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Tablespace_file_impl::restore_attributes(const Raw_record &r)
{
  if (check_parent_consistency(
        m_tablespace, r.read_ref_id(Tablespace_files::FIELD_TABLESPACE_ID)))
    return true;

  m_ordinal_position= r.read_uint(Tablespace_files::FIELD_ORDINAL_POSITION);
  m_filename=         r.read_str(Tablespace_files::FIELD_FILE_NAME);

  m_se_private_data.reset(
    Properties_impl::parse_properties(
      r.read_str(Tablespace_files::FIELD_SE_PRIVATE_DATA)));


  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Tablespace_file_impl::store_attributes(Raw_record *r)
{
  return r->store(Tablespace_files::FIELD_ORDINAL_POSITION, m_ordinal_position) ||
         r->store(Tablespace_files::FIELD_FILE_NAME, m_filename) ||
         r->store(Tablespace_files::FIELD_SE_PRIVATE_DATA, *m_se_private_data) ||
         r->store(Tablespace_files::FIELD_TABLESPACE_ID, m_tablespace->id());
}

///////////////////////////////////////////////////////////////////////////

void
Tablespace_file_impl::serialize(WriterVariant *wv) const
{

}

void
Tablespace_file_impl::deserialize(const RJ_Document *d)
{

}

///////////////////////////////////////////////////////////////////////////

void Tablespace_file_impl::debug_print(std::string &outb) const
{
  std::stringstream ss;
  ss
    << "TABLESPACE FILE OBJECT: { "
    << "m_ordinal_position: " << m_ordinal_position << "; "
    << "m_filename: " << m_filename << "; "
    << "m_se_private_data " << m_se_private_data->raw_string() << "; "
    << "m_tablespace {OID: " << m_tablespace->id() << "}";

  outb= ss.str();
}

///////////////////////////////////////////////////////////////////////////

Object_key *Tablespace_file_impl::create_primary_key() const
{
  return Tablespace_files::create_primary_key(
    m_tablespace->id(), m_ordinal_position);
}

bool Tablespace_file_impl::has_new_primary_key() const
{
  return m_tablespace->has_new_primary_key();
}

///////////////////////////////////////////////////////////////////////////
// Tablespace_file_impl::Factory implementation.
///////////////////////////////////////////////////////////////////////////

Collection_item *Tablespace_file_impl::Factory::create_item() const
{
  Tablespace_file_impl *f= new (std::nothrow) Tablespace_file_impl();
  f->m_tablespace= m_ts;
  return f;
}

///////////////////////////////////////////////////////////////////////////

#ifndef DBUG_OFF
Tablespace_file_impl::
Tablespace_file_impl(const Tablespace_file_impl &src,
                     Tablespace_impl *parent)
  : Weak_object(src),
    m_ordinal_position(src.m_ordinal_position), m_filename(src.m_filename),
    m_se_private_data(Properties_impl::
                      parse_properties(src.m_se_private_data->raw_string())),
    m_tablespace(parent)
{}
#endif /* !DBUG_OFF */

///////////////////////////////////////////////////////////////////////////
// Tablespace_file_type implementation.
///////////////////////////////////////////////////////////////////////////

void Tablespace_file_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Tablespace_files>();
}

///////////////////////////////////////////////////////////////////////////

}
