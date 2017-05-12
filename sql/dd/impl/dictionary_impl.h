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

#ifndef DD__DICTIONARY_IMPL_INCLUDED
#define DD__DICTIONARY_IMPL_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <memory>
#include <memory>
#include <string>

#include "dd/dictionary.h"           // dd::Dictionary
#include "dd/object_id.h"            // dd::Object_id
#include "dd/string_type.h"          // dd::String_type
#include "lex_string.h"
#include "m_string.h"
#include "table.h"                   // MYSQL_SCHEMA_NAME

class THD;
namespace dd {
class Object_table;
}  // namespace dd

namespace dd_schema_unittest {
  class SchemaTest;
}

namespace my_testing {
  class DD_initializer;
}

namespace dd {

///////////////////////////////////////////////////////////////////////////

enum class enum_dd_init_type;
namespace cache {
  class Dictionary_client;
}

///////////////////////////////////////////////////////////////////////////

class Dictionary_impl : public Dictionary
{
  friend class dd_schema_unittest::SchemaTest;
  friend class my_testing::DD_initializer;

  /////////////////////////////////////////////////////////////////////////
  // Implementation details.
  /////////////////////////////////////////////////////////////////////////

private:
  static Dictionary_impl *s_instance;

public:
  static bool init(enum_dd_init_type dd_init);
  static bool shutdown();

  static Dictionary_impl *instance();

private:
  Dictionary_impl()
  { }

public:
  virtual ~Dictionary_impl()
  { }

public:
  static uint get_target_dd_version();

  virtual uint get_actual_dd_version(THD *thd);

  virtual uint get_actual_dd_version(THD *thd, bool *exists);

  virtual const Object_table *get_dd_table(
    const String_type &schema_name, const String_type &table_name) const;

  virtual bool install_plugin_IS_table_metadata();

public:
  virtual bool is_dd_schema_name(const String_type &schema_name) const
  { return (schema_name == MYSQL_SCHEMA_NAME.str); }

  virtual bool is_dd_table_name(const String_type &schema_name,
                               const String_type &table_name) const
  { return (get_dd_table(schema_name, table_name) != NULL); }

  virtual int table_type_error_code(const String_type &schema_name,
                                    const String_type &table_name) const;

  virtual bool is_dd_table_access_allowed(bool is_dd_internal_thread,
                                          bool is_ddl_statement,
                                          const char *schema_name,
                                          size_t schema_length,
                                          const char *table_name) const;

  virtual bool is_system_view_name(const char *schema_name,
                                   const char *table_name) const;

public:
  static Object_id default_catalog_id()
  { return DEFAULT_CATALOG_ID; }

  static const String_type &default_catalog_name()
  { return DEFAULT_CATALOG_NAME; }

private:
  static Object_id DEFAULT_CATALOG_ID;
  static const String_type DEFAULT_CATALOG_NAME;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__DICTIONARY_IMPL_INCLUDED
