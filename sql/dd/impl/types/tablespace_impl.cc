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

#include "dd/impl/types/tablespace_impl.h"

#include "mysqld_error.h"                        // ER_*

#include "dd/impl/properties_impl.h"             // Properties_impl
#include "dd/impl/transaction_impl.h"            // Open_dictionary_tables_ctx
#include "dd/impl/raw/object_keys.h"             // Primary_id_key
#include "dd/impl/raw/raw_record.h"              // Raw_record
#include "dd/impl/tables/tablespaces.h"          // Tablespaces
#include "dd/impl/tables/tablespace_files.h"     // Tablespace_files
#include "dd/impl/types/tablespace_file_impl.h"  // Tablespace_file_impl

#include <sstream>

using dd::tables::Tablespaces;
using dd::tables::Tablespace_files;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Tablespace implementation.
///////////////////////////////////////////////////////////////////////////

const Dictionary_object_table &Tablespace::OBJECT_TABLE()
{
  return Tablespaces::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Tablespace::TYPE()
{
  static Tablespace_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Tablespace_impl implementation.
///////////////////////////////////////////////////////////////////////////

Tablespace_impl::Tablespace_impl()
 :m_options(new Properties_impl()),
  m_se_private_data(new Properties_impl()),
  m_files(new Tablespace_file_collection())
{ } /* purecov: tested */

///////////////////////////////////////////////////////////////////////////

bool Tablespace_impl::set_options_raw(const std::string &options_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(options_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_options.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Tablespace_impl::set_se_private_data_raw(
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

bool Tablespace_impl::validate() const
{
  if (m_files->is_empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Tablespace_impl::OBJECT_TABLE().name().c_str(),
             "No files associated with this tablespace.");
    return true;
  }

  if (m_engine.empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Tablespace_impl::OBJECT_TABLE().name().c_str(),
             "No engine set.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Tablespace_impl::restore_children(Open_dictionary_tables_ctx *otx)
{
  return m_files->restore_items(
    Tablespace_file_impl::Factory(this),
    otx,
    otx->get_table<Tablespace_file>(),
    Tablespace_files::create_key_by_tablespace_id(this->id()));
}

///////////////////////////////////////////////////////////////////////////

bool Tablespace_impl::store_children(Open_dictionary_tables_ctx *otx)
{
  return m_files->store_items(otx);
}

///////////////////////////////////////////////////////////////////////////

bool Tablespace_impl::drop_children(Open_dictionary_tables_ctx *otx)
{
  return m_files->drop_items(
    otx,
    otx->get_table<Tablespace_file>(),
    Tablespace_files::create_key_by_tablespace_id(this->id()));
}

/////////////////////////////////////////////////////////////////////////

bool Tablespace_impl::restore_attributes(const Raw_record &r)
{
  restore_id(r, Tablespaces::FIELD_ID);
  restore_name(r, Tablespaces::FIELD_NAME);

  m_comment=         r.read_str(Tablespaces::FIELD_COMMENT);

  m_options.reset(
    Properties_impl::parse_properties(
      r.read_str(Tablespaces::FIELD_OPTIONS)));

  m_se_private_data.reset(
    Properties_impl::parse_properties(
      r.read_str(Tablespaces::FIELD_SE_PRIVATE_DATA)));

  m_engine=          r.read_str(Tablespaces::FIELD_ENGINE);

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Tablespace_impl::store_attributes(Raw_record *r)
{
  return store_id(r, Tablespaces::FIELD_ID) ||
         store_name(r, Tablespaces::FIELD_NAME) ||
         r->store(Tablespaces::FIELD_COMMENT, m_comment) ||
         r->store(Tablespaces::FIELD_OPTIONS, *m_options) ||
         r->store(Tablespaces::FIELD_SE_PRIVATE_DATA, *m_se_private_data) ||
         r->store(Tablespaces::FIELD_ENGINE, m_engine);
}

///////////////////////////////////////////////////////////////////////////

void
Tablespace_impl::serialize(WriterVariant *wv) const
{

}

void
Tablespace_impl::deserialize(const RJ_Document *d)
{

}

///////////////////////////////////////////////////////////////////////////

bool Tablespace::update_id_key(id_key_type *key, Object_id id)
{
  key->update(id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Tablespace::update_name_key(name_key_type *key,
                                      const std::string &name)
{ return Tablespaces::update_object_key(key, name); }

///////////////////////////////////////////////////////////////////////////

void Tablespace_impl::debug_print(std::string &outb) const
{
  std::stringstream ss;
  ss
    << "TABLESPACE OBJECT: { "
    << "id: {OID: " << id() << "}; "
    << "m_name: " << name() << "; "
    << "m_comment: " << m_comment << "; "
    << "m_options " << m_options->raw_string() << "; "
    << "m_se_private_data " << m_se_private_data->raw_string() << "; "
    << "m_engine: " << m_engine << "; "
    << "m_files: " << m_files->size() << " [ ";

  std::unique_ptr<Tablespace_file_const_iterator> it(files());

  while (true)
  {
    const Tablespace_file *f= it->next();

    if (!f)
      break;

    std::string ob;
    f->debug_print(ob);
    ss << ob;
  }

  ss << "] }";

  outb= ss.str();
}

///////////////////////////////////////////////////////////////////////////

Tablespace_file *Tablespace_impl::add_file()
{
  return m_files->add(
    Tablespace_file_impl::Factory(this));
}

///////////////////////////////////////////////////////////////////////////

bool Tablespace_impl::remove_file(std::string data_file)
{
  std::unique_ptr<dd::Tablespace_file_iterator> it(files());
  Tablespace_file *tsf;

  while ((tsf= it->next()) != NULL)
  {
    if (!strcmp(tsf->filename().c_str(), data_file.c_str()))
    {
      m_files->remove(dynamic_cast<Tablespace_file_impl*>(tsf));
      return false;
    }
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////

Tablespace_file_const_iterator *Tablespace_impl::files() const
{
  return m_files->const_iterator();
}

///////////////////////////////////////////////////////////////////////////

Tablespace_file_iterator *Tablespace_impl::files()
{
  return m_files->iterator();
}

///////////////////////////////////////////////////////////////////////////
// Tablespace_type implementation.
///////////////////////////////////////////////////////////////////////////

void Tablespace_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Tablespaces>();

  otx->register_tables<Tablespace_file>();
}

///////////////////////////////////////////////////////////////////////////

#ifndef DBUG_OFF
Tablespace_impl::Tablespace_impl(const Tablespace_impl &src)
  : Weak_object(src), Entity_object_impl(src), m_comment(src.m_comment),
    m_options(Properties_impl::parse_properties(src.m_options->raw_string())),
    m_se_private_data(Properties_impl::
                      parse_properties(src.m_se_private_data->raw_string())),
    m_engine(src.m_engine),
    m_files(new Tablespace_file_collection())
{
  typedef Base_collection::Array::const_iterator i_type;
  i_type end= src.m_files->aref().end();
  m_files->aref().reserve(src.m_files->size());
  for (i_type i= src.m_files->aref().begin(); i != end; ++i)
  {
    m_files->aref().push_back(dynamic_cast<Tablespace_file_impl*>(*i)->
                              clone(this));
  }
}
#endif /* !DBUG_OFF */

///////////////////////////////////////////////////////////////////////////

}
