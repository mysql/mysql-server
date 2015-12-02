/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__VIEW_IMPL_INCLUDED
#define DD__VIEW_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/os_specific.h"               // DD_HEADER_BEGIN
#include "dd/impl/types/abstract_table_impl.h" // dd::Abstract_table_impl
#include "dd/types/dictionary_object_table.h"  // dd::Dictionary_object_table
#include "dd/types/view.h"                     // dd::View

#include <memory>   // std::unique_ptr

typedef struct charset_info_st CHARSET_INFO;

DD_HEADER_BEGIN

namespace dd {

///////////////////////////////////////////////////////////////////////////

class View_impl : virtual public Abstract_table_impl,
                  virtual public View
{
public:
  typedef Collection<View_table> View_table_collection;

public:
  View_impl();

  virtual ~View_impl()
  { }

public:
  virtual const Dictionary_object_table &object_table() const
  { return View::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_children(Open_dictionary_tables_ctx *otx);

  virtual bool store_children(Open_dictionary_tables_ctx *otx);

  virtual bool drop_children(Open_dictionary_tables_ctx *otx);

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  virtual void debug_print(std::string &outb) const;

public:
  /////////////////////////////////////////////////////////////////////////
  // enum_table_type.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_table_type type() const
  { return m_type; }

  /////////////////////////////////////////////////////////////////////////
  // regular/system view flag.
  /////////////////////////////////////////////////////////////////////////

  virtual void set_system_view(bool system_view)
  { m_type= system_view ? TT_SYSTEM_VIEW : TT_USER_VIEW; }

  /////////////////////////////////////////////////////////////////////////
  // collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id client_collation_id() const
  { return m_client_collation_id; }

  virtual void set_client_collation_id(Object_id client_collation_id)
  { m_client_collation_id= client_collation_id; }

  virtual Object_id connection_collation_id() const
  { return m_connection_collation_id; }

  virtual void set_connection_collation_id(Object_id connection_collation_id)
  { m_connection_collation_id= connection_collation_id; }

  /////////////////////////////////////////////////////////////////////////
  // definition/utf8.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &definition() const
  { return m_definition; }

  virtual void set_definition(const std::string &definition)
  { m_definition= definition; }

  virtual const std::string &definition_utf8() const
  { return m_definition_utf8; }

  virtual void set_definition_utf8(const std::string &definition_utf8)
  { m_definition_utf8= definition_utf8; }

  /////////////////////////////////////////////////////////////////////////
  // check_option.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_check_option check_option() const
  { return m_check_option; }

  virtual void set_check_option(enum_check_option check_option)
  { m_check_option= check_option; }

  /////////////////////////////////////////////////////////////////////////
  // is_updatable.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_updatable() const
  { return m_is_updatable; }

  virtual void set_updatable(bool updatable)
  { m_is_updatable= updatable; }

  /////////////////////////////////////////////////////////////////////////
  // algorithm.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_algorithm algorithm() const
  { return m_algorithm; }

  virtual void set_algorithm(enum_algorithm algorithm)
  { m_algorithm= algorithm; }

  /////////////////////////////////////////////////////////////////////////
  // security_type.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_security_type security_type() const
  { return m_security_type; }

  virtual void set_security_type(enum_security_type security_type)
  { m_security_type= security_type; }

  /////////////////////////////////////////////////////////////////////////
  // definer.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &definer_user() const
  { return m_definer_user; }

  virtual const std::string &definer_host() const
  { return m_definer_host; }

  virtual void set_definer(const std::string &username,
                           const std::string &hostname)
  {
    m_definer_user= username;
    m_definer_host= hostname;
  }

  /////////////////////////////////////////////////////////////////////////
  // View_table collection.
  /////////////////////////////////////////////////////////////////////////

  virtual View_table *add_table();

  virtual View_table_const_iterator *tables() const;

  virtual View_table_iterator *tables();

  View_table_collection *table_collection()
  { return m_tables.get(); }

private:
  enum_table_type    m_type;
  bool               m_is_updatable;
  enum_check_option  m_check_option;
  enum_algorithm     m_algorithm;
  enum_security_type m_security_type;

  std::string m_definition;
  std::string m_definition_utf8;
  std::string m_definer_user;
  std::string m_definer_host;

  // Collections.

  std::unique_ptr<View_table_collection> m_tables;

  // References.

  Object_id m_client_collation_id;
  Object_id m_connection_collation_id;

#ifndef DBUG_OFF
  View_impl(const View_impl &src);
  View_impl *clone() const
  {
    return new View_impl(*this);
  }
#endif /* !DBUG_OFF */
};

///////////////////////////////////////////////////////////////////////////

class View_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) View_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

DD_HEADER_END

#endif // DD__VIEW_IMPL_INCLUDED
