/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_DD_H
#define NDB_DD_H

#include "dd/string_type.h"

namespace dd {
  class Table;
  typedef String_type sdi_t;
}

bool ndb_sdi_serialize(class THD *thd,
                       const dd::Table *table_def,
                       const char* schema_name,
                       const char* tablespace_name,
                       dd::sdi_t& sdi);


void ndb_dd_fix_inplace_alter_table_def(dd::Table *table_def,
                                        const char* proper_table_name);

bool ndb_dd_serialize_table(class THD *thd,
                            const char* schema_name,
                            const char* table_name,
                            const char* tablespace_name,
                            dd::sdi_t& sdi);



bool ndb_dd_install_table(class THD *thd,
                          const char *schema_name,
                          const char *table_name,
                          const dd::sdi_t& sdi, bool force_overwrite);

bool ndb_dd_drop_table(class THD* thd,
                       const char* schema_name,
                       const char* table_name);

bool ndb_dd_rename_table(class THD* thd,
                         const char* old_schema_name,
                         const char* old_table_name,
                         const char* new_schema_name,
                         const char* new_table_name);

bool ndb_dd_table_get_engine(THD *thd,
                             const char *schema_name,
                             const char *table_name,
                             dd::String_type* engine);


/* Functions operating on dd::Table*, prefixed with ndb_dd_table_ */

/*
   Set the se_private_id property in table definition
*/
void ndb_dd_table_set_se_private_id(dd::Table* table_def, int private_id);

#endif
