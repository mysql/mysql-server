/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <utility>

#include "dd/string_type.h"     // dd::String_type
#include "prealloced_array.h"   // Prealloced_array

class THD;
struct st_mysql_const_lex_string;
struct handlerton;

namespace dd {
class Schema;
class Table;

namespace sdi_file {
const size_t FILENAME_PREFIX_CHARS= 16;
template<typename T>
String_type sdi_filename(const T *dd_object,
                         const String_type &schema);
bool store(THD *thd, const st_mysql_const_lex_string &sdi,
           const dd::Schema *schema);
bool store(THD *thd, handlerton*, const st_mysql_const_lex_string &sdi,
           const dd::Table *table, const dd::Schema *schema);
bool remove(const String_type &fname);
bool remove(THD *thd, const dd::Schema *schema);
bool remove(THD *thd, handlerton*, const dd::Table *table,
            const dd::Schema *schema);

/**
  Read an sdi file from disk and store in a buffer.

  @param thd thread handle
  @param fname path to sdi file to load
  @param buf where to store file content
  @retval true if an error occurs
  @retval false otherwise
*/
bool load(THD *thd, const dd::String_type &fname,
          dd::String_type *buf);

/**
  Instantiation of std::pair to represent the full path to an sdi
  file. Member first is the path, second is true if the path is inside
  datadir, false otherwise.
*/
typedef std::pair<dd::String_type, bool> Path_type;

/**
  Typedef for container type to use as out-parameter when expanding
  sdi file patterns into paths.
*/
typedef Prealloced_array<Path_type, 3> Paths_type;

/**
  Expand an sdi filename pattern into the set of full paths that
  match. The paths and a bool indicating if the path is inside data
  dir is appended to the Paths_type collection provided as argument.

  @param thd thread handle
  @param pattern filenam pattern to expand
  @param paths collection of expanded file paths
  @retval true if an error occurs
  @retval false otherwise
*/
bool expand_pattern(THD *thd, const struct st_mysql_lex_string &pattern,
                    Paths_type *paths);

/**
  Check that the MYD and MYI files for table exists.

  @param schema_name
  @param table_name
  @retval true if an error occurs
  @retval false otherwise
 */
bool check_data_files_exist(const dd::String_type &schema_name,
                            const dd::String_type &table_name);
} // sdi_file
} // namespace dd
#endif // !DD__SDI_FILE_INCLUDED
