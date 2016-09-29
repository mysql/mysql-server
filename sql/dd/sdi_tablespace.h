/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__SDI_TABLESPACE_INCLUDED
#define DD__SDI_TABLESPACE_INCLUDED

class THD;
struct handlerton;
struct st_mysql_const_lex_string;

namespace dd {
namespace sdi_tablespace {
/**
  See handlerton::store_schema_sdi_t.
*/
bool store(THD *thd, handlerton *hton, const st_mysql_const_lex_string &sdi, const Schema *schema,
           const Table *table);

/**
  See handlerton::store_table_sdi_t.
*/
bool store(THD *thd, handlerton*, const st_mysql_const_lex_string &sdi, const Table *table,
           const dd::Schema *schema);

/**
  See handlerton::store_tablespace_sdi_t.
*/
bool store(handlerton *hton, const st_mysql_const_lex_string &sdi, const Tablespace *tablespace);

/**
  See handlerton::remove_schema_sdi_t.
*/
bool remove(THD *thd, handlerton *hton, const Schema *schema,
            const Table *table);
/**
  See handlerton::remove_table_sdi_t.
*/
bool remove(THD *thd, handlerton*, const Table *table, const Schema *schema);

/**
  See handlerton::remove_tablespace_sdi_t.
*/
bool remove(handlerton *hton, const Tablespace *tablespace);
}
}
#endif // !DD__SDI_TABLESPACE_INCLUDED
