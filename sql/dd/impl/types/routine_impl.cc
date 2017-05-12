/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/types/routine_impl.h"

#include <new>
#include <sstream>

#include "dd/impl/raw/object_keys.h"             // Primary_id_key
#include "dd/impl/raw/raw_record.h"              // Raw_record
#include "dd/impl/tables/parameters.h"           // Parameters
#include "dd/impl/tables/routines.h"             // Routines
#include "dd/impl/transaction_impl.h"            // Open_dictionary_tables_ctx
#include "dd/impl/types/parameter_impl.h"        // Parameter_impl
#include "dd/string_type.h"                      // dd::String_type
#include "dd/types/parameter.h"
#include "dd/types/weak_object.h"
#include "lex_string.h"
#include "my_sys.h"
#include "my_user.h"                             // parse_user
#include "mysql_com.h"
#include "mysqld.h"
#include "mysqld_error.h"

using dd::tables::Routines;
using dd::tables::Parameters;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Routine implementation.
///////////////////////////////////////////////////////////////////////////

const Dictionary_object_table &Routine::OBJECT_TABLE()
{
  return Routines::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Routine::TYPE()
{
  static Routine_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Routine_impl implementation.
///////////////////////////////////////////////////////////////////////////

Routine_impl::Routine_impl()
 :m_routine_type(RT_PROCEDURE),
  m_sql_data_access(SDA_CONTAINS_SQL),
  m_security_type(View::ST_INVOKER),
  m_is_deterministic(false),
  m_sql_mode(0),
  m_created(0),
  m_last_altered(0),
  m_parameters(),
  m_schema_id(INVALID_OBJECT_ID),
  m_client_collation_id(INVALID_OBJECT_ID),
  m_connection_collation_id(INVALID_OBJECT_ID),
  m_schema_collation_id(INVALID_OBJECT_ID)
{ }

Routine_impl::~Routine_impl()
{ }

///////////////////////////////////////////////////////////////////////////

bool Routine_impl::validate() const
{
  if (schema_id() == INVALID_OBJECT_ID)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Routine_impl::OBJECT_TABLE().name().c_str(),
             "Schema ID is not set");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Routine_impl::restore_children(Open_dictionary_tables_ctx *otx)
{
  return m_parameters.restore_items(
    this,
    otx,
    otx->get_table<Parameter>(),
    Parameters::create_key_by_routine_id(this->id()));
}

///////////////////////////////////////////////////////////////////////////

bool Routine_impl::store_children(Open_dictionary_tables_ctx *otx)
{
  return m_parameters.store_items(otx);
}

///////////////////////////////////////////////////////////////////////////

bool Routine_impl::drop_children(Open_dictionary_tables_ctx *otx) const
{
  return m_parameters.drop_items(
    otx,
    otx->get_table<Parameter>(),
    Parameters::create_key_by_routine_id(this->id()));
}

/////////////////////////////////////////////////////////////////////////

bool Routine_impl::restore_attributes(const Raw_record &r)
{
  // Read id and name.
  restore_id(r, Routines::FIELD_ID);
  restore_name(r, Routines::FIELD_NAME);

  // Read enums
  m_routine_type= (enum_routine_type) r.read_int(Routines::FIELD_TYPE);

  m_sql_data_access=
    (enum_sql_data_access) r.read_int(Routines::FIELD_SQL_DATA_ACCESS);

  m_security_type=
    (View::enum_security_type) r.read_int(Routines::FIELD_SECURITY_TYPE);

  // Read booleans
  m_is_deterministic=   r.read_bool(Routines::FIELD_IS_DETERMINISTIC);

  // Read ulonglong
  m_sql_mode=     r.read_int(Routines::FIELD_SQL_MODE);
  m_created=      r.read_int(Routines::FIELD_CREATED);
  m_last_altered= r.read_int(Routines::FIELD_LAST_ALTERED);

  // Read references
  m_schema_id= r.read_ref_id(Routines::FIELD_SCHEMA_ID);
  m_client_collation_id=
    r.read_ref_id(Routines::FIELD_CLIENT_COLLATION_ID);
  m_connection_collation_id=
    r.read_ref_id(Routines::FIELD_CONNECTION_COLLATION_ID);
  m_schema_collation_id=
    r.read_ref_id(Routines::FIELD_SCHEMA_COLLATION_ID);

  // Read strings
  m_definition=            r.read_str(Routines::FIELD_DEFINITION);
  m_definition_utf8=       r.read_str(Routines::FIELD_DEFINITION_UTF8);
  m_parameter_str=         r.read_str(Routines::FIELD_PARAMETER_STR);
  m_comment=               r.read_str(Routines::FIELD_COMMENT);

  // Read definer user/host
  {
    String_type definer= r.read_str(Routines::FIELD_DEFINER);

    char user_name_holder[USERNAME_LENGTH + 1];
    LEX_STRING user_name= { user_name_holder, USERNAME_LENGTH };

    char host_name_holder[HOSTNAME_LENGTH + 1];
    LEX_STRING host_name= { host_name_holder, HOSTNAME_LENGTH };

    parse_user(definer.c_str(), definer.length(),
               user_name.str, &user_name.length,
               host_name.str, &host_name.length);

    m_definer_user.assign(user_name.str, user_name.length);
    m_definer_host.assign(host_name.str, host_name.length);
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Routine_impl::store_attributes(Raw_record *r)
{
  dd::Stringstream_type definer;
  definer << m_definer_user << '@' << m_definer_host;

  return store_id(r, Routines::FIELD_ID) ||
         store_name(r, Routines::FIELD_NAME) ||
         r->store(Routines::FIELD_SCHEMA_ID, m_schema_id) ||
         r->store(Routines::FIELD_NAME, name()) ||
         r->store(Routines::FIELD_TYPE, m_routine_type) ||
         r->store(Routines::FIELD_DEFINITION, m_definition) ||
         r->store(Routines::FIELD_DEFINITION_UTF8, m_definition_utf8) ||
         r->store(Routines::FIELD_PARAMETER_STR, m_parameter_str) ||
         r->store(Routines::FIELD_IS_DETERMINISTIC, m_is_deterministic) ||
         r->store(Routines::FIELD_SQL_DATA_ACCESS, m_sql_data_access) ||
         r->store(Routines::FIELD_SECURITY_TYPE, m_security_type) ||
         r->store(Routines::FIELD_DEFINER, definer.str()) ||
         r->store(Routines::FIELD_SQL_MODE, m_sql_mode) ||
         r->store(Routines::FIELD_CLIENT_COLLATION_ID, m_client_collation_id) ||
         r->store(Routines::FIELD_CONNECTION_COLLATION_ID, m_connection_collation_id) ||
         r->store(Routines::FIELD_SCHEMA_COLLATION_ID, m_schema_collation_id) ||
         r->store(Routines::FIELD_CREATED, m_created) ||
         r->store(Routines::FIELD_LAST_ALTERED, m_last_altered) ||
         r->store(Routines::FIELD_COMMENT, m_comment, m_comment.empty());
}

///////////////////////////////////////////////////////////////////////////

bool Routine::update_id_key(id_key_type *key, Object_id id)
{
  key->update(id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

void Routine_impl::debug_print(String_type &outb) const
{
  dd::Stringstream_type ss;
  ss
  << "id: {OID: " << id() << "}; "
  << "m_name: " << name() << "; "
  << "m_routine_type: " << m_routine_type << "; "
  << "m_sql_data_access: " << m_sql_data_access << "; "
  << "m_security_type: " << m_security_type << "; "
  << "m_is_deterministic: " << m_is_deterministic << "; "
  << "m_sql_mode: " << m_sql_mode << "; "
  << "m_created: " << m_created << "; "
  << "m_last_altered: " << m_last_altered << "; "
  << "m_definition: " << m_definition << "; "
  << "m_definition_utf8: " << m_definition_utf8 << "; "
  << "m_parameter_str: " << m_parameter_str << "; "
  << "m_definer_user: " << m_definer_user << "; "
  << "m_definer_host: " << m_definer_host << "; "
  << "m_comment: " << m_comment << "; "
  << "m_schema_id: {OID: " << m_schema_id << "}; "
  << "m_client_collation_id: " << m_client_collation_id << "; "
  << "m_connection_collation_id: " << m_connection_collation_id << "; "
  << "m_schema_collation_id: " << m_schema_collation_id << "; "
  << "m_parameters: " << m_parameters.size() << " [ ";

  for (const Parameter *f : parameters())
  {
    String_type ob;
    f->debug_print(ob);
    ss << ob;
  }

  ss << "] ";

  outb= ss.str();
}

///////////////////////////////////////////////////////////////////////////

Parameter *Routine_impl::add_parameter()
{
  Parameter_impl *p= new (std::nothrow) Parameter_impl(this);
  m_parameters.push_back(p);
  return p;
}

///////////////////////////////////////////////////////////////////////////
// Routine_type implementation.
///////////////////////////////////////////////////////////////////////////

void Routine_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Routines>();

  otx->register_tables<Parameter>();
}

///////////////////////////////////////////////////////////////////////////

Routine_impl::Routine_impl(const Routine_impl &src)
  :Weak_object(src), Entity_object_impl(src),
   m_routine_type(src.m_routine_type),
   m_sql_data_access(src.m_sql_data_access),
   m_security_type(src.m_security_type),
   m_is_deterministic(src.m_is_deterministic),
   m_sql_mode(src.m_sql_mode),
   m_created(src.m_created),
   m_last_altered(src.m_last_altered),
   m_definition(src.m_definition),
   m_definition_utf8(src.m_definition_utf8),
   m_parameter_str(src.m_parameter_str),
   m_definer_user(src.m_definer_user),
   m_definer_host(src.m_definer_host),
   m_comment(src.m_comment),
   m_parameters(),
   m_schema_id(src.m_schema_id),
   m_client_collation_id(src.m_client_collation_id),
   m_connection_collation_id(src.m_connection_collation_id),
   m_schema_collation_id(src.m_schema_collation_id)
{
  m_parameters.deep_copy(src.m_parameters, this);
}

///////////////////////////////////////////////////////////////////////////

}
