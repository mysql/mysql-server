/* Copyright (c) 2015, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/dd/dd_udt_type.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <memory>  // unique_ptr
#include <unordered_map>

#include "lex_string.h"
#include "m_string.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_sys.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/my_loglevel.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/strings/dtoa.h"
#include "mysql/strings/int2str.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "sql/dd/collection.h"               // dd::Collection
#include "sql/dd/dd.h"                       // dd::get_dictionary
#include "sql/dd/dictionary.h"               // dd::Dictionary
// TODO: Avoid exposing dd/impl headers in public files.
#include "sql/dd/impl/dictionary_impl.h"       // default_catalog_name
#include "sql/dd/impl/system_registry.h"       // dd::System_tables
#include "sql/dd/impl/tables/dd_properties.h"  // dd::tables:.DD_properties
#include "sql/dd/impl/utils.h"                 // dd::escape
#include "sql/dd/performance_schema/init.h"    // performance_schema::
                                               //   set_PS_version_for_table
#include "sql-common/my_decimal.h"
#include "sql/create_field.h"
#include "sql/dd/dd_version.h"  // DD_VERSION
#include "sql/dd/properties.h"  // dd::Properties
#include "sql/dd/string_type.h"
#include "sql/dd/types/schema.h"      // dd::Schema
#include "sql/dd/types/tablespace.h"  // dd::Tablespace
#include "sql/dd/types/udt_type.h"    // dd::UDT_Type
#include "sql/debug_sync.h"           // DEBUG_SYNC
#include "sql/log.h"
#include "sql/mdl.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"          // lower_case_table_names
#include "sql/psi_memory_key.h"  // key_memory_frm
#include "sql/sql_class.h"       // THD
#include "sql/sql_const.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_parse.h"

namespace dd {

bool udt_type_exists(dd::cache::Dictionary_client *client,
                     const char *schema_name, const char *name, bool *exists) {
  DBUG_TRACE;
  assert(exists);

  // Tables exist if they can be acquired.
  dd::cache::Dictionary_client::Auto_releaser releaser(client);
  const dd::UDT_Type *type_obj = nullptr;
  if (client->acquire(schema_name, name, &type_obj)) {
    // Error is reported by the dictionary subsystem.
    return true;
  }
  *exists = (type_obj != nullptr);

  return false;
}

bool create_udt_type(THD *thd, const dd::Schema &sch_obj,
                     const dd::String_type &type_name) {
  std::unique_ptr<dd::UDT_Type> obj(sch_obj.create_udt_type(thd));
  obj->set_name(type_name);
  return thd->dd_client()->store(obj.get());
}

bool drop_udt_type(THD *thd, const dd::UDT_Type &type_def) {
  return thd->dd_client()->drop(&type_def);
}

}  // namespace dd
