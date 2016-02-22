/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__DICTIONARY_IMPL_INCLUDED
#define DD__DICTIONARY_IMPL_INCLUDED

#include "my_global.h"
#include "table.h"                   // MYSQL_SCHEMA_NAME

#include "dd/dictionary.h"           // dd::Dictionary
#include "dd/object_id.h"            // dd::Object_id

#include <string>
#include <memory>

namespace dd_schema_unittest {
  class SchemaTest;
}

namespace dd {

///////////////////////////////////////////////////////////////////////////

namespace cache {
  class Dictionary_client;
}

///////////////////////////////////////////////////////////////////////////

class Dictionary_impl : public Dictionary
{
  friend class dd_schema_unittest::SchemaTest;

  /////////////////////////////////////////////////////////////////////////
  // Implementation details.
  /////////////////////////////////////////////////////////////////////////

private:
  static Dictionary_impl *s_instance;

public:
  static bool init(bool install);
  static bool shutdown();

  static Dictionary_impl *instance()
  { return s_instance; }

private:
  Dictionary_impl()
  { }

public:
  virtual ~Dictionary_impl()
  { }

public:
  static uint get_target_dd_version();

  virtual uint get_actual_dd_version(THD *thd);

  virtual const Object_table *get_dd_table(
    const std::string &schema_name, const std::string &table_name) const;

public:
  virtual bool is_dd_schema_name(const std::string &schema_name) const
  { return (schema_name == MYSQL_SCHEMA_NAME.str); }

  virtual bool is_dd_table_name(const std::string &schema_name,
                               const std::string &table_name) const
  { return (get_dd_table(schema_name, table_name) != NULL); }

  virtual bool is_system_view_name(const std::string &schema_name,
                                   const std::string &table_name) const;

public:
  static Object_id default_catalog_id()
  { return DEFAULT_CATALOG_ID; }

  static const std::string &default_catalog_name()
  { return DEFAULT_CATALOG_NAME; }

private:
  static Object_id DEFAULT_CATALOG_ID;
  static const std::string DEFAULT_CATALOG_NAME;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__DICTIONARY_IMPL_INCLUDED
