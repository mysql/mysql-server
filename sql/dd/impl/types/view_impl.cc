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

#include "dd/impl/types/view_impl.h"

#include "my_user.h"                          // parse_user
#include "mysqld_error.h"                     // ER_*

#include "dd/properties.h"                    // Needed for destructor
#include "dd/impl/collection_impl.h"          // Collection
#include "dd/impl/transaction_impl.h"         // Open_dictionary_tables_ctx
#include "dd/impl/raw/raw_record.h"           // Raw_record
#include "dd/impl/tables/tables.h"            // Tables
#include "dd/impl/tables/view_table_usage.h"  // View_table_usage
#include "dd/impl/types/view_table_impl.h"    // View_table_impl
#include "dd/types/column.h"                  // Column

#include <sstream>

using dd::tables::Tables;
using dd::tables::View_table_usage;

namespace
{
  enum enum_view_updatable_values
  {
    VIEW_NOT_UPDATABLE= 1,
    VIEW_UPDATABLE
  };
}

namespace dd {

///////////////////////////////////////////////////////////////////////////
// View implementation.
///////////////////////////////////////////////////////////////////////////

const Object_type &View::TYPE()
{
  static View_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// View_impl implementation.
///////////////////////////////////////////////////////////////////////////

View_impl::View_impl()
 :m_type(TT_USER_VIEW),
  m_is_updatable(false),
  m_check_option(CO_NONE),
  m_algorithm(VA_UNDEFINED),
  m_security_type(ST_INVOKER),
  m_tables(new View_table_collection()),
  m_client_collation_id(INVALID_OBJECT_ID),
  m_connection_collation_id(INVALID_OBJECT_ID)
{ }

///////////////////////////////////////////////////////////////////////////

bool View_impl::validate() const
{
  if (Abstract_table_impl::validate())
    return true;

  if (m_client_collation_id == INVALID_OBJECT_ID)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             View_impl::OBJECT_TABLE().name().c_str(),
             "No client collation object is associated with View.");
    return true;
  }

  if (m_connection_collation_id == INVALID_OBJECT_ID)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             View_impl::OBJECT_TABLE().name().c_str(),
             "Connection collation ID not set.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool View_impl::restore_children(Open_dictionary_tables_ctx *otx)
{
  return
    Abstract_table_impl::restore_children(otx) ||
    m_tables->restore_items(
      View_table_impl::Factory(this),
      otx,
      otx->get_table<View_table>(),
      View_table_usage::create_key_by_view_id(this->id()));
}

///////////////////////////////////////////////////////////////////////////

bool View_impl::store_children(Open_dictionary_tables_ctx *otx)
{
  return Abstract_table_impl::store_children(otx) ||
         m_tables->store_items(otx);
}

///////////////////////////////////////////////////////////////////////////

bool View_impl::drop_children(Open_dictionary_tables_ctx *otx)
{
  return m_tables->drop_items(
           otx,
           otx->get_table<View_table>(),
           View_table_usage::create_key_by_view_id(this->id())) ||
         Abstract_table_impl::drop_children(otx);
}


///////////////////////////////////////////////////////////////////////////

bool View_impl::restore_attributes(const Raw_record &r)
{
  m_type = (enum_table_type) r.read_int(Tables::FIELD_TYPE);

  if (m_type != TT_USER_VIEW && m_type != TT_SYSTEM_VIEW)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             View_impl::OBJECT_TABLE().name().c_str(),
             "Invalid view type found.");
    return true;
  }

  if (Abstract_table_impl::restore_attributes(r))
    return true;

  m_definition=      r.read_str(Tables::FIELD_VIEW_DEFINITION);
  m_definition_utf8= r.read_str(Tables::FIELD_VIEW_DEFINITION_UTF8);

  {
    std::string definer= r.read_str(Tables::FIELD_VIEW_DEFINER);

    // NOTE: this is a copy/paste from sp_head::set_definer().

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

  m_is_updatable= (r.read_int(Tables::FIELD_VIEW_IS_UPDATABLE) == VIEW_UPDATABLE);

  m_check_option=   (enum_check_option)   r.read_int(Tables::FIELD_VIEW_CHECK_OPTION);
  m_security_type=  (enum_security_type)  r.read_int(Tables::FIELD_VIEW_SECURITY_TYPE);
  m_algorithm=      (enum_algorithm)      r.read_int(Tables::FIELD_VIEW_ALGORITHM);

  m_client_collation_id= r.read_ref_id(Tables::FIELD_VIEW_CLIENT_COLLATION_ID);
  m_connection_collation_id= r.read_ref_id(Tables::FIELD_VIEW_CONNECTION_COLLATION_ID);

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool View_impl::store_attributes(Raw_record *r)
{
  //
  // Store view attributes
  //

  std::stringstream definer;
  definer << m_definer_user << '@' << m_definer_host;

  return
    Abstract_table_impl::store_attributes(r) ||
    r->store(Tables::FIELD_TYPE, m_type) ||
    r->store(Tables::FIELD_VIEW_DEFINITION, m_definition) ||
    r->store(Tables::FIELD_VIEW_DEFINITION_UTF8, m_definition_utf8) ||
    r->store(Tables::FIELD_VIEW_CHECK_OPTION, m_check_option) ||
    r->store(Tables::FIELD_VIEW_IS_UPDATABLE, m_is_updatable ? VIEW_UPDATABLE : VIEW_NOT_UPDATABLE) ||
    r->store(Tables::FIELD_VIEW_ALGORITHM, m_algorithm) ||
    r->store(Tables::FIELD_VIEW_SECURITY_TYPE, m_security_type) ||
    r->store(Tables::FIELD_VIEW_DEFINER, definer.str()) ||
    r->store_ref_id(Tables::FIELD_VIEW_CLIENT_COLLATION_ID, m_client_collation_id) ||
    r->store_ref_id(Tables::FIELD_VIEW_CONNECTION_COLLATION_ID, m_connection_collation_id);
}

///////////////////////////////////////////////////////////////////////////

void View_impl::debug_print(std::string &outb) const
{
  std::stringstream ss;

  std::string s;
  Abstract_table_impl::debug_print(s);

  ss
    << "VIEW OBJECT: { "
    << s
    << "m_definition: " << m_definition << "; "
    << "m_definition_utf8: " << m_definition_utf8 << "; "
    << "m_check_option: " << m_check_option << "; "
    << "m_is_updatable: " << (m_is_updatable ? "yes" : "no") << "; "
    << "m_algorithm: " << m_algorithm << "; "
    << "m_security_type: " << m_security_type << "; "
    << "m_definer_user: " << m_definer_user << "; "
    << "m_definer_host: " << m_definer_host << "; "
    << "m_client_collation: {OID: " << m_client_collation_id << "}; "
    << "m_connection_collation: {OID: " << m_connection_collation_id << "}; "
    << "m_tables: " << m_tables->size() << " [ ";

  std::unique_ptr<View_table_const_iterator> it(tables());

  while (true)
  {
    const View_table *f= it->next();

    if (!f)
      break;

    std::string ob;
    f->debug_print(ob);
    ss << ob;
  }
  ss << "] ";

  ss << " }";

  outb= ss.str();
}

///////////////////////////////////////////////////////////////////////////

View_table *View_impl::add_table()
{
  return m_tables->add(
    View_table_impl::Factory(this));
}

///////////////////////////////////////////////////////////////////////////

View_table_const_iterator *View_impl::tables() const
{
  return m_tables->const_iterator();
}

///////////////////////////////////////////////////////////////////////////

View_table_iterator *View_impl::tables()
{
  return m_tables->iterator();
}

///////////////////////////////////////////////////////////////////////////
// View_type implementation.
///////////////////////////////////////////////////////////////////////////

void View_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Tables>();

  otx->register_tables<Column>();
  otx->register_tables<View_table>();
}

///////////////////////////////////////////////////////////////////////////

#ifndef DBUG_OFF
View_impl::View_impl(const View_impl &src)
  : Weak_object(src), Entity_object_impl(src), Abstract_table_impl(src),
    m_type(src.m_type), m_is_updatable(src.m_is_updatable),
    m_check_option(src.m_check_option), m_algorithm(src.m_algorithm),
    m_security_type(src.m_security_type), m_definition(src.m_definition),
    m_definition_utf8(src.m_definition_utf8),
    m_definer_user(src.m_definer_user), m_definer_host(src.m_definer_host),
    m_tables(new View_table_collection()),
    m_client_collation_id(src.m_client_collation_id),
    m_connection_collation_id(src.m_connection_collation_id)
{
  typedef Base_collection::Array::const_iterator i_type;
  i_type end= src.m_tables->aref().end();
  m_tables->aref().reserve(src.m_tables->size());
  for (i_type i= src.m_tables->aref().begin(); i != end; ++i)
  {
    m_tables->aref().push_back(dynamic_cast<View_table_impl*>(*i)->
                               clone(this));
  }
}
#endif /* !DBUG_OFF */

///////////////////////////////////////////////////////////////////////////

}

