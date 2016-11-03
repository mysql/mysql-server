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

#ifndef DD__SDI_FILE_INCLUDED
#define DD__SDI_FILE_INCLUDED

#include "my_global.h"
#include "dd/string_type.h"     // dd::String_type

class THD;
struct st_mysql_const_lex_string;
struct handlerton;

namespace dd {
class Entity_object;
class Schema;
class Table;

namespace sdi_file {
const size_t FILENAME_PREFIX_CHARS= 16;
String_type sdi_filename(const dd::Entity_object *eo,
                         const String_type &schema);
bool store(THD *thd, const st_mysql_const_lex_string &sdi,
           const dd::Schema *schema);
bool store(THD *thd, handlerton*, const st_mysql_const_lex_string &sdi,
           const dd::Table *table, const dd::Schema *schema);
bool remove(const String_type &fname);
bool remove(THD *thd, const dd::Schema *schema);
bool remove(THD *thd, handlerton*, const dd::Table *table,
            const dd::Schema *schema);
}
}
#endif // !DD__SDI_FILE_INCLUDED
