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

#include "dd/impl/types/tablespace_file_impl.h"

#include <sstream>

#include "dd/impl/properties_impl.h"         // Properties_impl
#include "dd/impl/raw/raw_record.h"          // Raw_record
#include "dd/impl/sdi_impl.h"                // sdi read/write functions
#include "dd/impl/tables/tablespace_files.h" // Tablespace_files
#include "dd/impl/transaction_impl.h"        // Open_dictionary_tables_ctx
#include "dd/impl/types/tablespace_impl.h"   // Tablespace_impl
#include "dd/string_type.h"                  // dd::String_type
#include "dd/types/object_table.h"
#include "dd/types/weak_object.h"
#include "error_handler.h"                   // Internal_error_handler
#include "m_string.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"                    // ER_*
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

using dd::tables::Tablespace_files;

namespace dd {

class Object_key;
class Sdi_rcontext;
class Sdi_wcontext;

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

Tablespace_file_impl::Tablespace_file_impl(Tablespace_impl *tablespace)
 :m_ordinal_position(0),
  m_se_private_data(new Properties_impl()),
  m_tablespace(tablespace)
{ }

///////////////////////////////////////////////////////////////////////////

const Tablespace &Tablespace_file_impl::tablespace() const
{
  return *m_tablespace;
}

Tablespace &Tablespace_file_impl::tablespace()
{
  return *m_tablespace;
}

///////////////////////////////////////////////////////////////////////////

bool Tablespace_file_impl::set_se_private_data_raw(
  const String_type &se_private_data_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(se_private_data_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_se_private_data.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

class Tablespace_filename_error_handler : public Internal_error_handler
{
  const char *name;
public:
  Tablespace_filename_error_handler(const char *name_arg)
    : name(name_arg)
  { }

  virtual bool handle_condition(THD*,
                                uint sql_errno,
                                const char*,
                                Sql_condition::enum_severity_level*,
                                const char*)
  {
    if (sql_errno == ER_DUP_ENTRY)
    {
      my_error(ER_TABLESPACE_DUP_FILENAME, MYF(0), name);
      return true;
    }
    return false;
  }
};


bool Tablespace_file_impl::store(Open_dictionary_tables_ctx *otx)
{
  /*
    Translate ER_DUP_ENTRY errors to the more user-friendly
    ER_TABLESPACE_DUP_FILENAME. We should not report ER_DUP_ENTRY
    in any other cases (that would be a code bug).
  */
  Tablespace_filename_error_handler error_handler(m_tablespace->name().c_str());
  otx->get_thd()->push_internal_handler(&error_handler);
  bool error= Weak_object_impl::store(otx);
  otx->get_thd()->pop_internal_handler();
  return error;
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

static_assert(Tablespace_files::FIELD_SE_PRIVATE_DATA==3,
              "Tablespace_files definition has changed, review (de)ser memfuns");
void
Tablespace_file_impl::serialize(Sdi_wcontext*, Sdi_writer *w) const
{
  w->StartObject();
  write(w, m_ordinal_position, STRING_WITH_LEN("ordinal_position"));
  write(w, m_filename, STRING_WITH_LEN("filename"));
  write_properties(w, m_se_private_data, STRING_WITH_LEN("se_private_data"));
  w->EndObject();
}

///////////////////////////////////////////////////////////////////////////

bool
Tablespace_file_impl::deserialize(Sdi_rcontext*, const RJ_Value &val)
{
  read(&m_ordinal_position, val, "ordinal_position");
  read(&m_filename, val, "filename");
  read_properties(&m_se_private_data, val, "se_private_data");
  return false;
}

///////////////////////////////////////////////////////////////////////////

void Tablespace_file_impl::debug_print(String_type &outb) const
{
  dd::Stringstream_type ss;
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

Tablespace_file_impl::
Tablespace_file_impl(const Tablespace_file_impl &src,
                     Tablespace_impl *parent)
  : Weak_object(src),
    m_ordinal_position(src.m_ordinal_position), m_filename(src.m_filename),
    m_se_private_data(Properties_impl::
                      parse_properties(src.m_se_private_data->raw_string())),
    m_tablespace(parent)
{}

///////////////////////////////////////////////////////////////////////////
// Tablespace_file_type implementation.
///////////////////////////////////////////////////////////////////////////

void Tablespace_file_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Tablespace_files>();
}

///////////////////////////////////////////////////////////////////////////

}
